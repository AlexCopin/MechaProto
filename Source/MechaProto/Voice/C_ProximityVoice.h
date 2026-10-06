#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Online/CoreOnline.h"
#include "C_ProximityVoice.generated.h"

class APlayerState;
class UPDA_Voice;
class UVOIPTalker;

//Where the owning player's voice is heard from, spatialized and attenuated by distance (attach it to the head)
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_ProximityVoice : public USceneComponent
{
	GENERATED_BODY()

public:
	UC_ProximityVoice();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Voice")
	const UPDA_Voice* GetVoiceData() const;

	//0 when silent, only for remote players
	UFUNCTION(BlueprintPure, Category = "Voice")
	float GetVoiceLevel() const;

	UFUNCTION(BlueprintPure, Category = "Voice")
	bool IsTalking() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voice")
	TObjectPtr<UPDA_Voice> VoiceData;

	UPROPERTY(Transient)
	TObjectPtr<UVOIPTalker> Talker;

	//PlayerState and its unique id replicate late, so registration is retried
	void RefreshTalker();
	void UnregisterTalker();

	TWeakObjectPtr<APlayerState> RegisteredPlayerState;
	FUniqueNetIdWrapper RegisteredId;
	FTimerHandle RefreshTimer;
};
