#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Movement.generated.h"

class UAnimSequenceBase;

//Player movement tuning (slide), read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Movement : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----Slide
	//Ground speed needed to start a slide
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float SlideMinStartSpeed = 350.f;

	//Speed added when the slide starts
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "1500", Units = "CentimetersPerSecond"))
	float SlideEnterImpulse = 400.f;

	//Pull down the slope: the steeper the slope, the more of it accelerates the slide (gravity x sin(angle))
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "6000"))
	float SlideGravity = 2500.f;

	//Speed lost per second, on flat ground a slide only slows down
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "3000"))
	float SlideFriction = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "4000", Units = "CentimetersPerSecond"))
	float SlideMaxSpeed = 1800.f;

	//The slide ends under this speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float SlideMinSpeed = 300.f;

	//How fast the move input bends the slide sideways, it never adds speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0", UIMax = "3000"))
	float SlideSteering = 600.f;

	//Capsule half height while sliding, to fit under low gaps
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "20", UIMax = "96", Units = "cm"))
	float SlideHalfHeight = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
	bool bCanJumpOutOfSlide = true;

	//Extra drop of the first person camera while sliding, the slide animation already lowers the head
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Camera", meta = (ClampMin = "0", UIMax = "120", Units = "cm"))
	float SlideCameraDrop = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Camera", meta = (ClampMin = "0", UIMax = "30"))
	float SlideCameraInterpSpeed = 12.f;

	//-----Slide animations, played on the body slot (root locked, the movement drives the character)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	TObjectPtr<UAnimSequenceBase> SlideEnterAnimation;

	//Skips the run at the start of the enter animation (GASP clips: the drop starts at 0.75 s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0", UIMax = "3", Units = "s"))
	float SlideEnterStartTime = 0.7f;

	//Time played from SlideEnterStartTime before switching to the loop
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0.05", UIMax = "3", Units = "s"))
	float SlideEnterDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	TObjectPtr<UAnimSequenceBase> SlideLoopAnimation;

	//Slide ended faster than SlideExitRunSpeed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	TObjectPtr<UAnimSequenceBase> SlideExitRunAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	TObjectPtr<UAnimSequenceBase> SlideExitWalkAnimation;

	//Slide ended slower than SlideExitWalkSpeed (stopped by a wall)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	TObjectPtr<UAnimSequenceBase> SlideExitIdleAnimation;

	//Only the stand up part of the exit plays, then locomotion takes over (GASP clips: standing at 1 s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0.05", UIMax = "3", Units = "s"))
	float SlideExitDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float SlideExitRunSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0", UIMax = "500", Units = "CentimetersPerSecond"))
	float SlideExitWalkSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float SlideAnimationBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide|Animation")
	FName SlideAnimationSlot = FName("DefaultSlot");
};
