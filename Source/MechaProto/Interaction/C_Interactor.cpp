#include "C_Interactor.h"
#include "MP_Interactable.h"
#include "PDA_Interaction.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
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
	SetHighlight(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UC_Interactor::SetHighlight(AActor* Actor)
{
	for (const FHighlightedMesh& Highlighted : HighlightedMeshes)
	{
		if (UMeshComponent* Mesh = Highlighted.Mesh.Get())
		{
			Mesh->SetOverlayMaterial(Highlighted.PreviousOverlay.Get());
		}
	}
	HighlightedMeshes.Reset();

	UMaterialInterface* Overlay = GetInteractionData()->FocusOverlayMaterial;
	AppliedOverlay = Overlay;
	if (!Actor || !Overlay)
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->IsVisible())
		{
			HighlightedMeshes.Add({ Mesh, Mesh->GetOverlayMaterial() });
			Mesh->SetOverlayMaterial(Overlay);
		}
	}
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
	const ACharacter* User = Cast<ACharacter>(GetOwner());
	return FindTarget(User, GetInteractionData(), [User](AActor* Actor)
	{
		const IMP_Interactable* Interactable = Cast<IMP_Interactable>(Actor);
		return Interactable && Interactable->CanInteract(User);
	});
}

AActor* UC_Interactor::FindTarget(const APawn* Pawn, const UPDA_Interaction* Data, TFunctionRef<bool(AActor*)> IsTarget, const AActor* IgnoredActor)
{
	const UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World || !Data)
	{
		return nullptr;
	}

	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector Direction = Pawn->GetBaseAimRotation().Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Interact), false, Pawn);
	if (IgnoredActor)
	{
		Params.AddIgnoredActor(IgnoredActor);
	}

	//Exactly what is looked at (big things, stations)
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Start, Start + Direction * Data->InteractRange, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Data->InteractRadius), Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && IsTarget(HitActor))
		{
			return HitActor;
		}
	}
	if (Data->InteractAssistAngle <= 0.f)
	{
		return nullptr;
	}

	//Aim assist: the target whose center is closest to the view direction, nothing in between
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Start, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Data->InteractRange), Params);
	AActor* Best = nullptr;
	float BestDot = FMath::Cos(FMath::DegreesToRadians(Data->InteractAssistAngle));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Actor || !Component || Actor == Best)
		{
			continue;
		}

		const FVector Center = Component->Bounds.Origin;
		const FVector ToCenter = Center - Start;
		const float Distance = ToCenter.Size();
		if (Distance > Data->InteractRange || Distance < 1.f)
		{
			continue;
		}
		const float Dot = FVector::DotProduct(ToCenter / Distance, Direction);
		if (Dot <= BestDot || !IsTarget(Actor))
		{
			continue;
		}

		FHitResult SightHit;
		if (World->LineTraceSingleByChannel(SightHit, Start, Center, ECC_Visibility, Params) && SightHit.GetActor() != Actor)
		{
			continue;
		}
		Best = Actor;
		BestDot = Dot;
	}
	return Best;
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
		if (NewFocus != FocusedActor.Get())
		{
			SetHighlight(NewFocus);
		}
		FocusedActor = NewFocus;
		FocusedPrompt = NewPrompt;
		OnFocusChanged.Broadcast(NewFocus, NewPrompt);
	}
	//Follows edits of the overlay in the data during PIE
	else if (NewFocus && AppliedOverlay.Get() != GetInteractionData()->FocusOverlayMaterial)
	{
		SetHighlight(NewFocus);
	}

	if (NewFocus && GEngine && GetInteractionData()->bShowDebugPrompt)
	{
		GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.15f, FColor::White, FString::Printf(TEXT("[E] %s"), *NewPrompt.ToString()));
	}
}
