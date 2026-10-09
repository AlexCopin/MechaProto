#include "MP_HUD.h"
#include "C_PlayerRecovery.h"
#include "C_Ragdoll.h"
#include "C_StationUser.h"
#include "MP_AlertListWidget.h"
#include "MP_AlertSource.h"
#include "MP_AlertWidget.h"
#include "MP_Breakable.h"
#include "MP_MainMenuWidget.h"
#include "MP_PilotStation.h"
#include "MP_Ping.h"
#include "MP_PlayerStatsWidget.h"
#include "MP_WeaponStation.h"
#include "PDA_Breakable.h"
#include "PDA_HUD.h"
#include "PDA_Recovery.h"
#include "PDA_WeaponStation.h"
#include "Components/PanelWidget.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr int32 CooldownRingSegments = 48;
	//Crosshair sizes are given for this screen height
	constexpr float CrosshairReferenceHeight = 1080.f;
}

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

void AMP_HUD::DrawHUD()
{
	Super::DrawHUD();

	if (!MainMenuWidget && PlayerOwner && PlayerOwner->IsLocalController() && Canvas)
	{
		AMP_Ping::DrawPings(*this, *Canvas);
		DrawCrosshair();
		DrawOutsideWarning();
	}
}

void AMP_HUD::DrawOutsideWarning()
{
	const APawn* Pawn = PlayerOwner->GetPawn();
	const UC_PlayerRecovery* Recovery = Pawn ? Pawn->FindComponentByClass<UC_PlayerRecovery>() : nullptr;
	const float TimeLeft = Recovery ? Recovery->GetAutoRespawnTimeLeft() : -1.f;
	if (TimeLeft < 0.f)
	{
		return;
	}

	const UPDA_Recovery* Data = Recovery->GetRecoveryData();
	const FString Text = FText::Format(Data->OutsideText, FText::AsNumber(FMath::CeilToInt(TimeLeft))).ToString();
	UFont* Font = GEngine->GetLargeFont();
	const float Scale = Canvas->ClipY / CrosshairReferenceHeight * Data->OutsideTextScale;
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);
	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Y = Canvas->ClipY * 0.3f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 14.f * Scale, Y - 6.f * Scale, Width + 28.f * Scale, Height + 12.f * Scale);
	DrawText(Text, Data->OutsideTextColor, X, Y, Font, Scale);
}

void AMP_HUD::DrawCrosshair()
{
	const UPDA_HUD* Data = GetHUDData();
	const APawn* Pawn = PlayerOwner->GetPawn();
	if (!Data->bShowCrosshair || !Pawn || !Canvas)
	{
		return;
	}
	//Ragdolled: the camera follows the body, nothing to aim
	const UC_Ragdoll* Ragdoll = Pawn->FindComponentByClass<UC_Ragdoll>();
	if (Ragdoll && Ragdoll->IsRagdolled())
	{
		return;
	}

	const UC_StationUser* StationUser = Pawn->FindComponentByClass<UC_StationUser>();
	const AMP_Station* Station = StationUser ? StationUser->GetStation() : nullptr;
	const float Scale = Canvas->ClipY / CrosshairReferenceHeight;
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	//A broken system stops the station: its name under the crosshair
	const AMP_Breakable* Broken = Station ? Station->GetBrokenSystem() : nullptr;
	if (Broken)
	{
		const FString Text = FText::Format(Data->DisabledText, Broken->GetBreakableData()->DisplayName).ToString();
		UFont* Font = GEngine->GetMediumFont();
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Text, Width, Height, Font, Scale);
		DrawText(Text, Data->DisabledColor, Center.X - Width * 0.5f, Center.Y + (Data->CooldownRingRadius + 12.f) * Scale, Font, Scale);
	}
	if (Station && Station->IsA<AMP_PilotStation>() && !Data->bShowCrosshairForPilot)
	{
		return;
	}

	//Four lines around a dot, each over a slightly bigger dark one
	const float Gap = Data->CrosshairGap * Scale;
	const float Length = Data->CrosshairSize * Scale;
	const float HalfThickness = FMath::Max(1.f, Data->CrosshairThickness * Scale) * 0.5f;
	TArray<FBox2D, TInlineAllocator<5>> Bars;
	if (Length > 0.f)
	{
		Bars.Add(FBox2D(FVector2D(Center.X + Gap, Center.Y - HalfThickness), FVector2D(Center.X + Gap + Length, Center.Y + HalfThickness)));
		Bars.Add(FBox2D(FVector2D(Center.X - Gap - Length, Center.Y - HalfThickness), FVector2D(Center.X - Gap, Center.Y + HalfThickness)));
		Bars.Add(FBox2D(FVector2D(Center.X - HalfThickness, Center.Y + Gap), FVector2D(Center.X + HalfThickness, Center.Y + Gap + Length)));
		Bars.Add(FBox2D(FVector2D(Center.X - HalfThickness, Center.Y - Gap - Length), FVector2D(Center.X + HalfThickness, Center.Y - Gap)));
	}
	if (Data->bCrosshairDot)
	{
		Bars.Add(FBox2D(Center - FVector2D(HalfThickness), Center + FVector2D(HalfThickness)));
	}
	for (const FBox2D& Bar : Bars)
	{
		DrawRect(Data->CrosshairOutlineColor, Bar.Min.X - 1.f, Bar.Min.Y - 1.f, Bar.GetSize().X + 2.f, Bar.GetSize().Y + 2.f);
	}
	const FLinearColor Color = Broken ? Data->DisabledColor : Data->CrosshairColor;
	for (const FBox2D& Bar : Bars)
	{
		DrawRect(Color, Bar.Min.X, Bar.Min.Y, Bar.GetSize().X, Bar.GetSize().Y);
	}

	//Reloading: the ring fills up until the next shot
	const AMP_WeaponStation* Weapon = Cast<AMP_WeaponStation>(Station);
	if (Weapon && !Broken && Weapon->GetStationData()->FireCooldown >= Data->CooldownMinToShow)
	{
		const float Ready = StationUser->GetFireReadyPercent();
		if (Ready < 1.f)
		{
			DrawCooldownRing(Center, Scale, Ready);
		}
	}
}

void AMP_HUD::DrawCooldownRing(const FVector2D& Center, float Scale, float ReadyPercent)
{
	//From the top, clockwise
	const UPDA_HUD* Data = GetHUDData();
	const float Radius = Data->CooldownRingRadius * Scale;
	const float Thickness = FMath::Max(1.f, Data->CooldownRingThickness * Scale);
	const int32 Filled = FMath::RoundToInt(ReadyPercent * CooldownRingSegments);
	for (int32 Index = 0; Index < CooldownRingSegments; ++Index)
	{
		const float StartAngle = UE_TWO_PI * Index / CooldownRingSegments;
		const float EndAngle = UE_TWO_PI * (Index + 1) / CooldownRingSegments;
		const FVector2D Start = Center + FVector2D(FMath::Sin(StartAngle), -FMath::Cos(StartAngle)) * Radius;
		const FVector2D End = Center + FVector2D(FMath::Sin(EndAngle), -FMath::Cos(EndAngle)) * Radius;
		DrawLine(Start.X, Start.Y, End.X, End.Y, Index < Filled ? Data->CooldownColor : Data->CooldownBackColor, Thickness);
	}
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
