#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Interactable.h"
#include "MP_Item.generated.h"

class ACharacter;
class UPDA_Item;
class UStaticMeshComponent;

//Physics object a player picks up with interact and holds in hand (one at a time, UC_ItemHolder). Its use is OnUsed, per item
UCLASS()
class MECHAPROTO_API AMP_Item : public AActor, public IMP_Interactable
{
	GENERATED_BODY()

public:
	AMP_Item();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//-----IMP_Interactable
	virtual bool CanInteract(const ACharacter* User) const override;
	virtual void Interact(ACharacter* User) override;
	virtual FText GetInteractionText(const ACharacter* User) const override;

	UFUNCTION(BlueprintPure, Category = "Item")
	const UPDA_Item* GetItemData() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	ACharacter* GetHolder() const { return Holder; }

	UFUNCTION(BlueprintPure, Category = "Item")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	//Server, by UC_ItemHolder
	void Grab(ACharacter* NewHolder);
	void Release(const FVector& Location, const FVector& Velocity);
	void Use(ACharacter* User);

protected:
	//Root, simulates physics when not held
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UPDA_Item> ItemData;

	UPROPERTY(ReplicatedUsing = OnRep_Holder)
	TObjectPtr<ACharacter> Holder;

	UFUNCTION()
	void OnRep_Holder();

	//What the item does, server side, called with the use input while held
	UFUNCTION(BlueprintNativeEvent, Category = "Item")
	void OnUsed(ACharacter* User);

	//Attached to the holder's hand (first person arms for the holder) or free with physics, on every machine
	void ApplyHolder();

	TWeakObjectPtr<ACharacter> AppliedHolder;
	float LastUseTime = -100.f;
};
