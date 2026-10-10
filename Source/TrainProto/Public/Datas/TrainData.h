#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrainData.generated.h"

UCLASS()
class TRAINPROTO_API UTrainData : public UDataAsset {
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.0",Units="cm/s"))
	float MinSpeed;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.0",Units="cm/s"))
	float MaxSpeed;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.0",Units="cm/s^2"))
	float Acceleration;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.0",Units="cm/s^2"))
	float Deceleration;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement|Curve")
	TObjectPtr<UCurveFloat> AccelerationCurve;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement|Curve")
	TObjectPtr<UCurveFloat> DecelerationCurve;
	
};
