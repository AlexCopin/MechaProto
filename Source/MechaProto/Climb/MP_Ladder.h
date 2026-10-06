#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Ladder.generated.h"

class UArrowComponent;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

//Climbable ladder, placed against a wall: the arrow (actor forward) points to the side players climb from, the origin is the bottom
UCLASS()
class MECHAPROTO_API AMP_Ladder : public AActor
{
	GENERATED_BODY()

public:
	AMP_Ladder();

	virtual void OnConstruction(const FTransform& Transform) override;

	//Side the climber stands on
	FVector GetClimbNormal() const;
	float GetBottomZ() const;
	float GetTopZ() const;
	//Point in front of the ladder at the given height, Distance from the front of the rungs
	FVector GetClimbLocation(float Z, float Distance) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> ClimbVolume;

	//Ladder mesh stacked up to Height
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Segments;

	//Placeholder rails and rungs when no LadderMesh is set
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftRail;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightRail;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Rungs;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UArrowComponent> ClimbSideArrow;

	//Any ladder mesh: turned so its widest side is the width, scaled to Width, stacked and slightly stretched to fill Height
	UPROPERTY(EditAnywhere, Category = "Ladder")
	TObjectPtr<UStaticMesh> LadderMesh;

	//From the actor origin (bottom) to the top platform
	UPROPERTY(EditAnywhere, Category = "Ladder", meta = (ClampMin = "100", UIMax = "2000", Units = "cm"))
	float Height = 400.f;

	UPROPERTY(EditAnywhere, Category = "Ladder", meta = (ClampMin = "30", UIMax = "150", Units = "cm"))
	float Width = 60.f;

	//Placeholder rungs only
	UPROPERTY(EditAnywhere, Category = "Ladder", meta = (ClampMin = "10", UIMax = "60", Units = "cm"))
	float RungSpacing = 30.f;

	//How far in front of the rungs players can grab it
	UPROPERTY(EditAnywhere, Category = "Ladder", meta = (ClampMin = "20", UIMax = "200", Units = "cm"))
	float GrabDepth = 80.f;

	void BuildFromMesh();
	void BuildPlaceholder();

	//Distance from the origin to the climb side face of the ladder
	UPROPERTY()
	float FrontOffset = 0.f;
};
