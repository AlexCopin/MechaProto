#include "MP_WeaponStation.h"
#include "C_StationUser.h"
#include "MP_Projectile.h"
#include "PDA_WeaponStation.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMP_WeaponStation::AMP_WeaponStation()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PedestalMesh(TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlockMesh(TEXT("/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	if (PedestalMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(PedestalMesh.Object);
	}

	InteractVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("Interact Volume"));
	InteractVolume->SetupAttachment(Root);
	InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	InteractVolume->SetBoxExtent(FVector(100.f, 100.f, 140.f));
	InteractVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractVolume->SetGenerateOverlapEvents(false);
	InteractVolume->SetCanEverAffectNavigation(false);

	TurretYaw = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Yaw"));
	TurretYaw->SetupAttachment(Root);
	TurretYaw->SetRelativeLocation(FVector(0.f, 0.f, 140.f));

	TurretPitch = CreateDefaultSubobject<USceneComponent>(TEXT("Turret Pitch"));
	TurretPitch->SetupAttachment(TurretYaw);

	//Capsule center of a sitting user, floating 95 cm behind the gun and tilting with it: the gun is at the seated shoulders (95 cm above the feet, capsule half height 96)
	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Seat->SetupAttachment(TurretPitch);
	Seat->SetRelativeLocation(FVector(-95.f, 0.f, 0.f));

	//The bench sit pose puts the pelvis 53 cm up and 33 cm behind the capsule center (13 in the pose + 20 mesh offset)
	SeatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Seat Mesh"));
	SeatMesh->SetupAttachment(Seat);
	SeatMesh->SetRelativeLocation(FVector(-33.f, 0.f, -74.f));
	SeatMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.44f));
	SeatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BackrestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backrest Mesh"));
	BackrestMesh->SetupAttachment(Seat);
	BackrestMesh->SetRelativeLocation(FVector(-59.f, 0.f, -22.f));
	BackrestMesh->SetRelativeScale3D(FVector(0.08f, 0.45f, 0.6f));
	BackrestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BlockMesh.Succeeded())
	{
		SeatMesh->SetStaticMesh(BlockMesh.Object);
		BackrestMesh->SetStaticMesh(BlockMesh.Object);
	}

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
