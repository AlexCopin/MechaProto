#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "MP_MechStepShake.generated.h"

//A heavy thump: the view drops and bounces back, damped (DA_Mech FootstepShake). Make a BP child to change its shape
UCLASS()
class MECHAPROTO_API UMP_MechStepShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0.05", UIMax = "3", Units = "s"))
	float Duration = 0.8f;

	//Vertical drop of the view
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0", UIMax = "50", Units = "cm"))
	float VerticalAmplitude = 10.f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0", UIMax = "5", Units = "Degrees"))
	float PitchAmplitude = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0", UIMax = "5", Units = "Degrees"))
	float RollAmplitude = 0.35f;

	//Bounces per second
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0.1", UIMax = "30"))
	float Frequency = 6.f;

	//How fast the bounces die out
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0", UIMax = "20"))
	float Damping = 5.f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float ElapsedTime = 0.f;
	float RollSign = 1.f;
};

//Played on each local camera when a mech foot lands
UCLASS()
class MECHAPROTO_API UMP_MechStepShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UMP_MechStepShake(const FObjectInitializer& ObjectInitializer);
};
