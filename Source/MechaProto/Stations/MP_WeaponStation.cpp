#include "MP_WeaponStation.h"
#include "C_Ragdoll.h"
#include "C_StationUser.h"
#include "MP_Projectile.h"
#include "PDA_WeaponStation.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

AMP_WeaponStation::AMP_WeaponStation()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PedestalMesh(TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	if (PedestalMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(PedestalMesh.Object);
	}

	TurretYaw = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Yaw"));
	TurretYaw->SetupAttachment(Root);
	TurretYaw->SetRelativeLocation(FVector(0.f, 0.f, 140.f));

	TurretPitch = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Pitch"));
	TurretPitch->SetupAttachment(TurretYaw);

	//Template weapon meshes point along +Y
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gun Mesh"));
	GunMesh->SetupAttachment(TurretPitch);
	GunMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GunMesh->SetRelativeScale3D(FVector(3.f));
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(TurretPitch);
	Muzzle->SetRelativeLocation(FVector(170.f, 0.f, 10.f));

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Arm"));
	CameraArm->SetupAttachment(Root);
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->TargetArmLength = 500.f;
	CameraArm->bDoCollisionTest = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
}

void AMP_WeaponStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_WeaponStation, User);
	DOREPLIFETIME(AMP_WeaponStation, ReplicatedAim);
}

const UPDA_WeaponStation* AMP_WeaponStation::GetStationData() const
{
	if (ensureMsgf(StationData, TEXT("%s has no StationData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StationData;
	}
	return GetDefault<UPDA_WeaponStation>();
}

void AMP_WeaponStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UPDA_WeaponStation* Data = GetStationData();
	CameraArm->TargetArmLength = Data->CameraDistance;

	if (IsLocallyUsed())
	{
		//The user's mouse drives the camera, the turret follows the aim (UC_StationUser calls AimAt)
		CameraArm->SetWorldRotation(User->GetControlRotation());
		return;
	}

	//Other players: smooth toward the replicated aim
	TurretAim = FMath::RInterpTo(TurretAim, ReplicatedAim, DeltaSeconds, Data->TurretTurnSpeed);
	ApplyTurretRotation(TurretAim);
}

bool AMP_WeaponStation::CanInteract(const ACharacter* InUser) const
{
	if (!InUser || (User && User != InUser))
	{
		return false;
	}
	//Taken standing on the ground only (not sliding, on a ladder, falling or ragdolled), left any time
	return User == InUser || (InUser->GetCharacterMovement() && InUser->GetCharacterMovement()->IsMovingOnGround());
}

void AMP_WeaponStation::Interact(ACharacter* InUser)
{
	UC_StationUser* StationUser = InUser ? InUser->FindComponentByClass<UC_StationUser>() : nullptr;
	if (!StationUser)
	{
		return;
	}

	if (User == InUser)
	{
		StationUser->LeaveStation();
	}
	else if (!User)
	{
		StationUser->EnterStation(this);
	}
}

FText AMP_WeaponStation::GetInteractionText(const ACharacter* InUser) const
{
	return FText::Format(NSLOCTEXT("Station", "Use", "Use {0}"), GetStationData()->StationName);
}

void AMP_WeaponStation::SetUser(ACharacter* NewUser)
{
	User = NewUser;
	ForceNetUpdate();
}

bool AMP_WeaponStation::IsLocallyUsed() const
{
	return User && User->IsLocallyControlled();
}

FVector AMP_WeaponStation::ComputeAimPoint() const
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * GetStationData()->AimDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(StationAim), false, this);
	Params.AddIgnoredActor(User);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
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
	return GetWorld()->GetTimeSeconds() - LastFireTime >= GetStationData()->FireCooldown;
}

void AMP_WeaponStation::ServerFire(const FVector& AimPoint)
{
	if (!HasAuthority() || !User)
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

	Multicast_Fired();
}

void AMP_WeaponStation::Multicast_Fired_Implementation()
{
	//The user already played it when pressing fire
	if (!IsLocallyUsed())
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

void AMP_WeaponStation::Explode(const FVector& Center)
{
	const UPDA_WeaponStation* Data = GetStationData();
	const float Radius = Data->ExplosionRadius;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius));

	TSet<AActor*> ThrownPlayers;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		AActor* Actor = Overlap.GetActor();
		if (Component && Component->IsSimulatingPhysics())
		{
			Component->AddRadialImpulse(Center, Radius, Data->ExplosionImpulse, RIF_Linear, true);
		}

		UC_Ragdoll* Ragdoll = Actor ? Actor->FindComponentByClass<UC_Ragdoll>() : nullptr;
		if (Ragdoll && Data->bExplosionRagdollsPlayers && !ThrownPlayers.Contains(Actor))
		{
			ThrownPlayers.Add(Actor);
			const FVector Away = (Actor->GetActorLocation() - Center).GetSafeNormal2D();
			Ragdoll->StartRagdoll(Away * Data->ExplosionPlayerImpulse + FVector::UpVector * Data->ExplosionPlayerImpulseUp);
		}
	}

	Multicast_Exploded(Center);
}

void AMP_WeaponStation::Multicast_Exploded_Implementation(FVector_NetQuantize Center)
{
	const UPDA_WeaponStation* Data = GetStationData();
	if (Data->ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->ExplosionSound, Center);
	}
	if (Data->ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Data->ExplosionEffect, Center);
	}
	if (Data->bDrawExplosionDebug)
	{
		DrawDebugSphere(GetWorld(), Center, Data->ExplosionRadius, 16, FColor::Orange, false, 0.6f);
	}
	OnExploded(Center, Data->ExplosionRadius);
}
