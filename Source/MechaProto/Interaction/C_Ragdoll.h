#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "C_Ragdoll.generated.h"

class ACharacter;
class UPDA_Interaction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlapCountChanged, float, SlapCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlapped, AActor*, Slapper, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRagdollChanged, bool, bRagdolled);

USTRUCT()
struct FRagdollState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	FVector_NetQuantize10 Impulse = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize GetUpLocation = FVector::ZeroVector;
};

//Receives slaps, ragdolls the owning character after enough of them
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_Ragdoll : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_Ragdoll();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//Server only. Strength multiplies the pushes (knockback, ragdoll impulses), Value is what it counts toward the ragdoll (2 = two slaps)
	void ReceiveSlap(AActor* Slapper, const FVector& Direction, float Strength = 1.f, float Value = 1.f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ragdoll")
	void StartRagdoll(FVector Impulse);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ragdoll")
	void StopRagdoll();

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRagdolled() const { return RagdollState.bActive; }

	//Sum of the slap values received, ragdoll at SlapsToRagdoll
	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	float GetSlapCount() const { return SlapCount; }

	//0-1 for the HUD: slaps toward the ragdoll, then the ragdoll time left while ragdolled
	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	float GetStunPercent() const;

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	const UPDA_Interaction* GetInteractionData() const;

	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnSlapCountChanged OnSlapCountChanged;

	//Fired on every machine
	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnSlapped OnSlapped;

	//Fired on every machine
	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnRagdollChanged OnRagdollChanged;

protected:
	//Tuning shared with the other interaction components
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll")
	TObjectPtr<UPDA_Interaction> InteractionData;

	UPROPERTY(EditAnywhere, Category = "Ragdoll")
	FName PelvisBoneName = FName("pelvis");

	UPROPERTY(EditAnywhere, Category = "Ragdoll")
	FName RagdollCollisionProfile = FName("Ragdoll");

	UPROPERTY(ReplicatedUsing = OnRep_SlapCount)
	float SlapCount = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_RagdollState)
	FRagdollState RagdollState;

	UFUNCTION()
	void OnRep_SlapCount();

	UFUNCTION()
	void OnRep_RagdollState();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Slapped(AActor* Slapper, FVector_NetQuantizeNormal Direction, bool bKnockback, float Strength);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_AddRagdollImpulse(FVector_NetQuantize10 Impulse);

	void SetSlapCount(float NewCount);
	void ApplyRagdollState();
	void EnterRagdoll();
	void ExitRagdoll();
	FVector GetPelvisLocation() const;
	ACharacter* GetCharacter() const;

	//State actually applied on this machine
	bool bRagdollApplied = false;
	//Local time the ragdoll started on this machine
	float RagdollStartTime = 0.f;
	FTransform MeshRelativeTransform;
	FName MeshCollisionProfile;
	FTimerHandle SlapResetTimer;
	FTimerHandle GetUpTimer;
};
