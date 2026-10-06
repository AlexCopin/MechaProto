#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "MP_NetworkSubsystem.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNetworkError, const FString&, Error);

//LAN by IP: host = listen server on the current map, join = travel to an IP (port 7777 by default)
UCLASS(Config = Game)
class MECHAPROTO_API UMP_NetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Network")
	void HostGame();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void JoinGame(const FString& Address);

	//Back to a standalone game on the default map
	UFUNCTION(BlueprintCallable, Category = "Network")
	void LeaveGame();

	UFUNCTION(BlueprintPure, Category = "Network")
	FString GetLastJoinAddress() const { return LastJoinAddress; }

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnNetworkError OnNetworkError;

protected:
	UPROPERTY(Config)
	FString LastJoinAddress = TEXT("127.0.0.1");

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
