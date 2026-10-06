#include "MP_Item.h"
#include "C_ItemHolder.h"
#include "MechaProtoCharacter.h"
#include "PDA_Interaction.h"
#include "PDA_Item.h"
#include "Components/SkeletalMeshComponent.h"
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
}

void AMP_Item::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMP_Item, Holder);
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

void AMP_Item::Release(const FVector& Location, const FVector& Velocity)
{
	Holder = nullptr;
	ApplyHolder();
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	ForceNetUpdate();
}

void AMP_Item::Use(ACharacter* User)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (User != Holder || Now - LastUseTime < GetItemData()->UseCooldown)
	{
		return;
	}
	LastUseTime = Now;
	OnUsed(User);
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
	const FName Socket = ItemHolder ? ItemHolder->GetInteractionData()->HoldSocket : FName("hand_r");
	AttachToComponent(Hand, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	const FTransform& Offset = GetItemData()->HoldOffset;
	Mesh->SetRelativeLocationAndRotation(Offset.GetLocation(), Offset.GetRotation());
	Mesh->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
}
