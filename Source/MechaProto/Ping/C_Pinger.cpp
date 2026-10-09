#include "C_Pinger.h"
#include "MechaProto.h"
#include "C_ItemHolder.h"
#include "C_StationUser.h"
#include "MP_Breakable.h"
#include "MP_Enemy.h"
#include "MP_HullPlate.h"
#include "MP_Item.h"
#include "MP_Mech.h"
#include "MP_Ping.h"
#include "MP_Station.h"
#include "PDA_Station.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	//The server accepts a little faster than the cooldown (network jitter)
	constexpr float PingCooldownTolerance = 0.8f;
	//Beyond MaxDistance from the pawn: the pilot's chase camera is far behind it
	constexpr float PingReachMargin = 6000.f;
}

UC_Pinger::UC_Pinger()
{
	PrimaryComponentTick.bCanEverTick = false;
	//Server RPCs need a replicated component
	SetIsReplicatedByDefault(true);
}

const UPDA_Ping* UC_Pinger::GetPingData() const
{
	if (ensureMsgf(PingData, TEXT("%s has no PingData, using code defaults"), *GetPathNameSafe(this)))
	{
		return PingData;
	}
	return GetDefault<UPDA_Ping>();
}

void UC_Pinger::RequestPing()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPingTime < GetPingData()->Cooldown)
	{
		return;
	}

	AActor* Target = nullptr;
	USceneComponent* Component = nullptr;
	FVector Point = FVector::ZeroVector;
	if (!FindPingTarget(Target, Component, Point))
	{
		return;
	}
	LastPingTime = Now;
	Server_Ping(Target, Component, Point);
}

bool UC_Pinger::IsOutsideView() const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const AMP_Station* Station = Controller ? Cast<AMP_Station>(Controller->GetViewTarget()) : nullptr;
	return Station && !Station->GetBaseStationData()->bCameraCollision;
}

EMP_PingType UC_Pinger::ClassifyTarget(const AActor* Target) const
{
	if (const AMP_Enemy* Enemy = Cast<AMP_Enemy>(Target))
	{
		return Enemy->IsDead() ? EMP_PingType::Location : EMP_PingType::Enemy;
	}
	if (Cast<AMP_Breakable>(Target))
	{
		return EMP_PingType::System;
	}
	if (Cast<AMP_Item>(Target))
	{
		return EMP_PingType::Item;
	}
	if (const AMP_HullPlate* Plate = Cast<AMP_HullPlate>(Target))
	{
		return !GetPingData()->bHullOnlyWhenDamaged || Plate->IsDamaged() ? EMP_PingType::Hull : EMP_PingType::Location;
	}
	return EMP_PingType::Location;
}

bool UC_Pinger::FindPingTarget(AActor*& OutTarget, USceneComponent*& OutComponent, FVector& OutPoint) const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller)
	{
		return false;
	}
	const UPDA_Ping* Data = GetPingData();
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Direction = ViewRotation.Vector();
	const FVector End = ViewLocation + Direction * Data->MaxDistance;

	//Not the pinger's own body, seat or held item
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Ping), false);
	if (const APawn* Pawn = Controller->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);
		const UC_StationUser* StationUser = Pawn->FindComponentByClass<UC_StationUser>();
		if (StationUser && StationUser->GetStation())
		{
			Params.AddIgnoredActor(StationUser->GetStation());
		}
		const UC_ItemHolder* ItemHolder = Pawn->FindComponentByClass<UC_ItemHolder>();
		if (ItemHolder && ItemHolder->GetHeldItem())
		{
			Params.AddIgnoredActor(ItemHolder->GetHeldItem());
		}
	}

	//Station cameras see through the mech like its guns (projectile channel); inside, the glass and the intact portholes are windows
	const bool bOutside = IsOutsideView();
	if (!bOutside)
	{
		if (const AMP_Mech* Mech = AMP_Mech::FindMech(this))
		{
			TArray<UPrimitiveComponent*> Windows;
			Mech->GetSeeThroughComponents(Windows);
			for (const UPrimitiveComponent* Window : Windows)
			{
				Params.AddIgnoredComponent(Window);
			}
		}
		for (TActorIterator<AMP_HullPlate> It(GetWorld()); It; ++It)
		{
			if (!It->IsDamaged())
			{
				Params.AddIgnoredActor(*It);
			}
		}
	}
	const ECollisionChannel Channel = bOutside ? ECC_Projectile : ECC_Visibility;
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, Channel, Params);
	const float HitDistance = bHit ? Hit.Distance : Data->MaxDistance;
	AActor* Target = bHit && ClassifyTarget(Hit.GetActor()) != EMP_PingType::Location ? Hit.GetActor() : nullptr;

	//Nothing typed hit: the target closest to the aim's line in front of the hit, seen from the view
	if (!Target)
	{
		float BestCos = -1.f;
		auto Consider = [&](AActor* Candidate, const FVector& Center, float MaxAngle, float MaxRange)
		{
			const FVector ToCenter = Center - ViewLocation;
			const float Distance = ToCenter.Size();
			const float Cos = Distance > 1.f ? FVector::DotProduct(ToCenter / Distance, Direction) : -1.f;
			if (Distance > FMath::Min(MaxRange, HitDistance + 100.f) || Cos < FMath::Cos(FMath::DegreesToRadians(MaxAngle)) || Cos <= BestCos)
			{
				return;
			}
			FHitResult Sight;
			FCollisionQueryParams SightParams = Params;
			SightParams.AddIgnoredActor(Candidate);
			if (GetWorld()->LineTraceSingleByChannel(Sight, ViewLocation, Center, Channel, SightParams))
			{
				return;
			}
			Target = Candidate;
			BestCos = Cos;
		};
		for (TActorIterator<AMP_Enemy> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead())
			{
				Consider(*It, It->GetActorLocation(), Data->EnemyAssistAngle, Data->MaxDistance);
			}
		}
		if (!bOutside)
		{
			for (TActorIterator<AMP_Item> It(GetWorld()); It; ++It)
			{
				Consider(*It, It->GetActorLocation(), Data->SmallAssistAngle, Data->SmallAssistRange);
			}
			for (TActorIterator<AMP_Breakable> It(GetWorld()); It; ++It)
			{
				if (It->IsBroken())
				{
					Consider(*It, It->GetComponentsBoundingBox().GetCenter(), Data->SmallAssistAngle, Data->SmallAssistRange);
				}
			}
		}

		//A damaged plate or a hole whose opening the aim goes through (a hole has no collision); from outside, only seen from its outer side
		float BestHullDistance = HitDistance + 100.f;
		for (TActorIterator<AMP_HullPlate> It(GetWorld()); It && !Target; ++It)
		{
			if (Data->bHullOnlyWhenDamaged && !It->IsDamaged())
			{
				continue;
			}
			if (bOutside && FVector::DotProduct(Direction, It->GetOutsideDirection()) > 0.f)
			{
				continue;
			}
			const FTransform& PlateTransform = It->GetActorTransform();
			FVector HitLocation;
			FVector HitNormal;
			float HitTime = 0.f;
			if (FMath::LineExtentBoxIntersection(It->GetOpeningBox(), PlateTransform.InverseTransformPosition(ViewLocation), PlateTransform.InverseTransformPosition(End), FVector::ZeroVector, HitLocation, HitNormal, HitTime)
				&& HitTime * Data->MaxDistance < BestHullDistance)
			{
				BestHullDistance = HitTime * Data->MaxDistance;
				Target = *It;
			}
		}
	}

	if (Target)
	{
		OutTarget = Target;
		OutComponent = nullptr;
		OutPoint = Target->GetActorLocation();
		return true;
	}
	if (!bHit)
	{
		return false;
	}

	//A spot: on something moving (the mech's structure) it moves with it
	USceneComponent* Component = Hit.GetComponent();
	OutTarget = nullptr;
	if (Component && Component->Mobility == EComponentMobility::Movable && Component->IsSupportedForNetworking())
	{
		OutComponent = Component;
		OutPoint = Component->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	}
	else
	{
		OutComponent = nullptr;
		OutPoint = Hit.ImpactPoint;
	}
	return true;
}

void UC_Pinger::Server_Ping_Implementation(AActor* Target, USceneComponent* Component, FVector_NetQuantize10 Point)
{
	const UPDA_Ping* Data = GetPingData();
	const float Now = GetWorld()->GetTimeSeconds();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || Now - LastServerPingTime < Data->Cooldown * PingCooldownTolerance)
	{
		return;
	}

	//In reach of the player
	const EMP_PingType Type = ClassifyTarget(Target);
	if (Type == EMP_PingType::Location)
	{
		Target = nullptr;
	}
	const FVector WorldPoint = Target ? Target->GetActorLocation() : (Component ? Component->GetComponentTransform().TransformPosition(Point) : FVector(Point));
	const APawn* Pawn = Controller->GetPawn();
	if (Pawn && FVector::Dist(Pawn->GetActorLocation(), WorldPoint) > Data->MaxDistance + PingReachMargin)
	{
		return;
	}
	LastServerPingTime = Now;

	//The same target or a spot next to one of yours replaces it, beyond the max the oldest goes
	LivePings.RemoveAll([](const TWeakObjectPtr<AMP_Ping>& Ping) { return !Ping.IsValid(); });
	for (int32 Index = LivePings.Num() - 1; Index >= 0; --Index)
	{
		AMP_Ping* Ping = LivePings[Index].Get();
		const bool bSameTarget = Target && Ping->GetTargetActor() == Target;
		const bool bSameSpot = !Target && !Ping->GetTargetActor() && FVector::Dist(Ping->GetMarkerLocation(), WorldPoint) < Data->ReplaceRadius;
		if (bSameTarget || bSameSpot)
		{
			Ping->Destroy();
			LivePings.RemoveAt(Index);
		}
	}
	while (LivePings.Num() >= FMath::Max(1, Data->MaxLivePingsPerPlayer))
	{
		LivePings[0]->Destroy();
		LivePings.RemoveAt(0);
	}

	const FTransform SpawnTransform(WorldPoint);
	AMP_Ping* Ping = GetWorld()->SpawnActorDeferred<AMP_Ping>(AMP_Ping::StaticClass(), SpawnTransform, Controller, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ping)
	{
		return;
	}
	Ping->InitPing(PingData, Controller->PlayerState, Type, Target, Target ? nullptr : Component, Point);
	Ping->FinishSpawning(SpawnTransform);
	LivePings.Add(Ping);
	UE_LOG(LogMechaProto, Verbose, TEXT("%s pinged %s at %s (%s)"), *GetNameSafe(Controller->PlayerState), *GetNameSafe(Target), *WorldPoint.ToString(), *UEnum::GetValueAsString(Type));
}
