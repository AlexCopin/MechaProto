#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Voice.generated.h"

class USoundAttenuation;
class USoundEffectSourcePresetChain;

//Proximity voice chat tuning, read live (attenuation applies the next time a player starts talking)
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Voice : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//Hold the talk key to speak, otherwise the mic is always open
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	bool bPushToTalk = false;

	//How far voices carry, falloff, occlusion through walls
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundAttenuation> VoiceAttenuation;

	//Optional effects on every voice (radio, mech helmet...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<USoundEffectSourcePresetChain> VoiceEffectChain;

	//Mic level under which the mic counts as silent and sends nothing (engine default 0.08), lower it for quiet mics
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice", meta = (ClampMin = "0", UIMax = "0.3"))
	float MicSilenceThreshold = 0.08f;

	//Voice level above which a player counts as talking (UI indicator)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice", meta = (ClampMin = "0", UIMax = "1"))
	float TalkingLevelThreshold = 0.02f;

	//-----Debug
	//Prints the local mic state on screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowMicDebug = false;

	//Plays your own voice back to you (not spatialized), to check the mic alone
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bHearMyself = false;
};
