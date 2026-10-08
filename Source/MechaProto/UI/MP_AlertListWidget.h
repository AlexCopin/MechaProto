#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MP_AlertListWidget.generated.h"

class UPanelWidget;

//Where AMP_HUD stacks the alerts, layout done in a WBP child (a panel named AlertBox, usually a vertical box)
UCLASS(Abstract)
class MECHAPROTO_API UMP_AlertListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPanelWidget* GetAlertBox() const { return AlertBox; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> AlertBox;
};
