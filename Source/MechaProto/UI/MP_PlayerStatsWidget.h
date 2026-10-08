#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MP_PlayerStatsWidget.generated.h"

class UC_PlayerStats;
class UC_Ragdoll;
class UProgressBar;
class UTextBlock;

//Health, stamina and stun of the owning player's pawn, layout done in a WBP child (HealthBar, StaminaBar, StunBar, optional HealthText / StaminaText)
UCLASS(Abstract)
class MECHAPROTO_API UMP_PlayerStatsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

	//Slaps toward the ragdoll, then the ragdoll time left while ragdolled
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StunBar;

	//Current values as numbers
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	//For the visuals (flash, shake...), percents 0-1, not called for the first value
	UFUNCTION(BlueprintImplementableEvent, Category = "Stats")
	void OnHealthChanged(float Percent, float OldPercent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stats")
	void OnStaminaChanged(float Percent, float OldPercent);

	//Every frame while the ragdoll time drains
	UFUNCTION(BlueprintImplementableEvent, Category = "Stats")
	void OnStunChanged(float Percent, float OldPercent, bool bRagdolled);

	//Follows the pawn (respawn, possession)
	void UpdatePawn();

	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UC_PlayerStats> PlayerStats;
	TWeakObjectPtr<UC_Ragdoll> Ragdoll;
	//-1 until the first value
	float HealthPercent = -1.f;
	float StaminaPercent = -1.f;
	float StunPercent = -1.f;
};
