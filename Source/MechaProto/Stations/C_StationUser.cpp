#include "C_StationUser.h"
#include "MP_WeaponStation.h"
#include "PDA_WeaponStation.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

UC_StationUser::UC_StationUser()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UC_StationUser::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UC_StationUser, Station);
}

void UC_StationUser::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner()->HasAuthority())
	{
		LeaveStation();
	}
	Super::EndPlay(EndPlayReason);
}

void UC_StationUser::EnterStation(AMP_WeaponStation* NewStation)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || !NewStation || Station == NewStation)
	{
		return;
	}

	LeaveStation();
	Station = NewStation;
	NewStation->SetUser(Character);
	ApplyStation();
	Character->ForceNetUpdate();
}

void UC_StationUser::LeaveStation()
{
	if (!GetOwner()->HasAuthority() || !Station)
	{
		return;
	}

	Station->SetUser(nullptr);
	Station = nullptr;
	ApplyStation();
	GetOwner()->ForceNetUpdate();
}

void UC_StationUser::SetFiring(bool bInFiring)
{
	bFiring = bInFiring;
	if (bFiring)
	{
		bFiredThisPress = false;
	}
}

void UC_StationUser::OnRep_Station()
{
	ApplyStation();
}

void UC_StationUser::ApplyStation()
{
	AMP_WeaponStation* OldStation = AppliedStation.Get();
	if (OldStation == Station)
	{
		return;
	}
	AppliedStation = Station;
	bFiring = false;

	ApplyLocalView(Station, OldStation);
	ApplyManningPose(Station);
	OnStationChanged.Broadcast(Station);
}

void UC_StationUser::ApplyLocalView(AMP_WeaponStation* NewStation, AMP_WeaponStation* OldStation)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	//Third person camera of the station, back to the first person view when leaving
	const AMP_WeaponStation* DataStation = NewStation ? NewStation : OldStation;
	const float BlendTime = DataStation ? DataStation->GetStationData()->CameraBlendTime : 0.3f;
	PlayerController->SetViewTargetWithBlend(NewStation ? static_cast<AActor*>(NewStation) : Pawn, BlendTime);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (Subsystem && StationMappingContext)
	{
		if (NewStation)
		{
			Subsystem->AddMappingContext(StationMappingContext, StationMappingPriority);
		}
		else
		{
			Subsystem->RemoveMappingContext(StationMappingContext);
		}
	}
}

void UC_StationUser::ApplyManningPose(AMP_WeaponStation* NewStation)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (ManningMontage.IsValid())
	{
		AnimInstance->Montage_Stop(0.2f, ManningMontage.Get());
		ManningMontage.Reset();
	}

	const UPDA_WeaponStation* Data = NewStation ? NewStation->GetStationData() : nullptr;
	if (!Data || !Data->ManningAnimation)
	{
		return;
	}

	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(Data->ManningAnimation, Data->BodyAnimationSlot, 0.2f, 0.2f))
	{
		//Loops until leaving the station
		AnimInstance->Montage_SetNextSection(FName("Default"), FName("Default"), Montage);
		ManningMontage = Montage;
	}
}

void UC_StationUser::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Station || !Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	//Aim is predicted locally, sent to the server for the other players' view
	const FVector AimPoint = Station->ComputeAimPoint();
	Station->AimAt(AimPoint);
	if (!Pawn->HasAuthority())
	{
		AimSendTimer -= DeltaTime;
		if (AimSendTimer <= 0.f)
		{
			AimSendTimer = 0.1f;
			Server_SetAim(AimPoint);
		}
	}

	const UPDA_WeaponStation* Data = Station->GetStationData();
	if (bFiring && (Data->bAutomatic || !bFiredThisPress) && Station->IsFireReady(LastFireTime))
	{
		LastFireTime = GetWorld()->GetTimeSeconds();
		bFiredThisPress = true;
		Station->PlayFireEffects();
		Server_Fire(AimPoint);
	}
}

void UC_StationUser::Server_SetAim_Implementation(FVector_NetQuantize AimPoint)
{
	if (Station)
	{
		Station->AimAt(AimPoint);
	}
}

void UC_StationUser::Server_Fire_Implementation(FVector_NetQuantize AimPoint)
{
	if (Station)
	{
		Station->ServerFire(AimPoint);
	}
}
