#include "C_StationUser.h"
#include "MP_LookoutStation.h"
#include "MP_PilotStation.h"
#include "MP_WeaponStation.h"
#include "C_Ragdoll.h"
#include "PDA_WeaponStation.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

void UC_StationUser::EnterStation(AMP_Station* NewStation)
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

void UC_StationUser::AddDriveInput(float Right, float Forward)
{
	PendingDriveInput += FVector2D(Right, Forward);
}

void UC_StationUser::OnRep_Station()
{
	ApplyStation();
}

void UC_StationUser::ApplyStation()
{
	AMP_Station* OldStation = AppliedStation.Get();
	if (OldStation == Station)
	{
		return;
	}
	AppliedStation = Station;
	bFiring = false;
	PendingDriveInput = FVector2D::ZeroVector;
	SentDriveForward = 0;
	SentDriveTurn = 0;

	ApplySeat(Station, OldStation);
	ApplyLocalView(Station, OldStation);
	ApplyManningPose(Station);
	OnStationChanged.Broadcast(Station);
}

void UC_StationUser::ApplyLocalView(AMP_Station* NewStation, AMP_Station* OldStation)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	//Third person camera of the station, back to the first person view when leaving
	const AMP_Station* DataStation = NewStation ? NewStation : OldStation;
	const float BlendTime = DataStation ? DataStation->GetBaseStationData()->CameraBlendTime : 0.3f;
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

void UC_StationUser::ApplySeat(AMP_Station* NewStation, AMP_Station* OldStation)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	if (NewStation)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		//A turret turns a bit later on the server (aim sent at 10 Hz): that offset is not a movement error to correct
		Movement->bIgnoreClientMovementErrorChecksAndCorrection = true;
		Character->bUseControllerRotationYaw = false;
		Character->AttachToComponent(NewStation->GetSeat(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		return;
	}

	if (!OldStation)
	{
		return;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->bUseControllerRotationYaw = GetDefault<ACharacter>(Character->GetClass())->bUseControllerRotationYaw;
	Movement->bIgnoreClientMovementErrorChecksAndCorrection = GetDefault<UCharacterMovementComponent>(Movement->GetClass())->bIgnoreClientMovementErrorChecksAndCorrection;

	//Knocked off by a ragdoll: it handles the movement itself
	const UC_Ragdoll* Ragdoll = Character->FindComponentByClass<UC_Ragdoll>();
	if (Ragdoll && Ragdoll->IsRagdolled())
	{
		return;
	}
	Character->SetActorRotation(FRotator(0.f, Character->GetActorRotation().Yaw, 0.f));
	Movement->SetDefaultMovementMode();
	if (Character->HasAuthority())
	{
		//Stand up out of the seat and the pedestal
		Character->TeleportTo(Character->GetActorLocation(), Character->GetActorRotation());
	}
}

void UC_StationUser::ApplyManningPose(AMP_Station* NewStation)
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

	const UPDA_Station* Data = NewStation ? NewStation->GetBaseStationData() : nullptr;
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
		PendingDriveInput = FVector2D::ZeroVector;
		return;
	}

	if (AMP_WeaponStation* Weapon = Cast<AMP_WeaponStation>(Station))
	{
		TickWeapon(Weapon, DeltaTime);
	}
	else if (AMP_PilotStation* Pilot = Cast<AMP_PilotStation>(Station))
	{
		TickPilot(Pilot);
	}
	else if (AMP_LookoutStation* Lookout = Cast<AMP_LookoutStation>(Station))
	{
		TickLookout(Lookout, DeltaTime);
	}
	PendingDriveInput = FVector2D::ZeroVector;
}

void UC_StationUser::TickPilot(AMP_PilotStation* Pilot)
{
	//The move input comes every frame while held (DoMove), none this frame = released
	const int8 Forward = static_cast<int8>(FMath::RoundToInt(FMath::Clamp(PendingDriveInput.Y, -1.f, 1.f) * 127.f));
	const int8 Turn = static_cast<int8>(FMath::RoundToInt(FMath::Clamp(PendingDriveInput.X, -1.f, 1.f) * 127.f));
	if (Forward == SentDriveForward && Turn == SentDriveTurn)
	{
		return;
	}
	SentDriveForward = Forward;
	SentDriveTurn = Turn;

	if (GetOwner()->HasAuthority())
	{
		Pilot->SetDriveInput(FVector2D(Forward, Turn) / 127.f);
	}
	else
	{
		Server_SetDriveInput(Forward, Turn);
	}
}

void UC_StationUser::Server_SetDriveInput_Implementation(int8 Forward, int8 Turn)
{
	if (AMP_PilotStation* Pilot = Cast<AMP_PilotStation>(Station))
	{
		Pilot->SetDriveInput(FVector2D(Forward, Turn) / 127.f);
	}
}

void UC_StationUser::TickWeapon(AMP_WeaponStation* Weapon, float DeltaTime)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());

	//Aim is predicted locally, sent to the server for the other players' view
	const FVector AimPoint = Weapon->ComputeAimPoint();
	Weapon->AimAt(AimPoint);
	if (!Pawn->HasAuthority())
	{
		AimSendTimer -= DeltaTime;
		if (AimSendTimer <= 0.f)
		{
			AimSendTimer = 0.1f;
			Server_SetAim(AimPoint);
		}
	}

	const UPDA_WeaponStation* Data = Weapon->GetStationData();
	if (bFiring && (Data->bAutomatic || !bFiredThisPress) && Weapon->IsFireReady(LastFireTime))
	{
		LastFireTime = GetWorld()->GetTimeSeconds();
		bFiredThisPress = true;
		Weapon->PlayFireEffects();
		Server_Fire(AimPoint);
	}
}

float UC_StationUser::GetFireReadyPercent() const
{
	const AMP_WeaponStation* Weapon = Cast<AMP_WeaponStation>(Station);
	const float Cooldown = Weapon ? Weapon->GetStationData()->FireCooldown : 0.f;
	if (Cooldown <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - LastFireTime) / Cooldown, 0.f, 1.f);
}

void UC_StationUser::TickLookout(AMP_LookoutStation* Lookout, float DeltaTime)
{
	//The beam follows the view at once, the others get it like a gun's aim
	const FVector AimPoint = Lookout->ComputeAimPoint();
	Lookout->AimAt(AimPoint);
	if (!GetOwner()->HasAuthority())
	{
		AimSendTimer -= DeltaTime;
		if (AimSendTimer <= 0.f)
		{
			AimSendTimer = 0.1f;
			Server_SetAim(AimPoint);
		}
	}
}

void UC_StationUser::Server_SetAim_Implementation(FVector_NetQuantize AimPoint)
{
	if (AMP_WeaponStation* Weapon = Cast<AMP_WeaponStation>(Station))
	{
		Weapon->AimAt(AimPoint);
	}
	else if (AMP_LookoutStation* Lookout = Cast<AMP_LookoutStation>(Station))
	{
		Lookout->AimAt(AimPoint);
	}
}

void UC_StationUser::Server_Fire_Implementation(FVector_NetQuantize AimPoint)
{
	if (AMP_WeaponStation* Weapon = Cast<AMP_WeaponStation>(Station))
	{
		Weapon->ServerFire(AimPoint);
	}
}
