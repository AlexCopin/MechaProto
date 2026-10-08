#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Breakable.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UPDA_Item;
class USoundBase;

//One kind of mech system that breaks down (engine, pipe, rotor...), read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Breakable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//Shown in the prompt and the alerts
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable")
	FText DisplayName = FText::FromString(TEXT("System"));

	//-----Repair
	//The item data of the tool that repairs it: the player must hold an item with this data
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair")
	TObjectPtr<UPDA_Item> RequiredTool;

	//Tool hits to repair it (use or interact while looking at it, the tool's UseCooldown between hits)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair", meta = (ClampMin = "1", UIMax = "30"))
	int32 RepairHits = 5;

	//-----Breakdowns (AMP_BreakdownManager)
	//Chance to be the next one to break, relative to the other intact systems
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns", meta = (ClampMin = "0", UIMax = "10"))
	float BreakWeight = 1.f;

	//-----Visuals
	//Needs a vector parameter named ColorParameter, BasicShapeMaterial (has "Color") when empty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FName ColorParameter = FName("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor WorkingColor = FLinearColor(0.25f, 0.3f, 0.35f);

	//Blinks between the two while broken
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor BrokenColor = FLinearColor(1.f, 0.15f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0", UIMax = "5", Units = "Hertz"))
	float BrokenBlinkFrequency = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor RepairHitColor = FLinearColor::White;

	//The mesh turns around its up axis while working (rotor), stops when broken
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (UIMin = "-1080", UIMax = "1080", Units = "DegreesPerSecond"))
	float SpinSpeed = 0.f;

	//-----Effects
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<USoundBase> BreakSound;

	//Attached while broken (smoke, sparks, leak...)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> BrokenEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<USoundBase> RepairHitSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<USoundBase> RepairedSound;

	//-----Alerts on every player's HUD while broken: {0} = DisplayName, {1} = the tool's ItemName, {2} = the instance's LocationName
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alerts")
	FText BrokenAlertTitle = FText::FromString(TEXT("{0} BROKEN"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alerts")
	FText BrokenAlertMessage = FText::FromString(TEXT("{2} - bring the {1}"));

	//-----Debug
	//State and repair progress above it on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawDebugState = true;
};
