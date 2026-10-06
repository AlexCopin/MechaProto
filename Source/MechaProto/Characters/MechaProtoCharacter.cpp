// Copyright Epic Games, Inc. All Rights Reserved.

#include "MechaProtoCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "C_Slap.h"
#include "C_Ragdoll.h"
#include "MechaProto.h"

AMechaProtoCharacter::AMechaProtoCharacter()
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
}

void AMechaProtoCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SlapComponent->OnSlapSwing.AddDynamic(this, &AMechaProtoCharacter::OnSlapSwing);
	RagdollComponent->OnRagdollChanged.AddDynamic(this, &AMechaProtoCharacter::OnRagdollChanged);
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
	if (GetController() && !IsRagdolled())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AMechaProtoCharacter::DoJumpStart()
{
	if (IsRagdolled())
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
	SlapComponent->TrySlap();
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
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (bRagdolled)
	{
		//Owner sees his own body from the ragdoll camera
		bMeshOwnerNoSee = BodyMesh->bOwnerNoSee;
		MeshFirstPersonType = BodyMesh->FirstPersonPrimitiveType;
		BodyMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		BodyMesh->SetOwnerNoSee(false);
		FirstPersonMesh->SetVisibility(false, true);
		FirstPersonCameraComponent->Deactivate();
		RagdollCamera->Activate();
	}
	else
	{
		BodyMesh->SetFirstPersonPrimitiveType(MeshFirstPersonType);
		BodyMesh->SetOwnerNoSee(bMeshOwnerNoSee);
		FirstPersonMesh->SetVisibility(true, true);
		RagdollCamera->Deactivate();
		FirstPersonCameraComponent->Activate();
	}
}
