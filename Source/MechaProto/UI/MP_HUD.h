#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MP_Alert.h"
#include "MP_HUD.generated.h"

class UMP_AlertListWidget;
class UMP_AlertWidget;
class UMP_MainMenuWidget;
class UMP_PlayerStatsWidget;
class UPDA_HUD;

//Spawns and owns every widget of its local player: player stats and alerts in game, the main menu in standalone
//Alerts are global: RaiseAlert / ClearAlert from anywhere reach every local player's HUD
UCLASS()
class MECHAPROTO_API AMP_HUD : public AHUD
{
	GENERATED_BODY()

public:
	AMP_HUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

	//Shows the alert on every local player's screen, or refreshes it (same Key and Source) and restarts its timer
	//Local only: call it where every machine sees the change (an OnRep, and on the server which gets no OnRep)
	UFUNCTION(BlueprintCallable, Category = "HUD|Alerts", meta = (WorldContext = "WorldContextObject"))
	static void RaiseAlert(const UObject* WorldContextObject, const FMP_Alert& Alert);

	//Hides the alert with this Key and Source on every local player's screen
	UFUNCTION(BlueprintCallable, Category = "HUD|Alerts", meta = (WorldContext = "WorldContextObject"))
	static void ClearAlert(const UObject* WorldContextObject, FName Key, AActor* Source);

	//This HUD only
	void ShowAlert(const FMP_Alert& Alert);
	void HideAlert(FName Key, const AActor* Source);

	UFUNCTION(BlueprintPure, Category = "HUD")
	const UPDA_HUD* GetHUDData() const;

	UFUNCTION(BlueprintPure, Category = "HUD")
	UMP_PlayerStatsWidget* GetPlayerStatsWidget() const { return PlayerStatsWidget; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UPDA_HUD> HUDData;

	UPROPERTY(Transient)
	TObjectPtr<UMP_PlayerStatsWidget> PlayerStatsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMP_AlertListWidget> AlertListWidget;

	UPROPERTY(Transient)
	TObjectPtr<UMP_MainMenuWidget> MainMenuWidget;

	//Shown alerts, oldest first, with their end time (0 = until cleared)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMP_AlertWidget>> Alerts;
	TArray<float> AlertEndTimes;

	void ShowGameWidgets();
	void ShowMainMenu();
	//Crosshair, weapon cooldown ring and the broken system of the station (game widgets only)
	void DrawCrosshair();
	void DrawCooldownRing(const FVector2D& Center, float Scale, float ReadyPercent);
	//Plays its hide animation, it removes itself after
	void RemoveAlertAt(int32 Index);
	static void ForEachLocalHUD(const UObject* WorldContextObject, TFunctionRef<void(AMP_HUD&)> Function);
};
