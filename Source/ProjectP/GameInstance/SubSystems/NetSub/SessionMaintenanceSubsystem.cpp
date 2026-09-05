#include "SessionMaintenanceSubsystem.h"

#include "AccountSessionSubsystem.h"
#include "ServerConfigSubsystem.h"
#include "../../../Dungeon/DungeonPC.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace SessionMaintenanceConstants
{
    static constexpr float SessionPingIntervalSeconds = 60.0f;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 유지 서브시스템이 정리될 때 종료 로그아웃 요청과 세션 ping 타이머를 정리하는 함수
void USessionMaintenanceSubsystem::Deinitialize()
{
    RequestLogoutOnShutdown();
    StopSessionPing();

    Super::Deinitialize();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 ping 타이머를 시작하는 함수
void USessionMaintenanceSubsystem::StartSessionPing()
{
    const UAccountSessionSubsystem* AccountSessionSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    if (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SessionPingTimerHandle);
    World->GetTimerManager().SetTimer(
        SessionPingTimerHandle,
        this,
        &USessionMaintenanceSubsystem::SendSessionPing,
        SessionMaintenanceConstants::SessionPingIntervalSeconds,
        true,
        SessionMaintenanceConstants::SessionPingIntervalSeconds);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 ping 타이머를 중지하고 진행 중 플래그를 초기화하는 함수
void USessionMaintenanceSubsystem::StopSessionPing()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SessionPingTimerHandle);
    }

    bSessionPingInFlight = false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 ping 요청을 서버로 보내는 함수
void USessionMaintenanceSubsystem::SendSessionPing()
{
    UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    if (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty() || bSessionPingInFlight)
    {
        return;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString SessionPingUrl = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetSessionPingUrl()
        : TEXT("");
    if (SessionPingUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Session ping URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), AccountSessionSubsystem->GetLoginToken());

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    bSessionPingInFlight = true;

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(SessionPingUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);

    TWeakObjectPtr<USessionMaintenanceSubsystem> WeakThis(this);
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            USessionMaintenanceSubsystem* SessionMaintenanceSubsystem = WeakThis.Get();
            if (SessionMaintenanceSubsystem == nullptr)
            {
                return;
            }

            SessionMaintenanceSubsystem->bSessionPingInFlight = false;

            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("Session ping request failed."));
                return;
            }

            const int32 ResponseCode = Response->GetResponseCode();
            if (ResponseCode == 401)
            {
                UE_LOG(LogTemp, Warning, TEXT("Session ping token is inactive."));
                SessionMaintenanceSubsystem->StopSessionPing();

                if (UGameInstance* GameInstance = SessionMaintenanceSubsystem->GetGameInstance())
                {
                    if (UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance->GetSubsystem<UAccountSessionSubsystem>())
                    {
                        AccountSessionSubsystem->ClearLoginInfo();
                    }
                }

                return;
            }

            if (ResponseCode < 200 || ResponseCode >= 300)
            {
                UE_LOG(LogTemp, Warning, TEXT("Session ping failed. HTTP %d"), ResponseCode);
            }
        });

    if (!HttpRequest->ProcessRequest())
    {
        bSessionPingInFlight = false;
        UE_LOG(LogTemp, Warning, TEXT("Failed to start session ping request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 게임 인스턴스 종료 시 로그인 서버에 로그아웃 요청을 보내는 함수
void USessionMaintenanceSubsystem::RequestLogoutOnShutdown() const
{
    UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    if (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty())
    {
        return;
    }

    if (ShouldSkipLogoutOnShutdownForDungeonReconnect())
    {
        UE_LOG(LogTemp, Log, TEXT("Skip logout on shutdown because dungeon server must report OutGame state."));
        return;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString LogoutRequestUrl = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetLogoutRequestUrl()
        : TEXT("");
    if (LogoutRequestUrl.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Logout request URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), AccountSessionSubsystem->GetLoginToken());

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(LogoutRequestUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Connection"), TEXT("close"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->ProcessRequest();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 재접속 보존을 위해 종료 로그아웃 요청을 건너뛰어야 하는지 확인하는 함수
// Return Value : 현재 로컬 PlayerController가 던전 PlayerController이면 true, 아니면 false
bool USessionMaintenanceSubsystem::ShouldSkipLogoutOnShutdownForDungeonReconnect() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const APlayerController* PlayerController = GameInstance
        ? GameInstance->GetFirstLocalPlayerController()
        : nullptr;
    return PlayerController && PlayerController->IsA(ADungeonPC::StaticClass());
}
