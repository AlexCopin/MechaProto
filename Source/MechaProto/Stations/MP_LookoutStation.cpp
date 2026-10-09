#include "MP_LookoutStation.h"
#include "MechaProto.h"
#include "MP_Enemy.h"
#include "PDA_LookoutStation.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float MarkInterval = 0.1f;
	//Data edits during PIE show within this
	constexpr float ApplyInterval = 0.5f;
}

AMP_LookoutStation::AMP_LookoutStation()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	//A small console in front of the seat instead of a pedestal under it
	BaseMesh->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
	BaseMesh->SetRelativeScale3D(FVector(0.4f, 0.9f, 0.9f));

	Lamp = CreateDefaultSubobject<USceneComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Root);
	Lamp->SetUsingAbsoluteRotation(true);

	//Drum along the beam
	LampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lamp Mesh"));
	LampMesh->SetupAttachment(Lamp);
	LampMesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	LampMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));
	LampMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CylinderMesh.Succeeded())
	{
		LampMesh->SetStaticMesh(CylinderMesh.Object);
	}

	BeamLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam Light"));
	BeamLight->SetupAttachment(Lamp);
	BeamLight->SetRelativeLocation(FVector(35.f, 0.f, 0.f));
	BeamLight->SetMobility(EComponentMobility::Movable);
	BeamLight->SetIntensityUnits(ELightUnits::Candelas);
	BeamLight->SetCastShadows(true);

	//The engine cone points its apex up (+Z): pitched back so the apex is at the lamp and the base far along the beam
	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam Mesh"));
	BeamMesh->SetupAttachment(Lamp);
	BeamMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);
	BeamMesh->SetCanEverAffectNavigation(false);
	BeamMesh->bAffectDistanceFieldLighting = false;
	if (ConeMesh.Succeeded())
	{
		BeamMesh->SetStaticMesh(ConeMesh.Object);
	}
}

void AMP_LookoutStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_LookoutStation, ReplicatedAim);
}

const UPDA_Station* AMP_LookoutStation::GetBaseStationData() const
{
	return GetLookoutData();
}

const UPDA_LookoutStation* AMP_LookoutStation::GetLookoutData() const
{
	if (ensureMsgf(StationData, TEXT("%s has no StationData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StationData;
	}
	return GetDefault<UPDA_LookoutStation>();
}

void AMP_LookoutStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Lamp->SetRelativeLocation(LampOffset);
	Lamp->SetWorldRotation(FRotator(GetLookoutData()->IdlePitch, GetActorRotation().Yaw, 0.f));
	ApplyBeam();
}

void AMP_LookoutStation::BeginPlay()
{
	Super::BeginPlay();

	BeamPitch = GetLookoutData()->IdlePitch;
	ApplyBeam();
}

void AMP_LookoutStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMarks();
	Super::EndPlay(EndPlayReason);
}

void AMP_LookoutStation::ApplyBeam()
{
	const UPDA_LookoutStation* Data = GetLookoutData();
	BeamLight->SetIntensity(Data->LightIntensity);
	BeamLight->SetLightColor(Data->LightColor);
	BeamLight->SetAttenuationRadius(Data->BeamLength);
	BeamLight->SetOuterConeAngle(Data->BeamAngle);
	BeamLight->SetInnerConeAngle(Data->BeamAngle * 0.6f);
	ApplyBeamShape();
	BeamMesh->SetVisibility(Data->BeamMaterial != nullptr);
	if (Data->BeamMaterial)
	{
		//Reuses the instance already made from it
		if (UMaterialInstanceDynamic* Material = BeamMesh->CreateDynamicMaterialInstance(0, Data->BeamMaterial))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Data->BeamColor);
			Material->SetScalarParameterValue(TEXT("Intensity"), Data->BeamBrightness);
		}
	}
}

void AMP_LookoutStation::ApplyBeamShape()
{
	//As wide as the light where it stops
	const UPDA_LookoutStation* Data = GetLookoutData();
	const float Length = BeamReach > 0.f ? FMath::Min(BeamReach, Data->BeamLength) : Data->BeamLength;
	const float Radius = Length * FMath::Tan(FMath::DegreesToRadians(Data->BeamAngle));
	BeamMesh->SetRelativeLocation(FVector(Length * 0.5f, 0.f, 0.f));
	BeamMesh->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, Length / 100.f));
}

bool AMP_LookoutStation::TraceObstacle(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LookoutBeam), false, this);
	return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

FVector AMP_LookoutStation::GetCameraPivotLocation() const
{
	//From the lamp or the head's top, the offset turning with the head (Z straight up)
	const UPDA_LookoutStation* Data = GetLookoutData();
	const bool bAtLamp = Data->View == EMP_LookoutView::Head;
	const FVector Anchor = bAtLamp ? Lamp->GetComponentLocation() : Root->GetComponentTransform().TransformPosition(HeadTopOffset);
	const FVector Offset = bAtLamp ? Data->HeadViewOffset : Data->HeadTopViewOffset;
	return Anchor + FRotator(0.f, GetActorRotation().Yaw, 0.f).RotateVector(FVector(Offset.X, Offset.Y, 0.f)) + FVector(0.f, 0.f, Offset.Z);
}

float AMP_LookoutStation::GetCameraDistance() const
{
	const UPDA_LookoutStation* Data = GetLookoutData();
	return Data->View == EMP_LookoutView::Orbit ? Data->OrbitDistance : 0.f;
}

FVector AMP_LookoutStation::ComputeAimPoint() const
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * GetLookoutData()->BeamLength;

	//The projectile channel: through the mech's own hull, stops on enemies and the ground
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LookoutAim), false, this);
	Params.AddIgnoredActor(User);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Projectile, Params) ? Hit.ImpactPoint : End;
}

FRotator AMP_LookoutStation::GetAimRotation() const
{
	return IsLocallyUsed() ? UserAim : ReplicatedAim;
}

void AMP_LookoutStation::AimAt(const FVector& AimPoint)
{
	UserAim = (AimPoint - Lamp->GetComponentLocation()).Rotation();
	if (HasAuthority())
	{
		ReplicatedAim = UserAim;
	}
}

void AMP_LookoutStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UPDA_LookoutStation* Data = GetLookoutData();
	Lamp->SetRelativeLocation(LampOffset);

	//Out of the head's front (the mech turns the head), tilted toward the aim: at once on the user's machine, smoothed for the others
	const float TargetPitch = User ? FMath::Clamp(FRotator::NormalizeAxis(GetAimRotation().Pitch), Data->MinPitch, Data->MaxPitch) : Data->IdlePitch;
	BeamPitch = IsLocallyUsed() ? TargetPitch : FMath::FInterpConstantTo(BeamPitch, TargetPitch, DeltaSeconds, Data->BeamTurnSpeed);
	Lamp->SetWorldRotation(FRotator(BeamPitch, GetActorRotation().Yaw, 0.f));
	BeamMesh->SetHiddenInGame(Data->bHideBeamConeForUser && IsLocallyUsed());

	//The cone stops on the first wall or ground it meets, like the light's shadows
	const FVector LampLocation = Lamp->GetComponentLocation();
	FHitResult Hit;
	BeamReach = TraceObstacle(LampLocation, LampLocation + Lamp->GetForwardVector() * Data->BeamLength, Hit) ? FMath::Max(Hit.Distance, 1.f) : Data->BeamLength;
	ApplyBeamShape();

	ApplyTimer -= DeltaSeconds;
	if (ApplyTimer <= 0.f)
	{
		ApplyTimer = ApplyInterval;
		ApplyBeam();
	}
	MarkTimer -= DeltaSeconds;
	if (MarkTimer <= 0.f)
	{
		MarkTimer = MarkInterval;
		UpdateMarks();
	}
}

void AMP_LookoutStation::UpdateMarks()
{
	const UPDA_LookoutStation* Data = GetLookoutData();
	if (GetNetMode() == NM_DedicatedServer || !Data->MarkOverlayMaterial || (Data->bMarkOnlyWhenManned && !User))
	{
		ClearMarks();
		return;
	}

	//Living enemies within the cone and the beam's reach
	const FVector Origin = Lamp->GetComponentLocation();
	const FVector Axis = Lamp->GetForwardVector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(Data->MarkAngle));
	TArray<TWeakObjectPtr<UMeshComponent>> InBeam;
	for (TActorIterator<AMP_Enemy> It(GetWorld()); It; ++It)
	{
		if (It->IsDead())
		{
			continue;
		}
		const FVector ToEnemy = It->GetActorLocation() - Origin;
		const float Distance = ToEnemy.Size();
		if (Distance < 1.f || Distance > Data->BeamLength || FVector::DotProduct(ToEnemy / Distance, Axis) < MinDot)
		{
			continue;
		}
		//Lit only when nothing stands between it and the lamp
		FHitResult Hit;
		if (TraceObstacle(Origin, It->GetActorLocation(), Hit))
		{
			continue;
		}
		if (UStaticMeshComponent* Mesh = It->FindComponentByClass<UStaticMeshComponent>())
		{
			InBeam.Add(Mesh);
		}
	}

	for (const TWeakObjectPtr<UMeshComponent>& Mesh : Marked)
	{
		if (Mesh.IsValid() && !InBeam.Contains(Mesh))
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	}
	for (const TWeakObjectPtr<UMeshComponent>& Mesh : InBeam)
	{
		if (!Marked.Contains(Mesh))
		{
			Mesh->SetOverlayMaterial(Data->MarkOverlayMaterial);
		}
	}
	Marked = MoveTemp(InBeam);
}

void AMP_LookoutStation::ClearMarks()
{
	for (const TWeakObjectPtr<UMeshComponent>& Mesh : Marked)
	{
		if (Mesh.IsValid())
		{
			Mesh->SetOverlayMaterial(nullptr);
		}
	}
	Marked.Reset();
}
