#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Mech.h"
#include "MP_MechBlock.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EMP_MechBlockType : uint8
{
	//Filled volume, axis aligned
	Solid,
	//Room, corridor, shaft, hole: carved out of every solid, axis aligned
	Cavity,
	//See-through pane that blocks like a wall, axis aligned
	Glass,
	//Sloped floor slab walked on its top face (pitch and yaw only)
	Ramp,
	//Armor outside or decor inside: colored, no collision, any rotation; doorways, holes and portholes cut it
	Plate,
	//Where a moving part (Part) turns: a leg's hip, an arm's shoulder (its center)
	Pivot
};

//A piece of the mech's structure placed in the scene: a box of the actor's size (scale 1 = 1 m, or Size), moved, scaled, duplicated and deleted
//with the usual tools. Attached to the mech, which turns them into its structure and rebuilds when one changes. Editor only: gone in game
UCLASS(HideCategories = (Rendering, Replication, Collision, Input, Actor, LOD, Cooking, Physics, Networking, HLOD, WorldPartition, DataLayers))
class MECHAPROTO_API AMP_MechBlock : public AActor
{
	GENERATED_BODY()

public:
	AMP_MechBlock();

	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif

	UPROPERTY(EditAnywhere, Category = "Block")
	EMP_MechBlockType Type = EMP_MechBlockType::Cavity;

	//Size in cm, the same as the actor's scale x 100
	UPROPERTY(EditAnywhere, Category = "Block", meta = (ClampMin = "1", Units = "cm"))
	FVector Size = FVector(100.f);

	//Cavities: the mech zone whose colors paint the walls, floor and ceiling around it. Ramps: its floor color
	UPROPERTY(EditAnywhere, Category = "Block", meta = (EditCondition = "Type == EMP_MechBlockType::Cavity || Type == EMP_MechBlockType::Ramp"))
	FName Zone;

	//Solids, ramps and plates: a leg's or an arm's pieces move with it. Pivot: the part it turns
	UPROPERTY(EditAnywhere, Category = "Block", meta = (EditCondition = "Type != EMP_MechBlockType::Cavity && Type != EMP_MechBlockType::Glass"))
	EMP_MechPart Part = EMP_MechPart::Body;

	UPROPERTY(EditAnywhere, Category = "Block", meta = (EditCondition = "Type == EMP_MechBlockType::Plate"))
	FLinearColor Color = FLinearColor(0.62f, 0.57f, 0.48f);

	//Glow, 0 = none
	UPROPERTY(EditAnywhere, Category = "Block", meta = (EditCondition = "Type == EMP_MechBlockType::Plate", ClampMin = "0", UIMax = "100"))
	float Emissive = 0.f;

	//In the space of the actor it is attached to (the mech, built at rest)
	FTransform GetBoxTransform(const FTransform& MechTransform) const;
	FBox GetAlignedBox(const FTransform& MechTransform) const;

protected:
	virtual void BeginPlay() override;

	//Shown in the editor, colored by type
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Box;

	void RebuildMech() const;
};
