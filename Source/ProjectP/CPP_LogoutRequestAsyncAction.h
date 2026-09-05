#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "CPP_LogoutRequestAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLogoutRequestResult, const FString&, Message);

UCLASS()
class PROJECTP_API UCPP_LogoutRequestAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FLogoutRequestResult OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FLogoutRequestResult OnFailure;

    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CPP_Default_ServerUrl = ""), Category = "Yuno|Login")
    static UCPP_LogoutRequestAsyncAction* RequestLogout(UObject* WorldContextObject, const FString& LoginToken, const FString& ServerUrl);

    virtual void Activate() override;

private:
    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject;

    FString RequestLoginToken;
    FString RequestServerUrl;

    void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastFailure(const FString& Message);
};
