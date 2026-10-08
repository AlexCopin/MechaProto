// Copyright Epic Games, Inc. All Rights Reserved.

#include "MechaProtoCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "C_Slap.h"
#include "C_Ragdoll.h"
#include "C_ProximityVoice.h"
#include "C_CharacterMovement.h"
#include "C_Interactor.h"
#include "C_StationUser.h"
#include "C_ItemHolder.h"
#include "C_PlayerStats.h"
#include "PDA_Interaction.h"
#include "MP_WeaponStation.h"
#include "MP_Ladder.h"
#include "PDA_Movement.h"
#include "MechaProto.h"

AMechaProtoCharacter::AMechaProtoCharacter()
	: Super(FObjectInitializer::Get().SetDefaultSubobjectClass<UC_CharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);
	//Weapons are for enemies, projectiles fly through players
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	//Follows the pelvis bone, so it stays on the body while ragdolled
	RagdollSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Ragdoll Spring Arm"));
	RagdollSpringArm->SetupAttachment(GetMesh(), FName("pelvis"));
	RagdollSpringArm->SetUsingAbsoluteRotation(true);
	RagdollSpringArm->TargetArmLength = 350.0f;
	RagdollSpringArm->bUsePawnControlRotation = true;
	RagdollSpringArm->bDoCollisionTest = true;

	RagdollCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Ragdoll Camera"));
	RagdollCamera->SetupAttachment(RagdollSpringArm, USpringArmComponent::SocketName);
	RagdollCamera->bUsePawnControlRotation = false;
	RagdollCamera->SetAutoActivate(false);

	SlapComponent = CreateDefaultSubobject<UC_Slap>(TEXT("Slap"));
	RagdollComponent = CreateDefaultSubobject<UC_Ragdoll>(TEXT("Ragdoll"));

	Interactor = CreateDefaultSubobject<UC_Interactor>(TEXT("Interactor"));
	StationUser = CreateDefaultSubobject<UC_StationUser>(TEXT("Station User"));
	ItemHolder = CreateDefaultSubobject<UC_ItemHolder>(TEXT("Item Holder"));
	PlayerStats = CreateDefaultSubobject<UC_PlayerStats>(TEXT("Player Stats"));

	ProximityVoice = CreateDefaultSubobject<UC_ProximityVoice>(TEXT("Proximity Voice"));
	ProximityVoice->SetupAttachment(GetMesh(), FName("head"));
}

void AMechaProtoCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SlapComponent->OnSlapSwing.AddDynamic(this, &AMechaProtoCharacter::OnSlapSwing);
	RagdollComponent->OnRagdollChanged.AddDynamic(this, &AMechaProtoCharacter::OnRagdollChanged);
	StationUser->OnStationChanged.AddDynamic(this, &AMechaProtoCharacter::OnStationChanged);
}

void AMechaProtoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMechaProtoCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMechaProtoCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMechaProtoCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMechaProtoCharacter::LookInput);

		if (SlapAction)
		{
			EnhancedInputComponent->BindAction(SlapAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoSlap);
		}

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoInteract);
		}

		if (DropItemAction)
		{
			EnhancedInputComponent->BindAction(DropItemAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoDropItem);
		}

		if (UseItemAction)
		{
			EnhancedInputComponent->BindAction(UseItemAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoUseItem);
		}

		if (ThrowItemAction)
		{
			EnhancedInputComponent->BindAction(ThrowItemAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoThrowItem);
		}

		if (StationFireAction)
		{
			EnhancedInputComponent->BindAction(StationFireAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoStationFireStart);
			EnhancedInputComponent->BindAction(StationFireAction, ETriggerEvent::Completed, this, &AMechaProtoCharacter::DoStationFireEnd);
		}

		if (SlideAction)
		{
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoSlideStart);
			EnhancedInputComponent->BindAction(SlideAction, ETriggerEvent::Completed, this, &AMechaProtoCharacter::DoSlideEnd);
		}

		if (RunAction)
		{
			EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Started, this, &AMechaProtoCharacter::DoRunStart);
			EnhancedInputComponent->BindAction(RunAction, ETriggerEvent::Completed, this, &AMechaProtoCharacter::DoRunEnd);
		}
	}
	else
	{
		UE_LOG(LogMechaProto, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AMechaProtoCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AMechaProtoCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AMechaProtoCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMechaProtoCharacter::DoMove(float Right, float Forward)
{
	//Seated: the station uses it (the pilot station drives the mech)
	if (IsManningStation())
	{
		StationUser->AddDriveInput(Right, Forward);
		return;
	}
	if (GetController() && !IsRagdolled())
	{
		//Relative to the view: on a ladder the body faces the rungs while the camera looks freely
		const FRotator ViewYaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y), Right);
		AddMovementInput(ViewYaw.Vector(), Forward);
	}
}

void AMechaProtoCharacter::DoJumpStart()
{
	if (IsRagdolled() || IsManningStation())
	{
		return;
	}

	// pass Jump to the character
	Jump();
}

void AMechaProtoCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void AMechaProtoCharacter::DoSlap()
{
	if (!IsManningStation())
	{
		SlapComponent->TrySlap();
	}
}

void AMechaProtoCharacter::DoInteract()
{
	//Interact again to leave the station, it is behind the camera
	if (IsManningStation())
	{
		Interactor->InteractWith(StationUser->GetStation());
		return;
	}
	if (!IsRagdolled())
	{
		Interactor->TryInteract();
	}
}

void AMechaProtoCharacter::DoDropItem()
{
	if (!IsRagdolled())
	{
		ItemHolder->RequestDrop();
	}
}

void AMechaProtoCharacter::DoUseItem()
{
	if (!IsRagdolled() && !IsManningStation())
	{
		ItemHolder->RequestUse();
	}
}

void AMechaProtoCharacter::DoThrowItem()
{
	if (!IsRagdolled() && !IsManningStation())
	{
		ItemHolder->RequestThrow();
	}
}

void AMechaProtoCharacter::DoStationFireStart()
{
	StationUser->SetFiring(true);
}

void AMechaProtoCharacter::DoStationFireEnd()
{
	StationUser->SetFiring(false);
}

bool AMechaProtoCharacter::IsManningStation() const
{
	return StationUser && StationUser->IsManning();
}

bool AMechaProtoCharacter::IsRagdolled() const
{
	return RagdollComponent && RagdollComponent->IsRagdolled();
}

void AMechaProtoCharacter::OnSlapSwing()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && SlapMontage)
	{
		//The attack anim has root motion, the slap must not move the player
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		AnimInstance->Montage_Play(SlapMontage);
	}
}

void AMechaProtoCharacter::OnRagdollChanged(bool bRagdolled)
{
	//Knocked off the station, the held item falls
	if (bRagdolled && HasAuthority())
	{
		StationUser->LeaveStation();
		if (ItemHolder->GetInteractionData()->bDropItemWhenRagdolled)
		{
			ItemHolder->Drop();
		}
	}

	UpdateBodyView();
	if (bRagdolled)
	{
		//The ragdoll profile blocks projectiles, a player body on the floor still isn't a target
		GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
		FirstPersonCameraComponent->Deactivate();
		RagdollCamera->Activate();
	}
	else
	{
		RagdollCamera->Deactivate();
		FirstPersonCameraComponent->Activate();
	}
}

void AMechaProtoCharacter::OnStationChanged(AMP_Station* Station)
{
	UpdateBodyView();
}

void AMechaProtoCharacter::UpdateBodyView()
{
	const bool bShowBody = IsRagdolled() || IsManningStation();
	if (bShowBody == bBodyViewApplied)
	{
		return;
	}
	bBodyViewApplied = bShowBody;

	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (bShowBody)
	{
		bMeshOwnerNoSee = BodyMesh->bOwnerNoSee;
		MeshFirstPersonType = BodyMesh->FirstPersonPrimitiveType;
		BodyMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		BodyMesh->SetOwnerNoSee(false);
		FirstPersonMesh->SetVisibility(false, true);
	}
	else
	{
		BodyMesh->SetFirstPersonPrimitiveType(MeshFirstPersonType);
		BodyMesh->SetOwnerNoSee(bMeshOwnerNoSee);
		FirstPersonMesh->SetVisibility(true, true);
	}
}

void AMechaProtoCharacter::DoSlideStart()
{
	if (UC_CharacterMovement* Movement = GetMechaMovement(); Movement && !IsRagdolled() && !IsManningStation())
	{
		Movement->SetWantsToSlide(true);
	}
}

void AMechaProtoCharacter::DoSlideEnd()
{
	if (UC_CharacterMovement* Movement = GetMechaMovement())
	{
		Movement->SetWantsToSlide(false);
	}
}

void AMechaProtoCharacter::DoRunStart()
{
	//Held through a ragdoll or a station, it runs again once walking
	if (UC_CharacterMovement* Movement = GetMechaMovement())
	{
		Movement->SetWantsToRun(true);
	}
}

void AMechaProtoCharacter::DoRunEnd()
{
	if (UC_CharacterMovement* Movement = GetMechaMovement())
	{
		Movement->SetWantsToRun(false);
	}
}

bool AMechaProtoCharacter::IsSliding() const
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	return Movement && Movement->IsSliding();
}

bool AMechaProtoCharacter::IsOnLadder() const
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	return Movement && Movement->IsOnLadder();
}

bool AMechaProtoCharacter::CanJumpInternal_Implementation() const
{
	return IsSliding() || IsOnLadder() ? JumpIsAllowedInternal() : Super::CanJumpInternal_Implementation();
}

UC_CharacterMovement* AMechaProtoCharacter::GetMechaMovement() const
{
	return Cast<UC_CharacterMovement>(GetCharacterMovement());
}

void AMechaProtoCharacter::BeginPlay()
{
	Super::BeginPlay();

	FirstPersonMeshBaseLocation = FirstPersonMesh->GetRelativeLocation();
}

void AMechaProtoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSlideAnimation();
	UpdateLadderAnimation();

	if (IsLocallyControlled())
	{
		UpdateSlideCamera(DeltaSeconds);
		UpdateLadderCamera(DeltaSeconds);
	}
}

void AMechaProtoCharacter::UpdateSlideAnimation()
{
	const bool bSliding = IsSliding();
	if (bSliding == bWasSliding)
	{
		return;
	}
	bWasSliding = bSliding;

	const UC_CharacterMovement* Movement = GetMechaMovement();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Movement || !AnimInstance)
	{
		return;
	}
	const UPDA_Movement* Data = Movement->GetMovementData();
	const float Blend = Data->SlideAnimationBlendTime;
	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	GetWorldTimerManager().ClearTimer(SlideLoopTimer);
	GetWorldTimerManager().ClearTimer(SlideExitTimer);

	if (bSliding)
	{
		UAnimSequenceBase* Enter = Data->SlideEnterAnimation;
		if (!Enter)
		{
			PlaySlideLoop();
			return;
		}
		const float StartTime = FMath::Clamp(Data->SlideEnterStartTime, 0.f, Enter->GetPlayLength());
		AnimInstance->PlaySlotAnimationAsDynamicMontage(Enter, Data->BodyAnimationSlot, Blend, Blend, 1.f, 1, -1.f, StartTime);
		const float LoopDelay = FMath::Min(Data->SlideEnterDuration, Enter->GetPlayLength() - StartTime - Blend);
		GetWorldTimerManager().SetTimer(SlideLoopTimer, this, &AMechaProtoCharacter::PlaySlideLoop, FMath::Max(LoopDelay, 0.01f), false);
		return;
	}

	//No exit animation in the air, the jump/fall pose takes over
	UAnimSequenceBase* Exit = nullptr;
	if (!Movement->IsFalling())
	{
		const float Speed = GetVelocity().Size2D();
		Exit = Speed > Data->SlideExitRunSpeed ? Data->SlideExitRunAnimation : Speed > Data->SlideExitWalkSpeed ? Data->SlideExitWalkAnimation : Data->SlideExitIdleAnimation;
	}
	if (Exit)
	{
		SlideExitMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Exit, Data->BodyAnimationSlot, Blend, Blend);
		GetWorldTimerManager().SetTimer(SlideExitTimer, this, &AMechaProtoCharacter::StopSlideExit, Data->SlideExitDuration, false);
	}
	else
	{
		AnimInstance->StopSlotAnimation(Blend, Data->BodyAnimationSlot);
	}
}

void AMechaProtoCharacter::StopSlideExit()
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Movement || !AnimInstance || !SlideExitMontage.IsValid())
	{
		return;
	}
	AnimInstance->Montage_Stop(Movement->GetMovementData()->SlideAnimationBlendTime, SlideExitMontage.Get());
}

void AMechaProtoCharacter::PlaySlideLoop()
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Movement || !AnimInstance || !IsSliding())
	{
		return;
	}

	const UPDA_Movement* Data = Movement->GetMovementData();
	if (!Data->SlideLoopAnimation)
	{
		return;
	}
	const float Blend = Data->SlideAnimationBlendTime;
	if (UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Data->SlideLoopAnimation, Data->BodyAnimationSlot, Blend, Blend))
	{
		//Loops until the slide ends and something else plays on the slot
		AnimInstance->Montage_SetNextSection(FName("Default"), FName("Default"), Montage);
	}
}

void AMechaProtoCharacter::UpdateSlideCamera(float DeltaSeconds)
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	if (!Movement)
	{
		return;
	}

	//The body has no slide pose, so the first person arms and camera are moved down instead
	const UPDA_Movement* Data = Movement->GetMovementData();
	const float TargetOffset = Movement->IsSliding() ? -Data->SlideCameraDrop : 0.f;
	if (FMath::IsNearlyEqual(SlideCameraOffset, TargetOffset, 0.1f))
	{
		return;
	}
	SlideCameraOffset = FMath::FInterpTo(SlideCameraOffset, TargetOffset, DeltaSeconds, Data->SlideCameraInterpSpeed);
	FirstPersonMesh->SetRelativeLocation(FirstPersonMeshBaseLocation + FVector(0.f, 0.f, SlideCameraOffset));
}

void AMechaProtoCharacter::UpdateLadderCamera(float DeltaSeconds)
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	AController* PlayerController = GetController();
	if (!Movement || !PlayerController)
	{
		return;
	}
	const UPDA_Movement* Data = Movement->GetMovementData();

	const bool bOnLadder = Movement->IsOnLadder();
	const bool bGrabbed = bOnLadder && !bLadderCameraWasOnLadder;
	bLadderCameraWasOnLadder = bOnLadder;
	if (bGrabbed && Movement->GetCurrentLadder() && Data->LadderCameraBlendTime > 0.f)
	{
		LadderCameraStart = PlayerController->GetControlRotation();
		LadderCameraLast = LadderCameraStart;
		LadderCameraTarget = FRotator(Data->LadderCameraPitch, (-Movement->GetCurrentLadder()->GetClimbNormal()).Rotation().Yaw, 0.f);
		LadderCameraTime = 0.f;
	}
	if (!bOnLadder || LadderCameraTime < 0.f)
	{
		LadderCameraTime = -1.f;
		return;
	}

	//Looking around during the turn is added on top of it instead of fighting it
	const FRotator LookInput = (PlayerController->GetControlRotation() - LadderCameraLast).GetNormalized();
	LadderCameraStart += LookInput;
	LadderCameraTarget += LookInput;

	LadderCameraTime += DeltaSeconds;
	const float Alpha = FMath::Min(LadderCameraTime / Data->LadderCameraBlendTime, 1.f);
	LadderCameraLast = FMath::Lerp(LadderCameraStart, LadderCameraTarget, FMath::SmoothStep(0.f, 1.f, Alpha));
	PlayerController->SetControlRotation(LadderCameraLast);
	if (Alpha >= 1.f)
	{
		LadderCameraTime = -1.f;
	}
}

void AMechaProtoCharacter::UpdateLadderAnimation()
{
	const UC_CharacterMovement* Movement = GetMechaMovement();
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Movement || !AnimInstance)
	{
		return;
	}
	const UPDA_Movement* Data = Movement->GetMovementData();
	const float Blend = Data->LadderAnimationBlendTime;

	const bool bOnLadder = Movement->IsOnLadder();
	if (!bOnLadder)
	{
		if (bWasOnLadder && LadderMontage.IsValid())
		{
			AnimInstance->Montage_Stop(Blend, LadderMontage.Get());
		}
		bWasOnLadder = false;
		LadderMontage.Reset();
		return;
	}
	bWasOnLadder = true;

	//Velocity is replicated, so simulated proxies animate the same way
	const float VerticalSpeed = GetVelocity().Z;
	const bool bSlidingDown = VerticalSpeed < -0.5f * (Data->LadderClimbSpeed + Data->LadderSlideSpeed);
	UAnimSequenceBase* Wanted = bSlidingDown && Data->LadderSlideAnimation ? Data->LadderSlideAnimation.Get() : Data->LadderClimbAnimation.Get();
	if (!Wanted)
	{
		return;
	}

	//Start or switch the loop (also restarts it if something else played on the slot)
	if (!LadderMontage.IsValid() || LadderMontageAnimation.Get() != Wanted || !AnimInstance->Montage_IsPlaying(LadderMontage.Get()))
	{
		UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Wanted, Data->BodyAnimationSlot, Blend, Blend);
		if (!Montage)
		{
			return;
		}
		AnimInstance->Montage_SetNextSection(FName("Default"), FName("Default"), Montage);
		LadderMontage = Montage;
		LadderMontageAnimation = Wanted;
	}

	//The climb follows the speed: forward going up, backward going down, paused when still or sliding without a slide pose
	float PlayRate = 1.f;
	if (Wanted == Data->LadderClimbAnimation)
	{
		PlayRate = bSlidingDown ? 0.f : VerticalSpeed / Data->LadderClimbAnimationSpeed;
	}
	UAnimMontage* Montage = LadderMontage.Get();
	AnimInstance->Montage_SetPlayRate(Montage, PlayRate);

	//Section loops only wrap forward, wrap by hand when playing backward
	const float Length = Wanted->GetPlayLength();
	if (PlayRate < 0.f && AnimInstance->Montage_GetPosition(Montage) <= 0.02f)
	{
		AnimInstance->Montage_SetPosition(Montage, FMath::Max(Length - 0.02f, 0.f));
	}
}

