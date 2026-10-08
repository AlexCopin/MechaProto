#pragma once

#include "CoreMinimal.h"
#include "MP_Station.h"
#include "MP_PilotStation.generated.h"

class AMP_Mech;

//Drives the mech it is attached to (AMP_Mech): the user's move input walks (forward / back) and turns it (right / left),
//the camera turns with the mech. Stops it while a required system is broken
UCLASS()
class MECHAPROTO_API AMP_PilotStation : public AMP_Station
{
	GENERATED_BODY()

public:
	AMP_PilotStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual const UPDA_Station* GetBaseStationData() const override;
	virtual void SetUser(ACharacter* NewUser) override;

	//Server: X forward / backward, Y turn right / left, -1 to 1
	void SetDriveInput(const FVector2D& Input);

	//The mech it is attached to
	UFUNCTION(BlueprintPure, Category = "Station")
	AMP_Mech* GetMech() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	TObjectPtr<UPDA_Station> StationData;

	//Server
	FVector2D DriveInput = FVector2D::ZeroVector;
	//Last input given to the mech, only sent again when it changes (DebugDrive keeps working without a pilot)
	FVector2D AppliedInput = FVector2D::ZeroVector;
};
