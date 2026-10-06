#include "C_ProximityVoice.h"
#include "PDA_Voice.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/VoiceConfig.h"
#include "TimerManager.h"

UC_ProximityVoice::UC_ProximityVoice()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UC_ProximityVoice::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UC_ProximityVoice::RefreshTalker, 0.5f, true, 0.f);
	}
}

void UC_ProximityVoice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	UnregisterTalker();

	Super::EndPlay(EndPlayReason);
}

const UPDA_Voice* UC_ProximityVoice::GetVoiceData() const
{
	if (ensureMsgf(VoiceData, TEXT("%s has no VoiceData, using code defaults"), *GetPathNameSafe(this)))
	{
		return VoiceData;
	}
	return GetDefault<UPDA_Voice>();
}

float UC_ProximityVoice::GetVoiceLevel() const
{
	return Talker ? Talker->GetVoiceLevel() : 0.f;
}

bool UC_ProximityVoice::IsTalking() const
{
	return GetVoiceLevel() > GetVoiceData()->TalkingLevelThreshold;
}

void UC_ProximityVoice::RefreshTalker()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	//Own voice is never played back
	if (!PlayerState || Pawn->IsLocallyControlled() || !PlayerState->GetUniqueId().IsValid())
	{
		return;
	}

	if (!Talker)
	{
		Talker = NewObject<UVOIPTalker>(this);
	}

	//Settings are read each time the player starts talking, so data edits apply live
	const UPDA_Voice* Data = GetVoiceData();
	Talker->Settings.ComponentToAttachTo = this;
	Talker->Settings.AttenuationSettings = Data->VoiceAttenuation;
	Talker->Settings.SourceEffectChain = Data->VoiceEffectChain;

	if (RegisteredPlayerState != PlayerState)
	{
		UnregisterTalker();
		Talker->RegisterWithPlayerState(PlayerState);
		RegisteredPlayerState = PlayerState;
		RegisteredId = PlayerState->GetUniqueId();
	}
}

void UC_ProximityVoice::UnregisterTalker()
{
	//A new pawn of the same player may already own the slot
	if (Talker && RegisteredId.IsValid() && UVOIPStatics::GetVOIPTalkerForPlayer(RegisteredId) == Talker)
	{
		UVOIPStatics::ResetPlayerVoiceTalker(RegisteredId);
	}
	RegisteredPlayerState.Reset();
	RegisteredId = FUniqueNetIdWrapper();
}
