#include "C_PlayerStats.h"
#include "PDA_PlayerStats.h"
#include "C_CharacterMovement.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UC_PlayerStats::UC_PlayerStats()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UC_PlayerStats::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UC_PlayerStats, Health);
	DOREPLIFETIME(UC_PlayerStats, Stamina);
}

const UPDA_PlayerStats* UC_PlayerStats::GetStatsData() const
{
	if (ensureMsgf(StatsData, TEXT("%s has no StatsData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StatsData;
	}
	return GetDefault<UPDA_PlayerStats>();
}

void UC_PlayerStats::BeginPlay()
{
	Super::BeginPlay();

	//Clients get the values with the spawned pawn
	if (GetOwner()->HasAuthority())
	{
		Health = GetMaxHealth();
		Stamina = GetMaxStamina();
	}
}

void UC_PlayerStats::ModifyHealth(float Amount)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, GetMaxHealth());
	if (Health != OldHealth)
	{
		//OnRep doesn't run on the server
		OnRep_Health(OldHealth);
	}
}

void UC_PlayerStats::SetReplicatedStamina(float NewStamina)
{
	if (!GetOwner()->HasAuthority() || NewStamina == Stamina)
	{
		return;
	}

	const float OldStamina = Stamina;
	Stamina = NewStamina;
	OnRep_Stamina(OldStamina);
}

float UC_PlayerStats::GetStamina() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UC_CharacterMovement* Movement = Character ? Cast<UC_CharacterMovement>(Character->GetCharacterMovement()) : nullptr;
	return Movement && Movement->IsStaminaSimulated() ? Movement->GetStamina() : Stamina;
}

float UC_PlayerStats::GetMaxHealth() const
{
	return GetStatsData()->MaxHealth;
}

float UC_PlayerStats::GetHealthPercent() const
{
	return FMath::Clamp(Health / FMath::Max(GetMaxHealth(), 1.f), 0.f, 1.f);
}

float UC_PlayerStats::GetMaxStamina() const
{
	return GetStatsData()->MaxStamina;
}

float UC_PlayerStats::GetStaminaPercent() const
{
	return FMath::Clamp(GetStamina() / FMath::Max(GetMaxStamina(), 1.f), 0.f, 1.f);
}

void UC_PlayerStats::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, OldHealth);
}

void UC_PlayerStats::OnRep_Stamina(float OldStamina)
{
	OnStaminaChanged.Broadcast(Stamina, OldStamina);
}
