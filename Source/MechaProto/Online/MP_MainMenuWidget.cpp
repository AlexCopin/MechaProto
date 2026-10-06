#include "MP_MainMenuWidget.h"
#include "MP_NetworkSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

void UMP_MainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Btn_Host->OnClicked.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnHostClicked);
	Btn_Join->OnClicked.AddUniqueDynamic(this, &UMP_MainMenuWidget::OnJoinClicked);

	if (const UMP_NetworkSubsystem* Network = GetGameInstance()->GetSubsystem<UMP_NetworkSubsystem>())
	{
		TB_Address->SetText(FText::FromString(Network->GetLastJoinAddress()));
	}
}

void UMP_MainMenuWidget::OnHostClicked()
{
	CloseMenu();
	GetGameInstance()->GetSubsystem<UMP_NetworkSubsystem>()->HostGame();
}

void UMP_MainMenuWidget::OnJoinClicked()
{
	CloseMenu();
	GetGameInstance()->GetSubsystem<UMP_NetworkSubsystem>()->JoinGame(TB_Address->GetText().ToString());
}

void UMP_MainMenuWidget::CloseMenu()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}
