#include "MP_MechStepShake.h"

void UMP_MechStepShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void UMP_MechStepShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	ElapsedTime = 0.f;
	//The roll kicks either way, so two steps don't feel the same
	RollSign = FMath::RandBool() ? 1.f : -1.f;
}

void UMP_MechStepShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	ElapsedTime += Params.DeltaTime;

	//Drops first (sine starts going down), then bounces, damped; the shake scale is applied by the base class
	const float Envelope = FMath::Exp(-Damping * ElapsedTime) * FMath::Clamp((Duration - ElapsedTime) / (Duration * 0.25f), 0.f, 1.f);
	const float Wave = FMath::Sin(UE_TWO_PI * Frequency * ElapsedTime) * Envelope;
	OutResult.Location = FVector(0.f, 0.f, -VerticalAmplitude * Wave);
	OutResult.Rotation = FRotator(-PitchAmplitude * Wave, 0.f, RollAmplitude * RollSign * Wave);
}

bool UMP_MechStepShakePattern::IsFinishedImpl() const
{
	return ElapsedTime >= Duration;
}

UMP_MechStepShake::UMP_MechStepShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMP_MechStepShakePattern>(TEXT("RootShakePattern")))
{
}
