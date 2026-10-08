#include "MP_LookoutStation.h"
#include "PDA_Station.h"
#include "Components/StaticMeshComponent.h"

AMP_LookoutStation::AMP_LookoutStation()
{
	//A small console in front of the seat instead of a pedestal under it
	BaseMesh->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
	BaseMesh->SetRelativeScale3D(FVector(0.4f, 0.9f, 0.9f));
}

const UPDA_Station* AMP_LookoutStation::GetBaseStationData() const
{
	if (ensureMsgf(StationData, TEXT("%s has no StationData, using code defaults"), *GetPathNameSafe(this)))
	{
		return StationData;
	}
	return GetDefault<UPDA_Station>();
}
