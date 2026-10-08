#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "C_CharacterMovement.generated.h"

class AMP_Ladder;
class UC_PlayerStats;
class UPDA_Movement;

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
	None UMETA(Hidden),
	Slide,
	Ladder,
};

//Character movement with a predicted run (stamina), slide and ladder (custom movement modes, input sent in the saved moves)
UCLASS()
class MECHAPROTO_API UC_CharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

	class FSavedMove_Mecha : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSlide : 1;
		uint8 bSavedWantsToRun : 1;

		//Run state at the start of the move, put back for replays and combined moves
		uint8 bSavedRunExhausted : 1;
		float SavedStamina = 0.f;
		float SavedStaminaRegenDelayLeft = 0.f;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
		virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	};

	class FNetworkPredictionData_Client_Mecha : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Mecha(const UCharacterMovementComponent& ClientMovement) : FNetworkPredictionData_Client_Character(ClientMovement) {}

		virtual FSavedMovePtr AllocateNewMove() override;
	};

public:
	UC_CharacterMovement();

	//Hold to slide, called by the local player input
	UFUNCTION(BlueprintCallable, Category = "Slide")
	void SetWantsToSlide(bool bInWantsToSlide);

	UFUNCTION(BlueprintPure, Category = "Slide")
	bool IsSliding() const;

	//Hold to run, called by the local player input
	UFUNCTION(BlueprintCallable, Category = "Run")
	void SetWantsToRun(bool bInWantsToRun);

	//Running this move: walking forward or climbing a ladder with the run held, stamina left. Server and owning client only
	UFUNCTION(BlueprintPure, Category = "Run")
	bool IsRunning() const { return bIsRunning; }

	//Simulated here on the server and the owning client only, the others read UC_PlayerStats
	bool IsStaminaSimulated() const;
	float GetStamina() const { return FMath::Max(Stamina, 0.f); }

	UFUNCTION(BlueprintPure, Category = "Ladder")
	bool IsOnLadder() const;

	//Only valid on the server and the owning client (simulated proxies follow the replicated mode)
	UFUNCTION(BlueprintPure, Category = "Ladder")
	AMP_Ladder* GetCurrentLadder() const { return CurrentLadder.Get(); }

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsCustomMovementMode(ECustomMovementMode Mode) const;

	UFUNCTION(BlueprintPure, Category = "Movement")
	const UPDA_Movement* GetMovementData() const;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TObjectPtr<UPDA_Movement> MovementData;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	//Push strength from the data, read live
	virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity) override;

	//Each move: whether it runs, then stamina drain / regen
	void UpdateRun(float DeltaSeconds);
	float GetMaxStamina() const;
	UC_PlayerStats* GetPlayerStats() const;

	bool CanStartSlide() const;
	void EnterSlide();
	void PhysSlide(float DeltaTime, int32 Iterations);

	AMP_Ladder* FindOverlappingLadder() const;
	bool CanGrabLadder(const AMP_Ladder* Ladder) const;
	void GrabLadder(AMP_Ladder* Ladder);
	void LeaveLadder(const FVector& NewVelocity);
	void PhysLadder(float DeltaTime, int32 Iterations);
	//Move input in view space (X forward, Y right)
	FVector GetViewInput() const;
	//-1 down to 1 up, from the move input and the view
	float GetLadderClimbInput(const AMP_Ladder* Ladder) const;
	//The climb input, or the held input's direction while it doesn't change
	float GetLadderVerticalInput(const AMP_Ladder* Ladder) const;
	bool IsLadderInputHeld() const;

	TWeakObjectPtr<AMP_Ladder> CurrentLadder;
	//Input held when grabbing (view space) and its climb direction, kept until the input changes
	FVector LadderHeldInput = FVector::ZeroVector;
	float LadderHeldClimbSign = 0.f;
	//Counted in move time so replays agree
	float TimeSinceLadderLeft = 100.f;

	//Input state, replayed by the saved moves
	bool bWantsToSlide = false;
	bool bWantsToRun = false;

	//Run state, simulated the same way by the owning client and the server; the server copies the stamina to UC_PlayerStats
	bool bIsRunning = false;
	bool bRunExhausted = false;
	//-1 until the first move fills it
	float Stamina = -1.f;
	float StaminaRegenDelayLeft = 0.f;
	mutable TWeakObjectPtr<UC_PlayerStats> CachedPlayerStats;
};
