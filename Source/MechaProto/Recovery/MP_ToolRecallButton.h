#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Interactable.h"
#include "MP_ToolRecallButton.generated.h"

class AMP_Item;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPDA_Recovery;
class UStaticMeshComponent;
class UTextRenderComponent;

//A button on a pedestal next to a tool's spot: pressing it (interact) brings that tool back to where it was at the start, wherever it is,
//then waits DA_Recovery's RecallCooldown (button red meanwhile). Place one per tool in the mech and pick its Tool in the level
UCLASS()
class MECHAPROTO_API AMP_ToolRecallButton : public AActor, public IMP_Interactable
{
	GENERATED_BODY()

public:
	AMP_ToolRecallButton();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	//-----IMP_Interactable
	virtual bool CanInteract(const ACharacter* User) const override;
	virtual void Interact(ACharacter* User) override;
	virtual FText GetInteractionText(const ACharacter* User) const override;

	//Server: the tool back to its spot when the cooldown allows (console: ke * Recall)
	UFUNCTION(BlueprintCallable, Category = "Recovery")
	void Recall();

	//Seconds before it can be pressed again, 0 when ready
	UFUNCTION(BlueprintPure, Category = "Recovery")
	float GetCooldownLeft() const;

	UFUNCTION(BlueprintPure, Category = "Recovery")
	const UPDA_Recovery* GetRecoveryData() const;

	UFUNCTION(BlueprintPure, Category = "Recovery")
	AMP_Item* GetTool() const { return Tool; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	//Ready / cooling down color
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	//"RECALL" and the tool's name, on the pedestal's front (+X)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TObjectPtr<UPDA_Recovery> RecoveryData;

	//The tool this button brings back (pick it in the level)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Recovery")
	TObjectPtr<AMP_Item> Tool;

	//Server time when it can be pressed again
	UPROPERTY(ReplicatedUsing = OnRep_ReadyTime)
	float ReadyTime = 0.f;

	UFUNCTION()
	void OnRep_ReadyTime();

	//Every machine, after a recall (sound...)
	UFUNCTION(BlueprintImplementableEvent, Category = "Recovery")
	void OnRecalled();

	//Label and button color, again when the cooldown ends
	void ApplyLook();
	FText GetToolName() const;

	//Parent of the colored instances (BasicShapeMaterial, its Color parameter)
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ButtonMaterial;
	FTimerHandle ReadyTimer;
};
