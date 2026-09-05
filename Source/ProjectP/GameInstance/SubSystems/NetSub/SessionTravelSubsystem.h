#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "HttpFwd.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SessionTravelSubsystem.generated.h"

class APlayerController;
class UNetDriver;
class UWorld;

DECLARE_MULTICAST_DELEGATE_TwoParams(FSessionTravelResultNative, bool, const FString&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSessionTravelResultDynamic, bool, bSuccess, const FString&, Message);

UCLASS(BlueprintType)
class PROJECTP_API USessionTravelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FSessionTravelResultNative OnLobbyTravelResult;

    UPROPERTY(BlueprintAssignable, Category = "Lobby|Server")
    FSessionTravelResultDynamic OnVerifySessionAndTravelToLobbyResult;

    bool PrepareLobbyServerTravel(APlayerController* PlayerController, FString& OutLobbyServerAddress) const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Server")
    bool TravelToLobbyServer(APlayerController* PlayerController);

    bool TravelToDungeonServer(APlayerController* PlayerController, const FString& OverrideDungeonServerAddress = TEXT("")) const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Server")
    void VerifySessionAndTravelToLobby(APlayerController* PlayerController);

    void QueryDungeonMemberStateAndTravel(APlayerController* PlayerController);
    void HandleVerifySessionAndTravelResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void HandleDungeonMemberStateQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void FallbackTravelToLobbyAfterDungeonStateQuery(APlayerController* PlayerController, const FString& ReasonMessage);
    bool ShouldReconnectToDungeon(const FString& ConnectionState, bool bDungeonSessionJoinable) const;
    void BeginLobbyTravelWait();
    void ClearLobbyTravelWait();
    bool IsLobbyTravelWaitInProgress() const;
    void HandleLobbyTravelTimeout();
    void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
    void HandleLobbyNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    void HandleLobbyTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
    FString BuildLobbyNetworkFailureMessage(ENetworkFailure::Type FailureType, const FString& ErrorString) const;
    FString BuildLobbyTravelFailureMessage(ETravelFailure::Type FailureType, const FString& ErrorString) const;
    bool IsLobbyVersionMismatchNetworkFailure(ENetworkFailure::Type FailureType, const FString& ErrorString) const;
    bool IsLobbyVersionMismatchTravelFailure(ETravelFailure::Type FailureType, const FString& ErrorString) const;

private:
    FTimerHandle LobbyTravelTimeoutTimerHandle;
    bool bWaitingForLobbyTravelResult = false;
    bool bVerifySessionAndTravelInFlight = false;
    bool bDungeonMemberStateQueryInFlight = false;
    TWeakObjectPtr<APlayerController> PendingLobbyTravelPlayerController;

    void BroadcastLobbyTravelResult(bool bSuccess, const FString& Message);
};
