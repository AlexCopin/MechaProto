#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Interactable.h"
#include "MP_WeaponStation.generated.h"

class ACharacter;
class AMP_Breakable;
class UBoxComponent;
class UCameraComponent;
class UPDA_WeaponStation;
class USpringArmComponent;
class UStaticMeshComponent;

//Weapon station a player mans with interact: third person camera around it, fires projectiles at the aim (for enemies, never hits players)
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

	//Where the user's capsule goes: behind the gun, turning and tilting with it
	USceneComponent* GetSeat() const { return Seat; }

	//Server, called by UC_StationUser
	void SetUser(ACharacter* NewUser);

	//Where the user's camera looks (trace from the station camera), local
	FVector ComputeAimPoint() const;

	//Turns the turret toward a point: local for the user, replicated to the others by the server
	void AimAt(const FVector& AimPoint);

	bool IsFireReady(float LastFireTime) const;

	//A required system is broken: can't fire
	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsDisabled() const;

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
	//Mech systems this station needs (engine, rotor...): it can't fire while one of them is broken
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Station")
	TArray<TObjectPtr<AMP_Breakable>> RequiredSystems;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	//Big box only the interact trace sees (Visibility), so the station is easy to target
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> InteractVolume;

	//Yaw part of the turret
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretYaw;

	//Pitch part, carries the gun and the muzzle
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TurretPitch;

	//User's capsule center, on the pitch part: always behind the gun, turning and tilting with it. Moved in the BP to fit the gun
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Seat;

	//Visual only, under the sitting pose's pelvis
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SeatMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackrestMesh;

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

	//Shots fired, the other players play the fire effects when it changes (several shots between two updates play once)
	UPROPERTY(ReplicatedUsing = OnRep_FireCounter)
	uint8 FireCounter = 0;

	UFUNCTION()
	void OnRep_FireCounter();

	bool IsLocallyUsed() const;
	void ApplyTurretRotation(const FRotator& WorldAim);

	FRotator TurretAim = FRotator::ZeroRotator;
	float LastServerFireTime = -100.f;
};
