#include "C_CharacterMovement.h"
#include "PDA_Movement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

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

float UC_CharacterMovement::GetMaxSpeed() const
{
	return IsSliding() ? GetMovementData()->SlideMaxSpeed : Super::GetMaxSpeed();
}

float UC_CharacterMovement::GetMaxBrakingDeceleration() const
{
	//Slide speed loss is handled by SlideFriction
	return IsSliding() ? 0.f : Super::GetMaxBrakingDeceleration();
}

bool UC_CharacterMovement::CanAttemptJump() const
{
	//The engine refuses to jump while crouched, the slide is crouched
	if (IsSliding())
	{
		return IsJumpAllowed() && GetMovementData()->bCanJumpOutOfSlide;
	}
	return Super::CanAttemptJump();
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
}

void UC_CharacterMovement::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	if (CustomMovementMode == static_cast<uint8>(ECustomMovementMode::Slide))
	{
		PhysSlide(DeltaTime, Iterations);
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
