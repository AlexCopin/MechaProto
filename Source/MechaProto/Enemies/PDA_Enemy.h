#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Enemy.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

//One enemy type (swarm, big...), read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Enemy : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1", UIMax = "5000"))
	float MaxHealth = 100.f;

	//-----Movement (straight to the goal, on the ground)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", UIMax = "3000", Units = "CentimetersPerSecond"))
	float MoveSpeed = 400.f;

	//Random speed difference between enemies of this type (0.2 = +-20%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ClampMax = "0.9"))
	float SpeedVariation = 0.f;

	//Zigzag: the direction swings left and right of the goal by this angle
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ClampMax = "80", Units = "Degrees"))
	float WeaveAngle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", UIMax = "3", Units = "Hertz"))
	float WeaveFrequency = 0.5f;

	//Each enemy stops at a random point of this disc around the goal, so they don't stack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", UIMax = "5000", Units = "cm"))
	float GoalRadius = 600.f;

	//How fast the body turns to its movement direction
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", UIMax = "30"))
	float TurnSpeed = 8.f;

	//-----Hull attack: with hull plates (AMP_HullPlate) in the level it goes for the closest one facing it instead of its goal
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack")
	bool bAttackHull = true;

	//Damage to the plate per hit
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack", meta = (ClampMin = "0", UIMax = "500"))
	float HullDamage = 10.f;

	//Dies in its hit (crashes on the plate or the player), otherwise stays and hits every AttackInterval
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack")
	bool bKamikaze = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack", meta = (ClampMin = "0.1", UIMax = "10", Units = "s", EditCondition = "!bKamikaze"))
	float AttackInterval = 2.f;

	//Goes through the holes of broken plates to hunt the players inside, otherwise moves on to the next intact plate
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack")
	bool bEnterHoles = true;

	//A hole is picked over an intact plate up to this much farther
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack", meta = (ClampMin = "0", UIMax = "10000", Units = "cm", EditCondition = "bEnterHoles"))
	float HoleAttraction = 3000.f;

	//Last stretch to a plate, through a hole or to a player: leaves the ground and goes straight at it (jumps to plates high on the hull, pounces). 0 = stays on the ground
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hull Attack", meta = (ClampMin = "0", UIMax = "2000", Units = "cm"))
	float LeapDistance = 300.f;

	//-----Hunt: once inside the mech it chases the closest player
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunt", meta = (ClampMin = "0", UIMax = "3000", Units = "CentimetersPerSecond"))
	float HuntSpeed = 450.f;

	//Reaching a player slaps them (counts toward their ragdoll like a friend's slap)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunt")
	bool bSlapPlayers = true;

	//Gap between the bodies to hit a player
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hunt", meta = (ClampMin = "0", UIMax = "300", Units = "cm"))
	float PlayerReach = 40.f;

	//-----Collision (capsule, bottom on the ground)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "5", UIMax = "1000", Units = "cm"))
	float CapsuleRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "5", UIMax = "1500", Units = "cm"))
	float CapsuleHalfHeight = 40.f;

	//Players bump into it, otherwise they walk through
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	bool bBlockPlayers = false;

	//-----Visuals (engine basic shapes are centered on their pivot)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FVector MeshScale = FVector(0.8f);

	//Relative to the capsule center
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FVector MeshOffset = FVector::ZeroVector;

	//Needs a vector parameter named ColorParameter (BasicShapeMaterial has "Color")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FName ColorParameter = FName("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor Color = FLinearColor::Red;

	//Hop while moving, visual only
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0", UIMax = "200", Units = "cm"))
	float HopHeight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0", UIMax = "10", Units = "Hertz"))
	float HopFrequency = 3.f;

	//Flash when damaged
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor HitFlashColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "0", UIMax = "1", Units = "s"))
	float HitFlashDuration = 0.12f;

	//-----Death
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TObjectPtr<UNiagaraSystem> DeathEffect;

	//Draws a puff until there is a death effect
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	bool bDrawDeathDebug = true;

	//-----Death launch: the body becomes a physics object thrown away from what killed it (+-20% random)
	//Killed by a direct hit, along the shot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "4000", Units = "CentimetersPerSecond"))
	float DeathLaunchSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "4000", Units = "CentimetersPerSecond"))
	float DeathLaunchUpSpeed = 500.f;

	//Killed by an explosion, away from its center
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "6000", Units = "CentimetersPerSecond"))
	float DeathExplosionLaunchSpeed = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "6000", Units = "CentimetersPerSecond"))
	float DeathExplosionLaunchUpSpeed = 1200.f;

	//Random tumble
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "2000", Units = "DegreesPerSecond"))
	float DeathSpinSpeed = 540.f;

	//The body stays this long, then shrinks away
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0.1", UIMax = "20", Units = "s"))
	float CorpseLifeSpan = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0", UIMax = "3", Units = "s"))
	float CorpseShrinkTime = 0.5f;

	//-----Network
	//Movement updates per second, clients extrapolate in between
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "1", UIMax = "60"))
	float NetUpdateFrequency = 10.f;

	//-----Debug
	//Health above the enemy on every machine
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawDebugHealth = false;
};
