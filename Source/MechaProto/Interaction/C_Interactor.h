#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_Interactor.generated.h"

class APawn;
class UMaterialInterface;
class UMeshComponent;
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

	//What the pawn aims at within InteractRange: the first actor the look sweep hits if it is a target, else the target closest to the view direction within InteractAssistAngle and in sight
	static AActor* FindTarget(const APawn* Pawn, const UPDA_Interaction* Data, TFunctionRef<bool(AActor*)> IsTarget, const AActor* IgnoredActor = nullptr);

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
	//Local: the focus overlay on the focused actor's meshes, their own overlay given back when cleared
	void SetHighlight(AActor* Actor);

	struct FHighlightedMesh
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInterface> PreviousOverlay;
	};
	TArray<FHighlightedMesh> HighlightedMeshes;
	TWeakObjectPtr<UMaterialInterface> AppliedOverlay;

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;
	FTimerHandle FocusTimer;
};
