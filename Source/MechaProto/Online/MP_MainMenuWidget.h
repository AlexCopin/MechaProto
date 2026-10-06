#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MP_MainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;

//Host / Join menu, layout done in a WBP child (widgets named Btn_Host, Btn_Join, TB_Address)
UCLASS(Abstract)
class MECHAPROTO_API UMP_MainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Host;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> TB_Address;

	UFUNCTION()
	void OnHostClicked();

	UFUNCTION()
	void OnJoinClicked();

	void CloseMenu();
};
