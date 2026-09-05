#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ServerConfigSubsystem.generated.h"

UCLASS()
class PROJECTP_API UServerConfigSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetLoginServerBaseUrl() const;

    UFUNCTION(BlueprintPure, Category = "Backend|Server")
    FString GetGameBackendBaseUrl() const;

    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetLoginRequestUrl() const;

    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetRegisterRequestUrl() const;

    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetLogoutRequestUrl() const;

    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetSessionPingUrl() const;

    UFUNCTION(BlueprintPure, Category = "Login|Server")
    FString GetSessionVerifyUrl() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonSessionVerifyUrl() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonMemberStateUrl() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonMemberStateQueryUrl() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonAllocateUrl() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonShutdownUrl() const;

    UFUNCTION(BlueprintPure, Category = "Server|Telemetry")
    FString GetLobbyTelemetryUrl() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Server")
    FString GetLobbyServerAddress() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonServerAddress() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Server")
    FString GetDungeonStateServerAuthKey() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Server")
    bool IsLobbyTokenVerificationRequired() const;

    UFUNCTION(BlueprintPure, Category = "Server|Cheat")
    bool IsChatCheatAllowed() const;

private:
    FString BuildGameBackendUrl(const FString& EndpointPath) const;
};
