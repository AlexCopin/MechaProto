#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MP_AlertSource.generated.h"

class AMP_HUD;

UINTERFACE(MinimalAPI)
class UMP_AlertSource : public UInterface
{
	GENERATED_BODY()
};

//Actor whose state can show a lasting alert (a hull breach...): a HUD created after the state changed (late join) asks it again
class MECHAPROTO_API IMP_AlertSource
{
	GENERATED_BODY()

public:
	//Shows the alerts of the current state on this HUD (AMP_HUD::ShowAlert)
	virtual void ShowCurrentAlerts(AMP_HUD& HUD) = 0;
};
