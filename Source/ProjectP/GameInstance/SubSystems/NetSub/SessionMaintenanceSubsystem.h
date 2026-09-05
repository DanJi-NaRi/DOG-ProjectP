#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SessionMaintenanceSubsystem.generated.h"

UCLASS(BlueprintType)
class PROJECTP_API USessionMaintenanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Login|Session")
    void StartSessionPing();

    UFUNCTION(BlueprintCallable, Category = "Login|Session")
    void StopSessionPing();

    UFUNCTION(BlueprintCallable, Category = "Login|Session")
    void SendSessionPing();

    void RequestLogoutOnShutdown() const;

private:
    FTimerHandle SessionPingTimerHandle;
    bool bSessionPingInFlight = false;

    bool ShouldSkipLogoutOnShutdownForDungeonReconnect() const;
};
