#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Mech.generated.h"

class UMaterialInterface;

//Mech walking and look, read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Mech : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----Walk (driven by the pilot station)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0", UIMax = "2000", Units = "CentimetersPerSecond"))
	float MaxForwardSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0", UIMax = "2000", Units = "CentimetersPerSecond"))
	float MaxBackwardSpeed = 120.f;

	//Speeding up and slowing down, gentle so the crew and loose items keep their footing
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "1", UIMax = "2000"))
	float Acceleration = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "0", UIMax = "90", Units = "DegreesPerSecond"))
	float MaxTurnRate = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ClampMin = "1", UIMax = "180"))
	float TurnAcceleration = 30.f;

	//-----Look (structure and armor are engine cubes)
	//Colors every instance from its custom data (0-2 color, 3 emissive strength): M_MechPaint. Not world aligned: it moves with the mech
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TObjectPtr<UMaterialInterface> Material;

	//The structure's frame: outside, and inside where no zone paints it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	FLinearColor Color = FLinearColor(0.22f, 0.23f, 0.25f);

	//Depth of the zone colors around each cavity (AMP_Mech Zones), thinner than the walls so the outside keeps Color. Rebuild the structure to apply
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "1", UIMax = "40", Units = "cm"))
	float ZoneLiningThickness = 10.f;

	//Panoramic glass panes (GlassPanes), M_MechGlass
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	//-----Carry
	//Loose physics bodies inside the mech (items, ragdolls, corpses) move with it; moving the structure alone slides it under them
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry")
	bool bCarryPhysicsBodies = true;

	//-----Network
	//State updates per second, clients simulate the same input in between
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "1", UIMax = "60"))
	float NetUpdateFrequency = 20.f;

	//How fast a client's simulation is pulled to the server's state
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "0.1", UIMax = "20"))
	float NetCorrectionSpeed = 4.f;

	//-----Debug
	//Speed and turn rate on every screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDebug = true;
};
