#pragma once

#include "CoreMinimal.h"
#include "MP_Station.h"
#include "MP_LookoutStation.generated.h"

class UMeshComponent;
class UPDA_LookoutStation;
class USpotLightComponent;
class UStaticMeshComponent;

//The lookout seat in the head and the searchlight in front of it: seated, the view is at the lamp (zoomed), the mech turns its head toward it
//(AMP_Mech, the station hangs on the head's pivot) and the light tilts with it, showing the gunners what to shoot (enemies in it glow)
UCLASS()
class MECHAPROTO_API AMP_LookoutStation : public AMP_Station
{
	GENERATED_BODY()

public:
	AMP_LookoutStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual const UPDA_Station* GetBaseStationData() const override;
	//Where its data's View says (at the lamp, above the head, or around it), looking where the user aims
	virtual FVector GetCameraPivotLocation() const override;
	virtual float GetCameraDistance() const override;

	//Where the beam should point (world): the user's aim on their machine, the replicated one elsewhere
	FRotator GetAimRotation() const;

	UFUNCTION(BlueprintPure, Category = "Station")
	const UPDA_LookoutStation* GetLookoutData() const;

	//What the user's camera looks at (the first thing a projectile would hit, else the beam's end), local
	FVector ComputeAimPoint() const;

	//Points the beam at a point: at once for the user, replicated to the others by the server
	void AimAt(const FVector& AimPoint);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UPDA_LookoutStation> StationData;

	//The lamp from the station's origin, in front of the head (drag its diamond in the viewport)
	UPROPERTY(EditAnywhere, Category = "Station", meta = (MakeEditWidget = true))
	FVector LampOffset = FVector(0.f, 0.f, 300.f);

	//The head's top from the station's origin, for the HeadTop and Orbit views (default: the test mech's, its seat 1.8 m in front of the center)
	UPROPERTY(EditAnywhere, Category = "Station", meta = (MakeEditWidget = true))
	FVector HeadTopOffset = FVector(-180.f, 0.f, 680.f);

	//Turned toward the beam's aim, holds the light and the cone
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Lamp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LampMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> BeamLight;

	//Visible light cone, apex at the lamp
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	//The user's aim for the other machines
	UPROPERTY(Replicated)
	FRotator ReplicatedAim = FRotator::ZeroRotator;

	//Beam light, cone size and material from the data
	void ApplyBeam();
	//Overlay on the enemies in the beam, off on those that left it
	void UpdateMarks();
	void ClearMarks();

	//The light's current tilt (it points where the head faces)
	float BeamPitch = -25.f;
	FRotator UserAim = FRotator::ZeroRotator;
	float MarkTimer = 0.f;
	float ApplyTimer = 0.f;
	TArray<TWeakObjectPtr<UMeshComponent>> Marked;
};
