#include "C_PlayerRecovery.h"
#include "C_Ragdoll.h"
#include "C_StationUser.h"
#include "MP_Mech.h"
#include "PDA_Recovery.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	constexpr float OutsideCheckInterval = 0.25f;
}

UC_PlayerRecovery::UC_PlayerRecovery()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UC_PlayerRecovery::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UC_PlayerRecovery, AutoRespawnTime, COND_OwnerOnly);
	DOREPLIFETIME(UC_PlayerRecovery, RespawnCount);
}

const UPDA_Recovery* UC_PlayerRecovery::GetRecoveryData() const
{
	if (ensureMsgf(RecoveryData, TEXT("%s has no RecoveryData, using code defaults"), *GetPathNameSafe(this)))
	{
		return RecoveryData;
	}
	return GetDefault<UPDA_Recovery>();
}

void UC_PlayerRecovery::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(CheckTimer, this, &UC_PlayerRecovery::CheckOutside, OutsideCheckInterval, true, FMath::FRand() * OutsideCheckInterval);
	}
}

void UC_PlayerRecovery::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(CheckTimer);
	Super::EndPlay(EndPlayReason);
}

void UC_PlayerRecovery::CheckOutside()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const AMP_Mech* Mech = AMP_Mech::FindMech(this);
	const UPDA_Recovery* Data = GetRecoveryData();

	//Seated or ragdolled doesn't count (a ragdoll gets up first)
	const UC_StationUser* StationUser = Character ? Character->FindComponentByClass<UC_StationUser>() : nullptr;
	const UC_Ragdoll* Ragdoll = Character ? Character->FindComponentByClass<UC_Ragdoll>() : nullptr;
	const bool bBusy = (StationUser && StationUser->IsManning()) || (Ragdoll && Ragdoll->IsRagdolled());
	if (!Character || !Mech || !Data->bAutoRespawnOutside || bBusy || Mech->IsOnMech(Character->GetActorLocation(), Data->OnMechTraceDepth))
	{
		AutoRespawnTime = 0.f;
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (AutoRespawnTime <= 0.f)
	{
		AutoRespawnTime = Now + Data->OutsideRespawnDelay;
	}
	else if (Now >= AutoRespawnTime)
	{
		Respawn();
	}
}

void UC_PlayerRecovery::Respawn()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	//A free start on the mech (attached to it), in random order
	const AMP_Mech* Mech = AMP_Mech::FindMech(this);
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (!Mech || It->IsAttachedTo(Mech))
		{
			Starts.Add(*It);
		}
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RespawnSpot), false, Character);
	AActor* Start = nullptr;
	while (!Start && Starts.Num() > 0)
	{
		APlayerStart* Candidate = Starts[FMath::RandRange(0, Starts.Num() - 1)];
		Starts.Remove(Candidate);
		if (!GetWorld()->OverlapAnyTestByChannel(Candidate->GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Params))
		{
			Start = Candidate;
		}
	}
	if (!Start)
	{
		AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
		Start = GameMode ? GameMode->FindPlayerStart(Character->GetController()) : nullptr;
	}
	if (!Start)
	{
		return;
	}

	//Out of a ladder or a slide, falls onto the start's floor
	const FRotator Rotation(0.f, Start->GetActorRotation().Yaw, 0.f);
	Character->TeleportTo(Start->GetActorLocation(), Rotation);
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Falling);
	}
	if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
	{
		Controller->ClientSetRotation(Rotation);
	}

	AutoRespawnTime = 0.f;
	++RespawnCount;
	OnRespawned.Broadcast();
}

float UC_PlayerRecovery::GetAutoRespawnTimeLeft() const
{
	if (AutoRespawnTime <= 0.f)
	{
		return -1.f;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.f, AutoRespawnTime - static_cast<float>(Now));
}

void UC_PlayerRecovery::OnRep_RespawnCount()
{
	OnRespawned.Broadcast();
}
