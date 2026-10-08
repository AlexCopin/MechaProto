#pragma once

#include "CoreMinimal.h"
#include "MP_Station.h"
#include "MP_LookoutStation.generated.h"

//Seat that only gives a view: its camera (UPDA_Station camera settings) looks around through the mech's panoramic glass
UCLASS()
class MECHAPROTO_API AMP_LookoutStation : public AMP_Station
{
	GENERATED_BODY()

public:
	AMP_LookoutStation();

	virtual const UPDA_Station* GetBaseStationData() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UPDA_Station> StationData;
};
