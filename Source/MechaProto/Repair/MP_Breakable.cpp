#include "MP_Breakable.h"
#include "MechaProto.h"
#include "C_ItemHolder.h"
#include "MP_HUD.h"
#include "MP_Item.h"
#include "MP_RepairTool.h"
#include "PDA_Breakable.h"
#include "PDA_Item.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FName BrokenAlertKey()
	{
		return FName(TEXT("SystemBroken"));
	}

	const AMP_Item* GetHeldItem(const ACharacter* User)
	{
		const UC_ItemHolder* ItemHolder = User ? User->FindComponentByClass<UC_ItemHolder>() : nullptr;
		return ItemHolder ? ItemHolder->GetHeldItem() : nullptr;
	}
}

AMP_Breakable::AMP_Breakable()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	//Few actors, the state matters to everyone
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	SetCanBeDamaged(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	//Blocks like a wall, and Visibility for the interact trace
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	//Inside the mech: its own weapons fire through it
	Mesh->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(false);
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AMP_Breakable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Breakable, bBroken);
	DOREPLIFETIME(AMP_Breakable, RepairCount);
}

const UPDA_Breakable* AMP_Breakable::GetBreakableData() const
{
	if (ensureMsgf(BreakableData, TEXT("%s has no BreakableData, using code defaults"), *GetPathNameSafe(this)))
	{
		return BreakableData;
	}
	return GetDefault<UPDA_Breakable>();
}

void AMP_Breakable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (BreakableData && BreakableData->Material)
	{
		Mesh->SetMaterial(0, BreakableData->Material);
	}
}

void AMP_Breakable::BeginPlay()
{
	Super::BeginPlay();

	MeshBaseRotation = Mesh->GetRelativeRotation();
	ApplyData();
	//Late joiner: already broken
	if (bBroken)
	{
		OnRep_Broken();
	}
}

void AMP_Breakable::ApplyData()
{
	const UPDA_Breakable* Data = GetBreakableData();
	if (Data->Material)
	{
		Mesh->SetMaterial(0, Data->Material);
	}
	MaterialInstance = Mesh->CreateDynamicMaterialInstance(0);
	UpdateColor();
}

bool AMP_Breakable::HasRequiredTool(const ACharacter* User) const
{
	const AMP_Item* Item = GetHeldItem(User);
	const UPDA_Item* RequiredTool = GetBreakableData()->RequiredTool;
	return Item && RequiredTool && Item->GetItemData() == RequiredTool;
}

float AMP_Breakable::GetRepairPercent() const
{
	return bBroken ? FMath::Clamp(static_cast<float>(RepairCount) / FMath::Max(GetBreakableData()->RepairHits, 1), 0.f, 1.f) : 0.f;
}

bool AMP_Breakable::CanInteract(const ACharacter* User) const
{
	return bBroken;
}

void AMP_Breakable::Interact(ACharacter* User)
{
	//Same as using the tool: its cooldown applies, it finds this system from the user's view
	if (AMP_RepairTool* Tool = Cast<AMP_RepairTool>(const_cast<AMP_Item*>(GetHeldItem(User))))
	{
		Tool->Use(User);
	}
}

FText AMP_Breakable::GetInteractionText(const ACharacter* User) const
{
	const UPDA_Breakable* Data = GetBreakableData();
	if (HasRequiredTool(User))
	{
		return FText::Format(NSLOCTEXT("Repair", "Repair", "Repair {0} ({1}/{2})"), Data->DisplayName, RepairCount, Data->RepairHits);
	}
	const FText ToolName = Data->RequiredTool ? Data->RequiredTool->ItemName : NSLOCTEXT("Repair", "NoTool", "a tool");
	return FText::Format(NSLOCTEXT("Repair", "NeedsTool", "{0} broken - needs the {1}"), Data->DisplayName, ToolName);
}

void AMP_Breakable::Break()
{
	if (!HasAuthority() || bBroken)
	{
		return;
	}
	bBroken = true;
	RepairCount = 0;
	//OnRep doesn't run on the server
	OnRep_Broken();
	ForceNetUpdate();
}

void AMP_Breakable::Fix()
{
	if (!HasAuthority() || !bBroken)
	{
		return;
	}
	bBroken = false;
	RepairCount = 0;
	OnRep_Broken();
	ForceNetUpdate();
}

bool AMP_Breakable::RepairHit(ACharacter* User)
{
	if (!HasAuthority() || !bBroken || !HasRequiredTool(User))
	{
		return false;
	}

	const int32 OldRepairCount = RepairCount;
	++RepairCount;
	OnRep_RepairCount(OldRepairCount);
	if (RepairCount >= GetBreakableData()->RepairHits)
	{
		Fix();
	}
	ForceNetUpdate();
	return true;
}

void AMP_Breakable::OnRep_Broken()
{
	const UPDA_Breakable* Data = GetBreakableData();
	const bool bPlayEffects = GetNetMode() != NM_DedicatedServer && HasActorBegunPlay();

	if (bBroken)
	{
		if (bPlayEffects && Data->BreakSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Data->BreakSound, GetActorLocation());
		}
		if (bPlayEffects && Data->BrokenEffect && !BrokenEffectComponent)
		{
			BrokenEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(Data->BrokenEffect, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false);
		}
		AMP_HUD::RaiseAlert(this, MakeBrokenAlert());
	}
	else
	{
		if (BrokenEffectComponent)
		{
			BrokenEffectComponent->DestroyComponent();
			BrokenEffectComponent = nullptr;
		}
		if (bPlayEffects && Data->RepairedSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Data->RepairedSound, GetActorLocation());
		}
		AMP_HUD::ClearAlert(this, BrokenAlertKey(), this);
	}

	UpdateColor();
	OnBrokenChanged.Broadcast(this, bBroken);
}

void AMP_Breakable::OnRep_RepairCount(int32 OldRepairCount)
{
	//Reset to 0 on break/repair, only the hits count
	if (RepairCount <= OldRepairCount)
	{
		return;
	}

	const UPDA_Breakable* Data = GetBreakableData();
	HitFlashTimeLeft = 0.15f;
	if (Data->RepairHitSound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->RepairHitSound, GetActorLocation());
	}
	//The alert follows the progress
	if (bBroken)
	{
		AMP_HUD::RaiseAlert(this, MakeBrokenAlert());
	}
	OnRepairHit.Broadcast(this, RepairCount);
}

FMP_Alert AMP_Breakable::MakeBrokenAlert() const
{
	const UPDA_Breakable* Data = GetBreakableData();
	const FText ToolName = Data->RequiredTool ? Data->RequiredTool->ItemName : FText::GetEmpty();

	FMP_Alert Alert;
	Alert.Key = BrokenAlertKey();
	Alert.Source = const_cast<AMP_Breakable*>(this);
	Alert.Title = FText::Format(Data->BrokenAlertTitle, Data->DisplayName, ToolName, LocationName);
	Alert.Message = FText::Format(Data->BrokenAlertMessage, Data->DisplayName, ToolName, LocationName);
	Alert.Severity = EMP_AlertSeverity::Critical;
	Alert.Duration = 0.f;
	return Alert;
}

void AMP_Breakable::ShowCurrentAlerts(AMP_HUD& HUD)
{
	if (bBroken)
	{
		HUD.ShowAlert(MakeBrokenAlert());
	}
}

void AMP_Breakable::UpdateColor()
{
	if (!MaterialInstance)
	{
		return;
	}

	const UPDA_Breakable* Data = GetBreakableData();
	FLinearColor Color = Data->WorkingColor;
	if (bBroken)
	{
		const float Blink = Data->BrokenBlinkFrequency > 0.f ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * UE_TWO_PI * Data->BrokenBlinkFrequency) : 1.f;
		Color = FMath::Lerp(Data->WorkingColor, Data->BrokenColor, Blink);
	}
	if (HitFlashTimeLeft > 0.f)
	{
		Color = FMath::Lerp(Color, Data->RepairHitColor, HitFlashTimeLeft / 0.15f);
	}
	MaterialInstance->SetVectorParameterValue(Data->ColorParameter, Color);
}

void AMP_Breakable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UPDA_Breakable* Data = GetBreakableData();
	HitFlashTimeLeft = FMath::Max(0.f, HitFlashTimeLeft - DeltaSeconds);
	if (bBroken || HitFlashTimeLeft > 0.f)
	{
		UpdateColor();
	}

	//Cosmetic, each machine on its own
	if (!bBroken && Data->SpinSpeed != 0.f)
	{
		SpinAngle = FMath::Fmod(SpinAngle + Data->SpinSpeed * DeltaSeconds, 360.f);
		Mesh->SetRelativeRotation(FRotator(FQuat(MeshBaseRotation) * FQuat(FVector::UpVector, FMath::DegreesToRadians(SpinAngle))));
	}

	if (Data->bDrawDebugState && bBroken)
	{
		const FVector Top = Mesh->Bounds.Origin + FVector(0.f, 0.f, Mesh->Bounds.BoxExtent.Z + 40.f);
		DrawDebugString(GetWorld(), Top, FString::Printf(TEXT("%s BROKEN %d/%d"), *Data->DisplayName.ToString(), RepairCount, Data->RepairHits), nullptr, FColor::Red, 0.f, true, 1.2f);
	}
}
