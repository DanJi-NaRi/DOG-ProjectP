#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoginFlowSubsystem.generated.h"

UCLASS(BlueprintType)
class PROJECTP_API ULoginFlowSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Login")
    void HandleLoginSuccess(int32 UserIndex, const FString& Username, const FString& LoginToken);
};
