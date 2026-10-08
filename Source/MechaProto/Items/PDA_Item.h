#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Item.generated.h"

class UAnimSequenceBase;

//One item type (crate, canister...), its use comes from the item BP or a C++ child of AMP_Item
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Item : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText ItemName = FText::FromString(TEXT("Item"));

	//Placement relative to the hand socket while held
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FTransform HoldOffset = FTransform::Identity;

	//Overrides the mesh mass (0 = from the mesh volume)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", UIMax = "200", Units = "kg"))
	float Mass = 20.f;

	//Thrown (right click) and hitting a player = a slap from the thrower (counts toward the ragdoll)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bThrowSlapsPlayers = false;

	//Multiplies the pushes of that slap (DA_Interaction values)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", UIMax = "5", EditCondition = "bThrowSlapsPlayers"))
	float SlapStrength = 1.f;

	//That slap counts as this many slaps toward the ragdoll (DA_Interaction.SlapsToRagdoll)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", UIMax = "5", EditCondition = "bThrowSlapsPlayers"))
	float SlapValue = 1.f;

	//Time between two uses
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float UseCooldown = 0.5f;

	//-----Use animation, played on the holder's body (the first person arms copy it) at each use, on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation")
	TObjectPtr<UAnimSequenceBase> UseAnimation;

	//Part of the animation played: from this time, for this long (animation time), then it blends out
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation", meta = (ClampMin = "0", Units = "s"))
	float UseAnimationStartTime = 0.f;

	//0 = to the end
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation", meta = (ClampMin = "0", Units = "s"))
	float UseAnimationDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation", meta = (ClampMin = "0.1", UIMax = "4"))
	float UseAnimationPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float UseAnimationBlendTime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use Animation")
	FName BodyAnimationSlot = FName("DefaultSlot");
};
