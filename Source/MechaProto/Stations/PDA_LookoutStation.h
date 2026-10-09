#pragma once

#include "CoreMinimal.h"
#include "PDA_Station.h"
#include "PDA_LookoutStation.generated.h"

class UMaterialInterface;

//Where the lookout's camera is
UENUM(BlueprintType)
enum class EMP_LookoutView : uint8
{
	//At the lamp in front of the head: you are the head
	Head,
	//Above the head's top, seeing the floor around the mech
	HeadTop,
	//Third person behind and above the head, seeing it turn with its beam
	Orbit
};

//The lookout's searchlight in front of the mech's head: the head turns toward its user's aim (DA_Mech > Head), the light tilts up and down;
//enemies in the beam glow for the gunners. Read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_LookoutStation : public UPDA_Station
{
	GENERATED_BODY()

public:
	//-----View (instead of the base CameraOffset / CameraDistance; CameraFieldOfView still applies). Switch it during PIE to compare
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View")
	EMP_LookoutView View = EMP_LookoutView::Head;

	//Head: from the lamp, in the head's yaw (X forward, Z up)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View", meta = (EditCondition = "View == EMP_LookoutView::Head"))
	FVector HeadViewOffset = FVector(60.f, 0.f, 40.f);

	//HeadTop and Orbit: above the head's top (AMP_LookoutStation HeadTopOffset), in the head's yaw
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View", meta = (EditCondition = "View != EMP_LookoutView::Head"))
	FVector HeadTopViewOffset = FVector(200.f, 0.f, 500.f);

	//The user doesn't see the light's cone (it would sit in front of what they look at), only where it lands; the others see it
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View")
	bool bHideBeamConeForUser = true;

	//Orbit: how far behind that point, along the view
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "View", meta = (EditCondition = "View == EMP_LookoutView::Orbit", ClampMin = "0", UIMax = "10000", Units = "cm"))
	float OrbitDistance = 2500.f;

	//-----Beam
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "100", UIMax = "50000", Units = "cm"))
	float BeamLength = 25000.f;

	//Half angle of the light cone
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0.5", ClampMax = "45", Units = "Degrees"))
	float BeamAngle = 5.f;

	//Candelas at the beam's center (lux at 100 m = this / 10000)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0", UIMax = "20000000"))
	float LightIntensity = 3000000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FLinearColor LightColor = FLinearColor(1.f, 0.92f, 0.75f);

	//Visible cone, additive (M_LookoutBeam: Color, Intensity), brighter at the lamp
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam")
	FLinearColor BeamColor = FLinearColor(1.f, 0.85f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0", UIMax = "5"))
	float BeamBrightness = 1.5f;

	//How fast the light tilts toward its user's aim (other machines) or back to IdlePitch
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "1", UIMax = "720", Units = "DegreesPerSecond"))
	float BeamTurnSpeed = 90.f;

	//Tilt limits of the light (the head only turns around)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "-89", ClampMax = "0", Units = "Degrees"))
	float MinPitch = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "0", ClampMax = "89", Units = "Degrees"))
	float MaxPitch = 30.f;

	//Nobody on the lookout: the light rests at this tilt, the head faces forward
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Beam", meta = (ClampMin = "-89", ClampMax = "89", Units = "Degrees"))
	float IdlePitch = -25.f;

	//-----Marking: what the beam shows the gunners
	//Overlay on enemies in the beam (MI_LookoutMark), on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marking")
	TObjectPtr<UMaterialInterface> MarkOverlayMaterial;

	//Cone that marks, a bit wider than the light so a target at its edge counts
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marking", meta = (ClampMin = "0", ClampMax = "45", Units = "Degrees"))
	float MarkAngle = 8.f;

	//Only while someone is on the lookout (off: the resting light marks too)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marking")
	bool bMarkOnlyWhenManned = true;
};
