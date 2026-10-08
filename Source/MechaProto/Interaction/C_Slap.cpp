#include "C_Slap.h"
#include "C_Ragdoll.h"
#include "PDA_Interaction.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"

UC_Slap::UC_Slap()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UC_Slap::TrySlap()
{
	if (!CanSlap())
	{
		return;
	}

	//Swing plays right away for the slapper, the server decides the hit
	LastLocalSlapTime = GetWorld()->GetTimeSeconds();
	OnSlapSwing.Broadcast();
	Server_Slap();
}

bool UC_Slap::CanSlap() const
{
	return GetWorld()->GetTimeSeconds() - LastLocalSlapTime >= GetInteractionData()->SlapCooldown && !IsOwnerRagdolled();
}

void UC_Slap::Server_Slap_Implementation()
{
	const UPDA_Interaction* Data = GetInteractionData();
	APawn* Pawn = Cast<APawn>(GetOwner());
	const float Now = GetWorld()->GetTimeSeconds();
	//Small tolerance for network jitter
	if (!Pawn || IsOwnerRagdolled() || Now - LastServerSlapTime < Data->SlapCooldown * 0.8f)
	{
		return;
	}
	LastServerSlapTime = Now;

	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector Direction = Pawn->GetBaseAimRotation().Vector();
	const FVector End = Start + Direction * Data->SlapRange;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Slap), false, Pawn);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Data->SlapRadius), Params);

	bool bHit = false;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (UC_Ragdoll* Ragdoll = HitActor ? HitActor->FindComponentByClass<UC_Ragdoll>() : nullptr)
		{
			Ragdoll->ReceiveSlap(Pawn, Direction, Data->SlapStrength, Data->SlapValue);
			bHit = true;
			break;
		}
	}

	if (Data->bDrawSlapDebug)
	{
		DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, Data->SlapRange * 0.5f + Data->SlapRadius, Data->SlapRadius,
			FRotationMatrix::MakeFromZ(Direction).ToQuat(), bHit ? FColor::Green : FColor::Red, false, 1.f);
	}

	Multicast_PlaySlap();
}

void UC_Slap::Multicast_PlaySlap_Implementation()
{
	//Already played by the slapper
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		return;
	}
	OnSlapSwing.Broadcast();
}

const UPDA_Interaction* UC_Slap::GetInteractionData() const
{
	if (ensureMsgf(InteractionData, TEXT("%s has no InteractionData, using code defaults"), *GetPathNameSafe(this)))
	{
		return InteractionData;
	}
	return GetDefault<UPDA_Interaction>();
}

bool UC_Slap::IsOwnerRagdolled() const
{
	const UC_Ragdoll* Ragdoll = GetOwner() ? GetOwner()->FindComponentByClass<UC_Ragdoll>() : nullptr;
	return Ragdoll && Ragdoll->IsRagdolled();
}
