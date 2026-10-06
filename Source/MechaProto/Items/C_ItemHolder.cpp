#include "C_ItemHolder.h"
#include "MP_Item.h"
#include "PDA_Interaction.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UC_ItemHolder::UC_ItemHolder()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UC_ItemHolder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UC_ItemHolder, HeldItem);
}

void UC_ItemHolder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner()->HasAuthority())
	{
		Drop();
	}
	Super::EndPlay(EndPlayReason);
}

const UPDA_Interaction* UC_ItemHolder::GetInteractionData() const
{
	if (ensureMsgf(InteractionData, TEXT("%s has no InteractionData, using code defaults"), *GetPathNameSafe(this)))
	{
		return InteractionData;
	}
	return GetDefault<UPDA_Interaction>();
}

bool UC_ItemHolder::PickUp(AMP_Item* Item)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || HeldItem || !Item || Item->GetHolder())
	{
		return false;
	}

	HeldItem = Item;
	Item->Grab(Character);
	OnHeldItemChanged.Broadcast(HeldItem);
	Character->ForceNetUpdate();
	return true;
}

void UC_ItemHolder::Drop()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || !HeldItem)
	{
		return;
	}

	//Released in front of the eyes, not through a wall
	const UPDA_Interaction* Data = GetInteractionData();
	const FVector Eyes = Character->GetPawnViewLocation();
	const FVector Forward = Character->GetBaseAimRotation().Vector();
	FVector Location = Eyes + Forward * Data->DropDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ItemDrop), false, Character);
	Params.AddIgnoredActor(HeldItem);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Location, ECC_Visibility, Params))
	{
		Location = Hit.Location - Forward * 20.f;
	}
	const FVector Velocity = Character->GetVelocity() + Forward * Data->DropSpeed + FVector::UpVector * Data->DropUpSpeed;

	AMP_Item* Item = HeldItem;
	HeldItem = nullptr;
	Item->Release(Location, Velocity);
	OnHeldItemChanged.Broadcast(nullptr);
	Character->ForceNetUpdate();
}

void UC_ItemHolder::RequestDrop()
{
	if (HeldItem)
	{
		Server_Drop();
	}
}

void UC_ItemHolder::RequestUse()
{
	if (HeldItem)
	{
		Server_Use();
	}
}

void UC_ItemHolder::Server_Drop_Implementation()
{
	Drop();
}

void UC_ItemHolder::Server_Use_Implementation()
{
	if (HeldItem)
	{
		HeldItem->Use(Cast<ACharacter>(GetOwner()));
	}
}

void UC_ItemHolder::OnRep_HeldItem()
{
	OnHeldItemChanged.Broadcast(HeldItem);
}
