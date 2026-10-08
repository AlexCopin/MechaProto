#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Breakdowns.generated.h"

//How often the mech's systems break down (AMP_BreakdownManager), read live
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Breakdowns : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//Starts breaking things on BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns")
	bool bAutoStart = true;

	//Calm time at the start
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns", meta = (ClampMin = "0", UIMax = "600", Units = "s"))
	float FirstBreakdownDelay = 30.f;

	//Time between two breakdowns, random in this range
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns", meta = (ClampMin = "1", UIMax = "600", Units = "s"))
	float MinInterval = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns", meta = (ClampMin = "1", UIMax = "600", Units = "s"))
	float MaxInterval = 50.f;

	//No new breakdown while this many systems are broken (the timer waits)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns", meta = (ClampMin = "1", UIMax = "20"))
	int32 MaxBrokenAtOnce = 2;

	//Time to the next breakdown on screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDebug = true;
};
