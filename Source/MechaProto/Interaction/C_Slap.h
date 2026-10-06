#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_Slap.generated.h"

class UPDA_Interaction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlapSwing);

//Lets the owning pawn slap whatever has a UC_Ragdoll in front of it
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_Slap : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_Slap();

	//Called by the local player input
	UFUNCTION(BlueprintCallable, Category = "Slap")
	void TrySlap();

	UFUNCTION(BlueprintPure, Category = "Slap")
	bool CanSlap() const;

	UFUNCTION(BlueprintPure, Category = "Slap")
	const UPDA_Interaction* GetInteractionData() const;

	//Fired on every machine when the swing plays, hit or not
	UPROPERTY(BlueprintAssignable, Category = "Slap")
	FOnSlapSwing OnSlapSwing;

protected:
	//Tuning shared with the other interaction components
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap")
	TObjectPtr<UPDA_Interaction> InteractionData;

	UFUNCTION(Server, Reliable)
	void Server_Slap();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlap();

	bool IsOwnerRagdolled() const;

	float LastLocalSlapTime = -100.f;
	float LastServerSlapTime = -100.f;
};
