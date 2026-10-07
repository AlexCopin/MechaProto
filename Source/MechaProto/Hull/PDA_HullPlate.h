#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_HullPlate.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

//One hull plate type, read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_HullPlate : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//A swarm enemy crashing on it deals its HullDamage (10)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1", UIMax = "2000"))
	float MaxHealth = 100.f;

	//-----Visuals (the mesh box is stretched to the opening)
	//Engine cube when empty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UStaticMesh> Mesh;

	//Needs a vector parameter named ColorParameter, BasicShapeMaterial (has "Color") when empty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FName ColorParameter = FName("Color");

	//Full health, fades to DamagedColor as it loses health
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor IntactColor = FLinearColor(0.2f, 0.25f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor DamagedColor = FLinearColor(0.6f, 0.1f, 0.02f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor HitFlashColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float HitFlashDuration = 0.12f;

	//-----Break (the plate disappears and leaves a hole)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break")
	TObjectPtr<USoundBase> BreakSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break")
	TObjectPtr<UNiagaraSystem> BreakEffect;

	//Draws a box when it breaks until there is a break effect
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break")
	bool bDrawBreakDebug = true;

	//-----Debug
	//Health above the plate once damaged, "HOLE" when broken, on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawDebugHealth = true;
};
