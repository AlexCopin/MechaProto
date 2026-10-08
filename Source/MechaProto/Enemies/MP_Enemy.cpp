#include "MP_Enemy.h"
#include "MechaProto.h"
#include "PDA_Enemy.h"
#include "C_Ragdoll.h"
#include "MP_HullPlate.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float GroundCheckInterval = 0.25f;
	constexpr float NetSmoothSpeed = 10.f;
	constexpr float ThinkInterval = 0.5f;
	//Keeps its plate unless another one is clearly better
	constexpr float KeepTargetBonus = 300.f;
	//Contact with a plate, the spot is already against it
	constexpr float PlateStopDistance = 10.f;
	//How far past the hole it goes before hunting
	constexpr float EnterDepth = 150.f;
	//Ground trace start above the capsule top, low enough to stay under an upper floor inside the mech
	constexpr float GroundTraceUp = 150.f;
}

AMP_Enemy::AMP_Enemy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	//Big open map, the stations shoot far
	SetNetCullDistanceSquared(FMath::Square(40000.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitCapsuleSize(40.f, 40.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Projectile, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCanEverAffectNavigation(false);
	Collision->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
}

void AMP_Enemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Enemy, Health);
	DOREPLIFETIME(AMP_Enemy, bDead);
	DOREPLIFETIME(AMP_Enemy, DeathVelocity);
	DOREPLIFETIME(AMP_Enemy, DeathSpin);
}

const UPDA_Enemy* AMP_Enemy::GetEnemyData() const
{
	if (ensureMsgf(EnemyData, TEXT("%s has no EnemyData, using code defaults"), *GetPathNameSafe(this)))
	{
		return EnemyData;
	}
	return GetDefault<UPDA_Enemy>();
}

void AMP_Enemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (EnemyData)
	{
		ApplyData();
	}
}

void AMP_Enemy::BeginPlay()
{
	Super::BeginPlay();

	ApplyData();

	if (HasAuthority())
	{
		const UPDA_Enemy* Data = GetEnemyData();
		Health = Data->MaxHealth;
		SpeedScale = 1.f + FMath::FRandRange(-Data->SpeedVariation, Data->SpeedVariation);
		WeavePhase = FMath::FRandRange(0.f, UE_TWO_PI);
		NextGroundCheckTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, GroundCheckInterval);
		SnapToGround();
	}
	else
	{
		NetLocation = GetActorLocation();
		NetRotation = GetActorRotation();
	}
}

void AMP_Enemy::ApplyData()
{
	const UPDA_Enemy* Data = GetEnemyData();

	Collision->SetCapsuleSize(Data->CapsuleRadius, FMath::Max(Data->CapsuleHalfHeight, Data->CapsuleRadius));
	Collision->SetCollisionResponseToChannel(ECC_Pawn, Data->bBlockPlayers ? ECR_Block : ECR_Ignore);

	if (Data->Mesh)
	{
		Mesh->SetStaticMesh(Data->Mesh);
	}
	Mesh->SetRelativeScale3D(Data->MeshScale);
	Mesh->SetRelativeLocation(Data->MeshOffset);

	if (Data->Material)
	{
		Mesh->SetMaterial(0, Data->Material);
	}
	if (!IsTemplate())
	{
		MaterialInstance = Mesh->CreateDynamicMaterialInstance(0);
		if (MaterialInstance)
		{
			MaterialInstance->SetVectorParameterValue(Data->ColorParameter, Data->Color);
		}
	}

	if (HasAuthority())
	{
		SetNetUpdateFrequency(Data->NetUpdateFrequency);
	}
}

void AMP_Enemy::SetGoal(const FVector& GoalCenter)
{
	const FVector2D Offset = FMath::RandPointInCircle(GetEnemyData()->GoalRadius);
	GoalLocation = GoalCenter + FVector(Offset.X, Offset.Y, 0.f);
	bHasGoal = true;
	bReachedGoal = false;
}

void AMP_Enemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		UpdateCorpse();
		return;
	}

	if (HasAuthority())
	{
		UpdateBehavior(DeltaSeconds);
	}
	else
	{
		SmoothToNetLocation(DeltaSeconds);
	}
	UpdateVisuals(DeltaSeconds);
}

void AMP_Enemy::UpdateBehavior(float DeltaSeconds)
{
	if (State == EMP_EnemyState::None || GetWorld()->GetTimeSeconds() >= NextThinkTime)
	{
		Think();
	}

	switch (State)
	{
	case EMP_EnemyState::ToGoal:
		if (MoveTo(GoalLocation, GetEnemyData()->MoveSpeed * SpeedScale, 0.f, false, DeltaSeconds))
		{
			bReachedGoal = true;
			State = EMP_EnemyState::Idle;
			OnReachedGoal();
		}
		break;
	case EMP_EnemyState::ToPlate:
		UpdateToPlate(DeltaSeconds);
		break;
	case EMP_EnemyState::Entering:
		UpdateEntering(DeltaSeconds);
		break;
	case EMP_EnemyState::Hunting:
		UpdateHunting(DeltaSeconds);
		break;
	default:
		Collision->ComponentVelocity = FVector::ZeroVector;
		break;
	}
}

void AMP_Enemy::Think()
{
	NextThinkTime = GetWorld()->GetTimeSeconds() + ThinkInterval * FMath::FRandRange(0.8f, 1.2f);

	switch (State)
	{
	case EMP_EnemyState::Entering:
		return;
	case EMP_EnemyState::Hunting:
		TargetPlayer = FindClosestPlayer();
		return;
	default:
		break;
	}

	//Outside: the best plate, else the spawner's goal when the level has no plate at all
	bool bAnyPlate = false;
	if (GetEnemyData()->bAttackHull)
	{
		if (AMP_HullPlate* Plate = FindBestPlate(bAnyPlate))
		{
			SetTargetPlate(Plate);
			State = EMP_EnemyState::ToPlate;
			return;
		}
	}
	SetTargetPlate(nullptr);
	State = !bAnyPlate && bHasGoal && !bReachedGoal ? EMP_EnemyState::ToGoal : EMP_EnemyState::Idle;
}

AMP_HullPlate* AMP_Enemy::FindBestPlate(bool& bOutAnyPlate) const
{
	const UPDA_Enemy* Data = GetEnemyData();
	const FVector Location = GetActorLocation();
	AMP_HullPlate* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	bool bBestFacing = false;
	bOutAnyPlate = false;

	for (TActorIterator<AMP_HullPlate> It(GetWorld()); It; ++It)
	{
		AMP_HullPlate* Plate = *It;
		bOutAnyPlate = true;
		if (Plate->IsBroken() && !Data->bEnterHoles)
		{
			continue;
		}

		const FVector ToEnemy = Location - Plate->GetActorLocation();
		//Plates facing it first: enemies don't collide with the hull, the straight line to those never crosses it
		const bool bFacing = FVector::DotProduct(ToEnemy, Plate->GetOutsideDirection()) > 0.f;
		float Score = ToEnemy.Size();
		if (Plate->IsBroken())
		{
			Score -= Data->HoleAttraction;
		}
		if (Plate == TargetPlate.Get())
		{
			Score -= KeepTargetBonus;
		}

		if ((bFacing && !bBestFacing) || (bFacing == bBestFacing && Score < BestScore))
		{
			Best = Plate;
			BestScore = Score;
			bBestFacing = bFacing;
		}
	}
	return Best;
}

APawn* AMP_Enemy::FindClosestPlayer() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	const FVector Location = GetActorLocation();
	APawn* Closest = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(Location, Pawn->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			Closest = Pawn;
			ClosestDistanceSquared = DistanceSquared;
		}
	}
	return Closest;
}

void AMP_Enemy::SetTargetPlate(AMP_HullPlate* Plate)
{
	if (TargetPlate.Get() == Plate)
	{
		return;
	}

	TargetPlate = Plate;
	if (Plate)
	{
		//Its own spot along the plate, the body within the opening
		const float SideRoom = FMath::Max(0.f, Plate->GetWidth() * 0.5f - Collision->GetScaledCapsuleRadius());
		PlateSide = FMath::FRandRange(-SideRoom, SideRoom);
	}
}

void AMP_Enemy::UpdateToPlate(float DeltaSeconds)
{
	AMP_HullPlate* Plate = TargetPlate.Get();
	if (!Plate)
	{
		State = EMP_EnemyState::None;
		return;
	}

	const UPDA_Enemy* Data = GetEnemyData();
	//Against the outside face, at its walking height above the bottom of the opening
	const FVector Spot = Plate->GetPointInFront(Plate->GetThickness() * 0.5f + Collision->GetScaledCapsuleRadius(), Collision->GetScaledCapsuleHalfHeight(), PlateSide);
	if (!MoveTo(Spot, Data->MoveSpeed * SpeedScale, PlateStopDistance, true, DeltaSeconds))
	{
		return;
	}

	if (Plate->IsBroken())
	{
		if (Data->bEnterHoles)
		{
			State = EMP_EnemyState::Entering;
		}
		else
		{
			//Next intact plate
			Think();
		}
		return;
	}

	if (Data->bKamikaze)
	{
		Plate->ReceiveHullDamage(Data->HullDamage, this);
		Crash(Plate->GetOutsideDirection());
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextAttackTime)
	{
		NextAttackTime = Now + Data->AttackInterval;
		Plate->ReceiveHullDamage(Data->HullDamage, this);
	}
}

void AMP_Enemy::UpdateEntering(float DeltaSeconds)
{
	if (const AMP_HullPlate* Plate = TargetPlate.Get())
	{
		const FVector Inside = Plate->GetPointInFront(-(Plate->GetThickness() * 0.5f + Collision->GetScaledCapsuleRadius() + EnterDepth), Collision->GetScaledCapsuleHalfHeight(), PlateSide);
		if (!MoveTo(Inside, GetEnemyData()->MoveSpeed * SpeedScale, PlateStopDistance, true, DeltaSeconds))
		{
			return;
		}
	}

	SetTargetPlate(nullptr);
	State = EMP_EnemyState::Hunting;
	TargetPlayer = FindClosestPlayer();
	SnapToGround();
}

void AMP_Enemy::UpdateHunting(float DeltaSeconds)
{
	APawn* Player = TargetPlayer.Get();
	if (!Player)
	{
		Collision->ComponentVelocity = FVector::ZeroVector;
		return;
	}

	const UPDA_Enemy* Data = GetEnemyData();
	const float Reach = Collision->GetScaledCapsuleRadius() + Player->GetSimpleCollisionRadius() + Data->PlayerReach;
	if (MoveTo(Player->GetActorLocation(), Data->HuntSpeed * SpeedScale, Reach, true, DeltaSeconds))
	{
		HitPlayer(Player);
	}
}

void AMP_Enemy::HitPlayer(APawn* Player)
{
	const UPDA_Enemy* Data = GetEnemyData();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Data->bKamikaze && Now < NextAttackTime)
	{
		return;
	}
	NextAttackTime = Now + Data->AttackInterval;

	const FVector Direction = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (Data->bSlapPlayers)
	{
		if (UC_Ragdoll* Ragdoll = Player->FindComponentByClass<UC_Ragdoll>())
		{
			Ragdoll->ReceiveSlap(this, Direction, Data->SlapStrength, Data->SlapValue);
		}
	}
	if (Data->bKamikaze)
	{
		Crash(-Direction);
	}
}

void AMP_Enemy::Crash(const FVector& BounceDirection)
{
	LastDamageDirection = BounceDirection;
	bLastDamageWasExplosion = false;
	Kill();
}

bool AMP_Enemy::MoveTo(const FVector& Target, float Speed, float StopDistance, bool bCanLeap, float DeltaSeconds)
{
	const UPDA_Enemy* Data = GetEnemyData();
	const FVector Location = GetActorLocation();
	const FVector Delta = Target - Location;
	//Last stretch straight at the target, off the ground
	const bool bLeap = bCanLeap && Data->LeapDistance > 0.f && Delta.SizeSquared2D() <= FMath::Square(Data->LeapDistance);
	const FVector ToTarget = bLeap ? Delta : FVector(Delta.X, Delta.Y, 0.f);
	const float Distance = ToTarget.Size();
	if (Distance <= FMath::Max(StopDistance, 1.f))
	{
		Collision->ComponentVelocity = FVector::ZeroVector;
		return true;
	}

	FVector Direction = ToTarget / Distance;
	if (!bLeap && Data->WeaveAngle > 0.f)
	{
		//Straightens up close to the target so it doesn't circle around it
		const float Weave = FMath::Sin(GetWorld()->GetTimeSeconds() * UE_TWO_PI * Data->WeaveFrequency + WeavePhase) * Data->WeaveAngle * FMath::Clamp(Distance / 1500.f, 0.f, 1.f);
		Direction = Direction.RotateAngleAxis(Weave, FVector::UpVector);
	}

	const FVector Velocity = Direction * Speed;
	const float Step = FMath::Min(Speed * DeltaSeconds, Distance - StopDistance);
	const FRotator Rotation = Velocity.SizeSquared2D() > 1.f ? FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Velocity.Rotation().Yaw, 0.f), DeltaSeconds, Data->TurnSpeed) : GetActorRotation();
	SetActorLocationAndRotation(Location + Direction * Step, Rotation);
	//Replicated with the movement, clients extrapolate with it
	Collision->ComponentVelocity = Velocity;

	const float Now = GetWorld()->GetTimeSeconds();
	if (!bLeap && Now >= NextGroundCheckTime)
	{
		NextGroundCheckTime = Now + GroundCheckInterval;
		SnapToGround();
	}
	return false;
}

void AMP_Enemy::SnapToGround()
{
	const FVector Location = GetActorLocation();
	const float HalfHeight = Collision->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyGround), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, HalfHeight + GroundTraceUp), Location - FVector(0.f, 0.f, 3000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SetActorLocation(FVector(Location.X, Location.Y, Hit.ImpactPoint.Z + HalfHeight));
	}
}

void AMP_Enemy::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	NetVelocity = NewVelocity;
}

void AMP_Enemy::PostNetReceiveLocationAndRotation()
{
	//No snap: Tick blends toward it and extrapolates with the velocity until the next update
	NetLocation = FRepMovement::RebaseOntoLocalOrigin(GetReplicatedMovement().Location, this);
	NetRotation = GetReplicatedMovement().Rotation;
}

void AMP_Enemy::SmoothToNetLocation(float DeltaSeconds)
{
	NetLocation += NetVelocity * DeltaSeconds;
	SetActorLocationAndRotation(
		FMath::VInterpTo(GetActorLocation(), NetLocation, DeltaSeconds, NetSmoothSpeed),
		FMath::RInterpTo(GetActorRotation(), NetRotation, DeltaSeconds, NetSmoothSpeed));
	Collision->ComponentVelocity = NetVelocity;
}

void AMP_Enemy::UpdateVisuals(float DeltaSeconds)
{
	const UPDA_Enemy* Data = GetEnemyData();

	if (HitFlashTimeLeft > 0.f && MaterialInstance)
	{
		HitFlashTimeLeft = FMath::Max(0.f, HitFlashTimeLeft - DeltaSeconds);
		const float Alpha = Data->HitFlashDuration > 0.f ? HitFlashTimeLeft / Data->HitFlashDuration : 0.f;
		MaterialInstance->SetVectorParameterValue(Data->ColorParameter, FMath::Lerp(Data->Color, Data->HitFlashColor, Alpha));
	}

	float Hop = 0.f;
	if (Data->HopHeight > 0.f && GetVelocity().SizeSquared2D() > 100.f)
	{
		HopTime += DeltaSeconds;
		Hop = FMath::Abs(FMath::Sin(HopTime * UE_PI * Data->HopFrequency)) * Data->HopHeight;
	}
	Mesh->SetRelativeLocation(Data->MeshOffset + FVector(0.f, 0.f, Hop));

	if (Data->bDrawDebugHealth)
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, Collision->GetScaledCapsuleHalfHeight() + 60.f), FString::Printf(TEXT("%.0f"), Health), nullptr, FColor::White, 0.f, true, 1.5f);
	}
}

float AMP_Enemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	//Bullets push along the shot, explosions (radial, no shot direction) away from the projectile
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		LastDamageDirection = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
		bLastDamageWasExplosion = false;
	}
	else if (DamageCauser)
	{
		LastDamageDirection = GetActorLocation() - DamageCauser->GetActorLocation();
		bLastDamageWasExplosion = true;
	}

	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const float OldHealth = Health;
	Health = FMath::Max(0.f, Health - Damage);
	//OnRep doesn't run on the server
	OnRep_Health(OldHealth);

	if (Health <= 0.f)
	{
		Kill();
	}
	return OldHealth - Health;
}

void AMP_Enemy::Kill()
{
	if (!HasAuthority() || bDead)
	{
		return;
	}

	bDead = true;
	Health = 0.f;
	Collision->ComponentVelocity = FVector::ZeroVector;
	ComputeDeathLaunch();
	//The actor stays put, only the thrown body moves (on each machine)
	SetReplicateMovement(false);
	PlayDeath();
	SetLifeSpan(GetEnemyData()->CorpseLifeSpan);
	ForceNetUpdate();
}

void AMP_Enemy::OnRep_Health(float OldHealth)
{
	if (Health < OldHealth)
	{
		HitFlashTimeLeft = GetEnemyData()->HitFlashDuration;
	}
	OnHealthChanged.Broadcast(this, Health);
}

void AMP_Enemy::OnRep_Dead()
{
	if (bDead)
	{
		PlayDeath();
	}
}

void AMP_Enemy::PlayDeath()
{
	if (bDeathPlayed)
	{
		return;
	}
	bDeathPlayed = true;

	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeathTime = GetWorld()->GetTimeSeconds();
	LaunchCorpse();

	if (GetNetMode() != NM_DedicatedServer)
	{
		const UPDA_Enemy* Data = GetEnemyData();
		const FVector Location = GetActorLocation();
		if (Data->DeathSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Data->DeathSound, Location);
		}
		if (Data->DeathEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Data->DeathEffect, Location);
		}
		if (Data->bDrawDeathDebug)
		{
			DrawDebugSphere(GetWorld(), Location, Collision->GetScaledCapsuleRadius() * 1.5f, 8, Data->Color.ToFColor(true), false, 0.3f);
		}
	}

	ReceiveDied();
	OnDied.Broadcast(this);
}

void AMP_Enemy::ComputeDeathLaunch()
{
	const UPDA_Enemy* Data = GetEnemyData();
	FVector Away = FVector(LastDamageDirection.X, LastDamageDirection.Y, 0.f).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		//Killed without a hit (KillAllEnemies...)
		const FVector2D Random = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal();
		Away = FVector(Random.X, Random.Y, 0.f);
	}

	const float Speed = bLastDamageWasExplosion ? Data->DeathExplosionLaunchSpeed : Data->DeathLaunchSpeed;
	const float UpSpeed = bLastDamageWasExplosion ? Data->DeathExplosionLaunchUpSpeed : Data->DeathLaunchUpSpeed;
	DeathVelocity = Away * Speed * FMath::FRandRange(0.8f, 1.2f) + FVector::UpVector * UpSpeed * FMath::FRandRange(0.8f, 1.2f);
	DeathSpin = FMath::VRand() * Data->DeathSpinSpeed;
}

void AMP_Enemy::LaunchCorpse()
{
	//Thrown on every machine from the same velocity, not replicated after that (cosmetic)
	Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(DeathVelocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(DeathSpin);
}

void AMP_Enemy::UpdateCorpse()
{
	const UPDA_Enemy* Data = GetEnemyData();
	const float ShrinkStart = Data->CorpseLifeSpan - Data->CorpseShrinkTime;
	const float Elapsed = GetWorld()->GetTimeSeconds() - DeathTime;
	if (Elapsed <= ShrinkStart || !Mesh->IsVisible())
	{
		return;
	}

	const float Alpha = Data->CorpseShrinkTime > 0.f ? 1.f - (Elapsed - ShrinkStart) / Data->CorpseShrinkTime : 0.f;
	if (Alpha <= 0.01f)
	{
		Mesh->SetVisibility(false);
		Mesh->SetSimulatePhysics(false);
		return;
	}
	Mesh->SetWorldScale3D(Data->MeshScale * GetActorScale3D() * Alpha);
}

