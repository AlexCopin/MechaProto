// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Online/CoreOnline.h"
#include "MechaProtoPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UPDA_Voice;
class UC_Pinger;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class MECHAPROTO_API AMechaProtoPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	AMechaProtoPlayerController();

	//Console, tests: a ping where the view aims
	UFUNCTION(Exec)
	void DebugPing();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	//-----Ping (middle mouse button): a marker the whole crew sees where this player aims
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UC_Pinger> Pinger;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> PingAction;

	void OnPingStarted();

	//-----Voice
	UPROPERTY(EditAnywhere, Category="Voice")
	TObjectPtr<UPDA_Voice> VoiceData;

	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> PushToTalkAction;

	//Sent by the server on login, the voice mode then comes from VoiceData
	virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;

	const UPDA_Voice* GetVoiceData() const;
	void OnPushToTalkStarted();
	void OnPushToTalkCompleted();
	void ApplyVoiceMode();
	//Follows bPushToTalk edits during PIE, debug display
	void RefreshVoiceMode();
	void ShowMicDebug();
	//IsLocalPlayerTalking is broken for user 0 in the engine, the talking event is used instead
	void OnTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool bPushToTalkHeld = false;
	bool bAppliedPushToTalk = false;
	bool bLocalTalking = false;
	float AppliedMicThreshold = -1.f;
	FTimerHandle VoiceModeTimer;
	FDelegateHandle TalkingStateHandle;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
