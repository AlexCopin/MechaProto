#include "MP_WeaponStation.h"
#include "MechaProto.h"
#include "MP_Projectile.h"
#include "PDA_WeaponStation.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AMP_WeaponStation::AMP_WeaponStation()
{
	TurretYaw = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Yaw"));
	TurretYaw->SetupAttachment(Root);
	TurretYaw->SetRelativeLocation(FVector(0.f, 0.f, 140.f));

	TurretPitch = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Pitch"));
	TurretPitch->SetupAttachment(TurretYaw);

	//Floating 95 cm behind the gun and tilting with it: the gun is at the seated shoulders
	Seat->SetupAttachment(TurretPitch);
	Seat->SetRelativeLocation(FVector(-95.f, 0.f, 0.f));

	//Template weapon meshes point along +Y
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gun Mesh"));
	GunMesh->SetupAttachment(TurretPitch);
	GunMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GunMesh->SetRelativeScale3D(FVector(3.f));
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(TurretPitch);
	Muzzle->SetRelativeLocation(FVector(170.f, 0.f, 10.f));
}

void AMP_WeaponStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMount();
}

void AMP_WeaponStation::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyMount();
}

void AMP_WeaponStation::ApplyMount()
{
	TurretYaw->SetRelativeLocation(TurretOffset);
	if (bSeatOnTurret)
	{
		return;
	}

	//Seated at the station with a console in front, the gun is elsewhere
	if (Seat->GetAttachParent() != Root)
	{
		Seat->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	}
	Seat->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 140.f), FRotator::ZeroRotator);
	BaseMesh->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
	BaseMesh->SetRelativeScale3D(FVector(0.4f, 0.9f, 0.9f));
}

FVector AMP_WeaponStation::GetCameraPivot() const
{
	return TurretYaw->GetRelativeLocation() + GetStationData()->CameraOffset;
}

void AMP_WeaponStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_WeaponStation, ReplicatedAim);
	DOREPLIFETIME(AMP_WeaponStation, FireCounter);
}

const UPDA_WeaponStation* AMP_WeaponStation::GetStationData() const
{
	if (ensureMsgf(StationData, TEXT("%s has no StationData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StationData;
	}
	return GetDefault<UPDA_WeaponStation>();
}

const UPDA_Station* AMP_WeaponStation::GetBaseStationData() const
{
	return GetStationData();
}

void AMP_WeaponStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//The user's turret follows its aim (UC_StationUser calls AimAt), the other players smooth toward the replicated one
	if (!IsLocallyUsed())
	{
		TurretAim = FMath::RInterpTo(TurretAim, ReplicatedAim, DeltaSeconds, GetStationData()->TurretTurnSpeed);
		ApplyTurretRotation(TurretAim);
	}
}

FVector AMP_WeaponStation::ComputeAimPoint() const
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * GetStationData()->AimDistance;

	//The projectile channel: through the mech's own walls (the camera is often outside), stops on enemies and the ground
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StationAim), false, this);
	Params.AddIgnoredActor(User);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Projectile, Params) ? Hit.ImpactPoint : End;
}

void AMP_WeaponStation::AimAt(const FVector& AimPoint)
{
	TurretAim = (AimPoint - TurretPitch->GetComponentLocation()).Rotation();
	ApplyTurretRotation(TurretAim);

	if (HasAuthority())
	{
		ReplicatedAim = TurretAim;
	}
}

void AMP_WeaponStation::ApplyTurretRotation(const FRotator& WorldAim)
{
	TurretYaw->SetWorldRotation(FRotator(0.f, WorldAim.Yaw, 0.f));
	TurretPitch->SetRelativeRotation(FRotator(WorldAim.Pitch, 0.f, 0.f));
}

bool AMP_WeaponStation::IsFireReady(float LastFireTime) const
{
	return !IsDisabled() && GetWorld()->GetTimeSeconds() - LastFireTime >= GetStationData()->FireCooldown;
}

void AMP_WeaponStation::ServerFire(const FVector& AimPoint)
{
	if (!HasAuthority() || !User || IsDisabled())
	{
		return;
	}

	//Small tolerance for network jitter
	const UPDA_WeaponStation* Data = GetStationData();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerFireTime < Data->FireCooldown * 0.8f)
	{
		return;
	}
	LastServerFireTime = Now;

	AimAt(AimPoint);
	const FVector MuzzleLocation = Muzzle->GetComponentLocation();
	FVector Direction = (AimPoint - MuzzleLocation).GetSafeNormal();
	if (Direction.IsNearlyZero() || FVector::DotProduct(Direction, Muzzle->GetForwardVector()) < 0.f)
	{
		Direction = Muzzle->GetForwardVector();
	}
	if (Data->SpreadAngle > 0.f)
	{
		Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(Data->SpreadAngle));
	}

	const FTransform SpawnTransform(Direction.Rotation(), MuzzleLocation);
	AMP_Projectile* Projectile = GetWorld()->SpawnActorDeferred<AMP_Projectile>(AMP_Projectile::StaticClass(), SpawnTransform, this, User, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Projectile)
	{
		Projectile->InitProjectile(StationData);
		Projectile->FinishSpawning(SpawnTransform);
	}

	//Replicated to the others, the server plays its own when someone else fires (OnRep doesn't run here)
	++FireCounter;
	if (!IsLocallyUsed() && GetNetMode() != NM_DedicatedServer)
	{
		PlayFireEffects();
	}
}

void AMP_WeaponStation::OnRep_FireCounter()
{
	//The user already played it when pressing fire, nobody fires an unused station (late join)
	if (User && !IsLocallyUsed())
	{
		PlayFireEffects();
	}
}

void AMP_WeaponStation::PlayFireEffects()
{
	if (USoundBase* Sound = GetStationData()->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Muzzle->GetComponentLocation());
	}
	OnFired();
}
