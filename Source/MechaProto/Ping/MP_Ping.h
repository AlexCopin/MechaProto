#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PDA_Ping.h"
#include "MP_Ping.generated.h"

class AHUD;
class APlayerState;
class UCanvas;

//One crew ping, spawned by the server (UC_Pinger) and replicated to everyone: follows its target (enemy, item, system, plate) or stays on its
//spot, moving with the part of the mech it was put on. Every HUD draws it through walls (DrawPings). Ends after Lifetime
UCLASS(NotPlaceable)
class MECHAPROTO_API AMP_Ping : public AActor
{
	GENERATED_BODY()

public:
	AMP_Ping();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//Server, before FinishSpawning. Point: local to Component if any, else world
	void InitPing(UPDA_Ping* Data, APlayerState* InPinger, EMP_PingType InType, AActor* Target, USceneComponent* Component, const FVector& Point);

	//Every machine: each ping on this HUD's canvas (marker, label, distance and pinger; an arrow on the screen's edge out of view)
	static void DrawPings(AHUD& HUD, UCanvas& Canvas);

	UFUNCTION(BlueprintPure, Category = "Ping")
	FVector GetMarkerLocation() const;

	UFUNCTION(BlueprintPure, Category = "Ping")
	FText GetLabel() const;

	UFUNCTION(BlueprintPure, Category = "Ping")
	EMP_PingType GetPingType() const { return Type; }

	UFUNCTION(BlueprintPure, Category = "Ping")
	AActor* GetTargetActor() const { return TargetActor; }

	UFUNCTION(BlueprintPure, Category = "Ping")
	APlayerState* GetPinger() const { return Pinger; }

	const UPDA_Ping* GetPingData() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Replicated)
	TObjectPtr<UPDA_Ping> PingData;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> Pinger;

	UPROPERTY(Replicated)
	EMP_PingType Type = EMP_PingType::Location;

	//Followed by the marker (typed pings)
	UPROPERTY(Replicated)
	TObjectPtr<AActor> TargetActor;

	//A spot ping on something that moves (the mech's structure): attached to it at AnchorPoint
	UPROPERTY(ReplicatedUsing = OnRep_Anchor)
	TObjectPtr<USceneComponent> AnchorComponent;

	UPROPERTY(ReplicatedUsing = OnRep_Anchor)
	FVector_NetQuantize10 AnchorPoint;

	//Server times: fades and pops on every machine, a late joiner doesn't hear an old ping
	UPROPERTY(Replicated)
	float SpawnServerTime = 0.f;

	UPROPERTY(Replicated)
	float EndServerTime = 0.f;

	UFUNCTION()
	void OnRep_Anchor();

	void ApplyAnchor();
	//Server: an enemy ping ends shortly after its enemy is dead or gone
	void CheckTarget();
	FTimerHandle CheckTimer;

	float GetServerTime() const;
	//0-1, fading out at the end
	float GetAlpha() const;
	//Bigger when it appears
	float GetPopScale() const;
	void DrawMarker(UCanvas& Canvas, const FVector& ViewLocation, const FRotator& ViewRotation, float Scale) const;

	//Where a lost target was
	mutable FVector LastTargetLocation = FVector::ZeroVector;
};
