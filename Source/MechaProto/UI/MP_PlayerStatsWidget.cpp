#include "MP_PlayerStatsWidget.h"
#include "C_PlayerStats.h"
#include "C_Ragdoll.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

namespace
{
	//Sets the bar when the value changed, OutOldPercent = the previous one
	bool UpdateBar(UProgressBar* Bar, float& Percent, float NewPercent, float& OutOldPercent)
	{
		if (FMath::IsNearlyEqual(Percent, NewPercent, 0.0001f))
		{
			return false;
		}
		OutOldPercent = Percent;
		Percent = NewPercent;
		Bar->SetPercent(NewPercent);
		return true;
	}
}

void UMP_PlayerStatsWidget::UpdatePawn()
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (OwningPawn == Pawn.Get())
	{
		return;
	}

	Pawn = OwningPawn;
	PlayerStats = OwningPawn ? OwningPawn->FindComponentByClass<UC_PlayerStats>() : nullptr;
	Ragdoll = OwningPawn ? OwningPawn->FindComponentByClass<UC_Ragdoll>() : nullptr;
}

void UMP_PlayerStatsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdatePawn();
	float OldPercent = 0.f;

	if (const UC_PlayerStats* Stats = PlayerStats.Get())
	{
		if (UpdateBar(HealthBar, HealthPercent, Stats->GetHealthPercent(), OldPercent))
		{
			if (HealthText)
			{
				HealthText->SetText(FText::AsNumber(FMath::RoundToInt(Stats->GetHealth())));
			}
			if (OldPercent >= 0.f)
			{
				OnHealthChanged(HealthPercent, OldPercent);
			}
		}

		if (UpdateBar(StaminaBar, StaminaPercent, Stats->GetStaminaPercent(), OldPercent))
		{
			if (StaminaText)
			{
				StaminaText->SetText(FText::AsNumber(FMath::RoundToInt(Stats->GetStamina())));
			}
			if (OldPercent >= 0.f)
			{
				OnStaminaChanged(StaminaPercent, OldPercent);
			}
		}
	}

	if (const UC_Ragdoll* RagdollComponent = Ragdoll.Get())
	{
		if (UpdateBar(StunBar, StunPercent, RagdollComponent->GetStunPercent(), OldPercent) && OldPercent >= 0.f)
		{
			OnStunChanged(StunPercent, OldPercent, RagdollComponent->IsRagdolled());
		}
	}
}
