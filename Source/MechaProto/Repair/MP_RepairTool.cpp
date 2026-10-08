#include "MP_RepairTool.h"
#include "C_Interactor.h"
#include "C_ItemHolder.h"
#include "MP_Breakable.h"
#include "GameFramework/Character.h"

void AMP_RepairTool::OnUsed_Implementation(ACharacter* User)
{
	AMP_Breakable* Breakable = FindLookedAtBreakable(User);
	if (Breakable && Breakable->RepairHit(User))
	{
		OnRepairHit(Breakable, User);
	}
}

AMP_Breakable* AMP_RepairTool::FindLookedAtBreakable(const ACharacter* User) const
{
	const UC_ItemHolder* ItemHolder = User ? User->FindComponentByClass<UC_ItemHolder>() : nullptr;
	if (!ItemHolder)
	{
		return nullptr;
	}

	//Same targeting as the interact focus (look sweep, then aim assist)
	AActor* Target = UC_Interactor::FindTarget(User, ItemHolder->GetInteractionData(), [](AActor* Actor)
	{
		const AMP_Breakable* Breakable = Cast<AMP_Breakable>(Actor);
		return Breakable && Breakable->IsBroken();
	}, this);
	return Cast<AMP_Breakable>(Target);
}
