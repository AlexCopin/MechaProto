#include "MP_NetworkSubsystem.h"
#include "MechaProto.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

void UMP_NetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UMP_NetworkSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UMP_NetworkSubsystem::HandleTravelFailure);
	}
}

void UMP_NetworkSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	Super::Deinitialize();
}

void UMP_NetworkSubsystem::HostGame()
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World)
	{
		return;
	}

	const FString MapPath = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	UE_LOG(LogMechaProto, Log, TEXT("Hosting %s"), *MapPath);
	UGameplayStatics::OpenLevel(World, FName(*MapPath), true, TEXT("listen"));
}

void UMP_NetworkSubsystem::JoinGame(const FString& Address)
{
	const FString CleanAddress = Address.TrimStartAndEnd();
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (CleanAddress.IsEmpty() || !PC)
	{
		return;
	}

	LastJoinAddress = CleanAddress;
	SaveConfig();

	UE_LOG(LogMechaProto, Log, TEXT("Joining %s"), *CleanAddress);
	PC->ClientTravel(CleanAddress, TRAVEL_Absolute);
}

void UMP_NetworkSubsystem::LeaveGame()
{
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*UGameMapsSettings::GetGameDefaultMap()), true);
	}
}

void UMP_NetworkSubsystem::EnsureVoiceSession()
{
	const IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetGameInstance()->GetWorld());
	const IOnlineSessionPtr Sessions = OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
	if (!Sessions.IsValid() || Sessions->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	//Local only, not advertised on the LAN
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = true;
	Settings.bShouldAdvertise = false;
	Settings.bAllowJoinInProgress = true;
	Settings.NumPublicConnections = 8;

	const ULocalPlayer* LocalPlayer = GetGameInstance()->GetFirstGamePlayer();
	const bool bStarted = Sessions->CreateSession(LocalPlayer ? LocalPlayer->GetControllerId() : 0, NAME_GameSession, Settings);
	UE_LOG(LogMechaProto, Log, TEXT("Voice session %s"), bStarted ? TEXT("created") : TEXT("failed"));
}

void UMP_NetworkSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	UE_LOG(LogMechaProto, Warning, TEXT("Network failure: %s"), *Error);
	OnNetworkError.Broadcast(Error);
}

void UMP_NetworkSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	UE_LOG(LogMechaProto, Warning, TEXT("Travel failure: %s"), *Error);
	OnNetworkError.Broadcast(Error);
}
