#include "MP_MechBlock.h"
#include "Components/BoxComponent.h"

namespace
{
	FColor BlockColor(EMP_MechBlockType Type)
	{
		switch (Type)
		{
		case EMP_MechBlockType::Solid:
			return FColor(150, 150, 160);
		case EMP_MechBlockType::Cavity:
			return FColor(40, 220, 90);
		case EMP_MechBlockType::Glass:
			return FColor(60, 170, 255);
		case EMP_MechBlockType::Ramp:
			return FColor(255, 150, 0);
		case EMP_MechBlockType::Pivot:
			return FColor(255, 240, 0);
		default:
			return FColor(220, 80, 220);
		}
	}
}

AMP_MechBlock::AMP_MechBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;

	//A unit box: the actor's scale is the size
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(50.f), false);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->SetGenerateOverlapEvents(false);
	Box->SetMobility(EComponentMobility::Movable);
	Box->bHiddenInGame = true;
	Box->SetLineThickness(2.f);
}

void AMP_MechBlock::BeginPlay()
{
	Super::BeginPlay();

	//Editor data: the mech already holds the structure built from it
	Destroy();
}

void AMP_MechBlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Size = GetActorScale3D() * 100.f;
	Box->ShapeColor = BlockColor(Type);
	Box->MarkRenderStateDirty();
}

FTransform AMP_MechBlock::GetBoxTransform(const FTransform& MechTransform) const
{
	return GetActorTransform().GetRelativeTransform(MechTransform);
}

FBox AMP_MechBlock::GetAlignedBox(const FTransform& MechTransform) const
{
	return FBox(FVector(-50.f), FVector(50.f)).TransformBy(GetBoxTransform(MechTransform));
}

void AMP_MechBlock::RebuildMech() const
{
	if (AMP_Mech* Mech = Cast<AMP_Mech>(GetAttachParentActor()))
	{
		Mech->BuildStructure();
	}
}

#if WITH_EDITOR
void AMP_MechBlock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Size drives the scale, so a size typed in cm resizes the box
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(AMP_MechBlock, Size))
	{
		SetActorScale3D(Size.ComponentMax(FVector(1.f)) / 100.f);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildMech();
}

void AMP_MechBlock::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	Size = GetActorScale3D() * 100.f;
	RebuildMech();
}

void AMP_MechBlock::PostEditUndo()
{
	Super::PostEditUndo();
	RebuildMech();
}
#endif
