#include "C_Interactor.h"
#include "MP_Interactable.h"
#include "PDA_Interaction.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UC_Interactor::UC_Interactor()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UC_Interactor::BeginPlay()
{
	Super::BeginPlay();

	//Possession may come after BeginPlay on clients, UpdateFocus checks the local player
	if (GetNetMode() != NM_DedicatedServer)
	{
		GetWorld()->GetTimerManager().SetTimer(FocusTimer, this, &UC_Interactor::UpdateFocus, 0.1f, true);
	}
}

void UC_Interactor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(FocusTimer);
	Super::EndPlay(EndPlayReason);
}

const UPDA_Interaction* UC_Interactor::GetInteractionData() const
{
	if (ensureMsgf(InteractionData, TEXT("%s has no InteractionData, using code defaults"), *GetPathNameSafe(this)))
	{
		return InteractionData;
	}
	return GetDefault<UPDA_Interaction>();
}

void UC_Interactor::TryInteract()
{
	if (AActor* Target = FindLookedAtInteractable())
	{
		InteractWith(Target);
	}
}

void UC_Interactor::InteractWith(AActor* Target)
{
	if (Target)
	{
		Server_Interact(Target);
	}
}

void UC_Interactor::Server_Interact_Implementation(AActor* Target)
{
	ACharacter* User = Cast<ACharacter>(GetOwner());
	IMP_Interactable* Interactable = Cast<IMP_Interactable>(Target);
	if (!User || !Interactable || !Interactable->CanInteract(User))
	{
		return;
	}

	//Reach check with some slack for latency and big actors
	const FBox Bounds = Target->GetComponentsBoundingBox(true);
	const float Distance = FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(User->GetPawnViewLocation()));
	if (Distance > GetInteractionData()->InteractRange + 150.f)
	{
		return;
	}

	Interactable->Interact(User);
}

AActor* UC_Interactor::FindLookedAtInteractable() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}

	const UPDA_Interaction* Data = GetInteractionData();
	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector End = Start + Pawn->GetBaseAimRotation().Vector() * Data->InteractRange;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Interact), false, Pawn);

	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Data->InteractRadius), Params))
	{
		return nullptr;
	}

	AActor* HitActor = Hit.GetActor();
	const IMP_Interactable* Interactable = Cast<IMP_Interactable>(HitActor);
	return Interactable && Interactable->CanInteract(Cast<ACharacter>(GetOwner())) ? HitActor : nullptr;
}

void UC_Interactor::UpdateFocus()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	AActor* NewFocus = FindLookedAtInteractable();
	const IMP_Interactable* Interactable = Cast<IMP_Interactable>(NewFocus);
	const FText NewPrompt = Interactable ? Interactable->GetInteractionText(Cast<ACharacter>(GetOwner())) : FText::GetEmpty();

	if (NewFocus != FocusedActor.Get() || !NewPrompt.EqualTo(FocusedPrompt))
	{
		FocusedActor = NewFocus;
		FocusedPrompt = NewPrompt;
		OnFocusChanged.Broadcast(NewFocus, NewPrompt);
	}

	if (NewFocus && GEngine && GetInteractionData()->bShowDebugPrompt)
	{
		GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.15f, FColor::White, FString::Printf(TEXT("[E] %s"), *NewPrompt.ToString()));
	}
}
