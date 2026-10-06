#include "MP_Projectile.h"
#include "C_Ragdoll.h"
#include "MP_WeaponStation.h"
#include "PDA_WeaponStation.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

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
	ProjectileMovement->Velocity = NewVelocity;
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

	AMP_WeaponStation* Station = Cast<AMP_WeaponStation>(GetOwner());
	if (Data->ExplosionRadius > 0.f && Station)
	{
		Station->Explode(Hit.ImpactPoint);
	}
	else
	{
		const FVector Direction = GetVelocity().GetSafeNormal();
		if (OtherComp && OtherComp->IsSimulatingPhysics())
		{
			OtherComp->AddImpulseAtLocation(Direction * Data->HitImpulse * OtherComp->GetMass(), Hit.ImpactPoint);
		}

		UC_Ragdoll* Ragdoll = Other ? Other->FindComponentByClass<UC_Ragdoll>() : nullptr;
		if (Ragdoll && Data->bHitCountsAsSlap)
		{
			Ragdoll->ReceiveSlap(GetInstigator(), Direction);
		}
	}

	Destroy();
}
