#include "MP_Station.h"
#include "C_StationUser.h"
#include "MP_Breakable.h"
#include "PDA_Station.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMP_Station::AMP_Station()
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

	//Capsule center of a sitting user: the gun height of a weapon station (capsule half height 96, seated shoulders 95 cm above the feet)
	Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
	Seat->SetupAttachment(Root);
	Seat->SetRelativeLocation(FVector(0.f, 0.f, 140.f));

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

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Arm"));
	CameraArm->SetupAttachment(Root);
	CameraArm->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->TargetArmLength = 500.f;
	CameraArm->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
}

void AMP_Station::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Station, User);
}

const UPDA_Station* AMP_Station::GetBaseStationData() const
{
	return GetDefault<UPDA_Station>();
}

float AMP_Station::GetCameraDistance() const
{
	return GetBaseStationData()->CameraDistance;
}

FVector AMP_Station::GetCameraPivotLocation() const
{
	return Root->GetComponentTransform().TransformPosition(GetBaseStationData()->CameraOffset);
}

void AMP_Station::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UPDA_Station* Data = GetBaseStationData();
	CameraArm->TargetArmLength = GetCameraDistance();
	CameraArm->SetWorldLocation(GetCameraPivotLocation());
	CameraArm->bDoCollisionTest = Data->bCameraCollision;
	Camera->SetFieldOfView(Data->CameraFieldOfView);

	//The user's mouse turns the camera
	if (IsLocallyUsed())
	{
		CameraArm->SetWorldRotation(User->GetControlRotation());
	}
}

bool AMP_Station::CanInteract(const ACharacter* InUser) const
{
	if (!InUser || (User && User != InUser))
	{
		return false;
	}
	//Taken standing on the ground only (not sliding, on a ladder, falling or ragdolled), left any time
	return User == InUser || (InUser->GetCharacterMovement() && InUser->GetCharacterMovement()->IsMovingOnGround());
}

void AMP_Station::Interact(ACharacter* InUser)
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

FText AMP_Station::GetInteractionText(const ACharacter* InUser) const
{
	return FText::Format(NSLOCTEXT("Station", "Use", "Use {0}"), GetBaseStationData()->StationName);
}

void AMP_Station::SetUser(ACharacter* NewUser)
{
	User = NewUser;
	ForceNetUpdate();
}

bool AMP_Station::IsLocallyUsed() const
{
	return User && User->IsLocallyControlled();
}

bool AMP_Station::IsDisabled() const
{
	return GetBrokenSystem() != nullptr;
}

AMP_Breakable* AMP_Station::GetBrokenSystem() const
{
	for (AMP_Breakable* System : RequiredSystems)
	{
		if (System && System->IsBroken())
		{
			return System;
		}
	}
	return nullptr;
}
