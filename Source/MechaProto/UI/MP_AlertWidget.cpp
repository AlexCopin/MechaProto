#include "MP_AlertWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextBlock.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

void UMP_AlertWidget::SetAlert(const FMP_Alert& InAlert, bool bRefresh)
{
	Alert = InAlert;
	ApplyTexts();

	if (bRefresh)
	{
		OnAlertRefreshed(Alert);
		return;
	}
	if (ShowAnimation)
	{
		PlayAnimation(ShowAnimation);
	}
	OnAlertShown(Alert);
}

void UMP_AlertWidget::ApplyTexts()
{
	TitleText->SetText(Alert.Title);
	MessageText->SetText(Alert.Message);
	MessageText->SetVisibility(Alert.Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMP_AlertWidget::Hide()
{
	if (bHiding)
	{
		return;
	}
	bHiding = true;

	if (HideAnimation)
	{
		FWidgetAnimationDynamicEvent Finished;
		Finished.BindDynamic(this, &UMP_AlertWidget::OnHideAnimationFinished);
		BindToAnimationFinished(HideAnimation, Finished);
		PlayAnimation(HideAnimation);
		return;
	}
	RemoveFromParent();
}

void UMP_AlertWidget::OnHideAnimationFinished()
{
	RemoveFromParent();
}

void UMP_AlertWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AActor* Source = Alert.Source;
	const APlayerController* Controller = GetOwningPlayer();
	if (!Source || (!DirectionArrow && !DistanceText) || !Controller || !Controller->PlayerCameraManager)
	{
		return;
	}

	const FVector ToSource = Source->GetActorLocation() - Controller->PlayerCameraManager->GetCameraLocation();
	if (DirectionArrow)
	{
		DirectionArrow->SetRenderTransformAngle(FRotator::NormalizeAxis(ToSource.Rotation().Yaw - Controller->PlayerCameraManager->GetCameraRotation().Yaw));
	}
	const int32 Distance = FMath::RoundToInt(ToSource.Size() / 100.f);
	if (DistanceText && Distance != ShownDistance)
	{
		ShownDistance = Distance;
		DistanceText->SetText(FText::Format(NSLOCTEXT("MechaProto", "AlertDistance", "{0} m"), FText::AsNumber(Distance)));
	}
}
