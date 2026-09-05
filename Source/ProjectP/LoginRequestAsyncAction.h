#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LoginRequestAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLoginRequestResult, int32, UserIndex, const FString&, Username, const FString&, Message, const FString&, LoginToken);

UCLASS()
class PROJECTP_API ULoginRequestAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FLoginRequestResult OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FLoginRequestResult OnFailure;

    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CPP_Default_ServerUrl = ""), Category = "Yuno|Login")
    static ULoginRequestAsyncAction* RequestLogin(UObject* WorldContextObject, const FString& ID, const FString& Password, const FString& ServerUrl);

    virtual void Activate() override;

private:
    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject;

    FString RequestID;
    FString RequestPassword;
    FString RequestServerUrl;

    void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastFailure(const FString& Message);
};
