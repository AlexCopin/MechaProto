#pragma once

#include "CoreMinimal.h"
#include "MP_Station.h"
#include "MP_WeaponStation.generated.h"

class UPDA_WeaponStation;

//Weapon station a player mans with interact, fires projectiles at the aim (for enemies, never hits players)
//Open turret: seated behind the gun, turning with it. Arm gun (bSeatOnTurret off): seated at the station, the turret is out of the hull at TurretOffset (the mech's arms shoot)
UCLASS()
class MECHAPROTO_API AMP_WeaponStation : public AMP_Station
{
	GENERATED_BODY()

public:
	AMP_WeaponStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual const UPDA_Station* GetBaseStationData() const override;
	//Centered on the gun: the camera turns around it and follows it (UPDA_Station::CameraOffset from the gun, in its yaw, Z straight up)
	virtual FVector GetCameraPivotLocation() const override;

	//Where the gun points (world): the user's aim, smoothed from the replicated one on the other machines
	FRotator GetAimRotation() const { return TurretAim; }

	UFUNCTION(BlueprintPure, Category = "Station")
	const UPDA_WeaponStation* GetStationData() const;

	//Where the user's camera looks: what a projectile would hit (the mech and players are ignored), local
	FVector ComputeAimPoint() const;

	//Turns the turret toward a point: local for the user, replicated to the others by the server
	void AimAt(const FVector& AimPoint);

	bool IsFireReady(float LastFireTime) const;

	//Server: spawns a projectile from the muzzle toward the aim point
	void ServerFire(const FVector& AimPoint);

	//Plays the fire sound: locally by the user when firing, from FireCounter for the others
	void PlayFireEffects();

	//BP hooks for effects, every machine
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnFired();

	//Called by its projectiles when they explode
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnExploded(FVector Center, float Radius);

protected:
	//Yaw part of the turret
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretYaw;

	//Pitch part, carries the gun, the muzzle and the seat (always behind the gun, tilting with it)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretPitch;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	//Projectiles spawn here, along its forward
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UPDA_WeaponStation> StationData;

	//Turret (gun, muzzle) from the station's origin: out of the hull for an arm gun
	UPROPERTY(EditAnywhere, Category = "Station", meta = (MakeEditWidget = true))
	FVector TurretOffset = FVector(0.f, 0.f, 140.f);

	//The user sits behind the gun and turns with it. Off: seated at the station (a console in front), aiming the remote turret
	UPROPERTY(EditAnywhere, Category = "Station")
	bool bSeatOnTurret = true;

	//Turret, seat and base placement from TurretOffset / bSeatOnTurret
	void ApplyMount();

	//Turret aim for the other players
	UPROPERTY(Replicated)
	FRotator ReplicatedAim = FRotator::ZeroRotator;

	//Shots fired, the other players play the fire effects when it changes (several shots between two updates play once)
	UPROPERTY(ReplicatedUsing = OnRep_FireCounter)
	uint8 FireCounter = 0;

	UFUNCTION()
	void OnRep_FireCounter();

	void ApplyTurretRotation(const FRotator& WorldAim);

	FRotator TurretAim = FRotator::ZeroRotator;
	float LastServerFireTime = -100.f;
};
