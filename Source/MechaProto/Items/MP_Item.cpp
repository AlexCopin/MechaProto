#include "MP_Item.h"
#include "C_ItemHolder.h"
#include "MechaProtoCharacter.h"
#include "PDA_Interaction.h"
#include "PDA_Item.h"
#include "C_Ragdoll.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AMP_Item::AMP_Item()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	//Hit events for the throw
	Mesh->SetNotifyRigidBodyCollision(true);
	//Small and fast when thrown: no tunneling through the floor
	Mesh->SetUseCCD(true);
}

void AMP_Item::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Item, Holder);
	DOREPLIFETIME(AMP_Item, UseCount);
}

void AMP_Item::BeginPlay()
{
	Super::BeginPlay();

	const UPDA_Item* Data = GetItemData();
	if (Data->Mass > 0.f)
	{
		Mesh->SetMassOverrideInKg(NAME_None, Data->Mass, true);
	}
	ApplyHolder();

	if (HasAuthority())
	{
		Mesh->OnComponentHit.AddDynamic(this, &AMP_Item::OnMeshHit);
	}
}

const UPDA_Item* AMP_Item::GetItemData() const
{
	if (ensureMsgf(ItemData, TEXT("%s has no ItemData, using code defaults"), *GetPathNameSafe(this)))
	{
		return ItemData;
	}
	return GetDefault<UPDA_Item>();
}

bool AMP_Item::CanInteract(const ACharacter* User) const
{
	return !Holder;
}

void AMP_Item::Interact(ACharacter* User)
{
	if (UC_ItemHolder* ItemHolder = User ? User->FindComponentByClass<UC_ItemHolder>() : nullptr)
	{
		ItemHolder->PickUp(this);
	}
}

FText AMP_Item::GetInteractionText(const ACharacter* User) const
{
	//One item at a time
	const UC_ItemHolder* ItemHolder = User ? User->FindComponentByClass<UC_ItemHolder>() : nullptr;
	if (ItemHolder && ItemHolder->GetHeldItem())
	{
		return FText::Format(NSLOCTEXT("Item", "HandsFull", "Drop the {0} first"), ItemHolder->GetHeldItem()->GetItemData()->ItemName);
	}
	return FText::Format(NSLOCTEXT("Item", "PickUp", "Pick up {0}"), GetItemData()->ItemName);
}

void AMP_Item::Grab(ACharacter* NewHolder)
{
	Holder = NewHolder;
	ApplyHolder();
	ForceNetUpdate();
}

void AMP_Item::Release(const FVector& Location, const FVector& Velocity, ACharacter* Thrower)
{
	Holder = nullptr;
	ApplyHolder();
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetPhysicsLinearVelocity(Velocity);

	ThrownBy = Thrower;
	if (Thrower)
	{
		ThrowDirection = Velocity.GetSafeNormal();
		ThrowTime = GetWorld()->GetTimeSeconds();
		const UC_ItemHolder* ItemHolder = Thrower->FindComponentByClass<UC_ItemHolder>();
		const float Spin = ItemHolder ? ItemHolder->GetInteractionData()->ThrowSpin : 0.f;
		Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * Spin);
	}
	ForceNetUpdate();
}

void AMP_Item::OnMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ACharacter* Thrower = ThrownBy.Get();
	if (!Thrower || !OtherActor || OtherActor == Thrower || Holder)
	{
		return;
	}
	//Only the first thing hit counts (a bounce off the floor never slaps)
	ThrownBy = nullptr;

	const UC_ItemHolder* ItemHolder = Thrower->FindComponentByClass<UC_ItemHolder>();
	const float Window = ItemHolder ? ItemHolder->GetInteractionData()->ThrowSlapWindow : 0.f;
	if (!GetItemData()->bThrowSlapsPlayers || GetWorld()->GetTimeSeconds() - ThrowTime > Window)
	{
		return;
	}
	if (UC_Ragdoll* Ragdoll = OtherActor->FindComponentByClass<UC_Ragdoll>())
	{
		Ragdoll->ReceiveSlap(Thrower, ThrowDirection);
	}
}

void AMP_Item::Use(ACharacter* User)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (User != Holder || Now - LastUseTime < GetItemData()->UseCooldown)
	{
		return;
	}
	LastUseTime = Now;
	++UseCount;
	//OnRep doesn't run on the server
	OnRep_UseCount();
	ForceNetUpdate();
	OnUsed(User);
}

bool AMP_Item::PredictUse()
{
	const float Now = GetWorld()->GetTimeSeconds();
	//Small tolerance so the server's cooldown has passed too
	if (Now - LastPredictedUseTime < GetItemData()->UseCooldown * 1.05f)
	{
		return false;
	}
	LastPredictedUseTime = Now;
	PlayUseAnimation();
	return true;
}

void AMP_Item::OnRep_UseCount()
{
	//The holder already played it when predicting (a use from interact isn't predicted)
	const bool bLocalHolder = Holder && Holder->IsLocallyControlled();
	if (bLocalHolder && GetWorld()->GetTimeSeconds() - LastPredictedUseTime < 0.4f)
	{
		return;
	}
	if (bLocalHolder)
	{
		LastPredictedUseTime = GetWorld()->GetTimeSeconds();
	}
	PlayUseAnimation();
}

void AMP_Item::PlayUseAnimation()
{
	const UPDA_Item* Data = GetItemData();
	UAnimInstance* AnimInstance = Holder && Holder->GetMesh() ? Holder->GetMesh()->GetAnimInstance() : nullptr;
	if (!Data->UseAnimation || !AnimInstance || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Data->UseAnimation, Data->BodyAnimationSlot,
		Data->UseAnimationBlendTime, Data->UseAnimationBlendTime, Data->UseAnimationPlayRate, 1, -1.f, Data->UseAnimationStartTime);
	if (!Montage || Data->UseAnimationDuration <= 0.f)
	{
		return;
	}

	//Only a part of the clip (one swing of a loop)
	TWeakObjectPtr<UAnimInstance> WeakAnimInstance = AnimInstance;
	TWeakObjectPtr<UAnimMontage> WeakMontage = Montage;
	const float BlendTime = Data->UseAnimationBlendTime;
	GetWorldTimerManager().SetTimer(UseAnimationTimer, FTimerDelegate::CreateWeakLambda(this, [WeakAnimInstance, WeakMontage, BlendTime]()
	{
		if (WeakAnimInstance.IsValid() && WeakMontage.IsValid())
		{
			WeakAnimInstance->Montage_Stop(BlendTime, WeakMontage.Get());
		}
	}), Data->UseAnimationDuration / Data->UseAnimationPlayRate, false);
}

void AMP_Item::OnUsed_Implementation(ACharacter* User)
{
}

void AMP_Item::OnRep_Holder()
{
	ApplyHolder();
}

void AMP_Item::ApplyHolder()
{
	if (AppliedHolder.Get() == Holder && (Holder || !GetAttachParentActor()))
	{
		return;
	}
	AppliedHolder = Holder;

	if (!Holder)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetSimulatePhysics(true);
		if (HasAuthority())
		{
			SetReplicateMovement(true);
		}
		return;
	}

	//Each machine attaches it itself: the server stops replicating the movement while held
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HasAuthority())
	{
		SetReplicateMovement(false);
	}

	//The holder sees it in the first person hand, rendered like the arms
	USceneComponent* Hand = Holder->GetMesh();
	bool bFirstPerson = false;
	const AMechaProtoCharacter* MechaCharacter = Cast<AMechaProtoCharacter>(Holder);
	if (MechaCharacter && MechaCharacter->IsLocallyControlled() && MechaCharacter->GetFirstPersonMesh())
	{
		Hand = MechaCharacter->GetFirstPersonMesh();
		bFirstPerson = true;
	}

	const UC_ItemHolder* ItemHolder = Holder->FindComponentByClass<UC_ItemHolder>();
	const FName Socket = ItemHolder ? ItemHolder->GetInteractionData()->HoldSocket : FName("ring_metacarpal_r");
	AttachToComponent(Hand, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	const FTransform& Offset = GetItemData()->HoldOffset;
	Mesh->SetRelativeLocationAndRotation(Offset.GetLocation(), Offset.GetRotation());
	Mesh->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
}
