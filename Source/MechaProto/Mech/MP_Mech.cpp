#include "MP_Mech.h"
#include "MechaProto.h"
#include "PDA_Mech.h"
#include "Algo/BinarySearch.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	//Engine cube
	constexpr float CubeSize = 100.f;
	//Farther than this from the server's state, a client jumps there (late join)
	constexpr float NetSnapDistance = 500.f;

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

	//The floor the crew stands on, their movement base: it must be Movable and the same object on every machine (a default subobject)
	Structure = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Structure"));
	Structure->SetupAttachment(Root);
	Structure->SetMobility(EComponentMobility::Movable);
	Structure->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	//Its own weapons fire through it, the station cameras see through it
	Structure->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	Structure->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Structure->SetCanEverAffectNavigation(false);
	if (CubeMesh.Succeeded())
	{
		Structure->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Structure->SetMaterial(0, ShapeMaterial.Object);
	}
	//Color (3) and emissive strength (1) per instance, read by M_MechPaint
	Structure->NumCustomDataFloats = 4;

	//Looks only: no collision, not in the distance fields (big plates around rooms would darken them for Lumen)
	Armor = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Armor"));
	Armor->SetupAttachment(Root);
	Armor->SetMobility(EComponentMobility::Movable);
	Armor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Armor->SetCanEverAffectNavigation(false);
	Armor->bAffectDistanceFieldLighting = false;
	Armor->bAffectDynamicIndirectLighting = false;
	Armor->NumCustomDataFloats = 4;
	if (CubeMesh.Succeeded())
	{
		Armor->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Armor->SetMaterial(0, ShapeMaterial.Object);
	}

	//Same collision as the structure: blocks the crew, its own guns and cameras go through
	Glass = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Glass"));
	Glass->SetupAttachment(Root);
	Glass->SetMobility(EComponentMobility::Movable);
	Glass->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Glass->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	Glass->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Glass->SetCanEverAffectNavigation(false);
	Glass->SetCastShadow(false);
	if (CubeMesh.Succeeded())
	{
		Glass->SetStaticMesh(CubeMesh.Object);
	}
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
	Structure->ClearInstances();
	Armor->ClearInstances();
	Glass->ClearInstances();
	for (const FMP_MechBox& Pane : GlassPanes)
	{
		Glass->AddInstance(FTransform(FRotator::ZeroRotator, (Pane.Min + Pane.Max) * 0.5, (Pane.Max - Pane.Min) / CubeSize));
	}
	Structure->SetNumCustomDataFloats(4);
	Armor->SetNumCustomDataFloats(4);

	//Armor first: the structure can stop early
	for (const FMP_MechArmor& Plate : ArmorPlates)
	{
		const int32 Index = Armor->AddInstance(FTransform(Plate.Rotation, Plate.Center, Plate.Size / CubeSize));
		Armor->SetCustomData(Index, { Plate.Color.R, Plate.Color.G, Plate.Color.B, Plate.Emissive });
	}

	if (SolidBlocks.IsEmpty())
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

	//A grid on every face of every box, plus the lining depth around the painted cavities
	TArray<double> Cuts[3];
	for (const FMP_MechBox& Box : SolidBlocks)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Cuts[Axis].Append({ Box.Min[Axis], Box.Max[Axis] });
		}
	}
	for (const FMP_MechBox& Box : Cavities)
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
	for (const FMP_MechBox& Box : SolidBlocks)
	{
		ForEachCell(Box.Min, Box.Max, [&](int32 I, int32 J, int32 K) { Cells[CellIndex(I, J, K)] = 1; });
	}
	for (const FMP_MechBox& Box : Cavities)
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
	for (const FMP_MechBox& Box : Cavities)
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
				Instances.Add(FTransform(FRotator::ZeroRotator, (Min + Max) * 0.5, (Max - Min) / CubeSize));
				InstanceColors.Add(PaintColors[BoxPaint]);
			}
		}
	}
	//Ramps: slabs whose top surface goes from Start to End, longer at the bottom so it sinks into the lower floor
	for (const FMP_MechRamp& Ramp : Ramps)
	{
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
		Instances.Add(FTransform(Rotation, Center, FVector(Length + Sink, Ramp.Width, Ramp.Thickness) / CubeSize));
		const int32* ZoneIndex = ZoneIndices.Find(Ramp.Zone);
		InstanceColors.Add(ZoneIndex ? Zones[*ZoneIndex].FloorColor : Data->Color);
	}

	Structure->AddInstances(Instances, false);
	for (int32 Index = 0; Index < InstanceColors.Num(); ++Index)
	{
		const FLinearColor& Color = InstanceColors[Index];
		Structure->SetCustomData(Index, { Color.R, Color.G, Color.B, 0.f });
	}
}

void AMP_Mech::ApplyLook()
{
	const UPDA_Mech* Data = GetMechData();
	if (Data->Material)
	{
		Structure->SetMaterial(0, Data->Material);
		Armor->SetMaterial(0, Data->Material);
	}
	if (Data->GlassMaterial)
	{
		Glass->SetMaterial(0, Data->GlassMaterial);
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

	//Attached pieces follow, players standing inside move with their base, loose bodies are carried
	const FRotator NewRotation(0.f, SimYaw, 0.f);
	if (!GetActorLocation().Equals(SimLocation, 0.01) || !GetActorRotation().Equals(NewRotation, 0.0001f))
	{
		const FTransform OldTransform = GetActorTransform();
		SetActorLocationAndRotation(SimLocation, NewRotation);
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
	}
	ShowDebug();
}

void AMP_Mech::OnRep_MoveState()
{
	Speed = MoveState.Speed;
	TurnRate = MoveState.TurnRate;
	LocationError = FVector(MoveState.Location) - SimLocation;
	YawError = FMath::FindDeltaAngleDegrees(SimYaw, MoveState.Yaw);

	//Late join or a big gap: no blend
	if (LocationError.SizeSquared() > FMath::Square(NetSnapDistance))
	{
		SimLocation = MoveState.Location;
		SimYaw = MoveState.Yaw;
		LocationError = FVector::ZeroVector;
		YawError = 0.f;
	}
}

void AMP_Mech::ShowDebug() const
{
	if (!GEngine || GetNetMode() == NM_DedicatedServer || !GetMechData()->bShowDebug)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Cyan, FString::Printf(TEXT("Mech %.1f m/s, turning %.0f deg/s"), Speed / 100.f, TurnRate));
}
