#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_PlayerRecovery.generated.h"

class UPDA_Recovery;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerRespawned);

//Brings its character back aboard the mech after OutsideRespawnDelay outside it (fell out, thrown out...), at one of the mech's player starts
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_PlayerRecovery : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_PlayerRecovery();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//Server: to a free player start on the mech (any start without a mech), keeping the held item
	UFUNCTION(BlueprintCallable, Category = "Recovery")
	void Respawn();

	//Seconds before the automatic respawn while outside the mech, < 0 on it (owner and server)
	UFUNCTION(BlueprintPure, Category = "Recovery")
	float GetAutoRespawnTimeLeft() const;

	UFUNCTION(BlueprintPure, Category = "Recovery")
	const UPDA_Recovery* GetRecoveryData() const;

	//Every machine, after a respawn
	UPROPERTY(BlueprintAssignable, Category = "Recovery")
	FOnPlayerRespawned OnRespawned;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TObjectPtr<UPDA_Recovery> RecoveryData;

	//Server time of the automatic respawn, 0 while on the mech
	UPROPERTY(Replicated)
	float AutoRespawnTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_RespawnCount)
	uint8 RespawnCount = 0;

	UFUNCTION()
	void OnRep_RespawnCount();

	//Server, every few frames
	void CheckOutside();
	FTimerHandle CheckTimer;
};
