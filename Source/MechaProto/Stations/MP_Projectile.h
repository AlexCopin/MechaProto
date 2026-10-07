#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "MP_Projectile.generated.h"

class UPDA_WeaponStation;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

//Projectile fired by a weapon station (its owner), spawned and resolved on the server, replicated. Never hits players
UCLASS()
class MECHAPROTO_API AMP_Projectile : public AActor
{
	GENERATED_BODY()

public:
	AMP_Projectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	//Server, before FinishSpawning
	void InitProjectile(UPDA_WeaponStation* InData);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	//Replicated so clients build the same visuals
	UPROPERTY(Replicated)
	TObjectPtr<UPDA_WeaponStation> Data;

	//Set by the server on impact, clients play the explosion from it
	UPROPERTY(ReplicatedUsing = OnRep_ExplosionLocation)
	FVector_NetQuantize ExplosionLocation;

	UFUNCTION()
	void OnRep_ExplosionLocation();

	void ApplyData();
	//Server: damage to the enemies and impulses on physics objects in the radius, then the effects everywhere
	void Explode(const FVector& Center);
	void PlayExplosionEffects();
	//Hidden and stopped, kept alive a moment so the explosion replicates
	void Freeze();

	bool bHit = false;
	bool bExploded = false;
};
