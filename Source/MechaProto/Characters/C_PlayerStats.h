#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_PlayerStats.generated.h"

class UPDA_PlayerStats;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerStatChanged, float, NewValue, float, OldValue);

//Health and stamina of a player: server authoritative, replicated. Nothing drains them yet
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_PlayerStats : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_PlayerStats();

	//Every machine
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnPlayerStatChanged OnHealthChanged;

	//Every machine
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnPlayerStatChanged OnStaminaChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//Server: adds Amount (negative removes), clamped to [0, max]
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void ModifyHealth(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void ModifyStamina(float Amount);

	UFUNCTION(BlueprintPure, Category = "Stats")
	const UPDA_PlayerStats* GetStatsData() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMaxHealth() const;

	//0-1
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMaxStamina() const;

	//0-1
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetStaminaPercent() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UPDA_PlayerStats> StatsData;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	float Stamina = 0.f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void OnRep_Stamina(float OldStamina);
};
