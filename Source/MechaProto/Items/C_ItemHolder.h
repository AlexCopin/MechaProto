#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_ItemHolder.generated.h"

class AMP_Item;
class UPDA_Interaction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldItemChanged, AMP_Item*, Item);

//Holds one item in hand: picked up with interact, dropped to take another
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_ItemHolder : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_ItemHolder();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Items")
	AMP_Item* GetHeldItem() const { return HeldItem; }

	UFUNCTION(BlueprintPure, Category = "Items")
	const UPDA_Interaction* GetInteractionData() const;

	//Server
	bool PickUp(AMP_Item* Item);
	void Drop();
	//Server: released at ThrowSpeed, slaps the first player hit (bThrowSlapsPlayers)
	void Throw();

	//Local input
	void RequestDrop();
	void RequestUse();
	void RequestThrow();

	//Fired on every machine
	UPROPERTY(BlueprintAssignable, Category = "Items")
	FOnHeldItemChanged OnHeldItemChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TObjectPtr<UPDA_Interaction> InteractionData;

	UPROPERTY(ReplicatedUsing = OnRep_HeldItem)
	TObjectPtr<AMP_Item> HeldItem;

	UFUNCTION()
	void OnRep_HeldItem();

	UFUNCTION(Server, Reliable)
	void Server_Drop();

	UFUNCTION(Server, Reliable)
	void Server_Use();

	UFUNCTION(Server, Reliable)
	void Server_Throw();

	//In front of the eyes (not through a wall) at this speed along the view
	void ReleaseHeldItem(float Speed, float UpSpeed, bool bThrow);
};
