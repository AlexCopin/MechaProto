#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Interactable.h"
#include "MP_WeaponStation.generated.h"

class ACharacter;
class UCameraComponent;
class UPDA_WeaponStation;
class USpringArmComponent;
class UStaticMeshComponent;

//Weapon station a player mans with interact: third person camera around it, fires projectiles at the aim
UCLASS()
class MECHAPROTO_API AMP_WeaponStation : public AActor, public IMP_Interactable
{
	GENERATED_BODY()

public:
	AMP_WeaponStation();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	//-----IMP_Interactable
	virtual bool CanInteract(const ACharacter* User) const override;
	virtual void Interact(ACharacter* User) override;
	virtual FText GetInteractionText(const ACharacter* User) const override;

	UFUNCTION(BlueprintPure, Category = "Station")
	const UPDA_WeaponStation* GetStationData() const;

	UFUNCTION(BlueprintPure, Category = "Station")
	ACharacter* GetUser() const { return User; }

	//Server, called by UC_StationUser
	void SetUser(ACharacter* NewUser);

	//Where the user's camera looks (trace from the station camera), local
	FVector ComputeAimPoint() const;

	//Turns the turret toward a point: local for the user, replicated to the others by the server
	void AimAt(const FVector& AimPoint);

	bool IsFireReady(float LastFireTime) const;

	//Server: spawns a projectile from the muzzle toward the aim point
	void ServerFire(const FVector& AimPoint);

	//Plays the fire sound, called locally by the user and by the server for the others
	void PlayFireEffects();

	//Server, from a projectile of this station
	void Explode(const FVector& Center);

	//BP hooks for effects
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnFired();

	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnExploded(FVector Center, float Radius);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	//Yaw part of the turret
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretYaw;

	//Pitch part, carries the gun and the muzzle
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretPitch;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GunMesh;

	//Projectiles spawn here, along its forward
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UPDA_WeaponStation> StationData;

	UPROPERTY(Replicated)
	TObjectPtr<ACharacter> User;

	//Turret aim for the other players
	UPROPERTY(Replicated)
	FRotator ReplicatedAim = FRotator::ZeroRotator;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Fired();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Exploded(FVector_NetQuantize Center);

	bool IsLocallyUsed() const;
	void ApplyTurretRotation(const FRotator& WorldAim);

	FRotator TurretAim = FRotator::ZeroRotator;
	float LastServerFireTime = -100.f;
};
