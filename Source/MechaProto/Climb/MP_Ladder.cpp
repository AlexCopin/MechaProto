#include "MP_Ladder.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	//Engine cube is 100 cm
	constexpr float CubeSize = 100.f;
	constexpr float RailThickness = 6.f;
	constexpr float RungThickness = 4.f;
}

AMP_Ladder::AMP_Ladder()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ClimbVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("Climb Volume"));
	ClimbVolume->SetupAttachment(Root);
	ClimbVolume->SetCollisionProfileName(TEXT("Trigger"));
	ClimbVolume->SetGenerateOverlapEvents(true);

	Segments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Segments"));
	Segments->SetupAttachment(Root);

	LeftRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Left Rail"));
	LeftRail->SetupAttachment(Root);
	RightRail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Right Rail"));
	RightRail->SetupAttachment(Root);
	Rungs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rungs"));
	Rungs->SetupAttachment(Root);
	Rungs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		LeftRail->SetStaticMesh(CubeMesh.Object);
		RightRail->SetStaticMesh(CubeMesh.Object);
		Rungs->SetStaticMesh(CubeMesh.Object);
	}

	ClimbSideArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Climb Side"));
	ClimbSideArrow->SetupAttachment(Root);
	ClimbSideArrow->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	ClimbSideArrow->ArrowSize = 1.5f;
}

void AMP_Ladder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const bool bUseMesh = LadderMesh != nullptr;
	Segments->ClearInstances();
	Rungs->ClearInstances();
	for (UStaticMeshComponent* Placeholder : { LeftRail.Get(), RightRail.Get() })
	{
		Placeholder->SetVisibility(!bUseMesh);
		Placeholder->SetCollisionEnabled(bUseMesh ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}

	if (bUseMesh)
	{
		BuildFromMesh();
	}
	else
	{
		BuildPlaceholder();
	}

	//From behind the ladder to GrabDepth in front, up to above the top to grab it from the platform
	const float TopMargin = 120.f;
	ClimbVolume->SetBoxExtent(FVector(GrabDepth * 0.5f + 10.f, Width * 0.5f + 20.f, (Height + TopMargin) * 0.5f));
	ClimbVolume->SetRelativeLocation(FVector(FrontOffset + GrabDepth * 0.5f - 10.f, 0.f, (Height + TopMargin) * 0.5f));
}

void AMP_Ladder::BuildFromMesh()
{
	Segments->SetStaticMesh(LadderMesh);

	const FBox Bounds = LadderMesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize();
	if (Size.X <= UE_KINDA_SMALL_NUMBER || Size.Y <= UE_KINDA_SMALL_NUMBER || Size.Z <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	//Widest horizontal side becomes the width (actor Y), the other one the depth (actor X)
	const bool bWidthAlongX = Size.X >= Size.Y;
	const FRotator Rotation = bWidthAlongX ? FRotator(0.f, 90.f, 0.f) : FRotator::ZeroRotator;
	const float MeshWidth = bWidthAlongX ? Size.X : Size.Y;
	const float MeshDepth = bWidthAlongX ? Size.Y : Size.X;

	const float Scale = Width / MeshWidth;
	const int32 Count = FMath::Max(1, FMath::RoundToInt(Height / (Size.Z * Scale)));
	const float ScaleZ = Height / (Count * Size.Z);
	const FVector Scale3D(Scale, Scale, ScaleZ);
	FrontOffset = MeshDepth * Scale * 0.5f;

	//Mesh center and bottom moved to the actor origin
	const FVector Center = Bounds.GetCenter();
	const FVector Pivot = Rotation.RotateVector(FVector(Center.X, Center.Y, Bounds.Min.Z) * Scale3D);
	const float SegmentHeight = Size.Z * ScaleZ;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Location = FVector(0.f, 0.f, Index * SegmentHeight) - Pivot;
		Segments->AddInstance(FTransform(Rotation, Location, Scale3D));
	}
}

void AMP_Ladder::BuildPlaceholder()
{
	const float HalfWidth = Width * 0.5f;
	FrontOffset = RailThickness * 0.5f;

	LeftRail->SetRelativeLocation(FVector(0.f, -HalfWidth, Height * 0.5f));
	RightRail->SetRelativeLocation(FVector(0.f, HalfWidth, Height * 0.5f));
	LeftRail->SetRelativeScale3D(FVector(RailThickness, RailThickness, Height) / CubeSize);
	RightRail->SetRelativeScale3D(FVector(RailThickness, RailThickness, Height) / CubeSize);

	for (float Z = RungSpacing; Z < Height; Z += RungSpacing)
	{
		Rungs->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, Z), FVector(RungThickness, Width, RungThickness) / CubeSize));
	}
}

FVector AMP_Ladder::GetClimbNormal() const
{
	return GetActorForwardVector().GetSafeNormal2D();
}

float AMP_Ladder::GetBottomZ() const
{
	return GetActorLocation().Z;
}

float AMP_Ladder::GetTopZ() const
{
	return GetActorLocation().Z + Height;
}

FVector AMP_Ladder::GetClimbLocation(float Z, float Distance) const
{
	const FVector Base = GetActorLocation() + GetClimbNormal() * (FrontOffset + Distance);
	return FVector(Base.X, Base.Y, Z);
}
