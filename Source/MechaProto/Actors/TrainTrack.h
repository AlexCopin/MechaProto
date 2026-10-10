#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "TrainTrack.generated.h"

UCLASS()
class MECHAPROTO_API ATrainTrack : public AActor {
	GENERATED_BODY()
	
public:	
	ATrainTrack();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<USplineComponent> Spline;

};
