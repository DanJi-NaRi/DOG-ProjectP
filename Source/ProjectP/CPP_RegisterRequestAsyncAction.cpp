#include "CPP_RegisterRequestAsyncAction.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 회원 가입 요청 비동기 액션 객체를 생성하는 함수
// WorldContextObject : 월드 컨텍스트 객체, 일반적으로는 액터나 컴포넌트의 포인터가 들어감
// ID : 회원 가입에 사용할 ID
// Password : 회원 가입에 사용할 비밀번호
// Username : 회원 가입에 사용할 사용자 이름
// ServerUrl : 회원 가입 요청을 보낼 서버 URL, 비어 있으면 ServerConfigSubsystem의 회원 가입 URL을 사용함
// Return Value : UCPP_RegisterRequestAsyncAction 객체의 포인터, 요청 완료 시 OnSuccess 또는 OnFailure로 결과를 전달함
UCPP_RegisterRequestAsyncAction* UCPP_RegisterRequestAsyncAction::RequestRegister(
    UObject* WorldContextObject,
    const FString& ID,
    const FString& Password,
    const FString& Username,
    const FString& ServerUrl)
{
    UCPP_RegisterRequestAsyncAction* Action = NewObject<UCPP_RegisterRequestAsyncAction>();
    Action->WorldContextObject = WorldContextObject;
    Action->RequestID = ID;
    Action->RequestPassword = Password;
    Action->RequestUsername = Username;
    Action->RequestServerUrl = ServerUrl;
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 회원 가입 요청을 실제 서버로 보내는 함수
void UCPP_RegisterRequestAsyncAction::Activate()
{
    if (RequestServerUrl.IsEmpty())
    {
        UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
        UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
        if (const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr)
        {
            RequestServerUrl = ServerConfigSubsystem->GetRegisterRequestUrl();
        }
    }

    if (RequestServerUrl.IsEmpty())
    {
        BroadcastFailure(TEXT("Register server URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("ID"), RequestID);
    JsonObject->SetStringField(TEXT("password"), RequestPassword);
    JsonObject->SetStringField(TEXT("username"), RequestUsername);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(RequestServerUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCPP_RegisterRequestAsyncAction::HandleResponse);

    if (!HttpRequest->ProcessRequest())
    {
        BroadcastFailure(TEXT("Failed to start register request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 회원 가입 요청에 대한 서버 응답을 처리하는 함수
// Request : 서버로 보낸 HTTP 요청 객체
// Response : 서버에서 받은 HTTP 응답 객체
// bWasSuccessful : 서버와의 HTTP 통신 자체가 정상적으로 완료되었는지 여부, 회원 가입 성공 여부는 ResponseCode와 ok 필드를 추가로 확인함
void UCPP_RegisterRequestAsyncAction::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        BroadcastFailure(TEXT("Register server request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        BroadcastFailure(FString::Printf(TEXT("Invalid register response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        BroadcastFailure(Message.IsEmpty() ? FString::Printf(TEXT("Register failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    const TSharedPtr<FJsonObject>* UserObject = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("user"), UserObject) || UserObject == nullptr || !UserObject->IsValid())
    {
        BroadcastFailure(TEXT("Register response is missing user data."));
        return;
    }

    double UserIndexNumber = 0.0;
    FString ID;
    FString Username;
    (*UserObject)->TryGetNumberField(TEXT("user_Index"), UserIndexNumber);
    (*UserObject)->TryGetStringField(TEXT("ID"), ID);
    (*UserObject)->TryGetStringField(TEXT("username"), Username);

    OnSuccess.Broadcast(static_cast<int32>(UserIndexNumber), ID, Username, Message);
    SetReadyToDestroy();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 회원 가입 요청이 실패했을 때 결과를 전달하는 함수
// Message : 회원 가입 요청이 실패한 이유를 설명하는 메시지
void UCPP_RegisterRequestAsyncAction::BroadcastFailure(const FString& Message)
{
    OnFailure.Broadcast(0, FString(), FString(), Message);
    SetReadyToDestroy();
}
