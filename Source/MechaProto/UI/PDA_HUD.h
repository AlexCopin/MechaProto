#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_HUD.generated.h"

class UMP_AlertListWidget;
class UMP_AlertWidget;
class UMP_MainMenuWidget;
class UMP_PlayerStatsWidget;

//Widgets spawned by AMP_HUD and the alert settings, read live
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_HUD : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----Widgets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSubclassOf<UMP_PlayerStatsWidget> PlayerStatsWidgetClass;

	//Holds the alerts (its AlertBox panel)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSubclassOf<UMP_AlertListWidget> AlertListWidgetClass;

	//One alert, added to the list's AlertBox
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSubclassOf<UMP_AlertWidget> AlertWidgetClass;

	//Host / Join menu, shown instead of the game widgets when playing standalone (never in PIE listen/client)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	TSubclassOf<UMP_MainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widgets")
	bool bShowMainMenuInStandalone = true;

	//-----Alerts
	//The oldest ones are hidden beyond this
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alerts", meta = (ClampMin = "1", UIMax = "20"))
	int32 MaxAlerts = 5;

	//New alerts at the top of the list, otherwise at the bottom
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alerts")
	bool bNewestAlertFirst = true;

	//-----Crosshair at the screen's center, where the view aims (walking, a gun, the lookout). Sizes in pixels at 1080p, scaled with the screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	bool bShowCrosshair = true;

	//The pilot's chase camera aims at nothing
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	bool bShowCrosshairForPilot = false;

	//Length of each of the 4 lines (0 = only the dot)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "0", UIMax = "40"))
	float CrosshairSize = 6.f;

	//Empty space between the center and the lines
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "0", UIMax = "40"))
	float CrosshairGap = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair", meta = (ClampMin = "1", UIMax = "10"))
	float CrosshairThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	bool bCrosshairDot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);

	//1 px around the lines, so it shows on the bright sky too
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair")
	FLinearColor CrosshairOutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

	//-----Weapon cooldown: a ring around the crosshair fills up until the gun can fire again
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Cooldown", meta = (ClampMin = "1", UIMax = "100"))
	float CooldownRingRadius = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Cooldown", meta = (ClampMin = "1", UIMax = "10"))
	float CooldownRingThickness = 3.f;

	//Faster weapons (the gatling) show no ring, it would only flicker
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Cooldown", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float CooldownMinToShow = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Cooldown")
	FLinearColor CooldownColor = FLinearColor(1.f, 0.6f, 0.1f, 0.9f);

	//The part still to fill
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Cooldown")
	FLinearColor CooldownBackColor = FLinearColor(0.f, 0.f, 0.f, 0.35f);

	//-----Station disabled by a broken system: the crosshair takes this color and the text below names it ({0})
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Disabled")
	FLinearColor DisabledColor = FLinearColor(1.f, 0.15f, 0.1f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Disabled")
	FText DisabledText = FText::FromString(TEXT("{0} broken - repair it"));
};
