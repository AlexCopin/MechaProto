#include "MP_EnemySpawner.h"
#include "MP_Enemy.h"
#include "PDA_EnemyWaves.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

AMP_EnemySpawner::AMP_EnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	//Wave info for every player wherever they are
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
		Sprite->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	}
#endif
}

void AMP_EnemySpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_EnemySpawner, WaveNumber);
	DOREPLIFETIME(AMP_EnemySpawner, AliveEnemyCount);
}

const UPDA_EnemyWaves* AMP_EnemySpawner::GetWavesData() const
{
	if (ensureMsgf(WavesData, TEXT("%s has no WavesData, using code defaults"), *GetPathNameSafe(this)))
	{
		return WavesData;
	}
	return GetDefault<UPDA_EnemyWaves>();
}

void AMP_EnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && GetWavesData()->bAutoStart)
	{
		StartWaves();
	}
}

void AMP_EnemySpawner::StartWaves()
{
	if (!HasAuthority())
	{
		return;
	}
	WaveNumber = 0;
	ActiveGroups.Reset();
	State = EMP_WaveState::WaitingNextWave;
	NextWaveTime = GetWorld()->GetTimeSeconds() + GetWavesData()->FirstWaveDelay;
}

void AMP_EnemySpawner::StopWaves()
{
	if (HasAuthority())
	{
		ActiveGroups.Reset();
		State = EMP_WaveState::Stopped;
	}
}

void AMP_EnemySpawner::KillAllEnemies()
{
	if (!HasAuthority())
	{
		return;
	}
	for (const TWeakObjectPtr<AMP_Enemy>& Enemy : AliveEnemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->Kill();
		}
	}
	UpdateAliveEnemies();
}

void AMP_EnemySpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		UpdateAliveEnemies();

		switch (State)
		{
		case EMP_WaveState::WaitingNextWave:
			if (Now >= NextWaveTime)
			{
				StartNextWave();
			}
			break;
		case EMP_WaveState::Spawning:
			UpdateSpawning(Now);
			break;
		case EMP_WaveState::WaitingClear:
			if (AliveEnemies.IsEmpty())
			{
				State = EMP_WaveState::WaitingNextWave;
				NextWaveTime = Now + WaveDelayAfter;
			}
			break;
		default:
			break;
		}
	}

	ShowDebug();
}

void AMP_EnemySpawner::StartNextWave()
{
	const UPDA_EnemyWaves* Data = GetWavesData();
	if (Data->Waves.IsEmpty() || (!Data->bLoopWaves && WaveNumber >= Data->Waves.Num()))
	{
		State = EMP_WaveState::Stopped;
		return;
	}

	const FMP_EnemyWave& Wave = Data->Waves[WaveNumber % Data->Waves.Num()];
	const int32 Loop = WaveNumber / Data->Waves.Num();
	const float CountScale = FMath::Pow(Data->LoopCountMultiplier, Loop);
	++WaveNumber;

	const float Now = GetWorld()->GetTimeSeconds();
	ActiveGroups.Reset();
	for (const FMP_EnemyGroup& Group : Wave.Groups)
	{
		if (!Group.EnemyClass)
		{
			continue;
		}
		FActiveGroup& Active = ActiveGroups.AddDefaulted_GetRef();
		Active.EnemyClass = Group.EnemyClass;
		Active.Remaining = FMath::RoundToInt(Group.Count * CountScale);
		Active.SpawnInterval = Group.SpawnInterval;
		Active.NextSpawnTime = Now + Group.StartDelay;
		Active.BaseYaw = FMath::FRandRange(0.f, 360.f);
		Active.ArcWidth = Group.ArcWidth;
	}
	WaveDelayAfter = Wave.DelayAfter;
	bWaveWaitsForClear = Wave.bWaitForClear;
	State = EMP_WaveState::Spawning;
	OnWaveStarted.Broadcast(WaveNumber);
}

void AMP_EnemySpawner::UpdateSpawning(float Now)
{
	const int32 MaxAlive = GetWavesData()->MaxAliveEnemies;
	for (FActiveGroup& Group : ActiveGroups)
	{
		//Several per frame when the interval is shorter than a frame
		while (Group.Remaining > 0 && Now >= Group.NextSpawnTime && AliveEnemies.Num() < MaxAlive)
		{
			SpawnEnemy(Group);
			--Group.Remaining;
			Group.NextSpawnTime += Group.SpawnInterval;
		}
	}
	ActiveGroups.RemoveAll([](const FActiveGroup& Group) { return Group.Remaining <= 0; });

	if (ActiveGroups.IsEmpty())
	{
		State = bWaveWaitsForClear ? EMP_WaveState::WaitingClear : EMP_WaveState::WaitingNextWave;
		NextWaveTime = Now + WaveDelayAfter;
	}
}

void AMP_EnemySpawner::SpawnEnemy(const FActiveGroup& Group)
{
	const UPDA_EnemyWaves* Data = GetWavesData();
	const float Yaw = Group.BaseYaw + FMath::FRandRange(-0.5f, 0.5f) * Group.ArcWidth;
	const FVector Direction = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Center = GetActorLocation();
	//The enemy snaps itself to the ground
	const FVector Location = Center + Direction * (Data->SpawnRadius + FMath::FRandRange(0.f, Data->SpawnDepth));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	AMP_Enemy* Enemy = GetWorld()->SpawnActor<AMP_Enemy>(Group.EnemyClass, Location, (-Direction).Rotation(), Params);
	if (Enemy)
	{
		Enemy->SetGoal(Center);
		AliveEnemies.Add(Enemy);
	}
}

void AMP_EnemySpawner::UpdateAliveEnemies()
{
	AliveEnemies.RemoveAll([](const TWeakObjectPtr<AMP_Enemy>& Enemy) { return !Enemy.IsValid() || Enemy->IsDead(); });
	AliveEnemyCount = AliveEnemies.Num();
}

void AMP_EnemySpawner::ShowDebug() const
{
	if (!GEngine || GetNetMode() == NM_DedicatedServer || !GetWavesData()->bShowDebug)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Orange, FString::Printf(TEXT("Wave %d - %d enemies"), WaveNumber, AliveEnemyCount));
}
