// Copyright Epic Games, Inc. All Rights Reserved.


#include "MechaProtoPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "MechaProtoCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "MP_NetworkSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Net/VoiceConfig.h"
#include "PDA_Voice.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"
#include "MechaProto.h"
#include "Widgets/Input/SVirtualJoystick.h"

AMechaProtoPlayerController::AMechaProtoPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AMechaProtoCameraManager::StaticClass();
}

void AMechaProtoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogMechaProto, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AMechaProtoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
		if (EnhancedInput && PushToTalkAction)
		{
			EnhancedInput->BindAction(PushToTalkAction, ETriggerEvent::Started, this, &AMechaProtoPlayerController::OnPushToTalkStarted);
			EnhancedInput->BindAction(PushToTalkAction, ETriggerEvent::Completed, this, &AMechaProtoPlayerController::OnPushToTalkCompleted);
		}
	}
	
}

bool AMechaProtoPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AMechaProtoPlayerController::ClientEnableNetworkVoice_Implementation(bool bEnable)
{
	//Voice is only captured, sent and played while an online session exists, and the mic is only registered when one is created
	if (UMP_NetworkSubsystem* Network = GetGameInstance()->GetSubsystem<UMP_NetworkSubsystem>())
	{
		Network->EnsureVoiceSession();
	}

	const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OnlineSubsystem ? OnlineSubsystem->GetVoiceInterface() : nullptr;
	if (Voice.IsValid() && GetLocalPlayer())
	{
		Voice->RegisterLocalTalker(GetLocalPlayer()->GetControllerId());
		if (!TalkingStateHandle.IsValid())
		{
			TalkingStateHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &AMechaProtoPlayerController::OnTalkingStateChanged));
		}
	}

	ApplyVoiceMode();
	GetWorldTimerManager().SetTimer(VoiceModeTimer, this, &AMechaProtoPlayerController::RefreshVoiceMode, 0.5f, true);
}

const UPDA_Voice* AMechaProtoPlayerController::GetVoiceData() const
{
	if (ensureMsgf(VoiceData, TEXT("%s has no VoiceData, using code defaults"), *GetPathNameSafe(this)))
	{
		return VoiceData;
	}
	return GetDefault<UPDA_Voice>();
}

void AMechaProtoPlayerController::OnPushToTalkStarted()
{
	bPushToTalkHeld = true;
	if (bAppliedPushToTalk)
	{
		StartTalking();
	}
}

void AMechaProtoPlayerController::OnPushToTalkCompleted()
{
	bPushToTalkHeld = false;
	if (bAppliedPushToTalk)
	{
		StopTalking();
	}
}

void AMechaProtoPlayerController::ApplyVoiceMode()
{
	bAppliedPushToTalk = GetVoiceData()->bPushToTalk;
	ToggleSpeaking(!bAppliedPushToTalk || bPushToTalkHeld);
}

void AMechaProtoPlayerController::RefreshVoiceMode()
{
	const UPDA_Voice* Data = GetVoiceData();
	if (Data->bPushToTalk != bAppliedPushToTalk)
	{
		ApplyVoiceMode();
	}

	static IConsoleVariable* LoopbackVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("OSS.VoiceLoopback"));
	if (LoopbackVariable && LoopbackVariable->GetBool() != Data->bHearMyself)
	{
		LoopbackVariable->Set(Data->bHearMyself);
	}

	if (Data->MicSilenceThreshold != AppliedMicThreshold)
	{
		AppliedMicThreshold = Data->MicSilenceThreshold;
		UVOIPStatics::SetMicThreshold(AppliedMicThreshold);
	}

	if (Data->bShowMicDebug)
	{
		ShowMicDebug();
	}
}

void AMechaProtoPlayerController::ShowMicDebug()
{
	const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineVoicePtr Voice = OnlineSubsystem ? OnlineSubsystem->GetVoiceInterface() : nullptr;
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!GEngine || !LocalPlayer)
	{
		return;
	}

	FString Text;
	FColor Color = FColor::Red;
	const IOnlineSessionPtr Sessions = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!Voice.IsValid())
	{
		Text = TEXT("no voice interface (check [OnlineSubsystem] / [Voice] in DefaultEngine.ini)");
	}
	else if (!Sessions.IsValid() || Sessions->GetNumSessions() == 0)
	{
		Text = TEXT("no online session, voice is not processed");
	}
	else if (!Voice->IsHeadsetPresent(LocalPlayer->GetControllerId()))
	{
		Text = TEXT("mic not registered");
	}
	else if (bAppliedPushToTalk && !bPushToTalkHeld)
	{
		Text = TEXT("muted (hold push to talk)");
		Color = FColor::Yellow;
	}
	else
	{
		Text = bLocalTalking ? TEXT("TALKING") : TEXT("open, silent");
		Color = bLocalTalking ? FColor::Green : FColor::White;
	}

	//One line per local player, refreshed by the timer
	GEngine->AddOnScreenDebugMessage(int32(GetUniqueID()), 0.6f, Color, FString::Printf(TEXT("Mic %s: %s"), *GetNameSafe(PlayerState), *Text));
}

void AMechaProtoPlayerController::OnTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking)
{
	//Local talkers are reported with the identity id of their controller
	const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const IOnlineIdentityPtr Identity = OnlineSubsystem ? OnlineSubsystem->GetIdentityInterface() : nullptr;
	const FUniqueNetIdPtr LocalId = Identity.IsValid() && GetLocalPlayer() ? Identity->GetUniquePlayerId(GetLocalPlayer()->GetControllerId()) : nullptr;
	if (LocalId.IsValid() && *LocalId == *TalkerId)
	{
		bLocalTalking = bIsTalking;
	}
}

void AMechaProtoPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TalkingStateHandle.IsValid())
	{
		const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
		if (const IOnlineVoicePtr Voice = OnlineSubsystem ? OnlineSubsystem->GetVoiceInterface() : nullptr)
		{
			Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingStateHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}
