#include "MP_PilotStation.h"
#include "MP_Mech.h"
#include "PDA_Station.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"

AMP_PilotStation::AMP_PilotStation()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	//Console in front of the seat instead of a pedestal under it
	if (CubeMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CubeMesh.Object);
	}
	BaseMesh->SetRelativeLocation(FVector(70.f, 0.f, 50.f));
	BaseMesh->SetRelativeScale3D(FVector(0.3f, 1.2f, 1.f));
}

const UPDA_Station* AMP_PilotStation::GetBaseStationData() const
{
	if (ensureMsgf(StationData, TEXT("%s has no StationData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StationData;
	}
	return GetDefault<UPDA_Station>();
}

AMP_Mech* AMP_PilotStation::GetMech() const
{
	for (AActor* Parent = GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
	{
		if (AMP_Mech* Mech = Cast<AMP_Mech>(Parent))
		{
			return Mech;
		}
	}
	return nullptr;
}

void AMP_PilotStation::SetDriveInput(const FVector2D& Input)
{
	DriveInput = FVector2D(FMath::Clamp(Input.X, -1.f, 1.f), FMath::Clamp(Input.Y, -1.f, 1.f));
}

void AMP_PilotStation::SetUser(ACharacter* NewUser)
{
	Super::SetUser(NewUser);
	DriveInput = FVector2D::ZeroVector;
}

void AMP_PilotStation::Tick(float DeltaSeconds)
{
	AMP_Mech* Mech = GetMech();

	//The view turns with the mech, like the players walking inside
	if (Mech && IsLocallyUsed())
	{
		if (AController* Controller = User->GetController())
		{
			Controller->SetControlRotation(Controller->GetControlRotation() + FRotator(0.f, Mech->GetLastYawDelta(), 0.f));
		}
	}

	//Camera from the control rotation
	Super::Tick(DeltaSeconds);

	if (Mech && HasAuthority())
	{
		const FVector2D Input = User && !IsDisabled() ? DriveInput : FVector2D::ZeroVector;
		if (Input != AppliedInput)
		{
			AppliedInput = Input;
			Mech->SetPilotInput(Input);
		}
	}
}
