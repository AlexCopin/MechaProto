#include "Components/TrainMovementComponent.h"

#include "Kismet/GameplayStatics.h"

UTrainMovementComponent::UTrainMovementComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UTrainMovementComponent::BeginPlay() {
	Super::BeginPlay();

	if (ATrainTrack* T_Track = Cast<ATrainTrack>(UGameplayStatics::GetActorOfClass(GetWorld(),ATrainTrack::StaticClass()))) {
		Track = T_Track;
	} else {
		UE_LOG(LogTemp,Error,TEXT("TrainMovementComponent: No TrainTrack found in the world!"));
		// Add GEngine message
		return;
	}

	if (!IsValid(TrainData)) {
		UE_LOG(LogTemp,Error,TEXT("TrainMovementComponent: No TrainData assigned!"));
		return;
	}

	Distance = Track->Spline->GetDistanceAlongSplineAtLocation(GetOwner()->GetActorLocation(),ESplineCoordinateSpace::World);
	FVector Location = Track->Spline->GetLocationAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World);
	
	GetOwner()->SetActorLocation(Location);
	SetTargetSpeed(TrainData->MaxSpeed);
}

void UTrainMovementComponent::SetTargetSpeed(float NewSpeed) {
	if (!IsValid(TrainData)) {
		return;	
	}
	
	TargetSpeed = FMath::Clamp(NewSpeed, 0.f, TrainData->MaxSpeed);
	StartSpeed = CurrentSpeed;
	TransitionTime = 0.f;

	float Rate = (TargetSpeed > StartSpeed) ? TrainData->Acceleration : TrainData->Deceleration;
	TransitionDuration = FMath::Abs(TargetSpeed - StartSpeed) / Rate;
}

void UTrainMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(TrainData)) {
		return;
	}

	if (TransitionDuration > 0.f && TransitionTime < TransitionDuration) {
		TransitionTime += DeltaTime;
		float Alpha = FMath::Clamp(TransitionTime / TransitionDuration, 0.f, 1.f);
		bool bAccelerate = TargetSpeed > StartSpeed;
		UCurveFloat* Curve = bAccelerate ? TrainData->AccelerationCurve : TrainData->DecelerationCurve;

		float Eased = Curve->GetFloatValue(Alpha);
		CurrentSpeed = FMath::Lerp(StartSpeed,TargetSpeed,Eased);
	}
	
	Distance += CurrentSpeed * DeltaTime;
	Distance = FMath::Clamp(Distance, 0.f, Track->Spline->GetSplineLength());

	FVector Location = Track->Spline->GetLocationAtDistanceAlongSpline(Distance,ESplineCoordinateSpace::World);
	GetOwner()->SetActorLocation(Location);
}

