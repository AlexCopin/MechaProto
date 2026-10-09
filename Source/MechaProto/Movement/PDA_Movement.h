#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Movement.generated.h"

class UAnimSequenceBase;

//Player movement tuning (walk / run, slide, ladder), read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Movement : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----View
	//Field of view of the first person camera (stations set their own in their data)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View", meta = (ClampMin = "40", ClampMax = "120", Units = "Degrees"))
	float FieldOfView = 80.f;

	//-----Walk / run (hold run: faster, uses stamina; the max stamina is in DA_PlayerStats)
	//Normal ground speed, a fast walk
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "50", UIMax = "1500", Units = "CentimetersPerSecond"))
	float WalkSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "50", UIMax = "1500", Units = "CentimetersPerSecond"))
	float RunSpeed = 750.f;

	//Stamina used per second of running (100 max stamina / 20 = 5 s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0", UIMax = "100"))
	float RunStaminaCost = 20.f;

	//Stamina regained per second, once StaminaRegenDelay has passed without running
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0", UIMax = "100"))
	float StaminaRegenRate = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float StaminaRegenDelay = 1.f;

	//Out of stamina: no run until it is back to this, so it doesn't stutter at 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0", UIMax = "100"))
	float RunRestartStamina = 30.f;

	//Runs only when the move input is within this angle of the facing direction (no sideways or backward run)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float RunMaxInputAngle = 60.f;

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

	//Full body slot of the anim BP used by the slide and ladder animations
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName BodyAnimationSlot = FName("DefaultSlot");

	//-----Ladder (moving toward the rungs climbs, away goes down, looking down past LadderLookDownPitch forward goes down, hold slide to slide down)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "800", Units = "CentimetersPerSecond"))
	float LadderClimbSpeed = 150.f;

	//Climbing with the run held (up or down), uses the run's stamina (RunStaminaCost)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "800", Units = "CentimetersPerSecond"))
	float LadderSprintSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "2500", Units = "CentimetersPerSecond"))
	float LadderSlideSpeed = 900.f;

	//Looking down more than this makes forward go down, whichever way the view faces
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	float LadderLookDownPitch = 35.f;

	//Gap between the capsule and the rungs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "50", Units = "cm"))
	float LadderStandOff = 8.f;

	//How much the move input must point at the ladder to grab it (dot product)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", ClampMax = "1"))
	float LadderGrabInputDot = 0.5f;

	//How much the view must face the ladder too (dot product, 0.3 = within ~70 deg): walking past a ladder doesn't grab it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "-1", ClampMax = "1"))
	float LadderGrabViewDot = 0.3f;

	//No grab right after leaving a ladder
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float LadderRegrabDelay = 0.4f;

	//Push onto the platform when reaching the top
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float LadderTopExitSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float LadderTopExitUpSpeed = 250.f;

	//Jump away from the ladder
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "1500", Units = "CentimetersPerSecond"))
	float LadderJumpOffSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder", meta = (ClampMin = "0", UIMax = "1500", Units = "CentimetersPerSecond"))
	float LadderJumpOffUpSpeed = 300.f;

	//Time to turn the view to face the ladder when grabbing it, 0 leaves the view alone. Looking around during the turn adds to it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Camera", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float LadderCameraBlendTime = 0.35f;

	//Pitch the view turns to, 0 looks straight at the rungs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Camera", meta = (ClampMin = "-89", ClampMax = "89", Units = "Degrees"))
	float LadderCameraPitch = 0.f;

	//-----Ladder animation: one climb loop, played forward going up, backward going down, paused when still
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Animation")
	TObjectPtr<UAnimSequenceBase> LadderClimbAnimation;

	//Climb speed the animation was made for (play rate = vertical speed / this)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Animation", meta = (ClampMin = "1", UIMax = "800", Units = "CentimetersPerSecond"))
	float LadderClimbAnimationSpeed = 38.5f;

	//Optional pose while sliding down, otherwise the climb animation is paused
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Animation")
	TObjectPtr<UAnimSequenceBase> LadderSlideAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder|Animation", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float LadderAnimationBlendTime = 0.15f;

	//-----Pushing physics objects (items...) by walking into them, the push accelerates light ones more
	//Impulse on an object at rest (engine default 500)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Push", meta = (ClampMin = "0", UIMax = "5000"))
	float InitialPushImpulse = 300.f;

	//Force while walking into a moving object (engine default 750000, launches a 5 kg item at hundreds of m/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Push", meta = (ClampMin = "0", UIMax = "200000"))
	float PushForce = 20000.f;
};
