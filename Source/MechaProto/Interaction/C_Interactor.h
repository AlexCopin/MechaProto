#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_Interactor.generated.h"

class UPDA_Interaction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractFocusChanged, AActor*, FocusedActor, FText, Prompt);

//Finds the interactable the local player looks at and uses it on the server (IMP_Interactable)
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_Interactor : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_Interactor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//Local input: uses the focused interactable
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void TryInteract();

	//Local input: uses a given interactable (leave a station...)
	void InteractWith(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Interact")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interact")
	const UPDA_Interaction* GetInteractionData() const;

	//Local player only, for the prompt widget
	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FOnInteractFocusChanged OnFocusChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
	TObjectPtr<UPDA_Interaction> InteractionData;

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	void UpdateFocus();
	AActor* FindLookedAtInteractable() const;

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;
	FTimerHandle FocusTimer;
};
