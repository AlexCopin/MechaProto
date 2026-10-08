#include "C_CharacterMovement.h"
#include "PDA_Movement.h"
#include "MP_Ladder.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

namespace
{
	//Feet this close below the top of a ladder count as standing on its top platform
	constexpr float LadderTopZone = 20.f;
}

//-----Saved move

void UC_CharacterMovement::FSavedMove_Mecha::Clear()
{
	Super::Clear();
	bSavedWantsToSlide = 0;
}

uint8 UC_CharacterMovement::FSavedMove_Mecha::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSlide)
	{
		Result |= FLAG_Custom_0;
	}
	return Result;
}

bool UC_CharacterMovement::FSavedMove_Mecha::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Mecha* Other = static_cast<const FSavedMove_Mecha*>(NewMove.Get());
	if (bSavedWantsToSlide != Other->bSavedWantsToSlide)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void UC_CharacterMovement::FSavedMove_Mecha::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UC_CharacterMovement* Movement = Cast<UC_CharacterMovement>(C->GetCharacterMovement()))
	{
		bSavedWantsToSlide = Movement->bWantsToSlide;
	}
}

void UC_CharacterMovement::FSavedMove_Mecha::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UC_CharacterMovement* Movement = Cast<UC_CharacterMovement>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSlide = bSavedWantsToSlide;
	}
}

FSavedMovePtr UC_CharacterMovement::FNetworkPredictionData_Client_Mecha::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Mecha());
}

//-----Component

UC_CharacterMovement::UC_CharacterMovement()
{
	NavAgentProps.bCanCrouch = true;
}

void UC_CharacterMovement::SetWantsToSlide(bool bInWantsToSlide)
{
	bWantsToSlide = bInWantsToSlide;
}

bool UC_CharacterMovement::IsSliding() const
{
	return IsCustomMovementMode(ECustomMovementMode::Slide);
}

bool UC_CharacterMovement::IsOnLadder() const
{
	return IsCustomMovementMode(ECustomMovementMode::Ladder);
}

bool UC_CharacterMovement::IsCustomMovementMode(ECustomMovementMode Mode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(Mode);
}

const UPDA_Movement* UC_CharacterMovement::GetMovementData() const
{
	if (ensureMsgf(MovementData, TEXT("%s has no MovementData, using code defaults"), *GetPathNameSafe(this)))
	{
		return MovementData;
	}
	return GetDefault<UPDA_Movement>();
}

FNetworkPredictionData_Client* UC_CharacterMovement::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		UC_CharacterMovement* MutableThis = const_cast<UC_CharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Mecha(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UC_CharacterMovement::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
	const UPDA_Movement* Data = GetMovementData();
	InitialPushForceFactor = Data->InitialPushImpulse;
	PushForceFactor = Data->PushForce;
	Super::ApplyImpactPhysicsForces(Impact, ImpactAcceleration, ImpactVelocity);
}

float UC_CharacterMovement::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return GetMovementData()->SlideMaxSpeed;
	}
	if (IsOnLadder())
	{
		return FMath::Max(GetMovementData()->LadderClimbSpeed, GetMovementData()->LadderSlideSpeed);
	}
	return Super::GetMaxSpeed();
}

float UC_CharacterMovement::GetMaxBrakingDeceleration() const
{
	//Slide speed loss is handled by SlideFriction, the ladder sets its speed directly
	return IsSliding() || IsOnLadder() ? 0.f : Super::GetMaxBrakingDeceleration();
}

bool UC_CharacterMovement::CanAttemptJump() const
{
	//The engine refuses to jump while crouched, the slide is crouched
	if (IsSliding())
	{
		return IsJumpAllowed() && GetMovementData()->bCanJumpOutOfSlide;
	}
	if (IsOnLadder())
	{
		return IsJumpAllowed();
	}
	return Super::CanAttemptJump();
}

bool UC_CharacterMovement::DoJump(bool bReplayingMoves, float DeltaTime)
{
	//Jumping off a ladder pushes away from it instead of straight up
	if (IsOnLadder() && CharacterOwner && CharacterOwner->CanJump())
	{
		const UPDA_Movement* Data = GetMovementData();
		const FVector Normal = CurrentLadder.IsValid() ? CurrentLadder->GetClimbNormal() : -UpdatedComponent->GetForwardVector();
		LeaveLadder(Normal * Data->LadderJumpOffSpeed + FVector::UpVector * Data->LadderJumpOffUpSpeed);
		return true;
	}
	return Super::DoJump(bReplayingMoves, DeltaTime);
}

bool UC_CharacterMovement::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() || (IsSliding() && CanEverCrouch());
}

void UC_CharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSlide = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UC_CharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	TimeSinceLadderLeft += DeltaSeconds;
	if (!IsOnLadder() && (MovementMode == MOVE_Walking || MovementMode == MOVE_Falling || IsSliding()))
	{
		AMP_Ladder* Ladder = FindOverlappingLadder();
		if (Ladder && CanGrabLadder(Ladder))
		{
			GrabLadder(Ladder);
		}
	}

	//On a ladder the slide input slides down instead, see PhysLadder
	if (bWantsToSlide && MovementMode == MOVE_Walking && CanStartSlide())
	{
		EnterSlide();
	}
	else if (!bWantsToSlide && IsSliding())
	{
		SetMovementMode(MOVE_Walking);
	}

	//Applies the crouch the slide asked for
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UC_CharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	//Custom modes don't keep the feet on the ground when the capsule shrinks
	if (IsSliding())
	{
		bCrouchMaintainsBaseLocation = true;
	}

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(ECustomMovementMode::Slide) && !IsSliding())
	{
		bWantsToCrouch = false;
	}

	//The body faces the ladder while the camera looks freely, back to the class setting after
	if (CharacterOwner)
	{
		const bool bWasOnLadder = PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(ECustomMovementMode::Ladder);
		if (IsOnLadder())
		{
			CharacterOwner->bUseControllerRotationYaw = false;
		}
		else if (bWasOnLadder)
		{
			CharacterOwner->bUseControllerRotationYaw = GetDefault<ACharacter>(CharacterOwner->GetClass())->bUseControllerRotationYaw;
			CurrentLadder.Reset();
			TimeSinceLadderLeft = 0.f;
		}
	}
}

void UC_CharacterMovement::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	if (CustomMovementMode == static_cast<uint8>(ECustomMovementMode::Slide))
	{
		PhysSlide(DeltaTime, Iterations);
	}
	else if (CustomMovementMode == static_cast<uint8>(ECustomMovementMode::Ladder))
	{
		PhysLadder(DeltaTime, Iterations);
	}
}

bool UC_CharacterMovement::CanStartSlide() const
{
	return IsMovingOnGround() && Velocity.SizeSquared2D() >= FMath::Square(GetMovementData()->SlideMinStartSpeed);
}

void UC_CharacterMovement::EnterSlide()
{
	const UPDA_Movement* Data = GetMovementData();
	SetCrouchedHalfHeight(Data->SlideHalfHeight);
	bWantsToCrouch = true;
	Velocity += Velocity.GetSafeNormal2D() * Data->SlideEnterImpulse;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Slide));
}

void UC_CharacterMovement::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const UPDA_Movement* Data = GetMovementData();

	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (!CurrentFloor.IsWalkableFloor())
	{
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;

	//Gravity kept along the floor plane: downhill speeds up, uphill slows down
	Velocity += FVector::DownVector * Data->SlideGravity * DeltaTime;
	Velocity = FVector::VectorPlaneProject(Velocity, FloorNormal);

	//Steering bends the direction without adding speed
	const FVector InputDirection = FVector::VectorPlaneProject(Acceleration, FloorNormal).GetSafeNormal();
	float Speed = Velocity.Size();
	if (!InputDirection.IsNearlyZero() && Speed > UE_KINDA_SMALL_NUMBER)
	{
		const FVector SideDirection = FVector::VectorPlaneProject(InputDirection, Velocity / Speed);
		Velocity = (Velocity + SideDirection * Data->SlideSteering * DeltaTime).GetSafeNormal() * Speed;
	}

	Speed = FMath::Min(FMath::Max(Speed - Data->SlideFriction * DeltaTime, 0.f), Data->SlideMaxSpeed);
	if (Speed < Data->SlideMinSpeed)
	{
		SetMovementMode(MOVE_Walking);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	Velocity = Velocity.GetSafeNormal() * Speed;

	Iterations++;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	//Stick to the floor going down slopes, a ledge makes the next update fall
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (CurrentFloor.IsWalkableFloor())
	{
		AdjustFloorHeight();
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

//-----Ladder

AMP_Ladder* UC_CharacterMovement::FindOverlappingLadder() const
{
	if (!CharacterOwner)
	{
		return nullptr;
	}
	TArray<AActor*> Ladders;
	CharacterOwner->GetCapsuleComponent()->GetOverlappingActors(Ladders, AMP_Ladder::StaticClass());
	return Ladders.Num() > 0 ? Cast<AMP_Ladder>(Ladders[0]) : nullptr;
}

bool UC_CharacterMovement::CanGrabLadder(const AMP_Ladder* Ladder) const
{
	const UPDA_Movement* Data = GetMovementData();
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	if (TimeSinceLadderLeft < Data->LadderRegrabDelay || InputDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector Normal = Ladder->GetClimbNormal();
	const float FeetZ = UpdatedComponent->GetComponentLocation().Z - CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bBelowTop = FeetZ < Ladder->GetTopZ() - LadderTopZone;
	//From the front: walk into the rungs. From the top platform: walk toward the climb side
	const FVector GrabDirection = bBelowTop ? -Normal : Normal;
	return FVector::DotProduct(InputDirection, GrabDirection) > Data->LadderGrabInputDot;
}

void UC_CharacterMovement::GrabLadder(AMP_Ladder* Ladder)
{
	const float ClimbInput = GetLadderClimbInput(Ladder);
	LadderHeldInput = FMath::Abs(ClimbInput) > 0.1f ? GetViewInput().GetSafeNormal2D() : FVector::ZeroVector;
	LadderHeldClimbSign = FMath::Sign(ClimbInput);

	CurrentLadder = Ladder;
	bWantsToCrouch = false;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::Ladder));
}

void UC_CharacterMovement::LeaveLadder(const FVector& NewVelocity)
{
	Velocity = NewVelocity;
	SetMovementMode(MOVE_Falling);
}

void UC_CharacterMovement::PhysLadder(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	AMP_Ladder* Ladder = CurrentLadder.IsValid() ? CurrentLadder.Get() : FindOverlappingLadder();
	if (!Ladder)
	{
		LeaveLadder(FVector::ZeroVector);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}
	CurrentLadder = Ladder;

	const UPDA_Movement* Data = GetMovementData();
	const FVector Normal = Ladder->GetClimbNormal();
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();

	//Slide input slides down
	float VerticalSpeed = 0.f;
	if (bWantsToSlide)
	{
		VerticalSpeed = -Data->LadderSlideSpeed;
	}
	else
	{
		//The input held when grabbing keeps its direction until it changes: the view turning to face the ladder
		//would make the key that walked off the top platform climb back up
		const FVector ViewInput = GetViewInput();
		float ClimbInput = GetLadderClimbInput(Ladder);
		if (FVector::DotProduct(ViewInput.GetSafeNormal2D(), LadderHeldInput) > 0.7f)
		{
			ClimbInput = LadderHeldClimbSign * FMath::Min(ViewInput.Size2D(), 1.f);
		}
		else
		{
			LadderHeldInput = FVector::ZeroVector;
		}
		VerticalSpeed = ClimbInput * Data->LadderClimbSpeed;
	}

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const float FeetZ = OldLocation.Z - HalfHeight;

	//Top: step onto the platform
	if (VerticalSpeed > 0.f && FeetZ >= Ladder->GetTopZ() + 5.f)
	{
		LeaveLadder(-Normal * Data->LadderTopExitSpeed + FVector::UpVector * Data->LadderTopExitUpSpeed);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	//Bottom: back on the ground. Not the top platform's floor, getting on from the top starts standing on it
	if (VerticalSpeed < 0.f)
	{
		FindFloor(OldLocation, CurrentFloor, false);
		const bool bOnFloor = CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist < 5.f && FeetZ < Ladder->GetTopZ() - LadderTopZone;
		if (bOnFloor || FeetZ <= Ladder->GetBottomZ() + 2.f)
		{
			Velocity = FVector::ZeroVector;
			SetMovementMode(CurrentFloor.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);
			StartNewPhysics(DeltaTime, Iterations);
			return;
		}
	}

	Iterations++;
	bJustTeleported = false;

	//Pulled onto the climb line in front of the rungs, facing the ladder
	const FVector ClimbLocation = Ladder->GetClimbLocation(OldLocation.Z, Radius + Data->LadderStandOff);
	FVector Snap = ClimbLocation - OldLocation;
	Snap.Z = 0.f;
	const FVector Delta = FVector::UpVector * VerticalSpeed * DeltaTime + Snap * FMath::Min(DeltaTime * 15.f, 1.f);
	const FQuat FacingLadder = FRotator(0.f, (-Normal).Rotation().Yaw, 0.f).Quaternion();
	const FQuat NewRotation = FQuat::Slerp(UpdatedComponent->GetComponentQuat(), FacingLadder, FMath::Min(DeltaTime * 12.f, 1.f));

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, NewRotation, true, Hit);
	if (Hit.Time < 1.f)
	{
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

FVector UC_CharacterMovement::GetViewInput() const
{
	const FVector Input = Acceleration / FMath::Max(GetMaxAcceleration(), UE_KINDA_SMALL_NUMBER);
	return FRotator(0.f, CharacterOwner->GetControlRotation().Yaw, 0.f).UnrotateVector(Input);
}

float UC_CharacterMovement::GetLadderClimbInput(const AMP_Ladder* Ladder) const
{
	//Toward the rungs climbs, away goes down (input is view relative, so walking off the top platform goes down)
	//Looking down, forward goes down whatever the view faces
	const FVector ViewInput = GetViewInput();
	const FVector Input = Acceleration / FMath::Max(GetMaxAcceleration(), UE_KINDA_SMALL_NUMBER);
	const bool bLookingDown = CharacterOwner->GetControlRotation().GetNormalized().Pitch < -GetMovementData()->LadderLookDownPitch;
	const float ClimbInput = bLookingDown && ViewInput.X > UE_KINDA_SMALL_NUMBER ? -ViewInput.X : FVector::DotProduct(Input, -Ladder->GetClimbNormal());
	return FMath::Clamp(ClimbInput, -1.f, 1.f);
}

