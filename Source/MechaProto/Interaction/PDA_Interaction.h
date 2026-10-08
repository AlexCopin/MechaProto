#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Interaction.generated.h"

class UMaterialInterface;

//Tuning of the friendslop interactions, read live so it can be edited during PIE
UCLASS(BlueprintType)
class MECHAPROTO_API UPDA_Interaction : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//-----Slap
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap", meta = (ClampMin = "0", UIMax = "500", Units = "cm"))
	float SlapRange = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap", meta = (ClampMin = "1", UIMax = "100", Units = "cm"))
	float SlapRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap", meta = (ClampMin = "0", UIMax = "2", Units = "s"))
	float SlapCooldown = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap", meta = (ClampMin = "0", UIMax = "2000"))
	float SlapKnockback = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap", meta = (ClampMin = "0", UIMax = "1000"))
	float SlapKnockbackUp = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slap")
	bool bDrawSlapDebug = false;

	//-----Ragdoll
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "1", UIMax = "10"))
	int32 SlapsToRagdoll = 3;

	//Slap count goes back to 0 after this delay without being slapped
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "0", UIMax = "10", Units = "s"))
	float SlapCountResetDelay = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "0", UIMax = "3000"))
	float RagdollImpulse = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "0", UIMax = "3000"))
	float RagdollImpulseUp = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "0.1", UIMax = "10", Units = "s"))
	float RagdollDuration = 3.f;

	//Slapping a body already on the floor pushes it again
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll")
	bool bSlapRagdolledBodies = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ragdoll", meta = (ClampMin = "0", UIMax = "3000", EditCondition = "bSlapRagdolledBodies"))
	float RagdolledBodySlapImpulse = 400.f;

	//-----Interact (stations, items...)
	//Reach from the eyes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = "50", UIMax = "600", Units = "cm"))
	float InteractRange = 250.f;

	//Thickness of the look trace, forgiving aim
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = "0", UIMax = "50", Units = "cm"))
	float InteractRadius = 15.f;

	//Aim assist when the look trace finds nothing usable: the target closest to the view direction within this angle, in sight (small items on the floor). 0 = off
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = "0", ClampMax = "45", Units = "Degrees"))
	float InteractAssistAngle = 15.f;

	//Overlay drawn on the meshes of the interactable the local player can use (none = no highlight)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
	TObjectPtr<UMaterialInterface> FocusOverlayMaterial;

	//Prints "E: ..." on screen until a widget shows it (OnFocusChanged)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
	bool bShowDebugPrompt = true;

	//-----Held items
	//Bone or socket of the body and first person meshes the item is attached to
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	FName HoldSocket = FName("hand_r");

	//Released this far in front of the eyes
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "200", Units = "cm"))
	float DropDistance = 70.f;

	//Speed given to a dropped item along the view, plus the holder's velocity
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "2000", Units = "CentimetersPerSecond"))
	float DropSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "1000", Units = "CentimetersPerSecond"))
	float DropUpSpeed = 100.f;

	//Throw (right click): speed along the view, plus the holder's velocity
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "5000", Units = "CentimetersPerSecond"))
	float ThrowSpeed = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "2000", Units = "CentimetersPerSecond"))
	float ThrowUpSpeed = 350.f;

	//Random tumble of a thrown item
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "2000", Units = "DegreesPerSecond"))
	float ThrowSpin = 720.f;

	//A thrown item (bThrowSlapsPlayers) slaps the first player it hits within this time, unless it hit something else first
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", UIMax = "5", Units = "s"))
	float ThrowSlapWindow = 1.5f;

	//Slapped into a ragdoll: the held item falls
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	bool bDropItemWhenRagdolled = true;
};
