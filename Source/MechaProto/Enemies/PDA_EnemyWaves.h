#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_EnemyWaves.generated.h"

class AMP_Enemy;

//Enemies of one type coming from one direction
USTRUCT(BlueprintType)
struct FMP_EnemyGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AMP_Enemy> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", UIMax = "300"))
	int32 Count = 20;

	//Time between two spawns of this group
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float SpawnInterval = 0.05f;

	//From the start of the wave
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", UIMax = "60", Units = "s"))
	float StartDelay = 0.f;

	//The group comes from a random direction and spreads over this angle, 360 = from everywhere
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "360", Units = "Degrees"))
	float ArcWidth = 25.f;
};

USTRUCT(BlueprintType)
struct FMP_EnemyWave
{
	GENERATED_BODY()

	//Spawned together, each from its own direction
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FMP_EnemyGroup> Groups;

	//The next wave waits for every enemy of this one to die
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bWaitForClear = true;

	//Pause before the next wave, after the last spawn (or the clear)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", UIMax = "120", Units = "s"))
	float DelayAfter = 8.f;
};

//Wave list of an enemy spawner, read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_EnemyWaves : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TArray<FMP_EnemyWave> Waves;

	//Starts on BeginPlay, otherwise call StartWaves
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = "0", UIMax = "60", Units = "s"))
	float FirstWaveDelay = 5.f;

	//After the last wave, start again from the first one with more enemies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	bool bLoopWaves = true;

	//Enemy counts are multiplied by this on each loop
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = "1", UIMax = "3"))
	float LoopCountMultiplier = 1.5f;

	//No spawn above this many alive enemies (performance guard), the spawn waits
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = "1", UIMax = "1000"))
	int32 MaxAliveEnemies = 300;

	//-----Spawn ring around the spawner, the enemies walk to the spawner
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0", UIMax = "30000", Units = "cm"))
	float SpawnRadius = 9000.f;

	//Random extra distance, so a group arrives as a crowd and not a line
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0", UIMax = "5000", Units = "cm"))
	float SpawnDepth = 1000.f;

	//-----Debug
	//Wave number and alive enemies on screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowDebug = true;
};
