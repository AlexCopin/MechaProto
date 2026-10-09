#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Recovery.generated.h"

class USoundBase;

//Getting back what got lost: players outside the mech come back aboard, tool recall buttons. Read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Recovery : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----Players: outside the mech for a while, back at one of its player starts (keeping what they hold)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn")
	bool bAutoRespawnOutside = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", UIMax = "60", Units = "s"))
	float OutsideRespawnDelay = 8.f;

	//The mech's floor is looked for this far under the player: anything else first (the ground), or nothing, is outside
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "100", UIMax = "20000", Units = "cm"))
	float OnMechTraceDepth = 5000.f;

	//Shown to the player outside, {0} = seconds left
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn")
	FText OutsideText = FText::FromString(TEXT("Outside the mech - back aboard in {0} s"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn")
	FLinearColor OutsideTextColor = FLinearColor(1.f, 0.55f, 0.1f, 1.f);

	//Text size (1 = the engine's large font at 1080p), on a dark panel
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0.5", UIMax = "4"))
	float OutsideTextScale = 1.6f;

	//-----Tool recall buttons (AMP_ToolRecallButton): pressing one brings its tool back to where it was at the start
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall", meta = (ClampMin = "0", UIMax = "300", Units = "s"))
	float RecallCooldown = 20.f;

	//The tool leaves the hands of whoever holds it, otherwise a held tool can't be recalled
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	bool bRecallFromHands = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	TObjectPtr<USoundBase> RecallSound;

	//Prompts, {0} = the tool's name, {1} = seconds left
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	FText RecallText = FText::FromString(TEXT("Recall the {0}"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	FText RecallCooldownText = FText::FromString(TEXT("Recall the {0} (ready in {1} s)"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	FText RecallHeldText = FText::FromString(TEXT("The {0} is in someone's hands"));

	//Button top
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	FLinearColor ButtonReadyColor = FLinearColor(0.1f, 0.9f, 0.2f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recall")
	FLinearColor ButtonCooldownColor = FLinearColor(0.9f, 0.1f, 0.05f, 1.f);
};
