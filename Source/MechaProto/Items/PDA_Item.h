#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Item.generated.h"

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
	float Mass = 10.f;

	//Time between two uses
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float UseCooldown = 0.5f;
};
