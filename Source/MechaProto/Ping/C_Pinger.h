#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PDA_Ping.h"
#include "C_Pinger.generated.h"

class AMP_Ping;

//On the player controller: pings where its view aims (screen center), whatever the view (walking, seated on a station, ragdolled).
//The local player picks the target, the server checks the rate, types it and spawns the replicated AMP_Ping
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_Pinger : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_Pinger();

	//Local input
	UFUNCTION(BlueprintCallable, Category = "Ping")
	void RequestPing();

	UFUNCTION(BlueprintPure, Category = "Ping")
	const UPDA_Ping* GetPingData() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping")
	TObjectPtr<UPDA_Ping> PingData;

	//Target: a typed actor (enemy, system, item, plate); else a spot, local to Component when it moves (the mech's structure), else world
	UFUNCTION(Server, Reliable)
	void Server_Ping(AActor* Target, USceneComponent* Component, FVector_NetQuantize10 Point);

	//What the view aims at, false when nothing is in reach
	bool FindPingTarget(AActor*& OutTarget, USceneComponent*& OutComponent, FVector& OutPoint) const;
	//A station camera seeing out through the mech (gunners, lookout, pilot), else first person (or ragdoll) inside it
	bool IsOutsideView() const;
	//Location when it isn't a ping target
	EMP_PingType ClassifyTarget(const AActor* Target) const;

	float LastPingTime = -100.f;
	float LastServerPingTime = -100.f;
	//Server: this player's pings, oldest first
	TArray<TWeakObjectPtr<AMP_Ping>> LivePings;
};
