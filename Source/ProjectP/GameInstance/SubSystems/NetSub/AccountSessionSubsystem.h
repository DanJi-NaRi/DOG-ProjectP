#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AccountSessionSubsystem.generated.h"

UCLASS(BlueprintType)
class PROJECTP_API UAccountSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Login")
    void SetLoginInfo(int32 NewUserIndex, const FString& NewUsername);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void SetLoginToken(const FString& NewLoginToken);

    UFUNCTION(BlueprintCallable, Category = "Login")
    void ClearLoginInfo();

    UFUNCTION(BlueprintPure, Category = "Login")
    int32 GetUserIndex() const;

    UFUNCTION(BlueprintPure, Category = "Login")
    const FString& GetUsername() const;

    UFUNCTION(BlueprintPure, Category = "Login")
    const FString& GetLoginToken() const;

    UFUNCTION(BlueprintCallable, Category = "Character")
    void SetLastUsedCharacterId(int32 NewCharacterId);

    UFUNCTION(BlueprintCallable, Category = "Character")
    void ClearLastUsedCharacterId();

    UFUNCTION(BlueprintPure, Category = "Character")
    int32 GetLastUsedCharacterId() const;

private:
    UPROPERTY()
    int32 UserIndex = -1;

    UPROPERTY()
    FString Username;

    UPROPERTY()
    FString LoginToken;

    // 서버 이동(던전 입장/로비 복귀) 직전에 사용하던 캐릭터 ID. GameInstance 수명이라 이동 후에도 유지된다.
    UPROPERTY()
    int32 LastUsedCharacterId = -1;
};
