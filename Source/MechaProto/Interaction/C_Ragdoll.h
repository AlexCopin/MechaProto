#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "C_Ragdoll.generated.h"

class ACharacter;
class UPDA_Interaction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlapCountChanged, int32, SlapCount);
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

	//Server only
	void ReceiveSlap(AActor* Slapper, const FVector& Direction);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ragdoll")
	void StartRagdoll(FVector Impulse);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ragdoll")
	void StopRagdoll();

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRagdolled() const { return RagdollState.bActive; }

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	int32 GetSlapCount() const { return SlapCount; }

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
	int32 SlapCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_RagdollState)
	FRagdollState RagdollState;

	UFUNCTION()
	void OnRep_SlapCount();

	UFUNCTION()
	void OnRep_RagdollState();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Slapped(AActor* Slapper, FVector_NetQuantizeNormal Direction, bool bKnockback);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_AddRagdollImpulse(FVector_NetQuantize10 Impulse);

	void SetSlapCount(int32 NewCount);
	void ApplyRagdollState();
	void EnterRagdoll();
	void ExitRagdoll();
	FVector GetPelvisLocation() const;
	ACharacter* GetCharacter() const;

	//State actually applied on this machine
	bool bRagdollApplied = false;
	FTransform MeshRelativeTransform;
	FName MeshCollisionProfile;
	FTimerHandle SlapResetTimer;
	FTimerHandle GetUpTimer;
};
