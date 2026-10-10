#pragma once

#include "CoreMinimal.h"
#include "Actors/TrainTrack.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "Datas/TrainData.h"
#include "TrainMovementComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MECHAPROTO_API UTrainMovementComponent : public UActorComponent {
	GENERATED_BODY()

public:	
	UTrainMovementComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetTargetSpeed(float NewSpeed);

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTrainData> TrainData;
private:
	
	UPROPERTY()
	TObjectPtr<ATrainTrack> Track;
	
	float Distance;
	
	float CurrentSpeed;

	float TargetSpeed;
	float StartSpeed;
	
	float TransitionTime = 0.f;
	float TransitionDuration = 0.f;
};
