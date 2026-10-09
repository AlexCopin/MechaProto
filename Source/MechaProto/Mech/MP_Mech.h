#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "MP_Mech.generated.h"

class AMP_HullPlate;
class AMP_LookoutStation;
class AMP_WeaponStation;
class UInstancedStaticMeshComponent;
class UPDA_Mech;

//What a box or plate belongs to: the legs swing from the hip, the body rocks over them, the arms aim from the shoulder, the head turns on the neck
UENUM(BlueprintType)
enum class EMP_MechPart : uint8
{
	Body,
	LeftLeg,
	RightLeg,
	LeftArm,
	RightArm,
	Head
};

//Where a moving part turns (a Pivot block): a leg's hip, an arm's shoulder
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_MechPivot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	EMP_MechPart Part = EMP_MechPart::LeftLeg;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Location = FVector::ZeroVector;
};

//Box in the mech's space (cm, X forward, Z up from the ground)
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_MechBox
{
	GENERATED_BODY()

	//For the designer (Left foot, Core room...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Min = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Max = FVector::ZeroVector;

	//Cavities: the zone (Zones) whose colors paint the walls, floor and ceiling around it. None = the frame color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Zone;

	//Solids: the part they build (a leg is its solids, pivoting at the top center of its highest one). Cavities carve every part
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	EMP_MechPart Part = EMP_MechPart::Body;
};

//A part of the body with its own colors inside (left leg, hips, hall...), so the crew knows where it is
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_MechZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FLinearColor WallColor = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FLinearColor FloorColor = FLinearColor(0.15f, 0.15f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FLinearColor CeilingColor = FLinearColor(0.4f, 0.4f, 0.4f);
};

//Sloped floor from Start (bottom end, on its surface) up to End, Width across: a tilted slab in the structure
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_MechRamp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	float Width = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	float Thickness = 20.f;

	//Painted with this zone's floor color
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Zone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	EMP_MechPart Part = EMP_MechPart::Body;
};

//Visual plate (no collision): the mech's armor, joints and eyes outside, stripes, pipes and guide lines inside
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_MechArmor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FVector Size = FVector(100.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	FLinearColor Color = FLinearColor::White;

	//Glow (eyes, lights), 0 = none
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech", meta = (ClampMin = "0", UIMax = "100"))
	float Emissive = 0.f;

	//A leg's plates swing with it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	EMP_MechPart Part = EMP_MechPart::Body;
};

USTRUCT()
struct FMP_MechMoveState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	float Speed = 0.f;

	UPROPERTY()
	float TurnRate = 0.f;

	//Walk cycle, radians: the left foot lands at Pi/2, the right one at 3 Pi/2
	UPROPERTY()
	float GaitPhase = 0.f;
};

//The mech the crew lives in. Its structure is SolidBlocks with the Cavities carved out (rooms, corridors, shafts, openings), built in the editor as instances
//Every gameplay piece inside (stations, ladders, plates, systems, player starts, lights) is attached to it and moves with it; players standing inside are based on it
//The pilot station walks and turns it: the server simulates, clients simulate the same replicated input and blend toward the server's state
//Walking, its straight legs swing from the hips and the body rocks onto the standing foot; each landing foot shakes the cameras
UCLASS()
class MECHAPROTO_API AMP_Mech : public AActor
{
	GENERATED_BODY()

public:
	AMP_Mech();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
#endif

	//Server, from the pilot station: X forward / backward, Y turn right / left, -1 to 1
	void SetPilotInput(const FVector2D& Input);

	//Server: walks without a pilot, for tests (console: ke * DebugDrive 1 0). The pilot station takes over when its input changes
	UFUNCTION(BlueprintCallable, Category = "Mech")
	void DebugDrive(float Forward, float Turn);

	//Tests: the arms and the head aim this way (mech frame, yaw 0 = forward) while nobody mans their station (console: ke * DebugAim 0 -30)
	UFUNCTION(BlueprintCallable, Category = "Mech")
	void DebugAim(float Yaw, float Pitch);

	UFUNCTION(BlueprintPure, Category = "Mech")
	const UPDA_Mech* GetMechData() const;

	//Signed, cm/s
	UFUNCTION(BlueprintPure, Category = "Mech")
	float GetSpeed() const { return Speed; }

	//Yaw it turned this frame (degrees), for the views that turn with it
	float GetLastYawDelta() const { return LastYawDelta; }

	//Rebuilds the structure from the attached blocks (AMP_MechBlock, compiled into the arrays below) and portholes (AMP_HullPlate, each carves its
	//opening and cuts the plates around it). Also on construction and, in the editor, whenever a block or porthole moves, changes or is deleted
	UFUNCTION(CallInEditor, Category = "Structure")
	void BuildStructure();

	//A moving part's pivot (leg's hip, arm's shoulder): what is inside that part attaches to it (ladders, plates, lights, signs, stations)
	USceneComponent* GetPartPivot(EMP_MechPart Part) const;

	//A foot landed (every machine), after the camera shake and the sound: for effects. Strength is the stride, 0-1
	UFUNCTION(BlueprintImplementableEvent, Category = "Mech")
	void OnFootstep(bool bLeftFoot, FVector Location, float Strength);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	//What is left of SolidBlocks once Cavities are carved out, merged into as few boxes as possible, plus the ramps
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Structure;

	//ArmorPlates, visual only
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Armor;

	//GlassPanes: see-through, block like the walls
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Glass;

	//The legs: a pivot at each hip, turned by the gait, with the leg's structure (the crew's base there too) and armor
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> LeftLegPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeftLegStructure;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeftLegArmor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RightLegPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RightLegStructure;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RightLegArmor;

	//The arms: a pivot at each shoulder, turned toward their gunner's aim, with the arm's structure and armor
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> LeftArmPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeftArmStructure;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> LeftArmArmor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RightArmPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RightArmStructure;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> RightArmArmor;

	//The head: a pivot on the neck, turned toward the lookout's aim, with the head's structure, armor and glass band
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> HeadPivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> HeadStructure;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> HeadArmor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> HeadGlass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	TObjectPtr<UPDA_Mech> MechData;

	//-----Compiled from the attached blocks (edit those in the scene): the arrays the game uses
	//Filled volumes: feet, legs, torso, arms, head
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechBox> SolidBlocks;

	//Carved out of the solid: rooms, corridors, shafts, holes. Touching or overlapping cavities connect (portholes carve their own opening)
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechBox> Cavities;

	//Colors of the cavities' zones, painted on a thin layer of the structure around them (DA_Mech ZoneLiningThickness)
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechZone> Zones;

	//Sloped floors added after the carving (their cavity must be carved around them)
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechRamp> Ramps;

	//Plates with their own color, never in the way: the look outside, the decor inside. Cut around doorways, holes and portholes when built
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechArmor> ArmorPlates;

	//Glass boxes (panoramic windows): carve the opening, the pane closes it
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechBox> GlassPanes;

	//Moving parts' pivots from the Pivot blocks (without one: a leg turns at the top center of its highest solid, an arm at the middle of its side facing the body)
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Structure")
	TArray<FMP_MechPivot> PartPivots;

	//Pilot input for the clients' simulation
	UPROPERTY(Replicated)
	FVector2D PilotInput = FVector2D::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_MoveState)
	FMP_MechMoveState MoveState;

	UFUNCTION()
	void OnRep_MoveState();

	//A leg from its solids, in the mech's space: hip pivot (top center of the highest box), sole (bottom of the lowest box)
	struct FLegShape
	{
		bool bValid = false;
		FVector Pivot = FVector::ZeroVector;
		FVector SoleMin = FVector::ZeroVector;
		FVector SoleMax = FVector::ZeroVector;
	};
	FLegShape Legs[2];
	bool HasLegs() const { return Legs[0].bValid && Legs[1].bValid; }

	//An arm from its solids: shoulder pivot, and the way it points at rest (straight out to its side)
	struct FArmShape
	{
		bool bValid = false;
		FVector Pivot = FVector::ZeroVector;
		FVector RestDirection = FVector::YAxisVector;
	};
	FArmShape Arms[2];
	//The head's pivot (RestDirection unused)
	FArmShape HeadShape;
	float HeadYaw = 0.f;
	TWeakObjectPtr<AMP_LookoutStation> HeadStation;
	FQuat ArmRotations[2] = { FQuat::Identity, FQuat::Identity };
	//Current swing speed of each arm, deg/s
	float ArmSpeeds[2] = { 0.f, 0.f };
	//The weapon station in each arm, whose aim the arm follows
	TWeakObjectPtr<AMP_WeaponStation> ArmStations[2];
	float ArmSearchTimer = 0.f;
	bool bDebugArmAim = false;
	FRotator DebugArmAim = FRotator::ZeroRotator;

	void UpdatePartShapes();
	//Moving parts after the body: 0-1 legs, 2-3 arms, 4 head
	bool GetMovingPivot(int32 Index, FVector& OutPivot) const;
	USceneComponent* GetMovingPivotComponent(int32 Index) const;
	//Each arm swings toward its gun's aim, within DA_Mech's Arms limits
	void UpdateArms(float DeltaSeconds);
	//The head turns toward the lookout's aim
	void UpdateHead(float DeltaSeconds);

	//A porthole's opening in the mech's space, and the axis it looks through
	struct FOpening
	{
		FBox Box = FBox(ForceInit);
		int32 NormalAxis = 0;
	};
	TArray<FOpening> GatherOpenings(const AActor* Ignored) const;

	void BuildStructureIgnoring(const AActor* Ignored);
	//Carves one part's solids into Target, instances relative to Origin (its pivot)
	void BuildPart(EMP_MechPart Part, const TArray<FMP_MechBox>& AllCavities, UInstancedStaticMeshComponent* Target, const FVector& Origin);
	//A plate as instances: inside (decor) the other cavities cut it, outside (armor) the portholes and every cavity's surroundings do
	void AddPlate(const FMP_MechArmor& Plate, const TArray<FMP_MechBox>& AllCavities, const TArray<FOpening>& Openings, UInstancedStaticMeshComponent* Target, const FVector& Origin) const;

#if WITH_EDITOR
	//The attached blocks into the arrays; false without any (the arrays stay as they are)
	bool GatherBlocks(const AActor* Ignored);
	bool IsStructurePiece(const AActor* Actor) const;
	void OnEditorActorMoved(AActor* Actor);
	void OnEditorActorDeleted(AActor* Actor);
	void OnEditorPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
	FDelegateHandle ActorMovedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle PropertyChangedHandle;
#endif

	//Stride from the speed, phase at a steady cadence, a footstep when a foot lands
	void UpdateGait(float DeltaSeconds);
	//The body's rock and bob in the mech's space (the lowest sole corner on the ground), and the legs' swing
	FTransform ComputeBodyPose(float& OutLeftSwing, float& OutRightSwing) const;
	void PlayFootstep(int32 LegIndex);

	void ApplyLook();
	void ShowDebug() const;
	//Moves the loose physics bodies inside by the last move of the part holding them (body, legs, arms, head)
	//OldPivotTransforms: each moving part's pivot relative to the root before the move
	void CarryPhysicsBodies(const FTransform& OldTransform, TConstArrayView<FTransform> OldPivotTransforms);

	//Simulation, every machine
	FVector SimLocation = FVector::ZeroVector;
	float SimYaw = 0.f;
	float Speed = 0.f;
	float TurnRate = 0.f;
	float LastYawDelta = 0.f;
	//Walk cycle (0 to 2 Pi) and stride (0-1 of the full one)
	double GaitPhase = 0.0;
	float GaitWeight = 0.f;

	//Clients: what is left to blend toward the server's state
	FVector LocationError = FVector::ZeroVector;
	float YawError = 0.f;
	double GaitPhaseError = 0.0;
};
