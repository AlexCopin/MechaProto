#pragma once

#include "CoreMinimal.h"
#include "MP_Item.h"
#include "MP_RepairTool.generated.h"

class AMP_Breakable;

//Held item that repairs the broken systems needing it (UPDA_Breakable::RequiredTool = this item's data): use it looking at the system
UCLASS()
class MECHAPROTO_API AMP_RepairTool : public AMP_Item
{
	GENERATED_BODY()

protected:
	virtual void OnUsed_Implementation(ACharacter* User) override;

	//Server, after a hit that counted (sound, swing...)
	UFUNCTION(BlueprintImplementableEvent, Category = "Repair")
	void OnRepairHit(AMP_Breakable* Breakable, ACharacter* User);

	//Broken system the user looks at, within interact reach (UC_Interactor::FindTarget)
	AMP_Breakable* FindLookedAtBreakable(const ACharacter* User) const;
};
