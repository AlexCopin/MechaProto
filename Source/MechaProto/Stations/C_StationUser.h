#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "C_StationUser.generated.h"

class AMP_WeaponStation;
class UAnimMontage;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationChanged, AMP_WeaponStation*, Station);

//Lets the owning character man a weapon station: camera, input, aim and fire go through it while the character stays in the world
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
	AMP_WeaponStation* GetStation() const { return Station; }

	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsManning() const { return Station != nullptr; }

	//Server
	void EnterStation(AMP_WeaponStation* NewStation);
	void LeaveStation();

	//Local input, hold to fire
	void SetFiring(bool bInFiring);

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
	TObjectPtr<AMP_WeaponStation> Station;

	UFUNCTION()
	void OnRep_Station();

	UFUNCTION(Server, Unreliable)
	void Server_SetAim(FVector_NetQuantize AimPoint);

	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector_NetQuantize AimPoint);

	//Camera, input and pose follow the replicated station on every machine
	void ApplyStation();
	void ApplyLocalView(AMP_WeaponStation* NewStation, AMP_WeaponStation* OldStation);
	void ApplyManningPose(AMP_WeaponStation* NewStation);

	TWeakObjectPtr<AMP_WeaponStation> AppliedStation;
	TWeakObjectPtr<UAnimMontage> ManningMontage;
	bool bFiring = false;
	bool bFiredThisPress = false;
	float LastFireTime = -100.f;
	float AimSendTimer = 0.f;
};
