#include "C_Ragdoll.h"
#include "PDA_Interaction.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

UC_Ragdoll::UC_Ragdoll()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UC_Ragdoll::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UC_Ragdoll, SlapCount);
	DOREPLIFETIME(UC_Ragdoll, RagdollState);
}

void UC_Ragdoll::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Server keeps the capsule on the body so hits, replication and get up follow it
	if (bRagdollApplied && GetOwner()->HasAuthority())
	{
		GetOwner()->SetActorLocation(GetPelvisLocation());
	}
}

void UC_Ragdoll::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlapResetTimer);
		World->GetTimerManager().ClearTimer(GetUpTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UC_Ragdoll::ReceiveSlap(AActor* Slapper, const FVector& Direction, float Strength, float Value)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const UPDA_Interaction* Data = GetInteractionData();
	const FVector Dir = Direction.GetSafeNormal();

	if (IsRagdolled())
	{
		if (Data->bSlapRagdolledBodies)
		{
			Multicast_AddRagdollImpulse(Dir * Data->RagdolledBodySlapImpulse * Strength);
		}
		Multicast_Slapped(Slapper, Dir, false, Strength);
		return;
	}

	SetSlapCount(SlapCount + Value);
	if (Data->SlapCountResetDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(SlapResetTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			SetSlapCount(0.f);
		}), Data->SlapCountResetDelay, false);
	}

	if (SlapCount + KINDA_SMALL_NUMBER >= Data->SlapsToRagdoll)
	{
		Multicast_Slapped(Slapper, Dir, false, Strength);
		StartRagdoll((Dir * Data->RagdollImpulse + FVector::UpVector * Data->RagdollImpulseUp) * Strength);
		return;
	}

	Multicast_Slapped(Slapper, Dir, true, Strength);
}

void UC_Ragdoll::StartRagdoll(FVector Impulse)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || IsRagdolled())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(SlapResetTimer);
	SetSlapCount(0.f);

	RagdollState.bActive = true;
	RagdollState.Impulse = Impulse;
	RagdollState.GetUpLocation = GetOwner()->GetActorLocation();
	ApplyRagdollState();
	GetOwner()->ForceNetUpdate();

	GetWorld()->GetTimerManager().SetTimer(GetUpTimer, this, &UC_Ragdoll::StopRagdoll, GetInteractionData()->RagdollDuration, false);
}

void UC_Ragdoll::StopRagdoll()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsRagdolled())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(GetUpTimer);

	ACharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	//Stand up on the floor under the body
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Pelvis = GetPelvisLocation();
	FVector GetUpLocation = Pelvis + FVector::UpVector * HalfHeight;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RagdollGetUp), false, Character);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pelvis + FVector::UpVector * 50.f, Pelvis - FVector::UpVector * 200.f, ECC_Visibility, Params))
	{
		GetUpLocation = Hit.Location + FVector::UpVector * (HalfHeight + 2.f);
	}
	Character->TeleportTo(GetUpLocation, FRotator(0.f, Character->GetActorRotation().Yaw, 0.f));

	RagdollState.bActive = false;
	RagdollState.GetUpLocation = Character->GetActorLocation();
	ApplyRagdollState();
	GetOwner()->ForceNetUpdate();
}

float UC_Ragdoll::GetStunPercent() const
{
	const UPDA_Interaction* Data = GetInteractionData();
	if (bRagdollApplied)
	{
		const float Elapsed = GetWorld()->GetTimeSeconds() - RagdollStartTime;
		return Data->RagdollDuration > 0.f ? FMath::Clamp(1.f - Elapsed / Data->RagdollDuration, 0.f, 1.f) : 1.f;
	}
	return Data->SlapsToRagdoll > 0 ? FMath::Clamp(SlapCount / Data->SlapsToRagdoll, 0.f, 1.f) : 0.f;
}

void UC_Ragdoll::OnRep_SlapCount()
{
	OnSlapCountChanged.Broadcast(SlapCount);
}

void UC_Ragdoll::OnRep_RagdollState()
{
	ApplyRagdollState();
}

void UC_Ragdoll::Multicast_Slapped_Implementation(AActor* Slapper, FVector_NetQuantizeNormal Direction, bool bKnockback, float Strength)
{
	ACharacter* Character = GetCharacter();
	//Launch on server and owning client so prediction agrees
	if (bKnockback && Character && (Character->HasAuthority() || Character->IsLocallyControlled()))
	{
		const UPDA_Interaction* Data = GetInteractionData();
		const FVector Flat = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
		Character->LaunchCharacter((Flat * Data->SlapKnockback + FVector::UpVector * Data->SlapKnockbackUp) * Strength, true, true);
	}

	OnSlapped.Broadcast(Slapper, Direction);
}

void UC_Ragdoll::Multicast_AddRagdollImpulse_Implementation(FVector_NetQuantize10 Impulse)
{
	ACharacter* Character = GetCharacter();
	if (!bRagdollApplied || !Character || !Character->GetMesh())
	{
		return;
	}

	for (FBodyInstance* Body : Character->GetMesh()->Bodies)
	{
		if (Body)
		{
			Body->AddImpulse(Impulse, true);
		}
	}
}

void UC_Ragdoll::SetSlapCount(float NewCount)
{
	if (SlapCount == NewCount)
	{
		return;
	}

	SlapCount = NewCount;
	OnSlapCountChanged.Broadcast(SlapCount);
}

void UC_Ragdoll::ApplyRagdollState()
{
	if (RagdollState.bActive == bRagdollApplied)
	{
		return;
	}

	if (RagdollState.bActive)
	{
		EnterRagdoll();
	}
	else
	{
		ExitRagdoll();
	}
}

void UC_Ragdoll::EnterRagdoll()
{
	ACharacter* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	bRagdollApplied = true;
	RagdollStartTime = GetWorld()->GetTimeSeconds();

	//Stand up first (slide), the crouch moves the mesh and the ragdoll caches its offset
	if (Character->bIsCrouched && Character->GetCharacterMovement())
	{
		Character->GetCharacterMovement()->bWantsToCrouch = false;
		Character->GetCharacterMovement()->UnCrouch(true);
	}
	MeshRelativeTransform = Mesh->GetRelativeTransform();
	MeshCollisionProfile = Mesh->GetCollisionProfileName();

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	//Friends walk through the body
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Mesh->SetCollisionProfileName(RagdollCollisionProfile);
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	for (FBodyInstance* Body : Mesh->Bodies)
	{
		if (Body)
		{
			Body->AddImpulse(RagdollState.Impulse, true);
		}
	}

	SetComponentTickEnabled(true);
	OnRagdollChanged.Broadcast(true);
}

void UC_Ragdoll::ExitRagdoll()
{
	ACharacter* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	bRagdollApplied = false;
	SetComponentTickEnabled(false);

	Mesh->SetSimulatePhysics(false);
	Mesh->SetAllBodiesSimulatePhysics(false);
	Mesh->SetCollisionProfileName(MeshCollisionProfile);

	if (!Character->HasAuthority())
	{
		Character->SetActorLocation(RagdollState.GetUpLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepWorldTransform);
	Mesh->SetRelativeTransform(MeshRelativeTransform);

	//Back to the class defaults
	const UCapsuleComponent* DefaultCapsule = GetDefault<ACharacter>(Character->GetClass())->GetCapsuleComponent();
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, DefaultCapsule->GetCollisionResponseToChannel(ECC_Pawn));
	Capsule->SetCollisionResponseToChannel(ECC_Camera, DefaultCapsule->GetCollisionResponseToChannel(ECC_Camera));

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	OnRagdollChanged.Broadcast(false);
}

const UPDA_Interaction* UC_Ragdoll::GetInteractionData() const
{
	if (ensureMsgf(InteractionData, TEXT("%s has no InteractionData, using code defaults"), *GetPathNameSafe(this)))
	{
		return InteractionData;
	}
	return GetDefault<UPDA_Interaction>();
}

FVector UC_Ragdoll::GetPelvisLocation() const
{
	const ACharacter* Character = GetCharacter();
	if (!Character || !Character->GetMesh())
	{
		return GetOwner()->GetActorLocation();
	}
	return Character->GetMesh()->GetSocketLocation(PelvisBoneName);
}

ACharacter* UC_Ragdoll::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}
