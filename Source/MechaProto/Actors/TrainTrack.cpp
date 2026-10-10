#include "Actors/TrainTrack.h"

ATrainTrack::ATrainTrack() {
	PrimaryActorTick.bCanEverTick = true;
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	RootComponent = Spline;
}

void ATrainTrack::BeginPlay() {
	Super::BeginPlay();
	
}

void ATrainTrack::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

}

