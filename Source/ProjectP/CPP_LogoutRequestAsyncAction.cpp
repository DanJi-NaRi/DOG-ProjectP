#include "CPP_LogoutRequestAsyncAction.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 비동기 액션 객체를 생성하는 함수
// WorldContextObject : 월드 컨텍스트 객체, 일반적으로 위젯/액터/컴포넌트 등이 들어감
// LoginToken : 로그인 성공 시 서버에서 받은 원본 로그인 토큰
// ServerUrl : 로그아웃 요청을 보낼 서버 URL, 비어 있으면 ServerConfigSubsystem의 로그아웃 URL을 사용함
// Return Value : UCPP_LogoutRequestAsyncAction 객체 포인터, 요청 완료 시 OnSuccess 또는 OnFailure로 결과를 전달함
UCPP_LogoutRequestAsyncAction* UCPP_LogoutRequestAsyncAction::RequestLogout(
    UObject* WorldContextObject,
    const FString& LoginToken,
    const FString& ServerUrl)
{
    UCPP_LogoutRequestAsyncAction* Action = NewObject<UCPP_LogoutRequestAsyncAction>();
    Action->WorldContextObject = WorldContextObject;
    Action->RequestLoginToken = LoginToken;
    Action->RequestServerUrl = ServerUrl;
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청을 실제 서버로 보내는 함수
void UCPP_LogoutRequestAsyncAction::Activate()
{
    if (RequestServerUrl.IsEmpty())
    {
        UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
        UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
        if (const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr)
        {
            RequestServerUrl = ServerConfigSubsystem->GetLogoutRequestUrl();
        }
    }

    if (RequestServerUrl.IsEmpty())
    {
        BroadcastFailure(TEXT("Logout server URL is empty."));
        return;
    }

    if (RequestLoginToken.IsEmpty())
    {
        BroadcastFailure(TEXT("Login token is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), RequestLoginToken);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(RequestServerUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCPP_LogoutRequestAsyncAction::HandleResponse);

    if (!HttpRequest->ProcessRequest())
    {
        BroadcastFailure(TEXT("Failed to start logout request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청에 대한 서버 응답을 처리하는 함수
// Request : 서버로 보낸 HTTP 요청 객체
// Response : 서버에서 받은 HTTP 응답 객체
// bWasSuccessful : 서버와의 HTTP 통신 자체가 정상적으로 완료되었는지 여부, 로그아웃 성공 여부는 ResponseCode와 ok 필드를 추가로 확인함
void UCPP_LogoutRequestAsyncAction::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastFailure(TEXT("Logout server request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        BroadcastFailure(FString::Printf(TEXT("Invalid logout response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        BroadcastFailure(Message.IsEmpty() ? FString::Printf(TEXT("Logout failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    OnSuccess.Broadcast(Message);
    SetReadyToDestroy();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청이 실패했을 때 결과를 전달하는 함수
// Message : 로그아웃 요청 실패 이유를 설명하는 메시지
void UCPP_LogoutRequestAsyncAction::BroadcastFailure(const FString& Message)
{
    OnFailure.Broadcast(Message);
    SetReadyToDestroy();
}
