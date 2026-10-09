#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Alert.h"
#include "MP_AlertSource.h"
#include "MP_HullPlate.generated.h"

class UArrowComponent;
class UMaterialInstanceDynamic;
class UPDA_HullPlate;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHullPlateHealthChanged, AMP_HullPlate*, Plate, float, Health);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHullPlateBrokenChanged, AMP_HullPlate*, Plate, bool, bBroken);

//Breakable plate closing an opening of the mech's hull: the arrow (actor forward) points outside, the actor origin is the bottom center of the opening
//Enemies crash on it (server); at 0 health it disappears and leaves a hole the small ones go through to hunt the players inside
UCLASS()
class MECHAPROTO_API AMP_HullPlate : public AActor, public IMP_AlertSource
{
	GENERATED_BODY()

public:
	AMP_HullPlate();

	UPROPERTY(BlueprintAssignable, Category = "Hull")
	FOnHullPlateHealthChanged OnHealthChanged;

	//Every machine
	UPROPERTY(BlueprintAssignable, Category = "Hull")
	FOnHullPlateBrokenChanged OnBrokenChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ShowCurrentAlerts(AMP_HUD& HUD) override;

	//Server: an enemy hits it
	void ReceiveHullDamage(float Damage, AActor* Attacker);

	//Server: gives health back, closes the hole as soon as it has some
	UFUNCTION(BlueprintCallable, Category = "Hull")
	void Repair(float Amount);

	UFUNCTION(BlueprintPure, Category = "Hull")
	const UPDA_HullPlate* GetPlateData() const;

	UFUNCTION(BlueprintPure, Category = "Hull")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Hull")
	bool IsBroken() const { return bBroken; }

	FVector GetOutsideDirection() const { return GetActorForwardVector(); }
	//World point at Distance from the plate's middle plane (> 0 outside, < 0 inside), Up above the bottom of the opening, Side along its width from its center
	FVector GetPointInFront(float Distance, float Up, float Side) const;
	//Opening size with the actor scale
	float GetWidth() const;
	float GetHeight() const;
	float GetThickness() const;
	//The opening it closes, in its own space (before the actor's scale): X across the wall, Y the width, Z up from the origin
	FBox GetOpeningBox() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UArrowComponent> OutsideArrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	TObjectPtr<UPDA_HullPlate> PlateData;

	//Shown in the alerts (North 2, Left leg...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull")
	FText PlateName;

	//Opening closed by the plate, before the actor scale
	UPROPERTY(EditAnywhere, Category = "Hull", meta = (ClampMin = "20", UIMax = "1000", Units = "cm"))
	float Width = 140.f;

	UPROPERTY(EditAnywhere, Category = "Hull", meta = (ClampMin = "20", UIMax = "1000", Units = "cm"))
	float Height = 110.f;

	UPROPERTY(EditAnywhere, Category = "Hull", meta = (ClampMin = "2", UIMax = "200", Units = "cm"))
	float Thickness = 40.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void OnRep_Broken();

	void ApplyData();
	void UpdateMeshSize();
	void UpdateColor();
	void PlayBreakEffects();
	//bBreach: lasting critical alert, otherwise the timed damage one
	FMP_Alert MakeAlert(bool bBreach);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	float HitFlashTimeLeft = 0.f;
};
