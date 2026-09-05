#include "SessionTravelSubsystem.h"

#include "AccountSessionSubsystem.h"
#include "ServerConfigSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"

namespace SessionTravelConstants
{
    static constexpr float LobbyTravelTimeoutSeconds = 15.0f;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 이동 서브시스템이 생성될 때 로비 이동 결과 감지를 위한 엔진 델리게이트를 등록하는 함수
// Collection : 현재 GameInstance의 서브시스템 컬렉션
void USessionTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (GEngine)
    {
        GEngine->OnNetworkFailure().RemoveAll(this);
        GEngine->OnNetworkFailure().AddUObject(this, &USessionTravelSubsystem::HandleLobbyNetworkFailure);

        GEngine->OnTravelFailure().RemoveAll(this);
        GEngine->OnTravelFailure().AddUObject(this, &USessionTravelSubsystem::HandleLobbyTravelFailure);
    }

    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USessionTravelSubsystem::HandlePostLoadMapWithWorld);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 이동 서브시스템이 정리될 때 엔진 델리게이트와 로비 이동 대기 상태를 정리하는 함수
void USessionTravelSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().RemoveAll(this);
        GEngine->OnTravelFailure().RemoveAll(this);
    }

    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

    ClearLobbyTravelWait();
    bVerifySessionAndTravelInFlight = false;
    bDungeonMemberStateQueryInFlight = false;
    PendingLobbyTravelPlayerController.Reset();

    Super::Deinitialize();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 전에 필요한 주소와 로그인 토큰 상태를 검증하는 함수
// PlayerController : 로비 서버로 이동할 플레이어 컨트롤러
// OutLobbyServerAddress : 검증에 성공했을 때 사용할 로비 서버 주소
// Return Value : 로비 서버 이동 준비가 가능하면 true, 아니면 false
bool USessionTravelSubsystem::PrepareLobbyServerTravel(APlayerController* PlayerController, FString& OutLobbyServerAddress) const
{
    OutLobbyServerAddress.Empty();

    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to lobby server because PlayerController is null."));
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString LobbyServerAddress = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetLobbyServerAddress()
        : TEXT("");
    if (LobbyServerAddress.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to lobby server because LobbyServerAddress is empty."));
        return false;
    }

    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    const bool bRequireLobbyTokenVerification = ServerConfigSubsystem
        ? ServerConfigSubsystem->IsLobbyTokenVerificationRequired()
        : true;
    if (bRequireLobbyTokenVerification && (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to lobby server because login token is empty."));
        return false;
    }

    OutLobbyServerAddress = LobbyServerAddress;
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어를 로비 서버로 이동시키는 함수
// PlayerController : 로비 서버로 이동시킬 플레이어 컨트롤러
// Return Value : 로비 서버 이동 요청 성공 여부
bool USessionTravelSubsystem::TravelToLobbyServer(APlayerController* PlayerController)
{
    FString LobbyServerAddress;
    if (!PrepareLobbyServerTravel(PlayerController, LobbyServerAddress))
    {
        return false;
    }

    BeginLobbyTravelWait();
    PlayerController->ClientTravel(LobbyServerAddress, TRAVEL_Absolute);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어를 던전 서버로 이동시키는 함수
// PlayerController : 던전 서버로 이동할 플레이어 컨트롤러
// OverrideDungeonServerAddress : 설정 주소 대신 사용할 던전 서버 주소, 비어 있으면 설정 주소를 사용함
// Return Value : 던전 서버 이동 요청 성공 여부
bool USessionTravelSubsystem::TravelToDungeonServer(APlayerController* PlayerController, const FString& OverrideDungeonServerAddress) const
{
    if (!PlayerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to dungeon server because PlayerController is null."));
        return false;
    }

    FString DungeonServerAddress = OverrideDungeonServerAddress;
    DungeonServerAddress.TrimStartAndEndInline();
    if (DungeonServerAddress.IsEmpty())
    {
        const UGameInstance* GameInstance = GetGameInstance();
        const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
            ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
            : nullptr;
        DungeonServerAddress = ServerConfigSubsystem
            ? ServerConfigSubsystem->GetDungeonServerAddress()
            : TEXT("");
    }

    if (DungeonServerAddress.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to dungeon server because DungeonServerAddress is empty."));
        return false;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    if (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel to dungeon server because login token is empty."));
        return false;
    }

    PlayerController->ClientTravel(DungeonServerAddress, TRAVEL_Absolute);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 상태가 던전 복귀 대상인지 확인하는 함수
// ConnectionState : GameBackend에서 조회한 던전 멤버 접속 상태
// Return Value : 던전 서버로 이동해야 하면 true, 로비 서버로 이동해야 하면 false
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 세션을 검증한 뒤 성공하면 로비 또는 던전 서버 이동을 진행하는 함수
// PlayerController : 세션 검증 후 이동시킬 플레이어 컨트롤러
void USessionTravelSubsystem::VerifySessionAndTravelToLobby(APlayerController* PlayerController)
{
    if (bVerifySessionAndTravelInFlight || bDungeonMemberStateQueryInFlight)
    {
        BroadcastLobbyTravelResult(false, TEXT("Session travel is already in progress."));
        return;
    }

    if (!PlayerController)
    {
        BroadcastLobbyTravelResult(false, TEXT("Cannot travel to lobby server because PlayerController is null."));
        return;
    }

    if (IsLobbyTravelWaitInProgress())
    {
        UE_LOG(LogTemp, Warning, TEXT("Lobby server travel is already in progress."));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    if (AccountSessionSubsystem == nullptr || AccountSessionSubsystem->GetLoginToken().IsEmpty())
    {
        BroadcastLobbyTravelResult(false, TEXT("Cannot verify session because login token is empty."));
        return;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString VerifyUrl = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetSessionVerifyUrl()
        : TEXT("");
    if (VerifyUrl.IsEmpty())
    {
        BroadcastLobbyTravelResult(false, TEXT("Session verify URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), AccountSessionSubsystem->GetLoginToken());

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    bVerifySessionAndTravelInFlight = true;
    PendingLobbyTravelPlayerController = PlayerController;

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(VerifyUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &USessionTravelSubsystem::HandleVerifySessionAndTravelResponse);

    if (!HttpRequest->ProcessRequest())
    {
        bVerifySessionAndTravelInFlight = false;
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, TEXT("Failed to start session verify request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 검증 요청 응답을 처리하고 성공 시 던전 멤버 상태 조회를 진행하는 함수
// Request : 완료된 HTTP 요청 객체
// Response : 로그인 서버에서 받은 HTTP 응답 객체
// bWasSuccessful : HTTP 요청 처리 성공 여부
void USessionTravelSubsystem::HandleVerifySessionAndTravelResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    bVerifySessionAndTravelInFlight = false;

    if (!bWasSuccessful || !Response.IsValid())
    {
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, TEXT("Session verify request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, FString::Printf(TEXT("Invalid session verify response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, Message.IsEmpty() ? FString::Printf(TEXT("Session verify failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    const TSharedPtr<FJsonObject>* UserObject = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("user"), UserObject) || UserObject == nullptr || !UserObject->IsValid())
    {
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, TEXT("Session verify response is missing user data."));
        return;
    }

    double UserIndexNumber = 0.0;
    FString VerifiedUsername;
    (*UserObject)->TryGetNumberField(TEXT("user_Index"), UserIndexNumber);
    (*UserObject)->TryGetStringField(TEXT("username"), VerifiedUsername);

    const int32 VerifiedUserIndex = static_cast<int32>(UserIndexNumber);
    if (VerifiedUserIndex <= 0 || VerifiedUsername.IsEmpty())
    {
        PendingLobbyTravelPlayerController.Reset();
        BroadcastLobbyTravelResult(false, TEXT("Session verify response has invalid user data."));
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance->GetSubsystem<UAccountSessionSubsystem>())
        {
            AccountSessionSubsystem->SetLoginInfo(VerifiedUserIndex, VerifiedUsername);
        }
    }

    APlayerController* PlayerController = PendingLobbyTravelPlayerController.Get();
    PendingLobbyTravelPlayerController.Reset();

    QueryDungeonMemberStateAndTravel(PlayerController);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 세션 검증 후 던전 멤버 상태를 조회하고 상태에 맞는 서버 이동을 진행하는 함수
// PlayerController : 던전 멤버 상태 조회 후 이동시킬 플레이어 컨트롤러
void USessionTravelSubsystem::QueryDungeonMemberStateAndTravel(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        BroadcastLobbyTravelResult(false, TEXT("Cannot query dungeon member state because PlayerController is null."));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString QueryUrl = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetDungeonMemberStateQueryUrl()
        : TEXT("");
    if (QueryUrl.IsEmpty())
    {
        FallbackTravelToLobbyAfterDungeonStateQuery(PlayerController, TEXT("Dungeon member state query URL is empty."));
        return;
    }

    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    const FString LoginToken = AccountSessionSubsystem
        ? AccountSessionSubsystem->GetLoginToken()
        : TEXT("");

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), LoginToken);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    bDungeonMemberStateQueryInFlight = true;
    PendingLobbyTravelPlayerController = PlayerController;

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(QueryUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &USessionTravelSubsystem::HandleDungeonMemberStateQueryResponse);

    if (!HttpRequest->ProcessRequest())
    {
        bDungeonMemberStateQueryInFlight = false;
        PendingLobbyTravelPlayerController.Reset();
        FallbackTravelToLobbyAfterDungeonStateQuery(PlayerController, TEXT("Failed to start dungeon member state query request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 상태 조회 응답을 처리하고 로비 또는 던전 서버 이동을 결정하는 함수
// Request : 완료된 HTTP 요청 객체
// Response : GameBackend에서 받은 HTTP 응답 객체
// bWasSuccessful : HTTP 요청 처리 성공 여부
void USessionTravelSubsystem::HandleDungeonMemberStateQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    bDungeonMemberStateQueryInFlight = false;

    APlayerController* PlayerController = PendingLobbyTravelPlayerController.Get();
    PendingLobbyTravelPlayerController.Reset();

    if (!bWasSuccessful || !Response.IsValid())
    {
        FallbackTravelToLobbyAfterDungeonStateQuery(PlayerController, TEXT("Dungeon member state query request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        FallbackTravelToLobbyAfterDungeonStateQuery(PlayerController, FString::Printf(TEXT("Invalid dungeon member state query response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        FallbackTravelToLobbyAfterDungeonStateQuery(PlayerController, Message.IsEmpty() ? FString::Printf(TEXT("Dungeon member state query failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    FString ConnectionState = TEXT("Offline");
    FString DungeonSessionId;
    bool bDungeonSessionJoinable = false;
    const TSharedPtr<FJsonObject>* StateObject = nullptr;
    if (JsonObject->TryGetObjectField(TEXT("state"), StateObject) && StateObject && StateObject->IsValid())
    {
        (*StateObject)->TryGetStringField(TEXT("connectionState"), ConnectionState);
        (*StateObject)->TryGetStringField(TEXT("dungeonSessionId"), DungeonSessionId);
        (*StateObject)->TryGetBoolField(TEXT("isJoinable"), bDungeonSessionJoinable);
    }

    ConnectionState.TrimStartAndEndInline();
    DungeonSessionId.TrimStartAndEndInline();

    if (ConnectionState.Equals(TEXT("InGame"), ESearchCase::IgnoreCase))
    {
        BroadcastLobbyTravelResult(false, TEXT("이미 던전에 접속 중인 계정입니다."));
        return;
    }

    if (ShouldReconnectToDungeon(ConnectionState, bDungeonSessionJoinable))
    {
        if (!TravelToDungeonServer(PlayerController, DungeonSessionId))
        {
            BroadcastLobbyTravelResult(false, TEXT("Dungeon member state found, but dungeon travel failed."));
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("Session verified. Reconnecting to dungeon server. State: %s, Joinable: %s, SessionId: %s, Address: %s"), *ConnectionState, bDungeonSessionJoinable ? TEXT("true") : TEXT("false"), *DungeonSessionId, *DungeonSessionId);
        BroadcastLobbyTravelResult(true, TEXT("던전 서버로 복귀합니다."));
        return;
    }

    if (!TravelToLobbyServer(PlayerController))
    {
        BroadcastLobbyTravelResult(false, TEXT("Session verified, but lobby travel failed."));
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString LobbyServerAddress = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetLobbyServerAddress()
        : TEXT("");
    UE_LOG(LogTemp, Warning, TEXT("Session verified. Waiting for lobby server travel result: %s, DungeonState: %s"), *LobbyServerAddress, *ConnectionState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 상태 조회 실패 시 로비 서버 이동으로 대체하는 함수
// PlayerController : 로비 서버로 이동시킬 플레이어 컨트롤러
// ReasonMessage : 던전 멤버 상태 조회를 사용하지 못한 이유
void USessionTravelSubsystem::FallbackTravelToLobbyAfterDungeonStateQuery(APlayerController* PlayerController, const FString& ReasonMessage)
{
    UE_LOG(LogTemp, Warning, TEXT("Falling back to lobby travel after dungeon member state query. Reason: %s"), *ReasonMessage);

    if (!TravelToLobbyServer(PlayerController))
    {
        BroadcastLobbyTravelResult(false, ReasonMessage.IsEmpty() ? TEXT("Session verified, but lobby travel failed.") : ReasonMessage);
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    const FString LobbyServerAddress = ServerConfigSubsystem
        ? ServerConfigSubsystem->GetLobbyServerAddress()
        : TEXT("");
    UE_LOG(LogTemp, Warning, TEXT("Session verified. Waiting for lobby server travel result: %s"), *LobbyServerAddress);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 상태와 세션 입장 가능 여부를 기준으로 던전 재접속 대상인지 확인하는 함수
// ConnectionState : GameBackend에서 조회한 던전 멤버 접속 상태
// bDungeonSessionJoinable : GameBackend가 해당 던전 세션으로 클라이언트를 다시 보내도 된다고 판단했는지 여부
// Return Value : 던전 서버로 재접속해야 하면 true, 로비 서버로 이동해야 하면 false
bool USessionTravelSubsystem::ShouldReconnectToDungeon(const FString& ConnectionState, bool bDungeonSessionJoinable) const
{
    return ConnectionState.Equals(TEXT("OutGame"), ESearchCase::IgnoreCase)
        && bDungeonSessionJoinable;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 결과를 기다리는 상태로 전환하는 함수
void USessionTravelSubsystem::BeginLobbyTravelWait()
{
    bWaitingForLobbyTravelResult = true;

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start lobby travel timeout because GameInstance is null."));
        return;
    }

    GameInstance->GetTimerManager().ClearTimer(LobbyTravelTimeoutTimerHandle);
    GameInstance->GetTimerManager().SetTimer(
        LobbyTravelTimeoutTimerHandle,
        this,
        &USessionTravelSubsystem::HandleLobbyTravelTimeout,
        SessionTravelConstants::LobbyTravelTimeoutSeconds,
        false);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 대기 상태와 타임아웃 타이머를 초기화하는 함수
void USessionTravelSubsystem::ClearLobbyTravelWait()
{
    bWaitingForLobbyTravelResult = false;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().ClearTimer(LobbyTravelTimeoutTimerHandle);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 결과를 기다리는 중인지 반환하는 함수
// Return Value : 로비 서버 이동 결과 대기 중이면 true, 아니면 false
bool USessionTravelSubsystem::IsLobbyTravelWaitInProgress() const
{
    return bWaitingForLobbyTravelResult;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동이 정해진 시간 안에 완료되지 않았을 때 실패 결과를 전달하는 함수
void USessionTravelSubsystem::HandleLobbyTravelTimeout()
{
    if (!bWaitingForLobbyTravelResult)
    {
        return;
    }

    ClearLobbyTravelWait();
    BroadcastLobbyTravelResult(false, TEXT("로비 서버 연결 시간이 초과되었습니다. 로비 서버가 실행 중인지 확인해 주세요."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 맵 로드 완료 시 로비 서버 이동 성공 결과를 전달하는 함수
// LoadedWorld : 새로 로드된 월드
void USessionTravelSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
    if (!bWaitingForLobbyTravelResult)
    {
        return;
    }

    ClearLobbyTravelWait();
    BroadcastLobbyTravelResult(true, TEXT("로비 서버에 접속했습니다."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 접속 중 발생한 네트워크 실패를 처리하는 함수
// World : 실패가 발생한 월드
// NetDriver : 실패가 발생한 네트워크 드라이버
// FailureType : 언리얼 엔진이 전달한 네트워크 실패 타입
// ErrorString : 엔진 또는 서버가 전달한 상세 오류 문자열
void USessionTravelSubsystem::HandleLobbyNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    if (!bWaitingForLobbyTravelResult)
    {
        return;
    }

    const FString FailureMessage = BuildLobbyNetworkFailureMessage(FailureType, ErrorString);
    ClearLobbyTravelWait();
    BroadcastLobbyTravelResult(false, FailureMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 중 발생한 트래블 실패를 처리하는 함수
// World : 실패가 발생한 월드
// FailureType : 언리얼 엔진이 전달한 트래블 실패 타입
// ErrorString : 엔진이 전달한 상세 오류 문자열
void USessionTravelSubsystem::HandleLobbyTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    if (!bWaitingForLobbyTravelResult)
    {
        return;
    }

    const FString FailureMessage = BuildLobbyTravelFailureMessage(FailureType, ErrorString);
    ClearLobbyTravelWait();
    BroadcastLobbyTravelResult(false, FailureMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 네트워크 실패 타입과 상세 문자열을 UI에 보여줄 로비 접속 실패 메시지로 변환하는 함수
// FailureType : 언리얼 엔진이 전달한 네트워크 실패 타입
// ErrorString : 엔진 또는 서버가 전달한 상세 오류 문자열
// Return Value : UI에 표시할 실패 메시지
FString USessionTravelSubsystem::BuildLobbyNetworkFailureMessage(ENetworkFailure::Type FailureType, const FString& ErrorString) const
{
    if (IsLobbyVersionMismatchNetworkFailure(FailureType, ErrorString))
    {
        return TEXT("로비 서버와 클라이언트 버전이 달라 핸드셰이크에 실패했습니다. 같은 빌드로 실행해 주세요.");
    }

    switch (FailureType)
    {
    case ENetworkFailure::ConnectionTimeout:
        return TEXT("로비 서버 연결 시간이 초과되었습니다. 로비 서버가 실행 중인지 확인해 주세요.");
    case ENetworkFailure::ConnectionLost:
        return TEXT("로비 서버와의 연결이 끊어졌습니다.");
    case ENetworkFailure::PendingConnectionFailure:
        return ErrorString.IsEmpty() ? TEXT("로비 서버에 접속하지 못했습니다.") : FString::Printf(TEXT("로비 서버에 접속하지 못했습니다. %s"), *ErrorString);
    case ENetworkFailure::FailureReceived:
        return ErrorString.IsEmpty() ? TEXT("로비 서버가 접속을 거부했습니다.") : FString::Printf(TEXT("로비 서버가 접속을 거부했습니다. %s"), *ErrorString);
    case ENetworkFailure::NetDriverCreateFailure:
        return TEXT("로비 서버 연결을 시작하지 못했습니다.");
    default:
        return ErrorString.IsEmpty() ? TEXT("로비 서버 접속 중 네트워크 오류가 발생했습니다.") : FString::Printf(TEXT("로비 서버 접속 중 네트워크 오류가 발생했습니다. %s"), *ErrorString);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 트래블 실패 타입과 상세 문자열을 UI에 보여줄 로비 이동 실패 메시지로 변환하는 함수
// FailureType : 언리얼 엔진이 전달한 트래블 실패 타입
// ErrorString : 엔진이 전달한 상세 오류 문자열
// Return Value : UI에 표시할 실패 메시지
FString USessionTravelSubsystem::BuildLobbyTravelFailureMessage(ETravelFailure::Type FailureType, const FString& ErrorString) const
{
    if (IsLobbyVersionMismatchTravelFailure(FailureType, ErrorString))
    {
        return TEXT("로비 서버와 클라이언트 버전이 달라 핸드셰이크에 실패했습니다. 같은 빌드로 실행해 주세요.");
    }

    switch (FailureType)
    {
    case ETravelFailure::InvalidURL:
        return TEXT("로비 서버 주소가 올바르지 않습니다.");
    case ETravelFailure::PackageMissing:
    case ETravelFailure::NoDownload:
        return TEXT("로비 이동에 필요한 맵 또는 패키지를 불러올 수 없습니다.");
    case ETravelFailure::PendingNetGameCreateFailure:
    case ETravelFailure::ClientTravelFailure:
    case ETravelFailure::TravelFailure:
        return ErrorString.IsEmpty() ? TEXT("로비 서버로 이동하지 못했습니다.") : FString::Printf(TEXT("로비 서버로 이동하지 못했습니다. %s"), *ErrorString);
    default:
        return ErrorString.IsEmpty() ? TEXT("로비 서버 이동 중 오류가 발생했습니다.") : FString::Printf(TEXT("로비 서버 이동 중 오류가 발생했습니다. %s"), *ErrorString);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 네트워크 실패가 로비 서버와 클라이언트의 버전 불일치로 보이는지 확인하는 함수
// FailureType : 언리얼 엔진이 전달한 네트워크 실패 타입
// ErrorString : 엔진 또는 서버가 전달한 상세 오류 문자열
// Return Value : 버전 불일치로 판단되면 true, 아니면 false
bool USessionTravelSubsystem::IsLobbyVersionMismatchNetworkFailure(ENetworkFailure::Type FailureType, const FString& ErrorString) const
{
    switch (FailureType)
    {
    case ENetworkFailure::OutdatedClient:
    case ENetworkFailure::OutdatedServer:
    case ENetworkFailure::NetChecksumMismatch:
    case ENetworkFailure::NetGuidMismatch:
        return true;
    default:
        break;
    }

    return ErrorString.Contains(TEXT("incompatible"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("outdated"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("version"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("NetCL"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("checksum"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("guid mismatch"), ESearchCase::IgnoreCase);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 트래블 실패가 로비 서버와 클라이언트의 버전 불일치로 보이는지 확인하는 함수
// FailureType : 언리얼 엔진이 전달한 트래블 실패 타입
// ErrorString : 엔진이 전달한 상세 오류 문자열
// Return Value : 버전 불일치로 판단되면 true, 아니면 false
bool USessionTravelSubsystem::IsLobbyVersionMismatchTravelFailure(ETravelFailure::Type FailureType, const FString& ErrorString) const
{
    if (FailureType == ETravelFailure::PackageVersion)
    {
        return true;
    }

    return ErrorString.Contains(TEXT("incompatible"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("outdated"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("version"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("NetCL"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("checksum"), ESearchCase::IgnoreCase)
        || ErrorString.Contains(TEXT("handshake"), ESearchCase::IgnoreCase);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 결과를 외부 구독자에게 전달하는 함수
// bSuccess : 로비 서버 이동 결과 성공 여부
// Message : 결과 메시지
void USessionTravelSubsystem::BroadcastLobbyTravelResult(bool bSuccess, const FString& Message)
{
    OnLobbyTravelResult.Broadcast(bSuccess, Message);
    OnVerifySessionAndTravelToLobbyResult.Broadcast(bSuccess, Message);
}
