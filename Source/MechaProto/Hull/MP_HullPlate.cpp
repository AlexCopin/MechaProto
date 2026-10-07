#include "MP_HullPlate.h"
#include "PDA_HullPlate.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

AMP_HullPlate::AMP_HullPlate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	//Few actors, the state matters to everyone
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.f);
	//Weapons and explosions never damage the mech
	SetCanBeDamaged(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	//A piece of the hull wall: blocks players, physics and projectiles like the rest of it
	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate Mesh"));
	PlateMesh->SetupAttachment(Root);
	PlateMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PlateMesh->SetCanEverAffectNavigation(false);
	if (CubeMesh.Succeeded())
	{
		PlateMesh->SetStaticMesh(CubeMesh.Object);
	}
	//Has the "Color" parameter
	if (ShapeMaterial.Succeeded())
	{
		PlateMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	OutsideArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Outside"));
	OutsideArrow->SetupAttachment(Root);
	OutsideArrow->ArrowSize = 1.5f;
}

void AMP_HullPlate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_HullPlate, Health);
	DOREPLIFETIME(AMP_HullPlate, bBroken);
}

const UPDA_HullPlate* AMP_HullPlate::GetPlateData() const
{
	if (ensureMsgf(PlateData, TEXT("%s has no PlateData, using code defaults"), *GetPathNameSafe(this)))
	{
		return PlateData;
	}
	return GetDefault<UPDA_HullPlate>();
}

void AMP_HullPlate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (PlateData)
	{
		ApplyData();
	}
	else
	{
		UpdateMeshSize();
	}
}

void AMP_HullPlate::BeginPlay()
{
	Super::BeginPlay();

	ApplyData();

	if (HasAuthority())
	{
		Health = GetPlateData()->MaxHealth;
		UpdateColor();
	}
}

void AMP_HullPlate::ApplyData()
{
	const UPDA_HullPlate* Data = GetPlateData();
	if (Data->Mesh)
	{
		PlateMesh->SetStaticMesh(Data->Mesh);
	}
	if (Data->Material)
	{
		PlateMesh->SetMaterial(0, Data->Material);
	}
	UpdateMeshSize();

	if (!IsTemplate())
	{
		MaterialInstance = PlateMesh->CreateDynamicMaterialInstance(0);
		UpdateColor();
	}
}

void AMP_HullPlate::UpdateMeshSize()
{
	OutsideArrow->SetRelativeLocation(FVector(Thickness * 0.5f, 0.f, Height * 0.5f));

	const UStaticMesh* StaticMesh = PlateMesh->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}
	//Any mesh: its box is stretched to the opening, bottom on the actor origin
	const FBox Box = StaticMesh->GetBoundingBox();
	const FVector Size = Box.GetSize().ComponentMax(FVector(1.f));
	const FVector Scale(Thickness / Size.X, Width / Size.Y, Height / Size.Z);
	PlateMesh->SetRelativeScale3D(Scale);
	PlateMesh->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f) - Box.GetCenter() * Scale);
}

FVector AMP_HullPlate::GetPointInFront(float Distance, float Up, float Side) const
{
	return GetActorLocation() + GetActorForwardVector() * Distance + GetActorRightVector() * Side + GetActorUpVector() * Up;
}

float AMP_HullPlate::GetWidth() const
{
	return Width * GetActorScale3D().Y;
}

float AMP_HullPlate::GetHeight() const
{
	return Height * GetActorScale3D().Z;
}

float AMP_HullPlate::GetThickness() const
{
	return Thickness * GetActorScale3D().X;
}

void AMP_HullPlate::ReceiveHullDamage(float Damage, AActor* Attacker)
{
	if (!HasAuthority() || bBroken || Damage <= 0.f)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Max(0.f, Health - Damage);
	//OnRep doesn't run on the server
	OnRep_Health(OldHealth);

	if (Health <= 0.f)
	{
		bBroken = true;
		OnRep_Broken();
	}
	ForceNetUpdate();
}

void AMP_HullPlate::Repair(float Amount)
{
	if (!HasAuthority() || Amount <= 0.f)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Min(GetPlateData()->MaxHealth, Health + Amount);
	OnRep_Health(OldHealth);

	if (bBroken && Health > 0.f)
	{
		bBroken = false;
		OnRep_Broken();
	}
	ForceNetUpdate();
}

void AMP_HullPlate::OnRep_Health(float OldHealth)
{
	if (Health < OldHealth)
	{
		HitFlashTimeLeft = GetPlateData()->HitFlashDuration;
	}
	UpdateColor();
	OnHealthChanged.Broadcast(this, Health);
}

void AMP_HullPlate::OnRep_Broken()
{
	PlateMesh->SetVisibility(!bBroken);
	PlateMesh->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	if (bBroken)
	{
		PlayBreakEffects();
	}
	OnBrokenChanged.Broadcast(this, bBroken);
}

void AMP_HullPlate::PlayBreakEffects()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UPDA_HullPlate* Data = GetPlateData();
	const FVector Center = GetPointInFront(0.f, GetHeight() * 0.5f, 0.f);
	if (Data->BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->BreakSound, Center);
	}
	if (Data->BreakEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Data->BreakEffect, Center, GetActorRotation());
	}
	if (Data->bDrawBreakDebug)
	{
		DrawDebugBox(GetWorld(), Center, FVector(GetThickness(), GetWidth(), GetHeight()) * 0.5f, GetActorQuat(), FColor::Orange, false, 1.f, 0, 4.f);
	}
}

void AMP_HullPlate::UpdateColor()
{
	if (!MaterialInstance)
	{
		return;
	}

	const UPDA_HullPlate* Data = GetPlateData();
	//0 health and not broken = not replicated yet
	const float HealthAlpha = Health > 0.f ? FMath::Clamp(Health / Data->MaxHealth, 0.f, 1.f) : (bBroken ? 0.f : 1.f);
	FLinearColor Color = FMath::Lerp(Data->DamagedColor, Data->IntactColor, HealthAlpha);
	if (HitFlashTimeLeft > 0.f && Data->HitFlashDuration > 0.f)
	{
		Color = FMath::Lerp(Color, Data->HitFlashColor, HitFlashTimeLeft / Data->HitFlashDuration);
	}
	MaterialInstance->SetVectorParameterValue(Data->ColorParameter, Color);
}

void AMP_HullPlate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HitFlashTimeLeft > 0.f)
	{
		HitFlashTimeLeft = FMath::Max(0.f, HitFlashTimeLeft - DeltaSeconds);
		UpdateColor();
	}

	const UPDA_HullPlate* Data = GetPlateData();
	if (Data->bDrawDebugHealth && (bBroken || (Health > 0.f && Health < Data->MaxHealth)))
	{
		DrawDebugString(GetWorld(), GetPointInFront(0.f, GetHeight() + 50.f, 0.f), bBroken ? FString(TEXT("HOLE")) : FString::Printf(TEXT("%.0f"), Health), nullptr, bBroken ? FColor::Red : FColor::White, 0.f, true, 1.3f);
	}
}
