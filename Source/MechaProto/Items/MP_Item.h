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
	//Back home instead of being destroyed
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

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
	//Thrower: thrown (slaps the first player hit when bThrowSlapsPlayers), null for a simple drop
	void Release(const FVector& Location, const FVector& Velocity, ACharacter* Thrower = nullptr);
	void Use(ACharacter* User);

	//Local prediction of a use by the holder (cooldown), plays the animation: true if the use should be sent
	bool PredictUse();

	//Every machine: UseAnimation on the holder's body
	void PlayUseAnimation();

	//Server: back where it was at the start (out of its holder's hands first), still: recall buttons, fallen out of the world
	UFUNCTION(BlueprintCallable, Category = "Item")
	void ReturnHome();

	//Where it was at the start, now: it moves with the part of the mech it was in (hall, leg, arm...)
	FTransform GetHomeTransform() const;

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

	//Uses on the server, the others play the animation from it
	UPROPERTY(ReplicatedUsing = OnRep_UseCount)
	uint8 UseCount = 0;

	UFUNCTION()
	void OnRep_UseCount();

	//What the item does, server side, called with the use input while held
	UFUNCTION(BlueprintNativeEvent, Category = "Item")
	void OnUsed(ACharacter* User);

	//Attached to the holder's hand (first person arms for the holder) or free with physics, on every machine
	void ApplyHolder();

	//Server: a thrown item slaps the first player it hits
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	TWeakObjectPtr<ACharacter> AppliedHolder;
	//Server: the start spot relative to the mech's part holding it (the world without a mech)
	TWeakObjectPtr<USceneComponent> HomeParent;
	FTransform HomeRelative = FTransform::Identity;
	//Server, while the throw can slap
	TWeakObjectPtr<ACharacter> ThrownBy;
	FVector ThrowDirection = FVector::ZeroVector;
	float ThrowTime = 0.f;
	float LastUseTime = -100.f;
	//Holder's machine: last predicted use (its own OnRep is skipped)
	float LastPredictedUseTime = -100.f;
	FTimerHandle UseAnimationTimer;
};
