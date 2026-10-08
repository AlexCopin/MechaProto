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
};
