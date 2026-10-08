#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Station.generated.h"

class UAnimSequenceBase;

//What every station shares (name, third person camera, sitting pose), read live so it can be edited during PIE. The pilot seat uses it as is
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Station : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//Shown in the interact prompt
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FText StationName = FText::FromString(TEXT("Station"));

	//-----Camera (third person around the station, turned by the user's mouse)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0", UIMax = "6000", Units = "cm"))
	float CameraDistance = 500.f;

	//Pivot of the camera arm from the station's origin (on the floor)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector CameraOffset = FVector(0.f, 0.f, 220.f);

	//Pulls the camera in front of walls. Off: it goes through the mech's walls and sees outside
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bCameraCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float CameraBlendTime = 0.3f;

	//-----User animation (looped on the body while seated)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> ManningAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName BodyAnimationSlot = FName("DefaultSlot");
};
