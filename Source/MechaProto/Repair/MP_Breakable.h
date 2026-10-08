#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Alert.h"
#include "MP_AlertSource.h"
#include "MP_Interactable.h"
#include "MP_Breakable.generated.h"

class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UPDA_Breakable;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakableChanged, AMP_Breakable*, Breakable, bool, bBroken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakableRepairHit, AMP_Breakable*, Breakable, int32, RepairCount);

//A system inside the mech (engine, pipe, rotor...) that breaks down (AMP_BreakdownManager) and is repaired by hitting it with its tool in hand
//Its mesh is set in the BP. Other actors can depend on it (AMP_WeaponStation::RequiredSystems)
UCLASS()
class MECHAPROTO_API AMP_Breakable : public AActor, public IMP_Interactable, public IMP_AlertSource
{
	GENERATED_BODY()

public:
	AMP_Breakable();

	//Every machine
	UPROPERTY(BlueprintAssignable, Category = "Breakable")
	FOnBreakableChanged OnBrokenChanged;

	//Every machine
	UPROPERTY(BlueprintAssignable, Category = "Breakable")
	FOnBreakableRepairHit OnRepairHit;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//-----IMP_Interactable: focusable while broken (the prompt says which tool), interacting with the tool in hand is a repair hit
	virtual bool CanInteract(const ACharacter* User) const override;
	virtual void Interact(ACharacter* User) override;
	virtual FText GetInteractionText(const ACharacter* User) const override;

	//-----IMP_AlertSource
	virtual void ShowCurrentAlerts(AMP_HUD& HUD) override;

	//Server
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakable")
	void Break();

	//Server: instantly working again
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakable")
	void Fix();

	//Server: one hit of the user's held tool, false if broken but not the right tool
	bool RepairHit(ACharacter* User);

	//The user holds the tool this system needs
	bool HasRequiredTool(const ACharacter* User) const;

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

	//0-1 while broken
	UFUNCTION(BlueprintPure, Category = "Breakable")
	float GetRepairPercent() const;

	UFUNCTION(BlueprintPure, Category = "Breakable")
	const UPDA_Breakable* GetBreakableData() const;

	UFUNCTION(BlueprintPure, Category = "Breakable")
	FText GetLocationName() const { return LocationName; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	//Set the mesh in the BP; turns with SpinSpeed
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<UPDA_Breakable> BreakableData;

	//Where it is in the mech, shown in the alert (Core, Left arm...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable")
	FText LocationName;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;

	//Hits received since it broke
	UPROPERTY(ReplicatedUsing = OnRep_RepairCount)
	int32 RepairCount = 0;

	UFUNCTION()
	void OnRep_Broken();

	UFUNCTION()
	void OnRep_RepairCount(int32 OldRepairCount);

	void ApplyData();
	void UpdateColor();
	FMP_Alert MakeBrokenAlert() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BrokenEffectComponent;

	float HitFlashTimeLeft = 0.f;
	//Mesh rotation when placed, the spin is added to it
	FRotator MeshBaseRotation = FRotator::ZeroRotator;
	float SpinAngle = 0.f;
};
