#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_WeaponStation.generated.h"

class UAnimSequenceBase;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

//One weapon station type (missile launcher, gatling...), read live so it can be edited during PIE. Weapons are for enemies, players are never hit
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_WeaponStation : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//Shown in the interact prompt
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FText StationName = FText::FromString(TEXT("Weapon"));

	//-----Fire
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0.02", UIMax = "5", Units = "s"))
	float FireCooldown = 0.5f;

	//Keeps firing while the button is held, otherwise one shot per press
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	bool bAutomatic = true;

	//Random cone around the aim
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0", UIMax = "15", Units = "Degrees"))
	float SpreadAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<USoundBase> FireSound;

	//-----Projectile
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "100", UIMax = "20000", Units = "CentimetersPerSecond"))
	float ProjectileSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0", UIMax = "2"))
	float ProjectileGravityScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1", UIMax = "20", Units = "s"))
	float ProjectileLifeSpan = 5.f;

	//Collision sphere
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1", UIMax = "100", Units = "cm"))
	float ProjectileRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMesh> ProjectileMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	FVector ProjectileMeshScale = FVector(0.05f);

	//-----Hit (no explosion)
	//Push on physics objects hit (players are never hit)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0", UIMax = "3000"))
	float HitImpulse = 300.f;

	//-----Explosion (0 radius = no explosion), pushes physics objects, not players
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion", meta = (ClampMin = "0", UIMax = "2000", Units = "cm"))
	float ExplosionRadius = 0.f;

	//Velocity given to physics objects at the center, fading to the edge
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion", meta = (ClampMin = "0", UIMax = "5000"))
	float ExplosionImpulse = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	//Draws the radius until there is an explosion effect
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	bool bDrawExplosionDebug = true;

	//-----Aim & camera (third person around the station)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "100", UIMax = "2000", Units = "cm"))
	float CameraDistance = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float CameraBlendTime = 0.3f;

	//How far the aim trace looks for a target
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1000", UIMax = "100000", Units = "cm"))
	float AimDistance = 20000.f;

	//Turret follow speed for the other players' view
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1", UIMax = "30"))
	float TurretTurnSpeed = 12.f;

	//-----User animation (looped on the body while manning)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ManningAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName BodyAnimationSlot = FName("DefaultSlot");
};
