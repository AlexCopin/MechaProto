// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MechaProtoCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
class USpringArmComponent;
class UC_Slap;
class UC_Ragdoll;
class UC_ProximityVoice;
class UC_CharacterMovement;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class MECHAPROTO_API AMechaProtoCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	//Third person view on the body while ragdolled
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> RagdollSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> RagdollCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_Slap> SlapComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_Ragdoll> RagdollComponent;

	//This player's voice comes from the head, it follows the body when ragdolled
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_ProximityVoice> ProximityVoice;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> SlapAction;

	//Played on the body mesh, the first person arms copy its pose
	UPROPERTY(EditAnywhere, Category ="Slap")
	TObjectPtr<UAnimMontage> SlapMontage;

	//Hold to slide
	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> SlideAction;
	
public:
	AMechaProtoCharacter();

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSlap();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSlideStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSlideEnd();

	UFUNCTION(BlueprintPure, Category="Movement")
	bool IsSliding() const;

	//The engine refuses to jump while crouched, the slide is crouched
	virtual bool CanJumpInternal_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Ragdoll")
	bool IsRagdolled() const;

	UFUNCTION()
	virtual void OnSlapSwing();

	UFUNCTION()
	virtual void OnRagdollChanged(bool bRagdolled);

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//Lowers the first person view while sliding (owner only)
	void UpdateSlideCamera(float DeltaSeconds);
	FVector FirstPersonMeshBaseLocation = FVector::ZeroVector;
	float SlideCameraOffset = 0.f;

	//Enter / loop / exit animations, every machine follows the replicated slide state
	void UpdateSlideAnimation();
	void PlaySlideLoop();
	void StopSlideExit();
	bool bWasSliding = false;
	FTimerHandle SlideLoopTimer;
	FTimerHandle SlideExitTimer;
	TWeakObjectPtr<UAnimMontage> SlideExitMontage;

	//Body mesh render settings to restore after ragdoll
	bool bMeshOwnerNoSee = true;
	EFirstPersonPrimitiveType MeshFirstPersonType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	UC_Slap* GetSlapComponent() const { return SlapComponent; }
	UC_Ragdoll* GetRagdollComponent() const { return RagdollComponent; }
	UC_ProximityVoice* GetProximityVoice() const { return ProximityVoice; }
	UC_CharacterMovement* GetMechaMovement() const;

};

