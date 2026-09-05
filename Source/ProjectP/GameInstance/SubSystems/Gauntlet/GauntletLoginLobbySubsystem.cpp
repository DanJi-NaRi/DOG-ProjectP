#include "GauntletLoginLobbySubsystem.h"

#include "../../../Dungeon/DungeonGS.h"
#include "../../../Dungeon/DungeonPC.h"
#include "../../../GAS/MyPlayerState.h"
#include "../../../LoginRequestAsyncAction.h"
#include "../../../Lobby/CPP_LobbyGSB.h"
#include "../../../Lobby/CPP_LobbyPC.h"
#include "../../../Lobby/CPP_LobbyPS.h"
#include "../NetSub/LoginFlowSubsystem.h"
#include "../NetSub/ServerConfigSubsystem.h"
#include "../NetSub/SessionTravelSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace GauntletLoginLobby
{
    static constexpr TCHAR TestFlag[] = TEXT("GauntletLoginLobbyTest");
    static constexpr TCHAR PartyDungeonTestFlag[] = TEXT("GauntletPartyDungeonFlowTest");
    static constexpr TCHAR ClientIndexKey[] = TEXT("GauntletClientIndex=");
    static constexpr TCHAR FailureCaseKey[] = TEXT("GauntletLoginFailureCase=");
    static constexpr TCHAR CredentialsFileKey[] = TEXT("GauntletCredentialsFile=");
    static constexpr TCHAR DuplicateLoginFailureCase[] = TEXT("DuplicateLogin");
    static constexpr TCHAR WrongPasswordFailureCase[] = TEXT("WrongPassword");
    static constexpr TCHAR WrongIDFailureCase[] = TEXT("WrongID");
    static constexpr TCHAR ClearLoginTokenEndpoint[] = TEXT("/api/test/clear-login-token");
    static constexpr TCHAR TestAuthHeaderName[] = TEXT("X-Gauntlet-Test-Auth");
    static constexpr float StartDelaySeconds = 0.5f;
    static constexpr float LobbyAuthPollIntervalSeconds = 0.25f;
    static constexpr float LobbyAuthTimeoutSeconds = 15.0f;
    static constexpr float PartyDungeonPollIntervalSeconds = 0.25f;
    static constexpr float PartyDungeonStepTimeoutSeconds = 60.0f;
    static constexpr float PartyDungeonTravelTimeoutSeconds = 120.0f;
    static constexpr int32 PartyDungeonClientCount = 3;
    static constexpr int32 Client1Index = 1;
    static constexpr int32 Client2Index = 2;
    static constexpr int32 Client3Index = 3;
    static constexpr int32 Client1CharacterId = 100;
    static constexpr int32 Client2CharacterId = 200;
    static constexpr int32 Client3CharacterId = 300;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameInstance Subsystem 초기화 시 Gauntlet 로그인/로비 테스트 요청 여부를 확인하고 시작 타이머를 예약하는 함수
// Collection : 현재 GameInstance의 Subsystem 컬렉션
void UGauntletLoginLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (!FParse::Param(FCommandLine::Get(), GauntletLoginLobby::TestFlag) &&
        !FParse::Param(FCommandLine::Get(), GauntletLoginLobby::PartyDungeonTestFlag))
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().SetTimer(
            StartTestTimerHandle,
            this,
            &UGauntletLoginLobbySubsystem::StartTestIfRequested,
            GauntletLoginLobby::StartDelaySeconds,
            false);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameInstance Subsystem 정리 시 테스트 요청, 타이머, 로비 이동 대기 상태를 정리하는 함수
void UGauntletLoginLobbySubsystem::Deinitialize()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().ClearTimer(StartTestTimerHandle);
        GameInstance->GetTimerManager().ClearTimer(LobbyAuthWaitTimerHandle);
        GameInstance->GetTimerManager().ClearTimer(PartyDungeonFlowTimerHandle);
    }

    ClearActiveLoginRequest();
    ActiveClearLoginTokenRequest.Reset();
    ClearLobbyTravelWait();
    ClearPartyDungeonFlowWait();
    bIsRunning = false;

    Super::Deinitialize();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 커맨드라인 테스트 플래그가 있을 때 credentials를 읽고 기대 실패 로그인부터 시작하는 함수
void UGauntletLoginLobbySubsystem::StartTestIfRequested()
{
    if (bIsRunning ||
        (!FParse::Param(FCommandLine::Get(), GauntletLoginLobby::TestFlag) &&
            !FParse::Param(FCommandLine::Get(), GauntletLoginLobby::PartyDungeonTestFlag)))
    {
        return;
    }

    bIsRunning = true;

    FString FailureReason;
    if (!ReadCommandLine(FailureReason) || !LoadCredentials(FailureReason))
    {
        FinishFailure(FailureReason);
        return;
    }

    if (bRunPartyDungeonFlow)
    {
        UE_LOG(LogTemp, Display, TEXT("Starting Gauntlet party dungeon flow test. ClientIndex: %d"), ClientIndex);
        RunSuccessLogin();
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("Starting Gauntlet login lobby test. ClientIndex: %d, FailureCase: %s"), ClientIndex, *FailureCase);
    RunExpectedFailureLogin();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Gauntlet TestNode가 클라이언트에 주입한 테스트 커맨드라인 값을 읽는 함수
// OutFailureReason : 커맨드라인 파싱 실패 시 실패 이유를 반환함
// Return Value : 필수 커맨드라인 값을 모두 읽었으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::ReadCommandLine(FString& OutFailureReason)
{
    const bool bHasLoginLobbyFlag = FParse::Param(FCommandLine::Get(), GauntletLoginLobby::TestFlag);
    bRunPartyDungeonFlow = FParse::Param(FCommandLine::Get(), GauntletLoginLobby::PartyDungeonTestFlag);
    if (bHasLoginLobbyFlag && bRunPartyDungeonFlow)
    {
        OutFailureReason = TEXT("Only one Gauntlet login lobby test flag can be used at a time.");
        return false;
    }

    FString ClientIndexString;
    if (!FParse::Value(FCommandLine::Get(), GauntletLoginLobby::ClientIndexKey, ClientIndexString))
    {
        OutFailureReason = TEXT("GauntletClientIndex is missing.");
        return false;
    }

    ClientIndex = FCString::Atoi(*ClientIndexString);
    if (ClientIndex <= 0)
    {
        OutFailureReason = FString::Printf(TEXT("GauntletClientIndex is invalid: %s"), *ClientIndexString);
        return false;
    }

    if (bRunPartyDungeonFlow && ClientIndex > GauntletLoginLobby::PartyDungeonClientCount)
    {
        OutFailureReason = FString::Printf(TEXT("Gauntlet party dungeon client index is invalid: %d"), ClientIndex);
        return false;
    }

    if (bRunPartyDungeonFlow)
    {
        FailureCase.Empty();
    }
    else if (!FParse::Value(FCommandLine::Get(), GauntletLoginLobby::FailureCaseKey, FailureCase) || FailureCase.IsEmpty())
    {
        OutFailureReason = TEXT("GauntletLoginFailureCase is missing.");
        return false;
    }

    if (!bRunPartyDungeonFlow &&
        !FailureCase.Equals(GauntletLoginLobby::DuplicateLoginFailureCase, ESearchCase::IgnoreCase)
        && !FailureCase.Equals(GauntletLoginLobby::WrongPasswordFailureCase, ESearchCase::IgnoreCase)
        && !FailureCase.Equals(GauntletLoginLobby::WrongIDFailureCase, ESearchCase::IgnoreCase))
    {
        OutFailureReason = FString::Printf(TEXT("GauntletLoginFailureCase is invalid: %s"), *FailureCase);
        return false;
    }

    if (!FParse::Value(FCommandLine::Get(), GauntletLoginLobby::CredentialsFileKey, CredentialsFilePath) || CredentialsFilePath.IsEmpty())
    {
        OutFailureReason = TEXT("GauntletCredentialsFile is missing.");
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Jenkins가 생성한 credentials JSON 파일을 읽어 현재 클라이언트 계정 정보를 준비하는 함수
// OutFailureReason : 파일 읽기 또는 JSON 파싱 실패 시 실패 이유를 반환함
// Return Value : 현재 ClientIndex에 맞는 계정 정보를 찾았으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::LoadCredentials(FString& OutFailureReason)
{
    FString ResolvedCredentialsFilePath;
    if (!ResolveCredentialsFilePath(ResolvedCredentialsFilePath))
    {
        OutFailureReason = FString::Printf(TEXT("Gauntlet credentials file path is invalid: %s"), *CredentialsFilePath);
        return false;
    }

    FString CredentialsJson;
    if (!FFileHelper::LoadFileToString(CredentialsJson, *ResolvedCredentialsFilePath))
    {
        OutFailureReason = FString::Printf(TEXT("Failed to read Gauntlet credentials file: %s"), *ResolvedCredentialsFilePath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CredentialsJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutFailureReason = FString::Printf(TEXT("Failed to parse Gauntlet credentials JSON: %s"), *ResolvedCredentialsFilePath);
        return false;
    }

    RootObject->TryGetStringField(TEXT("testAuth"), TestAuth);
    TestAuth.TrimStartAndEndInline();
    if (TestAuth.IsEmpty())
    {
        OutFailureReason = TEXT("Gauntlet credentials JSON is missing testAuth.");
        return false;
    }

    return ReadAccountCredentials(RootObject, OutFailureReason);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// credentials 파일 경로를 절대 경로로 변환하고 정규화하는 함수
// OutResolvedPath : 정규화된 credentials 파일 절대 경로
// Return Value : 경로를 계산할 수 있으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::ResolveCredentialsFilePath(FString& OutResolvedPath) const
{
    OutResolvedPath = CredentialsFilePath;
    OutResolvedPath.TrimStartAndEndInline();
    if (OutResolvedPath.IsEmpty())
    {
        return false;
    }

    if (FPaths::IsRelative(OutResolvedPath))
    {
        OutResolvedPath = FPaths::Combine(FPaths::ProjectDir(), OutResolvedPath);
    }

    OutResolvedPath = FPaths::ConvertRelativePathToFull(OutResolvedPath);
    FPaths::NormalizeFilename(OutResolvedPath);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// credentials JSON에서 현재 ClientIndex와 일치하는 테스트 계정을 찾는 함수
// RootObject : credentials JSON 루트 객체
// OutFailureReason : 계정 정보를 찾지 못했을 때 실패 이유를 반환함
// Return Value : 현재 클라이언트 계정 정보를 찾았으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::ReadAccountCredentials(const TSharedPtr<FJsonObject>& RootObject, FString& OutFailureReason)
{
    AccountCredentials = FGauntletLoginLobbyAccountCredentials();
    AllAccountCredentials.Reset();

    const TArray<TSharedPtr<FJsonValue>>* AccountsArray = nullptr;
    if (!RootObject.IsValid() || !RootObject->TryGetArrayField(TEXT("accounts"), AccountsArray) || AccountsArray == nullptr)
    {
        OutFailureReason = TEXT("Gauntlet credentials JSON is missing accounts.");
        return false;
    }

    for (const TSharedPtr<FJsonValue>& AccountValue : *AccountsArray)
    {
        const TSharedPtr<FJsonObject> AccountObject = AccountValue.IsValid() ? AccountValue->AsObject() : nullptr;
        if (!AccountObject.IsValid())
        {
            continue;
        }

        double AccountClientIndexNumber = 0.0;
        AccountObject->TryGetNumberField(TEXT("clientIndex"), AccountClientIndexNumber);
        const int32 AccountClientIndex = static_cast<int32>(AccountClientIndexNumber);
        if (AccountClientIndex <= 0)
        {
            continue;
        }

        FGauntletLoginLobbyAccountCredentials ParsedCredentials;
        ParsedCredentials.ClientIndex = AccountClientIndex;
        AccountObject->TryGetStringField(TEXT("id"), ParsedCredentials.ID);
        AccountObject->TryGetStringField(TEXT("password"), ParsedCredentials.Password);
        AccountObject->TryGetStringField(TEXT("username"), ParsedCredentials.Username);

        ParsedCredentials.ID.TrimStartAndEndInline();
        ParsedCredentials.Password.TrimStartAndEndInline();
        ParsedCredentials.Username.TrimStartAndEndInline();
        if (ParsedCredentials.ID.IsEmpty() || ParsedCredentials.Password.IsEmpty())
        {
            OutFailureReason = FString::Printf(TEXT("Gauntlet credentials for client %d are incomplete."), AccountClientIndex);
            return false;
        }

        if (bRunPartyDungeonFlow && ParsedCredentials.Username.IsEmpty())
        {
            OutFailureReason = FString::Printf(TEXT("Gauntlet credentials for client %d are missing username."), AccountClientIndex);
            return false;
        }

        AllAccountCredentials.Add(AccountClientIndex, ParsedCredentials);
        if (AccountClientIndex == ClientIndex)
        {
            AccountCredentials = ParsedCredentials;
        }
    }

    if (AccountCredentials.ClientIndex != ClientIndex)
    {
        OutFailureReason = FString::Printf(TEXT("Gauntlet credentials for client %d were not found."), ClientIndex);
        return false;
    }

    if (bRunPartyDungeonFlow)
    {
        for (int32 RequiredClientIndex = 1; RequiredClientIndex <= GauntletLoginLobby::PartyDungeonClientCount; ++RequiredClientIndex)
        {
            const FGauntletLoginLobbyAccountCredentials* RequiredCredentials = AllAccountCredentials.Find(RequiredClientIndex);
            if (!RequiredCredentials || RequiredCredentials->Username.IsEmpty())
            {
                OutFailureReason = FString::Printf(TEXT("Gauntlet party dungeon credentials for client %d were not found."), RequiredClientIndex);
                return false;
            }
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 클라이언트별 기대 실패 케이스를 먼저 실행하는 함수
void UGauntletLoginLobbySubsystem::RunExpectedFailureLogin()
{
    FString AttemptID = AccountCredentials.ID;
    FString AttemptPassword = AccountCredentials.Password;

    if (FailureCase.Equals(GauntletLoginLobby::WrongPasswordFailureCase, ESearchCase::IgnoreCase))
    {
        AttemptPassword += TEXT("_wrong");
    }
    else if (FailureCase.Equals(GauntletLoginLobby::WrongIDFailureCase, ESearchCase::IgnoreCase))
    {
        AttemptID += TEXT("_wrong");
    }

    ClearActiveLoginRequest();
    ActiveLoginRequest = ULoginRequestAsyncAction::RequestLogin(this, AttemptID, AttemptPassword, TEXT(""));
    if (ActiveLoginRequest == nullptr)
    {
        FinishFailure(TEXT("Failed to create expected failure login request."));
        return;
    }

    ActiveLoginRequest->OnSuccess.AddDynamic(this, &UGauntletLoginLobbySubsystem::HandleExpectedFailureUnexpectedSuccess);
    ActiveLoginRequest->OnFailure.AddDynamic(this, &UGauntletLoginLobbySubsystem::HandleExpectedFailureResult);
    ActiveLoginRequest->Activate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기대한 로그인 실패 결과를 처리하고 다음 단계로 이동하는 함수
// UserIndex : 실패 요청에서는 사용하지 않는 값
// Username : 실패 요청에서는 사용하지 않는 값
// Message : 로그인 실패 이유 메시지
// LoginToken : 실패 요청에서는 사용하지 않는 값
void UGauntletLoginLobbySubsystem::HandleExpectedFailureResult(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    ClearActiveLoginRequest();

    UE_LOG(LogTemp, Display, TEXT("Gauntlet expected login failure confirmed. ClientIndex: %d, FailureCase: %s, Message: %s"), ClientIndex, *FailureCase, *Message);

    if (FailureCase.Equals(GauntletLoginLobby::DuplicateLoginFailureCase, ESearchCase::IgnoreCase))
    {
        ClearDuplicateLoginToken();
        return;
    }

    RunSuccessLogin();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기대 실패 단계에서 로그인 성공이 발생했을 때 테스트 실패로 처리하는 함수
// UserIndex : 예기치 않게 로그인에 성공한 유저의 DB user_Index
// Username : 예기치 않게 로그인에 성공한 유저 이름
// Message : 로그인 서버가 전달한 메시지
// LoginToken : 예기치 않게 발급된 로그인 토큰
void UGauntletLoginLobbySubsystem::HandleExpectedFailureUnexpectedSuccess(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    ClearActiveLoginRequest();
    FinishFailure(FString::Printf(TEXT("Expected login failure succeeded unexpectedly. UserIndex: %d, Username: %s"), UserIndex, *Username));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 중복 로그인 실패 확인 후 정상 로그인을 위해 테스트 계정의 기존 토큰을 제거하는 함수
void UGauntletLoginLobbySubsystem::ClearDuplicateLoginToken()
{
    const FString ClearLoginTokenUrl = BuildClearLoginTokenUrl();
    if (ClearLoginTokenUrl.IsEmpty())
    {
        FinishFailure(TEXT("Clear login token URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("ID"), AccountCredentials.ID);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    ActiveClearLoginTokenRequest = FHttpModule::Get().CreateRequest();
    ActiveClearLoginTokenRequest->SetURL(ClearLoginTokenUrl);
    ActiveClearLoginTokenRequest->SetVerb(TEXT("POST"));
    ActiveClearLoginTokenRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    ActiveClearLoginTokenRequest->SetHeader(GauntletLoginLobby::TestAuthHeaderName, TestAuth);
    ActiveClearLoginTokenRequest->SetContentAsString(RequestBody);
    ActiveClearLoginTokenRequest->OnProcessRequestComplete().BindUObject(this, &UGauntletLoginLobbySubsystem::HandleClearLoginTokenResponse);

    if (!ActiveClearLoginTokenRequest->ProcessRequest())
    {
        ActiveClearLoginTokenRequest.Reset();
        FinishFailure(TEXT("Failed to start clear-login-token request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// clear-login-token API 응답을 처리하고 정상 로그인 단계로 이동하는 함수
// Request : 완료된 HTTP 요청 객체
// Response : GameBackend에서 받은 HTTP 응답 객체
// bWasSuccessful : HTTP 요청 처리 성공 여부
void UGauntletLoginLobbySubsystem::HandleClearLoginTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    ActiveClearLoginTokenRequest.Reset();

    if (!bWasSuccessful || !Response.IsValid())
    {
        FinishFailure(TEXT("clear-login-token request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        FinishFailure(FString::Printf(TEXT("Invalid clear-login-token response. HTTP %d"), ResponseCode));
        return;
    }

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);
    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        FString Message;
        JsonObject->TryGetStringField(TEXT("message"), Message);
        FinishFailure(Message.IsEmpty() ? FString::Printf(TEXT("clear-login-token failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    RunSuccessLogin();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기대 실패 케이스 확인 후 정상 ID/PW로 로그인 요청을 실행하는 함수
void UGauntletLoginLobbySubsystem::RunSuccessLogin()
{
    ClearActiveLoginRequest();
    ActiveLoginRequest = ULoginRequestAsyncAction::RequestLogin(this, AccountCredentials.ID, AccountCredentials.Password, TEXT(""));
    if (ActiveLoginRequest == nullptr)
    {
        FinishFailure(TEXT("Failed to create success login request."));
        return;
    }

    ActiveLoginRequest->OnSuccess.AddDynamic(this, &UGauntletLoginLobbySubsystem::HandleSuccessLoginResult);
    ActiveLoginRequest->OnFailure.AddDynamic(this, &UGauntletLoginLobbySubsystem::HandleSuccessLoginFailure);
    ActiveLoginRequest->Activate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 정상 로그인 성공 결과를 저장하고 로비 이동을 시작하는 함수
// UserIndex : 로그인에 성공한 유저의 DB user_Index
// Username : 로그인에 성공한 유저 이름
// Message : 로그인 서버가 전달한 성공 메시지
// LoginToken : 로그인 서버에서 발급받은 세션 토큰
void UGauntletLoginLobbySubsystem::HandleSuccessLoginResult(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    ClearActiveLoginRequest();

    if (LoginToken.IsEmpty())
    {
        FinishFailure(TEXT("Success login returned an empty token."));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    ULoginFlowSubsystem* LoginFlowSubsystem = GameInstance
        ? GameInstance->GetSubsystem<ULoginFlowSubsystem>()
        : nullptr;
    if (LoginFlowSubsystem == nullptr)
    {
        FinishFailure(TEXT("LoginFlowSubsystem is missing."));
        return;
    }

    LoginFlowSubsystem->HandleLoginSuccess(UserIndex, Username, LoginToken);
    TravelToLobby();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 정상 로그인 단계에서 실패가 발생했을 때 테스트 실패로 처리하는 함수
// UserIndex : 실패 요청에서는 사용하지 않는 값
// Username : 실패 요청에서는 사용하지 않는 값
// Message : 로그인 실패 이유 메시지
// LoginToken : 실패 요청에서는 사용하지 않는 값
void UGauntletLoginLobbySubsystem::HandleSuccessLoginFailure(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    ClearActiveLoginRequest();
    FinishFailure(Message.IsEmpty() ? TEXT("Success login failed.") : Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 검증을 거쳐 로비 서버 이동을 요청하는 함수
void UGauntletLoginLobbySubsystem::TravelToLobby()
{
    UGameInstance* GameInstance = GetGameInstance();
    USessionTravelSubsystem* SessionTravelSubsystem = GameInstance
        ? GameInstance->GetSubsystem<USessionTravelSubsystem>()
        : nullptr;
    if (SessionTravelSubsystem == nullptr)
    {
        FinishFailure(TEXT("SessionTravelSubsystem is missing."));
        return;
    }

    APlayerController* PlayerController = GetFirstLocalPlayerController();
    if (PlayerController == nullptr)
    {
        FinishFailure(TEXT("Local PlayerController is missing."));
        return;
    }

    SessionTravelSubsystem->OnLobbyTravelResult.RemoveAll(this);
    SessionTravelSubsystem->OnLobbyTravelResult.AddUObject(this, &UGauntletLoginLobbySubsystem::HandleLobbyTravelResult);
    SessionTravelSubsystem->VerifySessionAndTravelToLobby(PlayerController);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 이동 결과를 받아 로비 인증 완료 대기를 시작하거나 실패 처리하는 함수
// bSuccess : 로비 이동 요청 결과 성공 여부
// Message : 로비 이동 결과 메시지
void UGauntletLoginLobbySubsystem::HandleLobbyTravelResult(bool bSuccess, const FString& Message)
{
    ClearLobbyTravelWait();

    if (!bSuccess)
    {
        FinishFailure(Message.IsEmpty() ? TEXT("Lobby travel failed.") : Message);
        return;
    }

    UWorld* World = GetWorld();
    LobbyAuthWaitDeadlineSeconds = (World ? World->GetTimeSeconds() : 0.0) + GauntletLoginLobby::LobbyAuthTimeoutSeconds;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().SetTimer(
            LobbyAuthWaitTimerHandle,
            this,
            &UGauntletLoginLobbySubsystem::WaitForLobbyAuthResult,
            GauntletLoginLobby::LobbyAuthPollIntervalSeconds,
            true);
    }

    WaitForLobbyAuthResult();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 PlayerController의 인증 완료 상태를 폴링하고 최종 성공 여부를 결정하는 함수
void UGauntletLoginLobbySubsystem::WaitForLobbyAuthResult()
{
    const ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetFirstLocalPlayerController());
    if (LobbyPC && LobbyPC->IsLobbyAuthVerified())
    {
        if (bRunPartyDungeonFlow)
        {
            StartPartyDungeonFlowAfterLobbyAuth();
            return;
        }

        FinishSuccess();
        return;
    }

    const UWorld* World = GetWorld();
    const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
    if (CurrentTimeSeconds >= LobbyAuthWaitDeadlineSeconds)
    {
        FinishFailure(TEXT("Lobby auth verification timed out."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 인증 완료 후 파티/던전 플로우 상태 머신을 시작하는 함수
void UGauntletLoginLobbySubsystem::StartPartyDungeonFlowAfterLobbyAuth()
{
    if (!bIsRunning || !bRunPartyDungeonFlow || PartyDungeonStep != EGauntletPartyDungeonStep::None)
    {
        return;
    }

    ClearLobbyTravelWait();
    SetPartyDungeonStep(EGauntletPartyDungeonStep::CreateParty, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().SetTimer(
            PartyDungeonFlowTimerHandle,
            this,
            &UGauntletLoginLobbySubsystem::TickPartyDungeonFlow,
            GauntletLoginLobby::PartyDungeonPollIntervalSeconds,
            true);
    }

    TickPartyDungeonFlow();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티/던전 플로우 단계를 폴링하고 역할별 처리를 실행하는 함수
void UGauntletLoginLobbySubsystem::TickPartyDungeonFlow()
{
    if (!bIsRunning || !bRunPartyDungeonFlow)
    {
        return;
    }

    if (FPlatformTime::Seconds() >= PartyDungeonStepDeadlineSeconds)
    {
        FinishPartyDungeonFailure(FString::Printf(TEXT("Party dungeon flow step timed out. Step: %d"), static_cast<int32>(PartyDungeonStep)));
        return;
    }

    switch (PartyDungeonStep)
    {
    case EGauntletPartyDungeonStep::CreateParty:
        HandlePartyDungeonCreateParty();
        break;
    case EGauntletPartyDungeonStep::InviteClient2:
        HandlePartyDungeonInviteClient2();
        break;
    case EGauntletPartyDungeonStep::AcceptInvite:
        HandlePartyDungeonAcceptInvite();
        break;
    case EGauntletPartyDungeonStep::RequestJoin:
        HandlePartyDungeonRequestJoin();
        break;
    case EGauntletPartyDungeonStep::AcceptJoinRequest:
        HandlePartyDungeonAcceptJoinRequest();
        break;
    case EGauntletPartyDungeonStep::SelectCharacter:
        HandlePartyDungeonSelectCharacter();
        break;
    case EGauntletPartyDungeonStep::SetReady:
        HandlePartyDungeonSetReady();
        break;
    case EGauntletPartyDungeonStep::EnterDungeon:
        HandlePartyDungeonEnterDungeon();
        break;
    case EGauntletPartyDungeonStep::DungeonAuth:
        HandlePartyDungeonAuth();
        break;
    case EGauntletPartyDungeonStep::SurrenderVote:
        HandlePartyDungeonSurrenderVote();
        break;
    case EGauntletPartyDungeonStep::LobbyReturn:
        HandlePartyDungeonLobbyReturn();
        break;
    default:
        break;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티/던전 플로우 단계를 변경하고 단계별 요청 전송 상태와 제한 시간을 초기화하는 함수
// NewStep : 새로 진행할 파티/던전 플로우 단계
// TimeoutSeconds : 해당 단계의 제한 시간
void UGauntletLoginLobbySubsystem::SetPartyDungeonStep(EGauntletPartyDungeonStep NewStep, double TimeoutSeconds)
{
    PartyDungeonStep = NewStep;
    bPartyDungeonStepActionSent = false;
    PartyDungeonStepDeadlineSeconds = FPlatformTime::Seconds() + TimeoutSeconds;
    UE_LOG(LogTemp, Display, TEXT("Gauntlet party dungeon step changed. ClientIndex: %d, Step: %d"), ClientIndex, static_cast<int32>(PartyDungeonStep));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client1이 파티를 생성하고 모든 클라이언트가 생성된 테스트 파티를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonCreateParty()
{
    FLobbyPartyInfo PartyInfo;
    if (TryGetTestPartyInfo(PartyInfo))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::InviteClient2, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client1Index)
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    if (!LobbyPC)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestCreateParty();
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client1이 Client2를 파티에 초대하고 초대 또는 가입 상태를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonInviteClient2()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo))
    {
        return;
    }

    if (IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client2Index) ||
        HasPendingInviteForClient(GauntletLoginLobby::Client2Index, PartyInfo.PartyId))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::AcceptInvite, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client1Index)
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::AcceptInvite, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    APlayerState* Client2PlayerState = FindLobbyPlayerStateForClient(GauntletLoginLobby::Client2Index);
    if (!LobbyPC || !Client2PlayerState)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestInvitePlayer(Client2PlayerState);
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client2가 파티 초대를 수락하고 모든 클라이언트가 Client2 가입을 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonAcceptInvite()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo))
    {
        return;
    }

    if (IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client2Index))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::RequestJoin, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client2Index ||
        !HasPendingInviteForClient(GauntletLoginLobby::Client2Index, PartyInfo.PartyId))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    if (!LobbyPC)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestAcceptPartyInvite(PartyInfo.PartyId);
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client3이 Client1 파티에 가입 신청을 보내고 가입 신청 상태를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonRequestJoin()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo))
    {
        return;
    }

    if (IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client3Index) ||
        HasPendingJoinRequestForClient(GauntletLoginLobby::Client3Index, PartyInfo.PartyId))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::AcceptJoinRequest, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client3Index || !IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client2Index))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    if (!LobbyPC)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestJoinParty(PartyInfo.PartyId);
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client1이 Client3의 파티 가입 신청을 수락하고 3인 파티 구성을 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonAcceptJoinRequest()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo))
    {
        return;
    }

    if (IsFullTestParty(PartyInfo))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::SelectCharacter, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client1Index ||
        !HasPendingJoinRequestForClient(GauntletLoginLobby::Client3Index, PartyInfo.PartyId))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    APlayerState* Client3PlayerState = FindLobbyPlayerStateForClient(GauntletLoginLobby::Client3Index);
    if (!LobbyPC || !Client3PlayerState)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestAcceptPartyJoinRequest(Client3PlayerState);
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 각 클라이언트가 역할에 맞는 캐릭터 ID를 선택하고 전체 선택 완료를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonSelectCharacter()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo) || !IsFullTestParty(PartyInfo))
    {
        return;
    }

    const int32 CharacterId = GetPartyDungeonCharacterIdForClient();
    bool bLocalCharacterSelected = false;
    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (IsTestPartyMember(PartyInfo, ClientIndex) &&
            MemberInfo.Username.Equals(AccountCredentials.Username, ESearchCase::IgnoreCase) &&
            MemberInfo.SelectedCharacterId == CharacterId)
        {
            bLocalCharacterSelected = true;
            break;
        }
    }

    if (!bLocalCharacterSelected && !bPartyDungeonStepActionSent)
    {
        if (ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController())
        {
            LobbyPC->RequestSelectPartyCharacter(CharacterId);
            bPartyDungeonStepActionSent = true;
        }
    }

    if (AreAllTestCharactersSelected(PartyInfo))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::SetReady, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 각 클라이언트가 Ready를 설정하고 모든 테스트 파티원이 Ready인지 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonSetReady()
{
    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo) || !AreAllTestCharactersSelected(PartyInfo))
    {
        return;
    }

    bool bLocalReady = false;
    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (MemberInfo.Username.Equals(AccountCredentials.Username, ESearchCase::IgnoreCase))
        {
            bLocalReady = MemberInfo.bIsReady;
            break;
        }
    }

    if (!bLocalReady && !bPartyDungeonStepActionSent)
    {
        if (ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController())
        {
            LobbyPC->RequestSetPartyReady(true);
            bPartyDungeonStepActionSent = true;
        }
    }

    if (AreAllTestMembersReady(PartyInfo))
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::EnterDungeon, GauntletLoginLobby::PartyDungeonTravelTimeoutSeconds);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client1이 던전 입장을 요청하고 모든 클라이언트가 던전 PlayerController 이동을 기다리는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonEnterDungeon()
{
    if (GetDungeonPlayerController())
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::DungeonAuth, GauntletLoginLobby::PartyDungeonTravelTimeoutSeconds);
        return;
    }

    if (ClientIndex != GauntletLoginLobby::Client1Index)
    {
        return;
    }

    FLobbyPartyInfo PartyInfo;
    if (!TryGetTestPartyInfo(PartyInfo) || !AreAllTestMembersReady(PartyInfo))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    if (!LobbyPC)
    {
        return;
    }

    if (!bPartyDungeonStepActionSent)
    {
        LobbyPC->RequestEnterDungeon();
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 인증과 선택 캐릭터 ID 반영을 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonAuth()
{
    ADungeonPC* DungeonPC = GetDungeonPlayerController();
    if (!DungeonPC || !DungeonPC->IsDungeonAuthVerified())
    {
        return;
    }

    const AMyPlayerState* MyPlayerState = DungeonPC->GetPlayerState<AMyPlayerState>();
    if (!MyPlayerState || MyPlayerState->GetSelectedCharacterId() != GetPartyDungeonCharacterIdForClient())
    {
        return;
    }

    SetPartyDungeonStep(EGauntletPartyDungeonStep::SurrenderVote, GauntletLoginLobby::PartyDungeonStepTimeoutSeconds);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client3이 항복 투표를 시작하고 Client1, Client2가 찬성 투표한 뒤 만장일치 상태를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonSurrenderVote()
{
    ADungeonPC* DungeonPC = GetDungeonPlayerController();
    ADungeonGS* DungeonGS = GetDungeonGameState();
    if (!DungeonPC || !DungeonGS)
    {
        return;
    }

    const FDungeonSurrenderVoteState& VoteState = DungeonGS->GetSurrenderVoteState();
    if (VoteState.LobbyTravelServerTime > 0.0f &&
        VoteState.RequiredCount >= GauntletLoginLobby::PartyDungeonClientCount &&
        VoteState.AgreeCount >= VoteState.RequiredCount)
    {
        SetPartyDungeonStep(EGauntletPartyDungeonStep::LobbyReturn, GauntletLoginLobby::PartyDungeonTravelTimeoutSeconds);
        return;
    }

    if (ClientIndex == GauntletLoginLobby::Client3Index)
    {
        if (VoteState.RequiredCount < GauntletLoginLobby::PartyDungeonClientCount)
        {
            return;
        }

        if (!VoteState.bVoteInProgress && !bPartyDungeonStepActionSent)
        {
            DungeonPC->RequestStartSurrenderVote();
            bPartyDungeonStepActionSent = true;
        }

        return;
    }

    if ((ClientIndex == GauntletLoginLobby::Client1Index || ClientIndex == GauntletLoginLobby::Client2Index) &&
        VoteState.bVoteInProgress &&
        !bPartyDungeonStepActionSent)
    {
        DungeonPC->RequestSubmitSurrenderVote(true);
        bPartyDungeonStepActionSent = true;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 완료 후 5초 뒤 로비로 복귀하고 로비 인증 완료 상태를 확인하는 함수
void UGauntletLoginLobbySubsystem::HandlePartyDungeonLobbyReturn()
{
    const ACPP_LobbyPC* LobbyPC = GetLobbyPlayerController();
    if (LobbyPC && LobbyPC->IsLobbyAuthVerified())
    {
        FinishPartyDungeonSuccess();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Client1이 생성한 테스트 파티 정보를 찾는 함수
// OutPartyInfo : 찾은 테스트 파티 정보
// Return Value : 테스트 파티를 찾았으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::TryGetTestPartyInfo(FLobbyPartyInfo& OutPartyInfo) const
{
    const FGauntletLoginLobbyAccountCredentials* LeaderCredentials = GetCredentialsForClient(GauntletLoginLobby::Client1Index);
    const ACPP_LobbyGSB* LobbyGSB = GetLobbyGameState();
    if (!LeaderCredentials || !LobbyGSB)
    {
        return false;
    }

    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.Username.Equals(LeaderCredentials->Username, ESearchCase::IgnoreCase) &&
                MemberInfo.UserIndex == PartyInfo.LeaderUserIndex)
            {
                OutPartyInfo = PartyInfo;
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 클라이언트가 파티 멤버에 포함되어 있는지 확인하는 함수
// PartyInfo : 확인할 파티 정보
// TargetClientIndex : 확인할 테스트 클라이언트 번호
// Return Value : 해당 클라이언트가 파티 멤버이면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::IsTestPartyMember(const FLobbyPartyInfo& PartyInfo, int32 TargetClientIndex) const
{
    const FGauntletLoginLobbyAccountCredentials* TargetCredentials = GetCredentialsForClient(TargetClientIndex);
    if (!TargetCredentials)
    {
        return false;
    }

    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (MemberInfo.Username.Equals(TargetCredentials->Username, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 테스트 계정 3개가 모두 같은 파티에 포함되어 있는지 확인하는 함수
// PartyInfo : 확인할 파티 정보
// Return Value : 테스트 3인 파티 구성이 완료되었으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::IsFullTestParty(const FLobbyPartyInfo& PartyInfo) const
{
    return PartyInfo.Members.Num() >= GauntletLoginLobby::PartyDungeonClientCount &&
        IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client1Index) &&
        IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client2Index) &&
        IsTestPartyMember(PartyInfo, GauntletLoginLobby::Client3Index);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 테스트 파티원 3명의 캐릭터 선택 상태가 역할 정의와 일치하는지 확인하는 함수
// PartyInfo : 확인할 파티 정보
// Return Value : 모든 테스트 파티원이 지정된 캐릭터를 선택했으면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::AreAllTestCharactersSelected(const FLobbyPartyInfo& PartyInfo) const
{
    if (!IsFullTestParty(PartyInfo))
    {
        return false;
    }

    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        const FGauntletLoginLobbyAccountCredentials* Client1Credentials = GetCredentialsForClient(GauntletLoginLobby::Client1Index);
        const FGauntletLoginLobbyAccountCredentials* Client2Credentials = GetCredentialsForClient(GauntletLoginLobby::Client2Index);
        const FGauntletLoginLobbyAccountCredentials* Client3Credentials = GetCredentialsForClient(GauntletLoginLobby::Client3Index);

        if (Client1Credentials &&
            MemberInfo.Username.Equals(Client1Credentials->Username, ESearchCase::IgnoreCase) &&
            MemberInfo.SelectedCharacterId != GauntletLoginLobby::Client1CharacterId)
        {
            return false;
        }

        if (Client2Credentials &&
            MemberInfo.Username.Equals(Client2Credentials->Username, ESearchCase::IgnoreCase) &&
            MemberInfo.SelectedCharacterId != GauntletLoginLobby::Client2CharacterId)
        {
            return false;
        }

        if (Client3Credentials &&
            MemberInfo.Username.Equals(Client3Credentials->Username, ESearchCase::IgnoreCase) &&
            MemberInfo.SelectedCharacterId != GauntletLoginLobby::Client3CharacterId)
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 테스트 파티원 3명이 모두 온라인, 캐릭터 선택, Ready 상태인지 확인하는 함수
// PartyInfo : 확인할 파티 정보
// Return Value : 던전 입장 조건을 만족하면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::AreAllTestMembersReady(const FLobbyPartyInfo& PartyInfo) const
{
    if (!AreAllTestCharactersSelected(PartyInfo))
    {
        return false;
    }

    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (!MemberInfo.bIsReady ||
            MemberInfo.ConnectionState != ELobbyPartyConnectionState::Online ||
            !MemberInfo.PlayerState)
        {
            return false;
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 클라이언트에게 대기 중인 파티 초대가 있는지 확인하는 함수
// TargetClientIndex : 초대 대상 테스트 클라이언트 번호
// PartyId : 확인할 파티 ID
// Return Value : 초대가 대기 중이면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::HasPendingInviteForClient(int32 TargetClientIndex, int32 PartyId) const
{
    const ACPP_LobbyGSB* LobbyGSB = GetLobbyGameState();
    if (!LobbyGSB)
    {
        return false;
    }

    for (const FLobbyPartyInviteInfo& PendingInvite : LobbyGSB->GetPendingPartyInvites())
    {
        if (PendingInvite.PartyId == PartyId &&
            DoesPlayerStateMatchClient(PendingInvite.TargetPlayerState.Get(), TargetClientIndex))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 클라이언트가 보낸 파티 가입 신청이 대기 중인지 확인하는 함수
// ApplicantClientIndex : 가입 신청을 보낸 테스트 클라이언트 번호
// PartyId : 확인할 파티 ID
// Return Value : 가입 신청이 대기 중이면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::HasPendingJoinRequestForClient(int32 ApplicantClientIndex, int32 PartyId) const
{
    const ACPP_LobbyGSB* LobbyGSB = GetLobbyGameState();
    if (!LobbyGSB)
    {
        return false;
    }

    for (const FLobbyPartyJoinRequestInfo& PendingJoinRequest : LobbyGSB->GetPendingPartyJoinRequests())
    {
        if (PendingJoinRequest.PartyId == PartyId &&
            DoesPlayerStateMatchClient(PendingJoinRequest.ApplicantPlayerState.Get(), ApplicantClientIndex))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerState의 Username이 지정한 테스트 클라이언트 계정과 일치하는지 확인하는 함수
// PlayerState : 확인할 PlayerState
// TargetClientIndex : 비교할 테스트 클라이언트 번호
// Return Value : 같은 테스트 계정이면 true, 아니면 false
bool UGauntletLoginLobbySubsystem::DoesPlayerStateMatchClient(APlayerState* PlayerState, int32 TargetClientIndex) const
{
    const FGauntletLoginLobbyAccountCredentials* TargetCredentials = GetCredentialsForClient(TargetClientIndex);
    if (!PlayerState || !TargetCredentials)
    {
        return false;
    }

    const ACPP_LobbyPS* LobbyPS = Cast<ACPP_LobbyPS>(PlayerState);
    const FString Username = LobbyPS ? LobbyPS->GetUsername() : PlayerState->GetPlayerName();
    return Username.Equals(TargetCredentials->Username, ESearchCase::IgnoreCase);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 온라인 유저 목록에서 지정한 테스트 클라이언트의 PlayerState를 찾는 함수
// TargetClientIndex : 찾을 테스트 클라이언트 번호
// Return Value : 찾은 PlayerState 포인터, 없으면 nullptr
APlayerState* UGauntletLoginLobbySubsystem::FindLobbyPlayerStateForClient(int32 TargetClientIndex) const
{
    const FGauntletLoginLobbyAccountCredentials* TargetCredentials = GetCredentialsForClient(TargetClientIndex);
    ACPP_LobbyGSB* LobbyGSB = GetLobbyGameState();
    if (!TargetCredentials || !LobbyGSB)
    {
        return nullptr;
    }

    TArray<FLobbyOnlineUserInfo> OnlineUsers;
    LobbyGSB->GetOnlineUsers(OnlineUsers);
    for (const FLobbyOnlineUserInfo& OnlineUser : OnlineUsers)
    {
        if (OnlineUser.Username.Equals(TargetCredentials->Username, ESearchCase::IgnoreCase))
        {
            return OnlineUser.PlayerState.Get();
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드의 로비 GameState를 반환하는 함수
// Return Value : 로비 GameState 포인터, 현재 월드가 로비가 아니면 nullptr
ACPP_LobbyGSB* UGauntletLoginLobbySubsystem::GetLobbyGameState() const
{
    UWorld* World = GetWorld();
    return World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 로컬 PlayerController를 로비 PlayerController로 반환하는 함수
// Return Value : 로비 PlayerController 포인터, 현재 월드가 로비가 아니면 nullptr
ACPP_LobbyPC* UGauntletLoginLobbySubsystem::GetLobbyPlayerController() const
{
    return Cast<ACPP_LobbyPC>(GetFirstLocalPlayerController());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드의 던전 GameState를 반환하는 함수
// Return Value : 던전 GameState 포인터, 현재 월드가 던전이 아니면 nullptr
ADungeonGS* UGauntletLoginLobbySubsystem::GetDungeonGameState() const
{
    UWorld* World = GetWorld();
    return World ? World->GetGameState<ADungeonGS>() : nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 로컬 PlayerController를 던전 PlayerController로 반환하는 함수
// Return Value : 던전 PlayerController 포인터, 현재 월드가 던전이 아니면 nullptr
ADungeonPC* UGauntletLoginLobbySubsystem::GetDungeonPlayerController() const
{
    return Cast<ADungeonPC>(GetFirstLocalPlayerController());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 클라이언트 역할에 맞는 테스트 캐릭터 ID를 반환하는 함수
// Return Value : Client1=100, Client2=200, Client3=300, 그 외 -1
int32 UGauntletLoginLobbySubsystem::GetPartyDungeonCharacterIdForClient() const
{
    switch (ClientIndex)
    {
    case GauntletLoginLobby::Client1Index:
        return GauntletLoginLobby::Client1CharacterId;
    case GauntletLoginLobby::Client2Index:
        return GauntletLoginLobby::Client2CharacterId;
    case GauntletLoginLobby::Client3Index:
        return GauntletLoginLobby::Client3CharacterId;
    default:
        return -1;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 테스트 클라이언트의 credentials를 반환하는 함수
// TargetClientIndex : 찾을 테스트 클라이언트 번호
// Return Value : credentials 포인터, 없으면 nullptr
const FGauntletLoginLobbyAccountCredentials* UGauntletLoginLobbySubsystem::GetCredentialsForClient(int32 TargetClientIndex) const
{
    return AllAccountCredentials.Find(TargetClientIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티/던전 플로우가 성공했음을 Gauntlet 로그 마커로 출력하는 함수
void UGauntletLoginLobbySubsystem::FinishPartyDungeonSuccess()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    PartyDungeonStep = EGauntletPartyDungeonStep::None;
    ClearActiveLoginRequest();
    ClearLobbyTravelWait();
    ClearPartyDungeonFlowWait();
    ActiveClearLoginTokenRequest.Reset();

    UE_LOG(LogTemp, Display, TEXT("GAUNTLET_PARTY_DUNGEON_CLIENT_%d_SUCCESS"), ClientIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티/던전 플로우 실패 이유를 정리하고 Gauntlet 실패 로그 마커를 출력하는 함수
// Reason : 실패 이유 메시지
void UGauntletLoginLobbySubsystem::FinishPartyDungeonFailure(const FString& Reason)
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    PartyDungeonStep = EGauntletPartyDungeonStep::None;
    ClearActiveLoginRequest();
    ClearLobbyTravelWait();
    ClearPartyDungeonFlowWait();
    ActiveClearLoginTokenRequest.Reset();

    UE_LOG(LogTemp, Error, TEXT("GAUNTLET_PARTY_DUNGEON_CLIENT_%d_FAILURE: %s"), ClientIndex, *Reason);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 모든 로그인/로비 검증이 성공했음을 Gauntlet 로그 마커로 출력하는 함수
void UGauntletLoginLobbySubsystem::FinishSuccess()
{
    if (bRunPartyDungeonFlow)
    {
        FinishPartyDungeonSuccess();
        return;
    }

    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    ClearActiveLoginRequest();
    ClearLobbyTravelWait();
    ClearPartyDungeonFlowWait();
    ActiveClearLoginTokenRequest.Reset();

    UE_LOG(LogTemp, Display, TEXT("GAUNTLET_LOGIN_LOBBY_CLIENT_%d_SUCCESS"), ClientIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 테스트 실패 이유를 정리하고 Gauntlet 실패 로그 마커를 출력하는 함수
// Reason : 실패 이유 메시지
void UGauntletLoginLobbySubsystem::FinishFailure(const FString& Reason)
{
    if (bRunPartyDungeonFlow)
    {
        FinishPartyDungeonFailure(Reason);
        return;
    }

    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;
    ClearActiveLoginRequest();
    ClearLobbyTravelWait();
    ClearPartyDungeonFlowWait();
    ActiveClearLoginTokenRequest.Reset();

    UE_LOG(LogTemp, Error, TEXT("GAUNTLET_LOGIN_LOBBY_CLIENT_%d_FAILURE: %s"), ClientIndex, *Reason);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 진행 중인 로그인 비동기 액션의 델리게이트 연결을 해제하고 참조를 정리하는 함수
void UGauntletLoginLobbySubsystem::ClearActiveLoginRequest()
{
    if (ActiveLoginRequest)
    {
        ActiveLoginRequest->OnSuccess.RemoveAll(this);
        ActiveLoginRequest->OnFailure.RemoveAll(this);
        ActiveLoginRequest = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 이동 결과 델리게이트와 로비 인증 대기 타이머를 정리하는 함수
void UGauntletLoginLobbySubsystem::ClearLobbyTravelWait()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().ClearTimer(LobbyAuthWaitTimerHandle);

        if (USessionTravelSubsystem* SessionTravelSubsystem = GameInstance->GetSubsystem<USessionTravelSubsystem>())
        {
            SessionTravelSubsystem->OnLobbyTravelResult.RemoveAll(this);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티/던전 플로우 폴링 타이머와 단계 제한 시간을 정리하는 함수
void UGauntletLoginLobbySubsystem::ClearPartyDungeonFlowWait()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().ClearTimer(PartyDungeonFlowTimerHandle);
    }

    PartyDungeonStepDeadlineSeconds = 0.0;
    bPartyDungeonStepActionSent = false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드의 첫 번째 로컬 PlayerController를 반환하는 함수
// Return Value : 로컬 PlayerController 포인터, 찾지 못하면 nullptr
APlayerController* UGauntletLoginLobbySubsystem::GetFirstLocalPlayerController() const
{
    UWorld* World = GetWorld();
    return World ? World->GetFirstPlayerController() : nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameBackend 기본 URL과 테스트 토큰 정리 Endpoint를 조합하는 함수
// Return Value : clear-login-token API URL
FString UGauntletLoginLobbySubsystem::BuildClearLoginTokenUrl() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UServerConfigSubsystem>()
        : nullptr;
    FString BaseUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetGameBackendBaseUrl() : TEXT("");
    BaseUrl.TrimStartAndEndInline();
    while (BaseUrl.EndsWith(TEXT("/")))
    {
        BaseUrl.LeftChopInline(1);
    }

    return BaseUrl.IsEmpty() ? TEXT("") : BaseUrl + GauntletLoginLobby::ClearLoginTokenEndpoint;
}
