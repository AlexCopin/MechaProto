#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_StationUser.generated.h"

class AMP_LookoutStation;
class AMP_PilotStation;
class AMP_Station;
class AMP_WeaponStation;
class UAnimMontage;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationChanged, AMP_Station*, Station);

//Lets the owning character use a station: seated and moving with it, its camera and input mapping
//Weapon station: aim and fire. Pilot station: the move input drives the mech
UCLASS(ClassGroup = (MechaProto), meta = (BlueprintSpawnableComponent))
class MECHAPROTO_API UC_StationUser : public UActorComponent
{
	GENERATED_BODY()

public:
	UC_StationUser();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Station")
	AMP_Station* GetStation() const { return Station; }

	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsManning() const { return Station != nullptr; }

	//Server
	void EnterStation(AMP_Station* NewStation);
	void LeaveStation();

	//Local input, hold to fire
	void SetFiring(bool bInFiring);

	//Local move input while seated, every frame it is held (the pilot station drives with it)
	void AddDriveInput(float Right, float Forward);

	//Fired on every machine
	UPROPERTY(BlueprintAssignable, Category = "Station")
	FOnStationChanged OnStationChanged;

protected:
	//Added while manning, above the character contexts so its keys win (fire on the slap button)
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> StationMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	int32 StationMappingPriority = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Station)
	TObjectPtr<AMP_Station> Station;

	UFUNCTION()
	void OnRep_Station();

	UFUNCTION(Server, Unreliable)
	void Server_SetAim(FVector_NetQuantize AimPoint);

	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector_NetQuantize AimPoint);

	//Sent when it changes, -127 to 127
	UFUNCTION(Server, Reliable)
	void Server_SetDriveInput(int8 Forward, int8 Turn);

	//Camera, input and pose follow the replicated station on every machine
	void ApplyStation();
	void ApplyLocalView(AMP_Station* NewStation, AMP_Station* OldStation);
	//Attached to the seat with movement off, every machine
	void ApplySeat(AMP_Station* NewStation, AMP_Station* OldStation);
	void ApplyManningPose(AMP_Station* NewStation);

	//Local user, each frame
	void TickWeapon(AMP_WeaponStation* Weapon, float DeltaTime);
	void TickPilot(AMP_PilotStation* Pilot);
	void TickLookout(AMP_LookoutStation* Lookout, float DeltaTime);

	TWeakObjectPtr<AMP_Station> AppliedStation;
	TWeakObjectPtr<UAnimMontage> ManningMontage;
	bool bFiring = false;
	bool bFiredThisPress = false;
	float LastFireTime = -100.f;
	float AimSendTimer = 0.f;
	//X right, Y forward, gathered this frame
	FVector2D PendingDriveInput = FVector2D::ZeroVector;
	int8 SentDriveForward = 0;
	int8 SentDriveTurn = 0;
};
