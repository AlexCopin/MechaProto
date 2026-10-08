#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_BreakdownManager.generated.h"

class AMP_Breakable;
class UBillboardComponent;
class UPDA_Breakdowns;

//Placed once in the level: breaks a random working AMP_Breakable every so often (server), timing from UPDA_Breakdowns
UCLASS()
class MECHAPROTO_API AMP_BreakdownManager : public AActor
{
	GENERATED_BODY()

public:
	AMP_BreakdownManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//Server: next breakdown after FirstBreakdownDelay
	UFUNCTION(BlueprintCallable, Category = "Breakdowns")
	void StartBreakdowns();

	UFUNCTION(BlueprintCallable, Category = "Breakdowns")
	void StopBreakdowns();

	//Server: breaks a random working system now (weighted by BreakWeight), null if none
	UFUNCTION(BlueprintCallable, Category = "Breakdowns")
	AMP_Breakable* BreakRandom();

	UFUNCTION(BlueprintPure, Category = "Breakdowns")
	const UPDA_Breakdowns* GetBreakdownsData() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakdowns")
	TObjectPtr<UPDA_Breakdowns> BreakdownsData;

	void ScheduleNext(float Delay);
	int32 CountBroken() const;

	bool bRunning = false;
	float NextBreakdownTime = 0.f;
};
