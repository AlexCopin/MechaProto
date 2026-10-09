#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Ping.generated.h"

class USoundBase;

//What a ping marks, from what it hit
UENUM(BlueprintType)
enum class EMP_PingType : uint8
{
	Location,
	Enemy,
	//A mech system (engine, pipe, rotor)
	System,
	//A tool or any held item
	Item,
	//A damaged or broken porthole
	Hull
};

//Marker look of one ping type, {0} in the label = the target's name
USTRUCT(BlueprintType)
struct FMP_PingStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping")
	FLinearColor Color = FLinearColor::White;
};

//Crew pings (middle mouse button): a marker every player sees through walls where one of them aims. Read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Ping : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPDA_Ping();

	//-----Ping
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping", meta = (ClampMin = "0.5", UIMax = "30", Units = "s"))
	float Lifetime = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping", meta = (ClampMin = "0", UIMax = "3", Units = "s"))
	float Cooldown = 0.35f;

	//The oldest goes beyond this
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping", meta = (ClampMin = "1", UIMax = "10"))
	int32 MaxLivePingsPerPlayer = 2;

	//A new ping on the same target, or this close to one of your spot pings, replaces it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping", meta = (ClampMin = "0", UIMax = "1000", Units = "cm"))
	float ReplaceRadius = 150.f;

	//An enemy ping stays this long once its enemy is dead
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float TargetLostLinger = 1.f;

	//-----Aim (from the screen's center: first person through the windows, station cameras through the mech)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "1000", UIMax = "100000", Units = "cm"))
	float MaxDistance = 30000.f;

	//A living enemy this close to the aim's line is pinged (seen from the view)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0", UIMax = "20", Units = "Degrees"))
	float EnemyAssistAngle = 3.f;

	//Same for items and broken systems within SmallAssistRange (inside views)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0", UIMax = "30", Units = "Degrees"))
	float SmallAssistAngle = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim", meta = (ClampMin = "0", UIMax = "5000", Units = "cm"))
	float SmallAssistRange = 1500.f;

	//A porthole is a Hull ping only when damaged or broken; intact, it is a window seen through
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	bool bHullOnlyWhenDamaged = true;

	//-----Look (sizes in pixels at 1080p, scaled with the screen)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TMap<EMP_PingType, FMP_PingStyle> Styles;

	//A System ping on a broken system, {0} = its name
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	FText BrokenSystemLabel = FText::FromString(TEXT("{0} BROKEN"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "2", UIMax = "60"))
	float MarkerSize = 14.f;

	//Label and distance text (1 = the engine's medium / small fonts at 1080p)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.5", UIMax = "3"))
	float TextScale = 1.3f;

	//At the screen's edge, pointing toward a ping out of view
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "2", UIMax = "60"))
	float ArrowSize = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", UIMax = "300"))
	float EdgeMargin = 48.f;

	//Above the top of a pinged enemy, item, system or plate
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", UIMax = "500", Units = "cm"))
	float MarkerHeight = 40.f;

	//Starts bigger and shrinks to its size
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float PopTime = 0.2f;

	//Fades out over the end of its life
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float FadeTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	bool bShowDistance = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	bool bShowPingerName = true;

	//-----Sound (heard by the whole crew when a ping appears)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> PingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0", UIMax = "2"))
	float PingVolume = 0.4f;

	const FMP_PingStyle& GetStyle(EMP_PingType Type) const;
};
