#include "Characters/TrainProtoCharacter.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"

ATrainProtoCharacter::ATrainProtoCharacter() {
	InteractComponent = CreateDefaultSubobject<UInteractComponent>(TEXT("InteractComponent"));
}

void ATrainProtoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		if (InteractAction) {
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATrainProtoCharacter::DoInteract);
		}
	}
}

void ATrainProtoCharacter::DoInteract() {
	InteractComponent->TryInteract();
}
