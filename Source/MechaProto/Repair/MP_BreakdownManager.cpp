#include "MP_BreakdownManager.h"
#include "MP_Breakable.h"
#include "PDA_Breakable.h"
#include "PDA_Breakdowns.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

AMP_BreakdownManager::AMP_BreakdownManager()
{
	PrimaryActorTick.bCanEverTick = true;
	//Server only, the breakables replicate their state
	bReplicates = false;

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

const UPDA_Breakdowns* AMP_BreakdownManager::GetBreakdownsData() const
{
	if (ensureMsgf(BreakdownsData, TEXT("%s has no BreakdownsData, using code defaults"), *GetPathNameSafe(this)))
	{
		return BreakdownsData;
	}
	return GetDefault<UPDA_Breakdowns>();
}

void AMP_BreakdownManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && GetBreakdownsData()->bAutoStart)
	{
		StartBreakdowns();
	}
}

void AMP_BreakdownManager::StartBreakdowns()
{
	if (!HasAuthority())
	{
		return;
	}
	bRunning = true;
	ScheduleNext(GetBreakdownsData()->FirstBreakdownDelay);
}

void AMP_BreakdownManager::StopBreakdowns()
{
	bRunning = false;
}

void AMP_BreakdownManager::ScheduleNext(float Delay)
{
	NextBreakdownTime = GetWorld()->GetTimeSeconds() + Delay;
}

int32 AMP_BreakdownManager::CountBroken() const
{
	int32 Count = 0;
	for (TActorIterator<AMP_Breakable> It(GetWorld()); It; ++It)
	{
		Count += It->IsBroken() ? 1 : 0;
	}
	return Count;
}

AMP_Breakable* AMP_BreakdownManager::BreakRandom()
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	TArray<AMP_Breakable*> Candidates;
	float TotalWeight = 0.f;
	for (TActorIterator<AMP_Breakable> It(GetWorld()); It; ++It)
	{
		const float Weight = It->GetBreakableData()->BreakWeight;
		if (!It->IsBroken() && Weight > 0.f)
		{
			Candidates.Add(*It);
			TotalWeight += Weight;
		}
	}

	float Pick = FMath::FRandRange(0.f, TotalWeight);
	for (AMP_Breakable* Breakable : Candidates)
	{
		Pick -= Breakable->GetBreakableData()->BreakWeight;
		if (Pick <= 0.f)
		{
			Breakable->Break();
			return Breakable;
		}
	}
	if (Candidates.Num() > 0)
	{
		Candidates.Last()->Break();
		return Candidates.Last();
	}
	return nullptr;
}

void AMP_BreakdownManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bRunning)
	{
		return;
	}

	const UPDA_Breakdowns* Data = GetBreakdownsData();
	const float Now = GetWorld()->GetTimeSeconds();
	const int32 BrokenCount = CountBroken();
	if (Now >= NextBreakdownTime)
	{
		//Too many broken: waits, breaks as soon as one is repaired
		if (BrokenCount < Data->MaxBrokenAtOnce)
		{
			BreakRandom();
			ScheduleNext(FMath::FRandRange(Data->MinInterval, FMath::Max(Data->MinInterval, Data->MaxInterval)));
		}
	}

	if (Data->bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Yellow,
			FString::Printf(TEXT("Next breakdown in %.0f s - %d broken (max %d)"), FMath::Max(0.f, NextBreakdownTime - Now), BrokenCount, Data->MaxBrokenAtOnce));
	}
}
