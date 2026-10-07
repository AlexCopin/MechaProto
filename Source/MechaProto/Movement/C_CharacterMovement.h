#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "C_CharacterMovement.generated.h"

class AMP_Ladder;
class UPDA_Movement;

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
	None UMETA(Hidden),
	Slide,
	Ladder,
};

//Character movement with a predicted slide and ladder (custom movement modes, input sent in the saved moves)
UCLASS()
class MECHAPROTO_API UC_CharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

	class FSavedMove_Mecha : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		uint8 bSavedWantsToSlide : 1;

		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
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

	TWeakObjectPtr<AMP_Ladder> CurrentLadder;
	//Input held when grabbing (view space) and its climb direction, kept until the input changes
	FVector LadderHeldInput = FVector::ZeroVector;
	float LadderHeldClimbSign = 0.f;
	//Counted in move time so replays agree
	float TimeSinceLadderLeft = 100.f;

	//Input state, replayed by the saved moves
	bool bWantsToSlide = false;
};
