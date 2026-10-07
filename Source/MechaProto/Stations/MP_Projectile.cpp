#include "MP_Projectile.h"
#include "MechaProto.h"
#include "MP_WeaponStation.h"
#include "PDA_WeaponStation.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	//Weapons are for enemies: players are never hit nor pushed
	bool IsPlayer(const AActor* Actor)
	{
		const APawn* Pawn = Cast<APawn>(Actor);
		return Pawn && Pawn->IsPlayerControlled();
	}
}

AMP_Projectile::AMP_Projectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(10.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile Movement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AMP_Projectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AMP_Projectile, Data, COND_InitialOnly);
	DOREPLIFETIME(AMP_Projectile, ExplosionLocation);
}

void AMP_Projectile::InitProjectile(UPDA_WeaponStation* InData)
{
	Data = InData;
	ApplyData();
	if (Data)
	{
		SetLifeSpan(Data->ProjectileLifeSpan);
	}
}

void AMP_Projectile::BeginPlay()
{
	Super::BeginPlay();

	ApplyData();

	//Never hit the shooter or its own station
	Collision->IgnoreActorWhenMoving(GetInstigator(), true);
	Collision->IgnoreActorWhenMoving(GetOwner(), true);

	//Clients spawn it after the movement component initialized, start it at full speed along the spawn rotation
	if (!HasAuthority() && Data)
	{
		ProjectileMovement->Velocity = GetActorForwardVector() * Data->ProjectileSpeed;
	}
}

void AMP_Projectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (!bExploded)
	{
		ProjectileMovement->Velocity = NewVelocity;
	}
}

void AMP_Projectile::ApplyData()
{
	if (!Data)
	{
		return;
	}

	Collision->SetSphereRadius(Data->ProjectileRadius);
	Mesh->SetStaticMesh(Data->ProjectileMesh);
	Mesh->SetRelativeScale3D(Data->ProjectileMeshScale);
	ProjectileMovement->InitialSpeed = Data->ProjectileSpeed;
	ProjectileMovement->MaxSpeed = Data->ProjectileSpeed;
	ProjectileMovement->ProjectileGravityScale = Data->ProjectileGravityScale;
}

void AMP_Projectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	//Clients only show the flight, the server resolves the hit
	if (bHit || !HasAuthority() || !Data)
	{
		return;
	}
	bHit = true;

	if (Data->ExplosionRadius > 0.f)
	{
		Explode(Hit.ImpactPoint);
		return;
	}

	if (OtherComp && OtherComp->IsSimulatingPhysics() && !IsPlayer(Other))
	{
		OtherComp->AddImpulseAtLocation(GetVelocity().GetSafeNormal() * Data->HitImpulse * OtherComp->GetMass(), Hit.ImpactPoint);
	}
	Destroy();
}

void AMP_Projectile::Explode(const FVector& Center)
{
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Data->ExplosionRadius));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->IsSimulatingPhysics() && !IsPlayer(Overlap.GetActor()))
		{
			Component->AddRadialImpulse(Center, Data->ExplosionRadius, Data->ExplosionImpulse, RIF_Linear, true);
		}
	}

	//Replicated to the clients, the server plays its own (OnRep doesn't run here)
	ExplosionLocation = Center;
	PlayExplosionEffects();
	Freeze();
	SetLifeSpan(1.f);
	ForceNetUpdate();
}

void AMP_Projectile::OnRep_ExplosionLocation()
{
	PlayExplosionEffects();
	Freeze();
}

void AMP_Projectile::PlayExplosionEffects()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	if (!Data || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (Data->ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Data->ExplosionSound, ExplosionLocation);
	}
	if (Data->ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Data->ExplosionEffect, ExplosionLocation);
	}
	if (Data->bDrawExplosionDebug)
	{
		DrawDebugSphere(GetWorld(), ExplosionLocation, Data->ExplosionRadius, 16, FColor::Orange, false, 0.6f);
	}
	if (AMP_WeaponStation* Station = Cast<AMP_WeaponStation>(GetOwner()))
	{
		Station->OnExploded(ExplosionLocation, Data->ExplosionRadius);
	}
}

void AMP_Projectile::Freeze()
{
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->SetUpdatedComponent(nullptr);
	SetReplicateMovement(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);
	SetActorLocation(ExplosionLocation);
}
