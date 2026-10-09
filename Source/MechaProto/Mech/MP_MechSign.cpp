#include "MP_MechSign.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AMP_MechSign::AMP_MechSign()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	//Readable from the front (+X)
	TextRender = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	TextRender->SetupAttachment(Root);
	TextRender->SetHorizontalAlignment(EHTA_Center);
	TextRender->SetVerticalAlignment(EVRTA_TextCenter);
	TextRender->SetCastShadow(false);
	TextRender->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Root);
	Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panel->SetCanEverAffectNavigation(false);
	Panel->SetCastShadow(false);
	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Panel->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AMP_MechSign::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySign();
}

void AMP_MechSign::BeginPlay()
{
	Super::BeginPlay();
	ApplySign();
}

void AMP_MechSign::ApplySign()
{
	TextRender->SetText(Text);
	TextRender->SetWorldSize(TextSize);
	TextRender->SetTextRenderColor(TextColor);

	//Panel around the text, 1 to 3 cm behind it
	TArray<FString> Lines;
	Text.ToString().ParseIntoArray(Lines, TEXT("\n"));
	int32 MaxChars = 1;
	for (const FString& Line : Lines)
	{
		MaxChars = FMath::Max(MaxChars, Line.Len());
	}
	const float Width = TextSize * 0.62f * MaxChars + 30.f;
	const float Height = TextSize * 1.15f * FMath::Max(Lines.Num(), 1) + 20.f;
	Panel->SetVisibility(bShowPanel);
	Panel->SetRelativeLocation(FVector(-2.f, 0.f, 0.f));
	Panel->SetRelativeScale3D(FVector(0.02f, Width / 100.f, Height / 100.f));
	if (bShowPanel)
	{
		if (UMaterialInstanceDynamic* Material = Panel->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), PanelColor);
		}
	}
}
