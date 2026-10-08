#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_PlayerStats.generated.h"

//Player health and stamina, read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_PlayerStats : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1", UIMax = "1000"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "1", UIMax = "1000"))
	float MaxStamina = 100.f;
};
