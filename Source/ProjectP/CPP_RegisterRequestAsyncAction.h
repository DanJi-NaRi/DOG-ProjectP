#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "CPP_RegisterRequestAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRegisterRequestResult, int32, UserIndex, const FString&, ID, const FString&, Username, const FString&, Message);

UCLASS()
class PROJECTP_API UCPP_RegisterRequestAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FRegisterRequestResult OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FRegisterRequestResult OnFailure;

    UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CPP_Default_ServerUrl = ""), Category = "Yuno|Login")
    static UCPP_RegisterRequestAsyncAction* RequestRegister(UObject* WorldContextObject, const FString& ID, const FString& Password, const FString& Username, const FString& ServerUrl);

    virtual void Activate() override;

private:
    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject;

    FString RequestID;
    FString RequestPassword;
    FString RequestUsername;
    FString RequestServerUrl;

    void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void BroadcastFailure(const FString& Message);
};
