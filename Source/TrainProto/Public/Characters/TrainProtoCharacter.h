#pragma once

#include "CoreMinimal.h"
#include "Characters/MechaProtoCharacter.h"
#include "Components/InteractComponent.h"
#include "TrainProtoCharacter.generated.h"

UCLASS()
class TRAINPROTO_API ATrainProtoCharacter : public AMechaProtoCharacter {
	GENERATED_BODY()

public:
	ATrainProtoCharacter();
	
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere,Category="Components")
	TObjectPtr<UInteractComponent> InteractComponent;

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoInteract();
	
};
