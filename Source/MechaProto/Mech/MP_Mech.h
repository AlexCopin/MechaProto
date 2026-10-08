#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "MP_Mech.generated.h"

class UInstancedStaticMeshComponent;
class UPDA_Mech;

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
};

//The mech the crew lives in. Its structure is SolidBlocks with the Cavities carved out (rooms, corridors, shafts, openings), built in the editor as instances
//Every gameplay piece inside (stations, ladders, plates, systems, player starts, lights) is attached to it and moves with it; players standing inside are based on it
//The pilot station walks and turns it: the server simulates, clients simulate the same replicated input and blend toward the server's state
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

	//Server, from the pilot station: X forward / backward, Y turn right / left, -1 to 1
	void SetPilotInput(const FVector2D& Input);

	//Server: walks without a pilot, for tests (console: ke * DebugDrive 1 0). The pilot station takes over when its input changes
	UFUNCTION(BlueprintCallable, Category = "Mech")
	void DebugDrive(float Forward, float Turn);

	UFUNCTION(BlueprintPure, Category = "Mech")
	const UPDA_Mech* GetMechData() const;

	//Signed, cm/s
	UFUNCTION(BlueprintPure, Category = "Mech")
	float GetSpeed() const { return Speed; }

	//Yaw it turned this frame (degrees), for the views that turn with it
	float GetLastYawDelta() const { return LastYawDelta; }

	//Rebuilds the structure and armor instances from the blocks, ramps and plates (also done on construction)
	UFUNCTION(CallInEditor, Category = "Structure")
	void BuildStructure();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mech")
	TObjectPtr<UPDA_Mech> MechData;

	//Filled volumes: feet, legs, torso, arms, head
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechBox> SolidBlocks;

	//Carved out of the solid: rooms, corridors, shafts, plate openings. Touching or overlapping cavities connect
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechBox> Cavities;

	//Colors of the cavities' zones, painted on a thin layer of the structure around them (DA_Mech ZoneLiningThickness)
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechZone> Zones;

	//Sloped floors added after the carving (their cavity must be carved around them)
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechRamp> Ramps;

	//Plates with their own color, never in the way: the look outside, the decor inside
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechArmor> ArmorPlates;

	//Glass boxes (panoramic windows): carve the opening, the pane closes it
	UPROPERTY(EditAnywhere, Category = "Structure")
	TArray<FMP_MechBox> GlassPanes;

	//Pilot input for the clients' simulation
	UPROPERTY(Replicated)
	FVector2D PilotInput = FVector2D::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_MoveState)
	FMP_MechMoveState MoveState;

	UFUNCTION()
	void OnRep_MoveState();

	void ApplyLook();
	void ShowDebug() const;
	//Moves the loose physics bodies inside by the mech's last move
	void CarryPhysicsBodies(const FTransform& OldTransform, const FTransform& NewTransform);
	bool IsInsideStructure(const FVector& LocalPoint) const;

	//Simulation, every machine
	FVector SimLocation = FVector::ZeroVector;
	float SimYaw = 0.f;
	float Speed = 0.f;
	float TurnRate = 0.f;
	float LastYawDelta = 0.f;

	//Clients: what is left to blend toward the server's state
	FVector LocationError = FVector::ZeroVector;
	float YawError = 0.f;
};
