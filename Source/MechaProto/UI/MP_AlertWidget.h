#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MP_Alert.h"
#include "MP_AlertWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

//One alert of the HUD's list, spawned by AMP_HUD, layout done in a WBP child (TitleText, MessageText, optional DirectionArrow / DistanceText / ShowAnimation / HideAnimation)
UCLASS(Abstract)
class MECHAPROTO_API UMP_AlertWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//New alert (plays ShowAnimation, OnAlertShown) or the same one raised again (OnAlertRefreshed)
	void SetAlert(const FMP_Alert& InAlert, bool bRefresh);

	//Plays HideAnimation, then removes itself
	void Hide();

	UFUNCTION(BlueprintPure, Category = "Alert")
	const FMP_Alert& GetAlert() const { return Alert; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	//Collapsed when the alert has no message
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	//Turned toward the alert's source as seen from the camera: pointing up (angle 0) = straight ahead
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DirectionArrow;

	//Distance to the source, "12 m"
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DistanceText;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnimation;

	//Visuals per severity (colors, sound...)
	UFUNCTION(BlueprintImplementableEvent, Category = "Alert")
	void OnAlertShown(const FMP_Alert& NewAlert);

	//Raised again while shown (each hit on a plate), the timer restarted
	UFUNCTION(BlueprintImplementableEvent, Category = "Alert")
	void OnAlertRefreshed(const FMP_Alert& NewAlert);

	UFUNCTION()
	void OnHideAnimationFinished();

	void ApplyTexts();

	UPROPERTY(Transient)
	FMP_Alert Alert;

	bool bHiding = false;
	int32 ShownDistance = -1;
};
