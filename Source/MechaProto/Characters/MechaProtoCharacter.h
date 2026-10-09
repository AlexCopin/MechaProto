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
class UC_Interactor;
class UC_StationUser;
class UC_ItemHolder;
class UC_PlayerStats;
class AMP_Station;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AMechaProtoCharacter : public ACharacter
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_Interactor> Interactor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_StationUser> StationUser;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_ItemHolder> ItemHolder;

	//Health and stamina, shown by the HUD
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UC_PlayerStats> PlayerStats;

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

	//Hold to run
	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> InteractAction;

	//Mapped in the station context, only active while manning
	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> StationFireAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> DropItemAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> UseItemAction;

	UPROPERTY(EditAnywhere, Category ="Input")
	TObjectPtr<UInputAction> ThrowItemAction;
	
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
	virtual void DoInteract();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDropItem();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoUseItem();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoThrowItem();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStationFireStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoStationFireEnd();

	//Movement, slide and slap are locked while manning a station
	UFUNCTION(BlueprintPure, Category="Station")
	bool IsManningStation() const;

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSlideStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSlideEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoRunStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoRunEnd();

	UFUNCTION(BlueprintPure, Category="Movement")
	bool IsSliding() const;

	UFUNCTION(BlueprintPure, Category="Movement")
	bool IsOnLadder() const;

	//The engine refuses to jump while crouched (slide) and off a ladder
	virtual bool CanJumpInternal_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Ragdoll")
	bool IsRagdolled() const;

	UFUNCTION()
	virtual void OnSlapSwing();

	UFUNCTION()
	virtual void OnRagdollChanged(bool bRagdolled);

	UFUNCTION()
	void OnStationChanged(AMP_Station* Station);

	//Ragdolled or seated: the owner sees the body from a third person camera instead of the first person arms
	void UpdateBodyView();
	bool bBodyViewApplied = false;

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

	//Climb loop played at a rate following the vertical speed, every machine
	void UpdateLadderAnimation();
	bool bWasOnLadder = false;
	TWeakObjectPtr<UAnimMontage> LadderMontage;
	TWeakObjectPtr<UAnimSequenceBase> LadderMontageAnimation;

	//The first person camera's field of view from DA_Movement (owner only)
	void UpdateFieldOfView();

	//Turns the view to face the ladder when grabbing it (owner only)
	void UpdateLadderCamera(float DeltaSeconds);
	bool bLadderCameraWasOnLadder = false;
	//Negative when not turning
	float LadderCameraTime = -1.f;
	FRotator LadderCameraStart = FRotator::ZeroRotator;
	FRotator LadderCameraTarget = FRotator::ZeroRotator;
	FRotator LadderCameraLast = FRotator::ZeroRotator;

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
	UC_Interactor* GetInteractor() const { return Interactor; }
	UC_StationUser* GetStationUser() const { return StationUser; }
	UC_ItemHolder* GetItemHolder() const { return ItemHolder; }
	UC_PlayerStats* GetPlayerStats() const { return PlayerStats; }

};

