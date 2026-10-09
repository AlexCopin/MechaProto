#include "MP_Mech.h"
#include "MechaProto.h"
#include "MP_HullPlate.h"
#include "MP_LookoutStation.h"
#include "MP_MechBlock.h"
#include "MP_WeaponStation.h"
#include "PDA_Mech.h"
#include "Algo/BinarySearch.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	//Engine cube
	constexpr float CubeSize = 100.f;
	//Farther than this from the server's state, a client jumps there (late join)
	constexpr float NetSnapDistance = 500.f;
	constexpr double NetSnapGaitPhase = 1.5;

	//Sorted, without near duplicates
	void CleanCuts(TArray<double>& Cuts)
	{
		Cuts.Sort();
		TArray<double> Unique;
		for (const double Cut : Cuts)
		{
			if (Unique.IsEmpty() || Cut - Unique.Last() > 0.5)
			{
				Unique.Add(Cut);
			}
		}
		Cuts = MoveTemp(Unique);
	}

	//The cut a box face landed on (CleanCuts keeps the first of near duplicates)
	int32 CutIndex(const TArray<double>& Cuts, double Value)
	{
		return Algo::LowerBound(Cuts, Value - 0.5);
	}

	//The floor the crew stands on, their movement base: Movable and the same object on every machine (a default subobject)
	void SetupStructureMesh(UInstancedStaticMeshComponent* Mesh, UStaticMesh* Cube, UMaterialInterface* Material)
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		//Its own weapons fire through it, the station cameras see through it
		Mesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMaterial(0, Material);
		//Color (3) and emissive strength (1) per instance, read by M_MechPaint
		Mesh->NumCustomDataFloats = 4;
	}

	//Looks only: no collision, not in the distance fields (big plates around rooms would darken them for Lumen)
	void SetupArmorMesh(UInstancedStaticMeshComponent* Mesh, UStaticMesh* Cube, UMaterialInterface* Material)
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bAffectDynamicIndirectLighting = false;
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMaterial(0, Material);
		Mesh->NumCustomDataFloats = 4;
	}

	//Moving parts after the body: 0-1 legs, 2-3 arms
	constexpr int32 MovingPartCount = 5;
	constexpr EMP_MechPart MovingParts[MovingPartCount] = { EMP_MechPart::LeftLeg, EMP_MechPart::RightLeg, EMP_MechPart::LeftArm, EMP_MechPart::RightArm, EMP_MechPart::Head };

	int32 MovingIndexOf(EMP_MechPart Part)
	{
		for (int32 Index = 0; Index < MovingPartCount; ++Index)
		{
			if (MovingParts[Index] == Part)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	//How often an arm looks for the weapon station inside it while it has none
	constexpr float ArmStationSearchInterval = 1.f;

	//How far outside the wall a porthole cuts the armor, and how deep under it the armor around a room stays
	constexpr double OpeningArmorReach = 150.0;
	constexpr double ArmorRoomMargin = 15.0;
	//A porthole's opening goes a little beyond its glass on both sides
	constexpr double OpeningExtraDepth = 10.0;

	bool Overlaps(const FBox& A, const FBox& B)
	{
		return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y && A.Min.Z < B.Max.Z && B.Min.Z < A.Max.Z;
	}

	int32 ThinAxis(const FVector& Size)
	{
		return Size.X <= Size.Y && Size.X <= Size.Z ? 0 : (Size.Y <= Size.Z ? 1 : 2);
	}

	void SortUnique(TArray<double>& Values)
	{
		Values.Sort();
		TArray<double> Unique;
		for (const double Value : Values)
		{
			if (Unique.IsEmpty() || Value - Unique.Last() > 0.01)
			{
				Unique.Add(Value);
			}
		}
		Values = MoveTemp(Unique);
	}

	//A flat plate minus the holes crossing it, as few boxes as possible
	void SubtractHoles(const FBox& Plate, const TArray<FBox>& Holes, TArray<FBox>& Out)
	{
		const int32 Thin = ThinAxis(Plate.GetSize());
		const int32 A = Thin == 0 ? 1 : 0;
		const int32 B = Thin == 2 ? 1 : 2;
		TArray<FBox> Cuts;
		for (const FBox& Hole : Holes)
		{
			if (Overlaps(Hole, Plate))
			{
				Cuts.Add(Hole.Overlap(Plate));
			}
		}
		if (Cuts.IsEmpty())
		{
			Out.Add(Plate);
			return;
		}

		TArray<double> As = { Plate.Min[A], Plate.Max[A] };
		TArray<double> Bs = { Plate.Min[B], Plate.Max[B] };
		for (const FBox& Cut : Cuts)
		{
			As.Append({ Cut.Min[A], Cut.Max[A] });
			Bs.Append({ Cut.Min[B], Cut.Max[B] });
		}
		SortUnique(As);
		SortUnique(Bs);
		const int32 NA = As.Num() - 1;
		const int32 NB = Bs.Num() - 1;
		TArray<uint8> Filled;
		Filled.Init(0, NA * NB);
		for (int32 J = 0; J < NB; ++J)
		{
			for (int32 I = 0; I < NA; ++I)
			{
				const double CA = (As[I] + As[I + 1]) * 0.5;
				const double CB = (Bs[J] + Bs[J + 1]) * 0.5;
				bool bInHole = false;
				for (const FBox& Cut : Cuts)
				{
					if (CA > Cut.Min[A] && CA < Cut.Max[A] && CB > Cut.Min[B] && CB < Cut.Max[B])
					{
						bInHole = true;
						break;
					}
				}
				Filled[J * NA + I] = bInHole ? 0 : 1;
			}
		}
		for (int32 J = 0; J < NB; ++J)
		{
			for (int32 I = 0; I < NA; ++I)
			{
				if (!Filled[J * NA + I])
				{
					continue;
				}
				int32 I2 = I;
				while (I2 + 1 < NA && Filled[J * NA + I2 + 1])
				{
					++I2;
				}
				auto IsRowFilled = [&](int32 Row)
				{
					for (int32 II = I; II <= I2; ++II)
					{
						if (!Filled[Row * NA + II])
						{
							return false;
						}
					}
					return true;
				};
				int32 J2 = J;
				while (J2 + 1 < NB && IsRowFilled(J2 + 1))
				{
					++J2;
				}
				for (int32 JJ = J; JJ <= J2; ++JJ)
				{
					for (int32 II = I; II <= I2; ++II)
					{
						Filled[JJ * NA + II] = 0;
					}
				}
				FBox Piece = Plate;
				Piece.Min[A] = As[I];
				Piece.Max[A] = As[I2 + 1];
				Piece.Min[B] = Bs[J];
				Piece.Max[B] = Bs[J2 + 1];
				Out.Add(Piece);
			}
		}
	}
}

AMP_Mech::AMP_Mech()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	//Moved by its own simulation on every machine, see Tick
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	UStaticMesh* Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UMaterialInterface* Material = ShapeMaterial.Succeeded() ? ShapeMaterial.Object : nullptr;

	Structure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Structure"));
	Structure->SetupAttachment(Root);
	SetupStructureMesh(Structure, Cube, Material);

	Armor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Armor"));
	Armor->SetupAttachment(Root);
	SetupArmorMesh(Armor, Cube, Material);

	//Each leg: a pivot at the hip (placed by BuildStructure) holding its structure and armor
	LeftLegPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Left Leg Pivot"));
	LeftLegPivot->SetupAttachment(Root);
	LeftLegPivot->SetMobility(EComponentMobility::Movable);
	LeftLegStructure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Left Leg Structure"));
	LeftLegStructure->SetupAttachment(LeftLegPivot);
	SetupStructureMesh(LeftLegStructure, Cube, Material);
	LeftLegArmor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Left Leg Armor"));
	LeftLegArmor->SetupAttachment(LeftLegPivot);
	SetupArmorMesh(LeftLegArmor, Cube, Material);

	RightLegPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Right Leg Pivot"));
	RightLegPivot->SetupAttachment(Root);
	RightLegPivot->SetMobility(EComponentMobility::Movable);
	RightLegStructure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Right Leg Structure"));
	RightLegStructure->SetupAttachment(RightLegPivot);
	SetupStructureMesh(RightLegStructure, Cube, Material);
	RightLegArmor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Right Leg Armor"));
	RightLegArmor->SetupAttachment(RightLegPivot);
	SetupArmorMesh(RightLegArmor, Cube, Material);

	//Each arm: a pivot at the shoulder holding its structure and armor
	LeftArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Left Arm Pivot"));
	LeftArmPivot->SetupAttachment(Root);
	LeftArmPivot->SetMobility(EComponentMobility::Movable);
	LeftArmStructure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Left Arm Structure"));
	LeftArmStructure->SetupAttachment(LeftArmPivot);
	SetupStructureMesh(LeftArmStructure, Cube, Material);
	LeftArmArmor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Left Arm Armor"));
	LeftArmArmor->SetupAttachment(LeftArmPivot);
	SetupArmorMesh(LeftArmArmor, Cube, Material);

	RightArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Right Arm Pivot"));
	RightArmPivot->SetupAttachment(Root);
	RightArmPivot->SetMobility(EComponentMobility::Movable);
	RightArmStructure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Right Arm Structure"));
	RightArmStructure->SetupAttachment(RightArmPivot);
	SetupStructureMesh(RightArmStructure, Cube, Material);
	RightArmArmor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Right Arm Armor"));
	RightArmArmor->SetupAttachment(RightArmPivot);
	SetupArmorMesh(RightArmArmor, Cube, Material);

	//The head: a pivot on the neck holding its structure, armor and glass
	HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("Head Pivot"));
	HeadPivot->SetupAttachment(Root);
	HeadPivot->SetMobility(EComponentMobility::Movable);
	HeadStructure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Head Structure"));
	HeadStructure->SetupAttachment(HeadPivot);
	SetupStructureMesh(HeadStructure, Cube, Material);
	HeadArmor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Head Armor"));
	HeadArmor->SetupAttachment(HeadPivot);
	SetupArmorMesh(HeadArmor, Cube, Material);

	//Same collision as the structure: blocks the crew, its own guns and cameras go through
	Glass = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Glass"));
	Glass->SetupAttachment(Root);
	Glass->SetMobility(EComponentMobility::Movable);
	Glass->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Glass->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	Glass->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Glass->SetCanEverAffectNavigation(false);
	Glass->SetCastShadow(false);
	Glass->SetStaticMesh(Cube);

	HeadGlass = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Head Glass"));
	HeadGlass->SetupAttachment(HeadPivot);
	HeadGlass->SetMobility(EComponentMobility::Movable);
	HeadGlass->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HeadGlass->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	HeadGlass->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	HeadGlass->SetCanEverAffectNavigation(false);
	HeadGlass->SetCastShadow(false);
	HeadGlass->SetStaticMesh(Cube);
}

void AMP_Mech::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Mech, PilotInput);
	DOREPLIFETIME(AMP_Mech, MoveState);
}

const UPDA_Mech* AMP_Mech::GetMechData() const
{
	if (ensureMsgf(MechData, TEXT("%s has no MechData, using code defaults"), *GetPathNameSafe(this)))
	{
		return MechData;
	}
	return GetDefault<UPDA_Mech>();
}

void AMP_Mech::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (MechData)
	{
		ApplyLook();
	}
	BuildStructure();
}

void AMP_Mech::BuildStructure()
{
	BuildStructureIgnoring(nullptr);
}

void AMP_Mech::BuildStructureIgnoring(const AActor* Ignored)
{
#if WITH_EDITOR
	GatherBlocks(Ignored);
#endif
	UpdatePartShapes();

	//Every cavity, plus each porthole's opening (no zone: a dark frame around the glass)
	const TArray<FOpening> Openings = GatherOpenings(Ignored);
	TArray<FMP_MechBox> AllCavities = Cavities;
	for (const FOpening& Opening : Openings)
	{
		FMP_MechBox& Box = AllCavities.AddDefaulted_GetRef();
		Box.Name = TEXT("Porthole");
		Box.Min = Opening.Box.Min;
		Box.Max = Opening.Box.Max;
	}

	UInstancedStaticMeshComponent* Structures[MovingPartCount + 1] = { Structure, LeftLegStructure, RightLegStructure, LeftArmStructure, RightArmStructure, HeadStructure };
	UInstancedStaticMeshComponent* Armors[MovingPartCount + 1] = { Armor, LeftLegArmor, RightLegArmor, LeftArmArmor, RightArmArmor, HeadArmor };
	for (int32 Index = 0; Index <= MovingPartCount; ++Index)
	{
		Structures[Index]->ClearInstances();
		Structures[Index]->SetNumCustomDataFloats(4);
		Armors[Index]->ClearInstances();
		Armors[Index]->SetNumCustomDataFloats(4);
	}
	//Glass on the head turns with it, the rest stays with the body
	Glass->ClearInstances();
	HeadGlass->ClearInstances();
	for (const FMP_MechBox& Pane : GlassPanes)
	{
		const bool bOnHead = Pane.Part == EMP_MechPart::Head && HeadShape.bValid;
		const FVector Center = (Pane.Min + Pane.Max) * 0.5 - (bOnHead ? HeadShape.Pivot : FVector::ZeroVector);
		(bOnHead ? HeadGlass : Glass)->AddInstance(FTransform(FRotator::ZeroRotator, Center, (Pane.Max - Pane.Min) / CubeSize));
	}

	//Moving parts at rest on their pivots, their pieces relative to it. What hangs on a pivot stays where it is when the pivot moves
	for (int32 Index = 0; Index < MovingPartCount; ++Index)
	{
		USceneComponent* PivotComponent = GetMovingPivotComponent(Index);
		FVector PivotLocation = FVector::ZeroVector;
		GetMovingPivot(Index, PivotLocation);
		TArray<TPair<USceneComponent*, FTransform>> Hanging;
		for (USceneComponent* Child : PivotComponent->GetAttachChildren())
		{
			if (Child && Child->GetOwner() != this)
			{
				Hanging.Emplace(Child, Child->GetComponentTransform());
			}
		}
		PivotComponent->SetRelativeLocationAndRotation(PivotLocation, FRotator::ZeroRotator);
		for (const TPair<USceneComponent*, FTransform>& Pair : Hanging)
		{
			Pair.Key->SetWorldTransform(Pair.Value);
		}
	}
	auto PartSlot = [this](EMP_MechPart Part)
	{
		const int32 Moving = MovingIndexOf(Part);
		FVector Unused;
		return Moving != INDEX_NONE && GetMovingPivot(Moving, Unused) ? Moving + 1 : 0;
	};
	auto PartOrigin = [this](EMP_MechPart Part)
	{
		FVector Pivot = FVector::ZeroVector;
		const int32 Moving = MovingIndexOf(Part);
		if (Moving != INDEX_NONE)
		{
			GetMovingPivot(Moving, Pivot);
		}
		return Pivot;
	};

	for (const FMP_MechArmor& Plate : ArmorPlates)
	{
		AddPlate(Plate, AllCavities, Openings, Armors[PartSlot(Plate.Part)], PartOrigin(Plate.Part));
	}

	BuildPart(EMP_MechPart::Body, AllCavities, Structure, FVector::ZeroVector);
	for (int32 Index = 0; Index < MovingPartCount; ++Index)
	{
		FVector Pivot;
		if (GetMovingPivot(Index, Pivot))
		{
			BuildPart(MovingParts[Index], AllCavities, Structures[Index + 1], Pivot);
		}
	}
}

TArray<AMP_Mech::FOpening> AMP_Mech::GatherOpenings(const AActor* Ignored) const
{
	TArray<FOpening> Openings;
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (const AActor* Actor : Attached)
	{
		const AMP_HullPlate* Plate = Cast<AMP_HullPlate>(Actor);
		if (!Plate || Plate == Ignored)
		{
			continue;
		}
		//Through the wall: a little deeper than the glass on both sides
		FBox Local = Plate->GetOpeningBox();
		const double Extra = OpeningExtraDepth / FMath::Max(Plate->GetActorScale3D().X, 0.01);
		Local.Min.X -= Extra;
		Local.Max.X += Extra;
		const FTransform PlateTransform = Plate->GetActorTransform().GetRelativeTransform(GetActorTransform());
		const FVector Forward = PlateTransform.GetUnitAxis(EAxis::X);
		FOpening& Opening = Openings.AddDefaulted_GetRef();
		Opening.Box = Local.TransformBy(PlateTransform);
		Opening.NormalAxis = FMath::Abs(Forward.X) >= FMath::Abs(Forward.Y) && FMath::Abs(Forward.X) >= FMath::Abs(Forward.Z) ? 0 : (FMath::Abs(Forward.Y) >= FMath::Abs(Forward.Z) ? 1 : 2);
	}
	return Openings;
}

void AMP_Mech::AddPlate(const FMP_MechArmor& Plate, const TArray<FMP_MechBox>& AllCavities, const TArray<FOpening>& Openings, UInstancedStaticMeshComponent* Target, const FVector& Origin) const
{
	auto AddInstance = [&](const FRotator& Rotation, const FVector& Center, const FVector& Size)
	{
		const int32 Index = Target->AddInstance(FTransform(Rotation, Center - Origin, Size / CubeSize));
		Target->SetCustomData(Index, { Plate.Color.R, Plate.Color.G, Plate.Color.B, Plate.Emissive });
	};
	//Tilted plates (slopes, slides) stay whole
	if (!Plate.Rotation.IsNearlyZero(0.01))
	{
		AddInstance(Plate.Rotation, Plate.Center, Plate.Size);
		return;
	}

	const FBox PlateBox(Plate.Center - Plate.Size * 0.5, Plate.Center + Plate.Size * 0.5);
	bool bInside = false;
	for (const FMP_MechBox& Cavity : AllCavities)
	{
		if (FBox(Cavity.Min, Cavity.Max).IsInsideOrOn(Plate.Center))
		{
			bInside = true;
			break;
		}
	}

	TArray<FBox> Holes;
	if (bInside)
	{
		//Decor: the plate pushed into its wall a little, so the doorways, holes and portholes in that wall cut it
		FBox Probe = PlateBox;
		const int32 Thin = ThinAxis(Probe.GetSize());
		Probe.Min[Thin] -= 3.0;
		Probe.Max[Thin] += 3.0;
		for (const FMP_MechBox& Cavity : AllCavities)
		{
			const FBox CavityBox(Cavity.Min, Cavity.Max);
			if (!CavityBox.IsInsideOrOn(Plate.Center) && Overlaps(CavityBox, Probe))
			{
				FBox Hole = CavityBox;
				Hole.Min[Thin] = Probe.Min[Thin] - 1.0;
				Hole.Max[Thin] = Probe.Max[Thin] + 1.0;
				Holes.Add(Hole);
			}
		}
	}
	else
	{
		//Armor: never over a porthole, never inside a room (what is left stays deep in the walls)
		for (const FOpening& Opening : Openings)
		{
			FBox Hole = Opening.Box;
			Hole.Min[Opening.NormalAxis] -= OpeningArmorReach;
			Hole.Max[Opening.NormalAxis] += OpeningArmorReach;
			Holes.Add(Hole);
		}
		for (const FMP_MechBox& Cavity : AllCavities)
		{
			Holes.Add(FBox(Cavity.Min, Cavity.Max).ExpandBy(ArmorRoomMargin));
		}
	}

	TArray<FBox> Pieces;
	SubtractHoles(PlateBox, Holes, Pieces);
	for (const FBox& Piece : Pieces)
	{
		AddInstance(FRotator::ZeroRotator, Piece.GetCenter(), Piece.GetSize());
	}
}

void AMP_Mech::BuildPart(EMP_MechPart Part, const TArray<FMP_MechBox>& AllCavities, UInstancedStaticMeshComponent* Target, const FVector& Origin)
{
	//A moving part without a pivot is built with the body
	TArray<FMP_MechBox> PartSolids;
	FBox PartBounds(ForceInit);
	for (const FMP_MechBox& Box : SolidBlocks)
	{
		const int32 Moving = MovingIndexOf(Box.Part);
		FVector Unused;
		const EMP_MechPart BoxPart = Moving != INDEX_NONE && GetMovingPivot(Moving, Unused) ? Box.Part : EMP_MechPart::Body;
		if (BoxPart == Part)
		{
			PartSolids.Add(Box);
			PartBounds += FBox(Box.Min, Box.Max);
		}
	}
	if (PartSolids.IsEmpty())
	{
		return;
	}

	const UPDA_Mech* Data = MechData ? MechData.Get() : GetDefault<UPDA_Mech>();
	const double Lining = Data->ZoneLiningThickness;
	TMap<FName, int32> ZoneIndices;
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		ZoneIndices.Add(Zones[Index].Name, Index);
	}

	//Every cavity carves every part: the ones reaching this part's solids or their lining
	TArray<FMP_MechBox> PartCavities;
	const FBox Reach = PartBounds.ExpandBy(Lining + 1.0);
	for (const FMP_MechBox& Box : AllCavities)
	{
		if (Reach.Intersect(FBox(Box.Min, Box.Max)))
		{
			PartCavities.Add(Box);
		}
	}

	//A grid on every face of every box, plus the lining depth around the painted cavities
	TArray<double> Cuts[3];
	for (const FMP_MechBox& Box : PartSolids)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Cuts[Axis].Append({ Box.Min[Axis], Box.Max[Axis] });
		}
	}
	for (const FMP_MechBox& Box : PartCavities)
	{
		const bool bPainted = ZoneIndices.Contains(Box.Zone);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Cuts[Axis].Append({ Box.Min[Axis], Box.Max[Axis] });
			if (bPainted)
			{
				Cuts[Axis].Append({ Box.Min[Axis] - Lining, Box.Max[Axis] + Lining });
			}
		}
	}
	for (TArray<double>& AxisCuts : Cuts)
	{
		CleanCuts(AxisCuts);
	}
	const TArray<double>& Xs = Cuts[0];
	const TArray<double>& Ys = Cuts[1];
	const TArray<double>& Zs = Cuts[2];
	const int32 NX = Xs.Num() - 1;
	const int32 NY = Ys.Num() - 1;
	const int32 NZ = Zs.Num() - 1;
	if (NX <= 0 || NY <= 0 || NZ <= 0)
	{
		return;
	}

	auto CellIndex = [NX, NY](int32 I, int32 J, int32 K)
	{
		return (K * NY + J) * NX + I;
	};
	//Cells covered by a box (from its min cut to its max cut)
	auto ForEachCell = [&](const FVector& Min, const FVector& Max, TFunctionRef<void(int32 I, int32 J, int32 K)> Function)
	{
		const int32 I0 = CutIndex(Xs, Min.X);
		const int32 I1 = CutIndex(Xs, Max.X);
		const int32 J0 = CutIndex(Ys, Min.Y);
		const int32 J1 = CutIndex(Ys, Max.Y);
		const int32 K0 = CutIndex(Zs, Min.Z);
		const int32 K1 = CutIndex(Zs, Max.Z);
		for (int32 K = K0; K < K1; ++K)
		{
			for (int32 J = J0; J < J1; ++J)
			{
				for (int32 I = I0; I < I1; ++I)
				{
					Function(I, J, K);
				}
			}
		}
	};

	//0 empty, 1 filled, 2 filled and already in a box: inside a block and outside every cavity
	TArray<uint8> Cells;
	Cells.SetNumZeroed(NX * NY * NZ);
	for (const FMP_MechBox& Box : PartSolids)
	{
		ForEachCell(Box.Min, Box.Max, [&](int32 I, int32 J, int32 K) { Cells[CellIndex(I, J, K)] = 1; });
	}
	for (const FMP_MechBox& Box : PartCavities)
	{
		ForEachCell(Box.Min, Box.Max, [&](int32 I, int32 J, int32 K) { Cells[CellIndex(I, J, K)] = 0; });
	}

	//Paint 0 is the frame, then 3 per zone: wall, floor, ceiling. A filled cell within the lining depth of a painted cavity, facing one of
	//its sides (not its edges or corners), takes that side's color; the closest cavity wins
	TArray<FLinearColor> PaintColors = { Data->Color };
	for (const FMP_MechZone& Zone : Zones)
	{
		PaintColors.Append({ Zone.WallColor, Zone.FloorColor, Zone.CeilingColor });
	}
	TArray<uint16> Paint;
	Paint.SetNumZeroed(Cells.Num());
	TArray<double> PaintDistance;
	PaintDistance.Init(TNumericLimits<double>::Max(), Cells.Num());
	for (const FMP_MechBox& Box : PartCavities)
	{
		const int32* ZoneIndex = ZoneIndices.Find(Box.Zone);
		if (!ZoneIndex)
		{
			continue;
		}
		ForEachCell(Box.Min - FVector(Lining), Box.Max + FVector(Lining), [&](int32 I, int32 J, int32 K)
		{
			const int32 Index = CellIndex(I, J, K);
			if (Cells[Index] != 1)
			{
				return;
			}
			const FVector Center((Xs[I] + Xs[I + 1]) * 0.5, (Ys[J] + Ys[J + 1]) * 0.5, (Zs[K] + Zs[K + 1]) * 0.5);
			int32 OutsideAxis = INDEX_NONE;
			double Distance = 0.0;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double Outside = FMath::Max(Box.Min[Axis] - Center[Axis], Center[Axis] - Box.Max[Axis]);
				if (Outside > 0.0)
				{
					if (OutsideAxis != INDEX_NONE)
					{
						return;
					}
					OutsideAxis = Axis;
					Distance = Outside;
				}
			}
			if (OutsideAxis == INDEX_NONE || Distance >= PaintDistance[Index])
			{
				return;
			}
			const int32 Side = OutsideAxis != 2 ? 0 : (Center.Z < Box.Min.Z ? 1 : 2);
			Paint[Index] = static_cast<uint16>(1 + *ZoneIndex * 3 + Side);
			PaintDistance[Index] = Distance;
		});
	}

	//Greedy merge of same paint cells into boxes: grow along X, then Y, then Z
	TArray<FTransform> Instances;
	TArray<FLinearColor> InstanceColors;
	for (int32 K = 0; K < NZ; ++K)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const int32 StartIndex = CellIndex(I, J, K);
				if (Cells[StartIndex] != 1)
				{
					continue;
				}
				const uint16 BoxPaint = Paint[StartIndex];
				auto IsFree = [&](int32 II, int32 JJ, int32 KK)
				{
					const int32 Index = CellIndex(II, JJ, KK);
					return Cells[Index] == 1 && Paint[Index] == BoxPaint;
				};

				int32 I2 = I;
				while (I2 + 1 < NX && IsFree(I2 + 1, J, K))
				{
					++I2;
				}
				auto IsRowFree = [&](int32 Row, int32 Layer)
				{
					for (int32 II = I; II <= I2; ++II)
					{
						if (!IsFree(II, Row, Layer))
						{
							return false;
						}
					}
					return true;
				};
				int32 J2 = J;
				while (J2 + 1 < NY && IsRowFree(J2 + 1, K))
				{
					++J2;
				}
				auto IsLayerFree = [&](int32 Layer)
				{
					for (int32 JJ = J; JJ <= J2; ++JJ)
					{
						if (!IsRowFree(JJ, Layer))
						{
							return false;
						}
					}
					return true;
				};
				int32 K2 = K;
				while (K2 + 1 < NZ && IsLayerFree(K2 + 1))
				{
					++K2;
				}

				for (int32 KK = K; KK <= K2; ++KK)
				{
					for (int32 JJ = J; JJ <= J2; ++JJ)
					{
						for (int32 II = I; II <= I2; ++II)
						{
							Cells[CellIndex(II, JJ, KK)] = 2;
						}
					}
				}
				const FVector Min(Xs[I], Ys[J], Zs[K]);
				const FVector Max(Xs[I2 + 1], Ys[J2 + 1], Zs[K2 + 1]);
				Instances.Add(FTransform(FRotator::ZeroRotator, (Min + Max) * 0.5 - Origin, (Max - Min) / CubeSize));
				InstanceColors.Add(PaintColors[BoxPaint]);
			}
		}
	}
	//Ramps: slabs whose top surface goes from Start to End, longer at the bottom so it sinks into the lower floor
	for (const FMP_MechRamp& Ramp : Ramps)
	{
		if (Ramp.Part != Part)
		{
			continue;
		}
		const FVector Along = Ramp.End - Ramp.Start;
		const double Length = Along.Size();
		if (Length < 1.0)
		{
			continue;
		}
		const FRotator Rotation = Along.Rotation();
		const FVector Direction = Along / Length;
		const FVector Normal = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
		constexpr double Sink = 40.0;
		const FVector Center = (Ramp.Start + Ramp.End) * 0.5 - Direction * Sink * 0.5 - Normal * Ramp.Thickness * 0.5;
		Instances.Add(FTransform(Rotation, Center - Origin, FVector(Length + Sink, Ramp.Width, Ramp.Thickness) / CubeSize));
		const int32* ZoneIndex = ZoneIndices.Find(Ramp.Zone);
		InstanceColors.Add(ZoneIndex ? Zones[*ZoneIndex].FloorColor : Data->Color);
	}

	Target->AddInstances(Instances, false);
	for (int32 Index = 0; Index < InstanceColors.Num(); ++Index)
	{
		const FLinearColor& Color = InstanceColors[Index];
		Target->SetCustomData(Index, { Color.R, Color.G, Color.B, 0.f });
	}
}

void AMP_Mech::ApplyLook()
{
	const UPDA_Mech* Data = GetMechData();
	if (Data->Material)
	{
		for (UInstancedStaticMeshComponent* Mesh : { Structure.Get(), Armor.Get(), LeftLegStructure.Get(), LeftLegArmor.Get(), RightLegStructure.Get(), RightLegArmor.Get(),
			LeftArmStructure.Get(), LeftArmArmor.Get(), RightArmStructure.Get(), RightArmArmor.Get(), HeadStructure.Get(), HeadArmor.Get() })
		{
			Mesh->SetMaterial(0, Data->Material);
		}
	}
	if (Data->GlassMaterial)
	{
		Glass->SetMaterial(0, Data->GlassMaterial);
		HeadGlass->SetMaterial(0, Data->GlassMaterial);
	}
}

bool AMP_Mech::IsInsideStructure(const FVector& LocalPoint) const
{
	for (const FMP_MechBox& Block : SolidBlocks)
	{
		if (FBox(Block.Min, Block.Max).IsInsideOrOn(LocalPoint))
		{
			return true;
		}
	}
	return false;
}

void AMP_Mech::CarryPhysicsBodies(const FTransform& OldTransform, const FTransform& NewTransform)
{
	const FBoxSphereBounds Bounds = Structure->Bounds;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MechCarry), false, this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Bounds.Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(Bounds.BoxExtent), Params);

	//Each body keeps its place relative to the mech (a ragdoll overlaps once per bone)
	TSet<UPrimitiveComponent*> Carried;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Component || Carried.Contains(Component) || !Component->IsSimulatingPhysics() || !IsInsideStructure(OldTransform.InverseTransformPosition(Component->GetComponentLocation())))
		{
			continue;
		}
		Carried.Add(Component);
		Component->SetWorldTransform(Component->GetComponentTransform().GetRelativeTransform(OldTransform) * NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AMP_Mech::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//Before BeginPlay and the first replicated state
	SimLocation = GetActorLocation();
	SimYaw = GetActorRotation().Yaw;
	UpdatePartShapes();
}

void AMP_Mech::UpdatePartShapes()
{
	auto FindPivot = [this](EMP_MechPart Part, FVector& OutLocation)
	{
		for (const FMP_MechPivot& Pivot : PartPivots)
		{
			if (Pivot.Part == Part)
			{
				OutLocation = Pivot.Location;
				return true;
			}
		}
		return false;
	};

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const EMP_MechPart Part = Index == 0 ? EMP_MechPart::LeftLeg : EMP_MechPart::RightLeg;
		const FMP_MechBox* Top = nullptr;
		const FMP_MechBox* Bottom = nullptr;
		for (const FMP_MechBox& Box : SolidBlocks)
		{
			if (Box.Part != Part)
			{
				continue;
			}
			if (!Top || Box.Max.Z > Top->Max.Z)
			{
				Top = &Box;
			}
			if (!Bottom || Box.Min.Z < Bottom->Min.Z)
			{
				Bottom = &Box;
			}
		}

		FLegShape& Leg = Legs[Index];
		Leg = FLegShape();
		if (Top && Bottom)
		{
			Leg.bValid = true;
			Leg.Pivot = FVector((Top->Min.X + Top->Max.X) * 0.5, (Top->Min.Y + Top->Max.Y) * 0.5, Top->Max.Z);
			FindPivot(Part, Leg.Pivot);
			Leg.SoleMin = Bottom->Min;
			Leg.SoleMax = FVector(Bottom->Max.X, Bottom->Max.Y, Bottom->Min.Z);
		}
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const EMP_MechPart Part = Index == 0 ? EMP_MechPart::LeftArm : EMP_MechPart::RightArm;
		FBox Bounds(ForceInit);
		for (const FMP_MechBox& Box : SolidBlocks)
		{
			if (Box.Part == Part)
			{
				Bounds += FBox(Box.Min, Box.Max);
			}
		}

		FArmShape& Arm = Arms[Index];
		Arm = FArmShape();
		if (!Bounds.IsValid)
		{
			continue;
		}
		Arm.bValid = true;
		const double Side = Bounds.GetCenter().Y >= 0.0 ? 1.0 : -1.0;
		//Without a pivot block: the middle of its side facing the body
		Arm.Pivot = FVector(Bounds.GetCenter().X, Side > 0.0 ? Bounds.Min.Y : Bounds.Max.Y, Bounds.GetCenter().Z);
		FindPivot(Part, Arm.Pivot);
		Arm.RestDirection = FVector(0.0, Side, 0.0);
	}

	//The head turns on the bottom center of its solids, unless a pivot block says otherwise
	FBox HeadBounds(ForceInit);
	for (const FMP_MechBox& Box : SolidBlocks)
	{
		if (Box.Part == EMP_MechPart::Head)
		{
			HeadBounds += FBox(Box.Min, Box.Max);
		}
	}
	HeadShape = FArmShape();
	if (HeadBounds.IsValid)
	{
		HeadShape.bValid = true;
		HeadShape.Pivot = FVector(HeadBounds.GetCenter().X, HeadBounds.GetCenter().Y, HeadBounds.Min.Z);
		FindPivot(EMP_MechPart::Head, HeadShape.Pivot);
	}
}

#if WITH_EDITOR
bool AMP_Mech::GatherBlocks(const AActor* Ignored)
{
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	TArray<const AMP_MechBlock*> Blocks;
	for (const AActor* Actor : Attached)
	{
		const AMP_MechBlock* Block = Cast<AMP_MechBlock>(Actor);
		if (Block && Block != Ignored && IsValid(Block))
		{
			Blocks.Add(Block);
		}
	}
	if (Blocks.IsEmpty())
	{
		return false;
	}
	//A stable order: the first cavity wins a lining both reach
	Blocks.Sort([](const AMP_MechBlock& A, const AMP_MechBlock& B) { return A.GetActorLabel() < B.GetActorLabel(); });

	SolidBlocks.Reset();
	Cavities.Reset();
	GlassPanes.Reset();
	Ramps.Reset();
	ArmorPlates.Reset();
	PartPivots.Reset();
	const FTransform MechTransform = GetActorTransform();
	for (const AMP_MechBlock* Block : Blocks)
	{
		const FName Name(*Block->GetActorLabel());
		const FTransform BoxTransform = Block->GetBoxTransform(MechTransform);
		const FVector Size = BoxTransform.GetScale3D() * CubeSize;
		switch (Block->Type)
		{
		case EMP_MechBlockType::Solid:
		case EMP_MechBlockType::Cavity:
		case EMP_MechBlockType::Glass:
		{
			const FBox Box = Block->GetAlignedBox(MechTransform);
			FMP_MechBox Entry;
			Entry.Name = Name;
			Entry.Min = Box.Min;
			Entry.Max = Box.Max;
			Entry.Zone = Block->Type == EMP_MechBlockType::Cavity ? Block->Zone : NAME_None;
			Entry.Part = Block->Type != EMP_MechBlockType::Cavity ? Block->Part : EMP_MechPart::Body;
			(Block->Type == EMP_MechBlockType::Solid ? SolidBlocks : (Block->Type == EMP_MechBlockType::Cavity ? Cavities : GlassPanes)).Add(Entry);
			break;
		}
		case EMP_MechBlockType::Ramp:
		{
			//Its top face, from the lower end to the higher one
			const FVector Forward = BoxTransform.GetUnitAxis(EAxis::X);
			const FVector Top = BoxTransform.GetLocation() + BoxTransform.GetUnitAxis(EAxis::Z) * Size.Z * 0.5;
			const FVector EndA = Top - Forward * Size.X * 0.5;
			const FVector EndB = Top + Forward * Size.X * 0.5;
			FMP_MechRamp& Ramp = Ramps.AddDefaulted_GetRef();
			Ramp.Name = Name;
			Ramp.Start = EndA.Z <= EndB.Z ? EndA : EndB;
			Ramp.End = EndA.Z <= EndB.Z ? EndB : EndA;
			Ramp.Width = Size.Y;
			Ramp.Thickness = Size.Z;
			Ramp.Zone = Block->Zone;
			Ramp.Part = Block->Part;
			break;
		}
		case EMP_MechBlockType::Pivot:
		{
			FMP_MechPivot& Pivot = PartPivots.AddDefaulted_GetRef();
			Pivot.Part = Block->Part;
			Pivot.Location = BoxTransform.GetLocation();
			break;
		}
		case EMP_MechBlockType::Plate:
		{
			FMP_MechArmor& Plate = ArmorPlates.AddDefaulted_GetRef();
			Plate.Name = Name;
			Plate.Center = BoxTransform.GetLocation();
			Plate.Size = Size;
			Plate.Rotation = BoxTransform.Rotator();
			Plate.Color = Block->Color;
			Plate.Emissive = Block->Emissive;
			Plate.Part = Block->Part;
			break;
		}
		}
	}
	return true;
}

bool AMP_Mech::IsStructurePiece(const AActor* Actor) const
{
	if (!Actor || (!Actor->IsA<AMP_MechBlock>() && !Actor->IsA<AMP_HullPlate>()))
	{
		return false;
	}
	for (const AActor* Parent = Actor->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
	{
		if (Parent == this)
		{
			return true;
		}
	}
	return false;
}

void AMP_Mech::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	//Portholes moved or resized and blocks deleted in the editor rebuild the structure (blocks rebuild it themselves while they move)
	const UWorld* World = GetWorld();
	if (GEngine && World && World->WorldType == EWorldType::Editor && !PropertyChangedHandle.IsValid())
	{
		ActorMovedHandle = GEngine->OnActorMoved().AddUObject(this, &AMP_Mech::OnEditorActorMoved);
		ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &AMP_Mech::OnEditorActorDeleted);
		PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &AMP_Mech::OnEditorPropertyChanged);
	}
}

void AMP_Mech::BeginDestroy()
{
	if (PropertyChangedHandle.IsValid())
	{
		if (GEngine)
		{
			GEngine->OnActorMoved().Remove(ActorMovedHandle);
			GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
		}
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
		PropertyChangedHandle.Reset();
	}
	Super::BeginDestroy();
}

void AMP_Mech::OnEditorActorMoved(AActor* Actor)
{
	if (IsStructurePiece(Actor) && !Actor->IsA<AMP_MechBlock>())
	{
		BuildStructure();
	}
}

void AMP_Mech::OnEditorActorDeleted(AActor* Actor)
{
	if (IsStructurePiece(Actor))
	{
		BuildStructureIgnoring(Actor);
	}
}

void AMP_Mech::OnEditorPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	const UActorComponent* Component = Cast<UActorComponent>(Object);
	const AActor* Actor = Component ? Component->GetOwner() : Cast<AActor>(Object);
	if (IsStructurePiece(Actor) && !Actor->IsA<AMP_MechBlock>())
	{
		BuildStructure();
	}
}
#endif

USceneComponent* AMP_Mech::GetPartPivot(EMP_MechPart Part) const
{
	const int32 Moving = MovingIndexOf(Part);
	return Moving != INDEX_NONE ? GetMovingPivotComponent(Moving) : nullptr;
}

USceneComponent* AMP_Mech::GetMovingPivotComponent(int32 Index) const
{
	USceneComponent* const Pivots[MovingPartCount] = { LeftLegPivot.Get(), RightLegPivot.Get(), LeftArmPivot.Get(), RightArmPivot.Get(), HeadPivot.Get() };
	return Pivots[Index];
}

bool AMP_Mech::GetMovingPivot(int32 Index, FVector& OutPivot) const
{
	if (Index < 2)
	{
		if (Legs[Index].bValid)
		{
			OutPivot = Legs[Index].Pivot;
			return true;
		}
		return false;
	}
	const FArmShape& Shape = Index < 4 ? Arms[Index - 2] : HeadShape;
	if (Shape.bValid)
	{
		OutPivot = Shape.Pivot;
		return true;
	}
	return false;
}

void AMP_Mech::UpdateHead(float DeltaSeconds)
{
	if (!HeadShape.bValid)
	{
		return;
	}
	//The lookout inside the head
	if (!HeadStation.IsValid())
	{
		TArray<USceneComponent*> Hanging;
		HeadPivot->GetChildrenComponents(true, Hanging);
		for (const USceneComponent* Child : Hanging)
		{
			if (AMP_LookoutStation* Station = Cast<AMP_LookoutStation>(Child->GetOwner()))
			{
				HeadStation = Station;
				break;
			}
		}
	}

	//Toward the lookout's aim around the neck, else facing forward
	const UPDA_Mech* Data = GetMechData();
	float Target = 0.f;
	const AMP_LookoutStation* Station = HeadStation.Get();
	if (Data->bTurnHead && Station && Station->GetUser())
	{
		Target = FRotator::NormalizeAxis(Station->GetAimRotation().Yaw - SimYaw);
	}
	else if (Data->bTurnHead && bDebugArmAim)
	{
		Target = DebugArmAim.Yaw;
	}
	HeadYaw = FRotator::NormalizeAxis(FMath::FixedTurn(HeadYaw, Target, Data->HeadTurnSpeed * DeltaSeconds));
	HeadPivot->SetRelativeRotation(FRotator(0.f, HeadYaw, 0.f));
}

void AMP_Mech::UpdateArms(float DeltaSeconds)
{
	const UPDA_Mech* Data = GetMechData();
	ArmSearchTimer -= DeltaSeconds;
	const bool bSearch = ArmSearchTimer <= 0.f;
	if (bSearch)
	{
		ArmSearchTimer = ArmStationSearchInterval;
	}

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FArmShape& Arm = Arms[Index];
		if (!Arm.bValid)
		{
			continue;
		}
		USceneComponent* Pivot = GetMovingPivotComponent(Index + 2);
		//The gun in this arm (attached under its pivot)
		if (!ArmStations[Index].IsValid() && bSearch)
		{
			TArray<USceneComponent*> Hanging;
			Pivot->GetChildrenComponents(true, Hanging);
			for (const USceneComponent* Child : Hanging)
			{
				if (AMP_WeaponStation* Station = Cast<AMP_WeaponStation>(Child->GetOwner()))
				{
					ArmStations[Index] = Station;
					break;
				}
			}
		}

		//Toward the aim in the mech's frame: swung forward or back around the vertical, lifted or lowered around the horizontal across it
		FQuat Target = FQuat::Identity;
		const AMP_WeaponStation* Station = ArmStations[Index].Get();
		const bool bManned = Station && Station->GetUser();
		if (Data->bAimArms && (bManned || bDebugArmAim))
		{
			const FVector Aim = bManned ? FRotator(0.f, -SimYaw, 0.f).RotateVector(Station->GetAimRotation().Vector()) : DebugArmAim.Vector();
			const FVector Flat = FVector(Aim.X, Aim.Y, 0.f).GetSafeNormal();
			const double Yaw = Flat.IsNearlyZero() ? 0.0 : FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Arm.RestDirection, Flat).Z, FVector::DotProduct(Arm.RestDirection, Flat)));
			const double Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Aim.Z, -1.0, 1.0)));
			const FVector LiftAxis = FVector::CrossProduct(Arm.RestDirection, FVector::UpVector);
			Target = FQuat(FVector::UpVector, FMath::DegreesToRadians(FMath::Clamp(Yaw, -Data->ArmMaxYaw, Data->ArmMaxYaw)))
				* FQuat(LiftAxis, FMath::DegreesToRadians(FMath::Clamp(Elevation, -Data->ArmMaxPitchDown, Data->ArmMaxPitchUp)));
		}
		ArmRotations[Index] = FMath::QInterpConstantTo(ArmRotations[Index], Target, DeltaSeconds, FMath::DegreesToRadians(Data->ArmTurnSpeed));
		Pivot->SetRelativeRotation(ArmRotations[Index]);
	}
}

void AMP_Mech::UpdateGait(float DeltaSeconds)
{
	const UPDA_Mech* Data = GetMechData();
	const float FullSpeed = FMath::Max(Data->MaxForwardSpeed, 1.f);
	const float HipSpeed = FMath::Abs(Speed) + FMath::Abs(FMath::DegreesToRadians(TurnRate)) * Data->TurnStepRadius;
	const float TargetWeight = HasLegs() && Data->bAnimateWalk ? FMath::Min(HipSpeed / FullSpeed, 1.f) : 0.f;
	GaitWeight = FMath::FInterpTo(GaitWeight, TargetWeight, DeltaSeconds, Data->GaitBlendSpeed);

	//Same cadence at any speed, the stride follows the speed: the standing foot stays planted
	double PhaseStep = 0.0;
	if (HasLegs() && TargetWeight > 0.01f)
	{
		const double LegLength = (Legs[0].Pivot.Z - Legs[0].SoleMin.Z + Legs[1].Pivot.Z - Legs[1].SoleMin.Z) * 0.5;
		const double FullStride = 2.0 * LegLength * FMath::Sin(FMath::DegreesToRadians(Data->SwingAngle));
		if (FullStride > 1.0)
		{
			PhaseStep = (Speed < -1.f ? -1.0 : 1.0) * UE_DOUBLE_PI * FullSpeed / FullStride * DeltaSeconds;
		}
	}
	//Clients: pulled toward the server's phase
	if (!HasAuthority())
	{
		const double Correction = GaitPhaseError * FMath::Min(1.0, DeltaSeconds * Data->NetCorrectionSpeed);
		PhaseStep += Correction;
		GaitPhaseError -= Correction;
	}

	//A foot lands at the widest stride: the left one at Pi/2, the right one at 3 Pi/2 (step boundaries every Pi from Pi/2)
	const double NewPhase = GaitPhase + PhaseStep;
	const int64 OldStep = FMath::FloorToInt64((GaitPhase - UE_DOUBLE_HALF_PI) / UE_DOUBLE_PI);
	const int64 NewStep = FMath::FloorToInt64((NewPhase - UE_DOUBLE_HALF_PI) / UE_DOUBLE_PI);
	GaitPhase = FMath::Fmod(NewPhase, UE_DOUBLE_TWO_PI);
	if (GaitPhase < 0.0)
	{
		GaitPhase += UE_DOUBLE_TWO_PI;
	}
	if (NewStep != OldStep)
	{
		const int64 Boundary = FMath::Max(OldStep, NewStep);
		PlayFootstep(((Boundary % 2) + 2) % 2 == 0 ? 0 : 1);
	}
}

FTransform AMP_Mech::ComputeBodyPose(float& OutLeftSwing, float& OutRightSwing) const
{
	OutLeftSwing = 0.f;
	OutRightSwing = 0.f;
	if (!HasLegs() || GaitWeight <= KINDA_SMALL_NUMBER)
	{
		return FTransform::Identity;
	}

	//Positive swing moves the foot forward; the legs go opposite ways
	const UPDA_Mech* Data = GetMechData();
	const float Swing = Data->SwingAngle * GaitWeight * FMath::Sin(GaitPhase);
	OutLeftSwing = Swing;
	OutRightSwing = -Swing;

	//Waddle: the body turns on the standing foot's outer edge so the swinging foot leaves the ground, level again when it lands.
	//Positive roll lifts +Y (the left leg swings), turning on the right sole's outer edge
	const float Roll = Data->WaddleRoll * GaitWeight * FMath::Cos(GaitPhase);
	const FVector Edge(0.0, Roll >= 0.f ? Legs[1].SoleMin.Y : Legs[0].SoleMax.Y, 0.0);
	const FQuat RollQuat(FVector::ForwardVector, FMath::DegreesToRadians(Roll));
	FTransform Pose(RollQuat, Edge - RollQuat.RotateVector(Edge));

	//Lowest sole corner on the ground (the long boots rock on their heel and toe)
	double LowestZ = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FLegShape& Leg = Legs[Index];
		const FQuat SwingQuat(FRotator(Index == 0 ? OutLeftSwing : OutRightSwing, 0.f, 0.f));
		for (const FVector& Corner : { Leg.SoleMin, FVector(Leg.SoleMax.X, Leg.SoleMin.Y, Leg.SoleMin.Z), FVector(Leg.SoleMin.X, Leg.SoleMax.Y, Leg.SoleMin.Z), Leg.SoleMax })
		{
			const FVector Swung = Leg.Pivot + SwingQuat.RotateVector(Corner - Leg.Pivot);
			LowestZ = FMath::Min(LowestZ, Pose.TransformPosition(Swung).Z);
		}
	}
	Pose.AddToTranslation(FVector(0.0, 0.0, -LowestZ));
	return Pose;
}

void AMP_Mech::PlayFootstep(int32 LegIndex)
{
	const UPDA_Mech* Data = GetMechData();
	if (GetNetMode() == NM_DedicatedServer || GaitWeight < Data->MinStepWeight || !Legs[LegIndex].bValid)
	{
		return;
	}

	//Middle of the sole, where it is now
	const FLegShape& Leg = Legs[LegIndex];
	const USceneComponent* Pivot = LegIndex == 0 ? LeftLegPivot : RightLegPivot;
	const FVector Location = Pivot->GetComponentTransform().TransformPosition((Leg.SoleMin + Leg.SoleMax) * 0.5 - Leg.Pivot);
	UE_LOG(LogMechaProto, Verbose, TEXT("%s footstep %s, stride %.2f"), *GetName(), LegIndex == 0 ? TEXT("left") : TEXT("right"), GaitWeight);

	//Every local camera, stronger near the foot
	if (Data->FootstepShake)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (!Controller || !Controller->IsLocalController() || !Controller->PlayerCameraManager)
			{
				continue;
			}
			const float Distance = FVector::Dist(Controller->PlayerCameraManager->GetCameraLocation(), Location);
			const float Far = FMath::Clamp(FMath::GetRangePct(Data->FootstepInnerRadius, FMath::Max(Data->FootstepOuterRadius, Data->FootstepInnerRadius + 1.f), Distance), 0.f, 1.f);
			const float Scale = Data->FootstepShakeScale * GaitWeight * FMath::Lerp(1.f, Data->FootstepFarScale, Far);
			if (Scale > KINDA_SMALL_NUMBER)
			{
				Controller->PlayerCameraManager->StartCameraShake(Data->FootstepShake, Scale);
			}
		}
	}
	if (Data->FootstepSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->FootstepSound, Location, GaitWeight);
	}
	OnFootstep(LegIndex == 0, Location, GaitWeight);
}

void AMP_Mech::BeginPlay()
{
	Super::BeginPlay();

	ApplyLook();
	if (HasAuthority())
	{
		SetNetUpdateFrequency(GetMechData()->NetUpdateFrequency);
		MoveState.Location = SimLocation;
		MoveState.Yaw = SimYaw;
	}
}

void AMP_Mech::SetPilotInput(const FVector2D& Input)
{
	if (HasAuthority())
	{
		PilotInput = FVector2D(FMath::Clamp(Input.X, -1.f, 1.f), FMath::Clamp(Input.Y, -1.f, 1.f));
	}
}

void AMP_Mech::DebugDrive(float Forward, float Turn)
{
	SetPilotInput(FVector2D(Forward, Turn));
}

void AMP_Mech::DebugAim(float Yaw, float Pitch)
{
	bDebugArmAim = true;
	DebugArmAim = FRotator(Pitch, Yaw, 0.f);
}

void AMP_Mech::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//Same simulation on every machine from the replicated input
	const UPDA_Mech* Data = GetMechData();
	const float TargetSpeed = PilotInput.X * (PilotInput.X >= 0.f ? Data->MaxForwardSpeed : Data->MaxBackwardSpeed);
	Speed = FMath::FInterpConstantTo(Speed, TargetSpeed, DeltaSeconds, Data->Acceleration);
	TurnRate = FMath::FInterpConstantTo(TurnRate, PilotInput.Y * Data->MaxTurnRate, DeltaSeconds, Data->TurnAcceleration);
	float YawStep = TurnRate * DeltaSeconds;
	SimYaw += YawStep;
	SimLocation += FRotator(0.f, SimYaw, 0.f).Vector() * Speed * DeltaSeconds;

	//Clients: pulled toward the server's state
	if (!HasAuthority())
	{
		const float Alpha = FMath::Min(1.f, DeltaSeconds * Data->NetCorrectionSpeed);
		const FVector LocationStep = LocationError * Alpha;
		const float YawCorrection = YawError * Alpha;
		SimLocation += LocationStep;
		LocationError -= LocationStep;
		SimYaw += YawCorrection;
		YawError -= YawCorrection;
		YawStep += YawCorrection;
	}
	LastYawDelta = YawStep;
	UpdateGait(DeltaSeconds);

	//Legs swing from the hips, the body rocks and bobs over them
	float LeftSwing = 0.f;
	float RightSwing = 0.f;
	const FTransform NewTransform = ComputeBodyPose(LeftSwing, RightSwing) * FTransform(FRotator(0.f, SimYaw, 0.f), SimLocation);
	if (HasLegs())
	{
		LeftLegPivot->SetRelativeRotation(FRotator(LeftSwing, 0.f, 0.f));
		RightLegPivot->SetRelativeRotation(FRotator(RightSwing, 0.f, 0.f));
	}
	UpdateArms(DeltaSeconds);
	UpdateHead(DeltaSeconds);

	//Attached pieces follow, players standing inside move with their base, loose bodies are carried
	if (!GetActorTransform().Equals(NewTransform, 0.001))
	{
		const FTransform OldTransform = GetActorTransform();
		SetActorTransform(NewTransform);
		if (Data->bCarryPhysicsBodies)
		{
			CarryPhysicsBodies(OldTransform, GetActorTransform());
		}
	}

	if (HasAuthority())
	{
		MoveState.Location = SimLocation;
		MoveState.Yaw = FRotator::NormalizeAxis(SimYaw);
		MoveState.Speed = Speed;
		MoveState.TurnRate = TurnRate;
		MoveState.GaitPhase = static_cast<float>(GaitPhase);
	}
	ShowDebug();
}

void AMP_Mech::OnRep_MoveState()
{
	Speed = MoveState.Speed;
	TurnRate = MoveState.TurnRate;
	LocationError = FVector(MoveState.Location) - SimLocation;
	YawError = FMath::FindDeltaAngleDegrees(SimYaw, MoveState.Yaw);
	GaitPhaseError = FMath::UnwindRadians(static_cast<double>(MoveState.GaitPhase) - GaitPhase);

	//Late join or a big gap: no blend
	if (LocationError.SizeSquared() > FMath::Square(NetSnapDistance))
	{
		SimLocation = MoveState.Location;
		SimYaw = MoveState.Yaw;
		LocationError = FVector::ZeroVector;
		YawError = 0.f;
	}
	if (FMath::Abs(GaitPhaseError) > NetSnapGaitPhase)
	{
		GaitPhase = MoveState.GaitPhase;
		GaitPhaseError = 0.0;
	}
}

void AMP_Mech::ShowDebug() const
{
	if (!GEngine || GetNetMode() == NM_DedicatedServer || !GetMechData()->bShowDebug)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Cyan, FString::Printf(TEXT("Mech %.1f m/s, turning %.0f deg/s, stride %.0f%%"), Speed / 100.f, TurnRate, GaitWeight * 100.f));
}
