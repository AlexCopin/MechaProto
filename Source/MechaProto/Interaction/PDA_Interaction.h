#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PDA_Interaction.generated.h"

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
};
