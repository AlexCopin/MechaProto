#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_EnemySpawner.generated.h"

class AMP_Enemy;
class UBillboardComponent;
class UPDA_EnemyWaves;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);

UENUM()
enum class EMP_WaveState : uint8
{
	Stopped,
	WaitingNextWave,
	Spawning,
	WaitingClear,
};

//Placed at the center of the map: spawns the waves of UPDA_EnemyWaves on a ring around itself (server), the enemies walk to it
UCLASS()
class MECHAPROTO_API AMP_EnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AMP_EnemySpawner();

	//Server
	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnWaveStarted OnWaveStarted;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//Server: starts from the first wave after FirstWaveDelay
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StartWaves();

	//Server: stops spawning, the alive enemies stay
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void StopWaves();

	//Server
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void KillAllEnemies();

	UFUNCTION(BlueprintPure, Category = "Waves")
	const UPDA_EnemyWaves* GetWavesData() const;

	//0 before the first wave, keeps counting through the loops
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetWaveNumber() const { return WaveNumber; }

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetAliveEnemyCount() const { return AliveEnemyCount; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TObjectPtr<UPDA_EnemyWaves> WavesData;

	UPROPERTY(Replicated)
	int32 WaveNumber = 0;

	UPROPERTY(Replicated)
	int32 AliveEnemyCount = 0;

	struct FActiveGroup
	{
		TSubclassOf<AMP_Enemy> EnemyClass;
		int32 Remaining = 0;
		float SpawnInterval = 0.f;
		float NextSpawnTime = 0.f;
		float BaseYaw = 0.f;
		float ArcWidth = 0.f;
	};

	void StartNextWave();
	void UpdateSpawning(float Now);
	void SpawnEnemy(const FActiveGroup& Group);
	void UpdateAliveEnemies();
	void ShowDebug() const;

	EMP_WaveState State = EMP_WaveState::Stopped;
	float NextWaveTime = 0.f;
	float WaveDelayAfter = 0.f;
	bool bWaveWaitsForClear = false;
	TArray<FActiveGroup> ActiveGroups;
	TArray<TWeakObjectPtr<AMP_Enemy>> AliveEnemies;
};
