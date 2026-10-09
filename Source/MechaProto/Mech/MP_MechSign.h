#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_MechSign.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

//Text on a dark panel so the crew finds its way ("LEFT LEG 3 / HIPS ^ FOOT v"): put it against a wall, forward facing the room.
//The panel follows the text's size. Attach it to the mech (or a leg's pivot in a leg) so it moves with it
UCLASS()
class MECHAPROTO_API AMP_MechSign : public AActor
{
	GENERATED_BODY()

public:
	AMP_MechSign();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, Category = "Sign", meta = (MultiLine = true))
	FText Text = NSLOCTEXT("MechSign", "Default", "SIGN");

	//Letter height
	UPROPERTY(EditAnywhere, Category = "Sign", meta = (ClampMin = "1", UIMax = "200", Units = "cm"))
	float TextSize = 30.f;

	UPROPERTY(EditAnywhere, Category = "Sign")
	FColor TextColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Sign")
	bool bShowPanel = true;

	UPROPERTY(EditAnywhere, Category = "Sign", meta = (EditCondition = "bShowPanel"))
	FLinearColor PanelColor = FLinearColor(0.015f, 0.015f, 0.02f);

protected:
	virtual void BeginPlay() override;
	void ApplySign();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> TextRender;

	//Behind the text, sized to it
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Panel;
};
