#include "MP_ToolRecallButton.h"
#include "MechaProto.h"
#include "MP_Item.h"
#include "PDA_Item.h"
#include "PDA_Recovery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	//A late joiner's first ReadyTime isn't a recall happening now
	constexpr float RecallEventWindow = 1.f;
	const FLinearColor RecallPedestalColor(0.05f, 0.05f, 0.06f, 1.f);
}

AMP_ToolRecallButton::AMP_ToolRecallButton()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	//Tinted through its Color parameter
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = BasicShapeMaterial.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	//1 m pedestal, origin on the floor
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base Mesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	BaseMesh->SetRelativeScale3D(FVector(0.4f, 0.5f, 1.f));
	BaseMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BaseMesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	BaseMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (CubeMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CubeMesh.Object);
	}

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Button Mesh"));
	ButtonMesh->SetupAttachment(Root);
	ButtonMesh->SetRelativeLocation(FVector(0.f, 0.f, 104.f));
	ButtonMesh->SetRelativeScale3D(FVector(0.28f, 0.28f, 0.08f));
	ButtonMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	ButtonMesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	ButtonMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	if (CylinderMesh.Succeeded())
	{
		ButtonMesh->SetStaticMesh(CylinderMesh.Object);
	}
	BaseMesh->SetMaterial(0, ShapeMaterial);
	ButtonMesh->SetMaterial(0, ShapeMaterial);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(21.f, 0.f, 75.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(9.f);
	Label->SetTextRenderColor(FColor::White);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMP_ToolRecallButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_ToolRecallButton, ReadyTime);
}

const UPDA_Recovery* AMP_ToolRecallButton::GetRecoveryData() const
{
	if (ensureMsgf(RecoveryData, TEXT("%s has no RecoveryData, using code defaults"), *GetPathNameSafe(this)))
	{
		return RecoveryData;
	}
	return GetDefault<UPDA_Recovery>();
}

void AMP_ToolRecallButton::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AMP_ToolRecallButton::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
}

FText AMP_ToolRecallButton::GetToolName() const
{
	return Tool ? Tool->GetItemData()->ItemName : FText::FromString(TEXT("?"));
}

float AMP_ToolRecallButton::GetCooldownLeft() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || ReadyTime <= 0.f)
	{
		return 0.f;
	}
	return FMath::Max(0.f, ReadyTime - static_cast<float>(GameState->GetServerWorldTimeSeconds()));
}

bool AMP_ToolRecallButton::CanInteract(const ACharacter* User) const
{
	//Focusable during the cooldown too, the prompt says how long
	return Tool != nullptr;
}

void AMP_ToolRecallButton::Interact(ACharacter* User)
{
	Recall();
}

FText AMP_ToolRecallButton::GetInteractionText(const ACharacter* User) const
{
	const UPDA_Recovery* Data = GetRecoveryData();
	if (Tool && Tool->GetHolder() && !Data->bRecallFromHands)
	{
		return FText::Format(Data->RecallHeldText, GetToolName());
	}
	const float CooldownLeft = GetCooldownLeft();
	if (CooldownLeft > 0.f)
	{
		return FText::Format(Data->RecallCooldownText, GetToolName(), FText::AsNumber(FMath::CeilToInt(CooldownLeft)));
	}
	return FText::Format(Data->RecallText, GetToolName());
}

void AMP_ToolRecallButton::Recall()
{
	const UPDA_Recovery* Data = GetRecoveryData();
	if (!HasAuthority() || !Tool || GetCooldownLeft() > 0.f || (Tool->GetHolder() && !Data->bRecallFromHands))
	{
		return;
	}

	Tool->ReturnHome();
	UE_LOG(LogMechaProto, Verbose, TEXT("%s recalled %s"), *GetName(), *Tool->GetName());

	//The server plays it itself, OnRep doesn't run here
	ReadyTime = GetWorld()->GetTimeSeconds() + Data->RecallCooldown;
	ForceNetUpdate();
	OnRep_ReadyTime();
}

void AMP_ToolRecallButton::OnRep_ReadyTime()
{
	ApplyLook();

	const UPDA_Recovery* Data = GetRecoveryData();
	if (Data->RecallCooldown - GetCooldownLeft() > RecallEventWindow)
	{
		return;
	}
	if (Data->RecallSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->RecallSound, ButtonMesh->GetComponentLocation());
	}
	OnRecalled();
}

void AMP_ToolRecallButton::ApplyLook()
{
	const UPDA_Recovery* Data = GetRecoveryData();
	Label->SetText(FText::Format(FText::FromString(TEXT("RECALL\n{0}")), GetToolName().ToUpper()));

	//Colors in game only: an instance made in the editor would be saved in the level
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (!ButtonMaterial && ShapeMaterial)
	{
		ButtonMaterial = ButtonMesh->CreateDynamicMaterialInstance(0, ShapeMaterial);
		if (UMaterialInstanceDynamic* BaseMaterial = BaseMesh->CreateDynamicMaterialInstance(0, ShapeMaterial))
		{
			BaseMaterial->SetVectorParameterValue(TEXT("Color"), RecallPedestalColor);
		}
	}
	const float CooldownLeft = GetCooldownLeft();
	if (ButtonMaterial)
	{
		ButtonMaterial->SetVectorParameterValue(TEXT("Color"), CooldownLeft > 0.f ? Data->ButtonCooldownColor : Data->ButtonReadyColor);
	}

	//Green again when ready, on every machine
	if (CooldownLeft > 0.f)
	{
		GetWorldTimerManager().SetTimer(ReadyTimer, this, &AMP_ToolRecallButton::ApplyLook, CooldownLeft + 0.05f, false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(ReadyTimer);
	}
}
