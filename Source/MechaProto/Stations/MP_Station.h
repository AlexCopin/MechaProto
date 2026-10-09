#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MP_Interactable.h"
#include "MP_Station.generated.h"

class ACharacter;
class AMP_Breakable;
class UBoxComponent;
class UCameraComponent;
class UPDA_Station;
class USpringArmComponent;
class UStaticMeshComponent;

//Seat a player takes with interact (UC_StationUser): seated with a looping pose, third person camera turned by the mouse
//Child classes give it a job: AMP_WeaponStation fires, AMP_PilotStation walks the mech
UCLASS(Abstract)
class MECHAPROTO_API AMP_Station : public AActor, public IMP_Interactable
{
	GENERATED_BODY()

public:
	AMP_Station();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	//-----IMP_Interactable
	virtual bool CanInteract(const ACharacter* User) const override;
	virtual void Interact(ACharacter* User) override;
	virtual FText GetInteractionText(const ACharacter* User) const override;

	//Name, camera and pose (the child's data asset)
	virtual const UPDA_Station* GetBaseStationData() const;

	//Camera arm pivot in the world: CameraOffset from the station's origin (a weapon station pivots on its gun, the lookout on its lamp)
	virtual FVector GetCameraPivotLocation() const;

	//Camera arm length (CameraDistance; the lookout picks it from its view)
	virtual float GetCameraDistance() const;

	UFUNCTION(BlueprintPure, Category = "Station")
	ACharacter* GetUser() const { return User; }

	//Where the user's capsule goes
	USceneComponent* GetSeat() const { return Seat; }

	//Server, called by UC_StationUser
	virtual void SetUser(ACharacter* NewUser);

	//A required system is broken: the station doesn't work
	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsDisabled() const;

	//The first broken required system, null when the station works
	UFUNCTION(BlueprintPure, Category = "Station")
	AMP_Breakable* GetBrokenSystem() const;

protected:
	//Mech systems this station needs (engine, rotor...): it doesn't work while one of them is broken
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Station")
	TArray<TObjectPtr<AMP_Breakable>> RequiredSystems;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	//Big box only the interact trace sees (Visibility), so the station is easy to target
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> InteractVolume;

	//User's capsule center (the weapon station moves it onto its turret)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Seat;

	//Visual only, under the sitting pose's pelvis
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SeatMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackrestMesh;

	//Placed by the data each frame (CameraOffset, CameraDistance), turned by the user's control rotation
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Replicated)
	TObjectPtr<ACharacter> User;

	bool IsLocallyUsed() const;
};
