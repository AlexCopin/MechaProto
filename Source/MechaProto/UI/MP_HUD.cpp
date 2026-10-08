#include "MP_HUD.h"
#include "MP_AlertListWidget.h"
#include "MP_AlertSource.h"
#include "MP_AlertWidget.h"
#include "MP_MainMenuWidget.h"
#include "MP_PlayerStatsWidget.h"
#include "PDA_HUD.h"
#include "Components/PanelWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AMP_HUD::AMP_HUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

const UPDA_HUD* AMP_HUD::GetHUDData() const
{
	if (ensureMsgf(HUDData, TEXT("%s has no HUDData, using code defaults"), *GetPathNameSafe(this)))
	{
		return HUDData;
	}
	return GetDefault<UPDA_HUD>();
}

void AMP_HUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	//Standalone = not hosting nor joined yet
	const UPDA_HUD* Data = GetHUDData();
	if (GetNetMode() == NM_Standalone && Data->bShowMainMenuInStandalone && Data->MainMenuWidgetClass)
	{
		ShowMainMenu();
		return;
	}
	ShowGameWidgets();
}

void AMP_HUD::ShowGameWidgets()
{
	const UPDA_HUD* Data = GetHUDData();
	if (Data->PlayerStatsWidgetClass)
	{
		PlayerStatsWidget = CreateWidget<UMP_PlayerStatsWidget>(PlayerOwner, Data->PlayerStatsWidgetClass);
		if (PlayerStatsWidget)
		{
			PlayerStatsWidget->AddToPlayerScreen(0);
		}
	}
	if (Data->AlertListWidgetClass)
	{
		AlertListWidget = CreateWidget<UMP_AlertListWidget>(PlayerOwner, Data->AlertListWidgetClass);
		if (AlertListWidget)
		{
			AlertListWidget->AddToPlayerScreen(1);
		}
	}

	//Lasting alerts raised before this HUD existed (late join)
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IMP_AlertSource* Source = Cast<IMP_AlertSource>(*It))
		{
			Source->ShowCurrentAlerts(*this);
		}
	}
}

void AMP_HUD::ShowMainMenu()
{
	MainMenuWidget = CreateWidget<UMP_MainMenuWidget>(PlayerOwner, GetHUDData()->MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return;
	}

	MainMenuWidget->AddToPlayerScreen(10);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	PlayerOwner->SetInputMode(InputMode);
	PlayerOwner->SetShowMouseCursor(true);
}

void AMP_HUD::ForEachLocalHUD(const UObject* WorldContextObject, TFunctionRef<void(AMP_HUD&)> Function)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController())
		{
			continue;
		}
		if (AMP_HUD* HUD = Cast<AMP_HUD>(Controller->GetHUD()))
		{
			Function(*HUD);
		}
	}
}

void AMP_HUD::RaiseAlert(const UObject* WorldContextObject, const FMP_Alert& Alert)
{
	ForEachLocalHUD(WorldContextObject, [&Alert](AMP_HUD& HUD)
	{
		HUD.ShowAlert(Alert);
	});
}

void AMP_HUD::ClearAlert(const UObject* WorldContextObject, FName Key, AActor* Source)
{
	ForEachLocalHUD(WorldContextObject, [Key, Source](AMP_HUD& HUD)
	{
		HUD.HideAlert(Key, Source);
	});
}

void AMP_HUD::ShowAlert(const FMP_Alert& Alert)
{
	const UPDA_HUD* Data = GetHUDData();
	UPanelWidget* AlertBox = AlertListWidget ? AlertListWidget->GetAlertBox() : nullptr;
	if (!AlertBox || !Data->AlertWidgetClass)
	{
		return;
	}

	const float EndTime = Alert.Duration > 0.f ? GetWorld()->GetTimeSeconds() + Alert.Duration : 0.f;
	for (int32 Index = 0; Index < Alerts.Num(); ++Index)
	{
		if (Alerts[Index]->GetAlert().Matches(Alert.Key, Alert.Source))
		{
			Alerts[Index]->SetAlert(Alert, true);
			AlertEndTimes[Index] = EndTime;
			return;
		}
	}

	UMP_AlertWidget* Widget = CreateWidget<UMP_AlertWidget>(PlayerOwner, Data->AlertWidgetClass);
	if (!Widget)
	{
		return;
	}
	if (Data->bNewestAlertFirst)
	{
		AlertBox->InsertChildAt(0, Widget);
	}
	else
	{
		AlertBox->AddChild(Widget);
	}
	Widget->SetAlert(Alert, false);
	Alerts.Add(Widget);
	AlertEndTimes.Add(EndTime);

	while (Alerts.Num() > Data->MaxAlerts)
	{
		RemoveAlertAt(0);
	}
}

void AMP_HUD::HideAlert(FName Key, const AActor* Source)
{
	for (int32 Index = Alerts.Num() - 1; Index >= 0; --Index)
	{
		if (Alerts[Index]->GetAlert().Matches(Key, Source))
		{
			RemoveAlertAt(Index);
		}
	}
}

void AMP_HUD::RemoveAlertAt(int32 Index)
{
	if (UMP_AlertWidget* Widget = Alerts[Index])
	{
		Widget->Hide();
	}
	Alerts.RemoveAt(Index);
	AlertEndTimes.RemoveAt(Index);
}

void AMP_HUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Alerts.Num() - 1; Index >= 0; --Index)
	{
		if (!Alerts[Index] || (AlertEndTimes[Index] > 0.f && Now >= AlertEndTimes[Index]))
		{
			RemoveAlertAt(Index);
		}
	}
}
