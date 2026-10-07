#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Enemy.generated.h"

class AMP_HullPlate;
class APawn;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class UPDA_Enemy;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyHealthChanged, AMP_Enemy*, Enemy, float, Health);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AMP_Enemy*, Enemy);

//Server behavior
UENUM()
enum class EMP_EnemyState : uint8
{
	None,
	//No hull plate in the level: walks to the spawner's goal
	ToGoal,
	ToPlate,
	//Through the hole of its broken plate
	Entering,
	//Inside the mech, chases the closest player
	Hunting,
	Idle,
};

//Lightweight enemy (no controller, no character movement): the server walks it on the ground and replicates the movement at a low rate, clients extrapolate. Killed by the weapon stations
//Goes for the mech's hull plates (crashes on them), goes through the holes and hunts the players inside
UCLASS()
class MECHAPROTO_API AMP_Enemy : public AActor
{
	GENERATED_BODY()

public:
	AMP_Enemy();

	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyDied OnDied;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PostNetReceiveLocationAndRotation() override;

	//Server: walks to a random point of the data's GoalRadius around this location when the level has no hull plate
	void SetGoal(const FVector& GoalCenter);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	const UPDA_Enemy* GetEnemyData() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool HasReachedGoal() const { return bReachedGoal; }

	//Server
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void Kill();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UPDA_Enemy> EnemyData;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	//Set with bDead, every machine throws its own copy of the body with it
	UPROPERTY(Replicated)
	FVector_NetQuantize10 DeathVelocity = FVector::ZeroVector;

	//Degrees per second
	UPROPERTY(Replicated)
	FVector_NetQuantize10 DeathSpin = FVector::ZeroVector;

	//Server, when it stops at its goal
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy")
	void OnReachedGoal();

	//Every machine, after the death effects
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy", meta = (DisplayName = "On Died"))
	void ReceiveDied();

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void OnRep_Dead();

	void ApplyData();
	//Server
	void UpdateBehavior(float DeltaSeconds);
	//Picks its plate or player
	void Think();
	//Closest plate facing it (a hole counts HoleAttraction closer), bOutAnyPlate when the level has plates
	AMP_HullPlate* FindBestPlate(bool& bOutAnyPlate) const;
	APawn* FindClosestPlayer() const;
	void SetTargetPlate(AMP_HullPlate* Plate);
	void UpdateToPlate(float DeltaSeconds);
	void UpdateEntering(float DeltaSeconds);
	void UpdateHunting(float DeltaSeconds);
	void HitPlayer(APawn* Player);
	//Kamikaze: dies bouncing off what it hit
	void Crash(const FVector& BounceDirection);
	//Moves toward Target until StopDistance from it (horizontal, 3D in the leap), true once there
	bool MoveTo(const FVector& Target, float Speed, float StopDistance, bool bCanLeap, float DeltaSeconds);
	void SnapToGround();
	void SmoothToNetLocation(float DeltaSeconds);
	void UpdateVisuals(float DeltaSeconds);
	void PlayDeath();
	//Server: launch away from the last damage
	void ComputeDeathLaunch();
	//Every machine: the mesh becomes a thrown physics body, then shrinks away
	void LaunchCorpse();
	void UpdateCorpse();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;

	//Server movement
	FVector GoalLocation = FVector::ZeroVector;
	bool bHasGoal = false;
	bool bReachedGoal = false;
	float SpeedScale = 1.f;
	float WeavePhase = 0.f;
	float NextGroundCheckTime = 0.f;

	//Server behavior
	EMP_EnemyState State = EMP_EnemyState::None;
	TWeakObjectPtr<AMP_HullPlate> TargetPlate;
	TWeakObjectPtr<APawn> TargetPlayer;
	//Its spot along the plate's width, so they spread over it
	float PlateSide = 0.f;
	float NextThinkTime = 0.f;
	float NextAttackTime = 0.f;

	//Client extrapolation
	FVector NetLocation = FVector::ZeroVector;
	FRotator NetRotation = FRotator::ZeroRotator;
	FVector NetVelocity = FVector::ZeroVector;

	//Server, from the last damage
	FVector LastDamageDirection = FVector::ZeroVector;
	bool bLastDamageWasExplosion = false;
	float DeathTime = 0.f;

	float HitFlashTimeLeft = 0.f;
	float HopTime = 0.f;
	bool bDeathPlayed = false;
};
