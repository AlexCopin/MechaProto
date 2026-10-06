#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MP_Interactable.generated.h"

class ACharacter;

UINTERFACE(MinimalAPI)
class UMP_Interactable : public UInterface
{
	GENERATED_BODY()
};

//Something a player can use with the interact key (stations, items...). Found by UC_Interactor's look trace
class MECHAPROTO_API IMP_Interactable
{
	GENERATED_BODY()

public:
	//Checked locally for the prompt and again on the server
	virtual bool CanInteract(const ACharacter* User) const { return true; }

	//Server only
	virtual void Interact(ACharacter* User) = 0;

	//Prompt shown to the local player
	virtual FText GetInteractionText(const ACharacter* User) const { return FText::GetEmpty(); }
};
