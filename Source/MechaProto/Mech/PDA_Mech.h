#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MP_MechStepShake.h"
#include "PDA_Mech.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class USoundBase;

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

	//-----Gait: straight legs swinging from the hip, the body rocking onto the standing foot. The cadence stays the same at any speed,
	//the stride follows the speed so the standing foot stays planted
	//Off, the mech glides with straight legs
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait")
	bool bAnimateWalk = true;

	//How far each leg swings forward and back at full speed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", UIMax = "30", Units = "Degrees"))
	float SwingAngle = 10.f;

	//Side rock onto the standing foot's outer edge, lifting the swinging foot off the ground
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", UIMax = "10", Units = "Degrees"))
	float WaddleRoll = 2.5f;

	//Turning in place steps too: the hips' speed at this distance from the center counts as walking
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", UIMax = "3000", Units = "cm"))
	float TurnStepRadius = 750.f;

	//How fast the stride follows a speed change
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.1", UIMax = "10"))
	float GaitBlendSpeed = 2.f;

	//-----Arms: each one swings from its shoulder toward where its gunner aims (the gun on the fist aims exactly), back to rest without one
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms")
	bool bAimArms = true;

	//Forward and back from their rest, straight out to the side
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", ClampMax = "90", Units = "Degrees"))
	float ArmMaxYaw = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", ClampMax = "80", Units = "Degrees"))
	float ArmMaxPitchUp = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", ClampMax = "80", Units = "Degrees"))
	float ArmMaxPitchDown = 30.f;

	//Heavy and slow (players walk inside): top speed, reached and left at ArmAcceleration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "1", UIMax = "180", Units = "DegreesPerSecond"))
	float ArmTurnSpeed = 10.f;

	//Deg/s per second: the arm speeds up and brakes smoothly instead of starting and stopping at once
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "1", UIMax = "180"))
	float ArmAcceleration = 8.f;

	//A still arm waits until its gunner aims farther than this from it (the gun on the fist still aims exactly), so small aim changes don't rock its corridor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms", meta = (ClampMin = "0", UIMax = "45", Units = "Degrees"))
	float ArmDeadZone = 10.f;

	//-----Head: turns on the neck toward where the lookout aims (its searchlight in front), back to facing forward without one
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Head")
	bool bTurnHead = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Head", meta = (ClampMin = "1", UIMax = "360", Units = "DegreesPerSecond"))
	float HeadTurnSpeed = 45.f;

	//-----Footsteps: each landing foot shakes the local cameras (stronger near that foot) and plays FootstepSound, on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TSubclassOf<UCameraShakeBase> FootstepShake = UMP_MechStepShake::StaticClass();

	//At full stride within FootstepInnerRadius of the foot (a smaller stride shakes less)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", UIMax = "5"))
	float FootstepShakeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", UIMax = "10000", Units = "cm"))
	float FootstepInnerRadius = 1500.f;

	//From FootstepInnerRadius to here the shake fades to FootstepFarScale, which stays beyond (the head still feels it)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", UIMax = "20000", Units = "cm"))
	float FootstepOuterRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", ClampMax = "1"))
	float FootstepFarScale = 0.35f;

	//Below this stride (0-1 of the full one) a landing foot is silent
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStepWeight = 0.15f;

	//Thud at the landing foot (none yet)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TObjectPtr<USoundBase> FootstepSound;

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
	//Speed, turn rate and stride on every screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDebug = true;
};
