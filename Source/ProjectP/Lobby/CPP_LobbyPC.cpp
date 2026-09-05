// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_LobbyPC.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "../GameInstance/SubSystems/NetSub/AccountSessionSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "../Widget/LobbyMainWidget.h"
#include "../Widget/MyUIManagerSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Lobby
#include "CPP_LobbyGMB.h"
#include "CPP_LobbyGSB.h"
#include "CPP_LobbyPS.h"


void ACPP_LobbyPC::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Lobby UI initialization failed because MyUIManagerSubsystem is missing."));
    }
    else if (!LobbyPrimaryLayoutClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPrimaryLayoutClass is not set. Assign WBP_LobbyPrimaryGameLayout in C2B_LobbyPC defaults."));
    }
    else if (!UIManager->EnsurePrimaryLayoutUsingClass(this, LobbyPrimaryLayoutClass))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create the lobby primary layout."));
    }

    UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UAccountSessionSubsystem>() : nullptr;
    if (!AccountSessionSubsystem)
    {
        return;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString& Username = AccountSessionSubsystem->GetUsername();
    const bool bRequireLobbyTokenVerification = !ServerConfigSubsystem || ServerConfigSubsystem->IsLobbyTokenVerificationRequired();
    if (!bRequireLobbyTokenVerification)
    {
        if (!Username.IsEmpty())
        {
            ServerSetUsername(Username);
        }

        return;
    }

    const FString& LoginToken = AccountSessionSubsystem->GetLoginToken();
    if (LoginToken.IsEmpty())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Login token is empty."));
        return;
    }

    ServerSubmitLoginToken(LoginToken);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이어 스택이 생성한 로비 메인 위젯을 컨트롤러에 등록하는 함수
// InLobbyMainWidget : 등록할 로비 메인 위젯
void ACPP_LobbyPC::RegisterLobbyMainWidget(ULobbyMainWidget* InLobbyMainWidget)
{
    LobbyMainWidget = InLobbyMainWidget;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 등록된 인스턴스와 동일한 로비 메인 위젯만 컨트롤러 참조에서 해제하는 함수
// InLobbyMainWidget : 등록 해제를 요청한 로비 메인 위젯
void ACPP_LobbyPC::UnregisterLobbyMainWidget(ULobbyMainWidget* InLobbyMainWidget)
{
    if (LobbyMainWidget.Get() == InLobbyMainWidget)
    {
        LobbyMainWidget.Reset();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이어 스택이 생성해 등록한 로비 메인 위젯을 반환하는 함수
// Return Value : 등록된 로비 메인 위젯, 없으면 nullptr
ULobbyMainWidget* ACPP_LobbyPC::GetLobbyMainWidget() const
{
    return LobbyMainWidget.Get();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 플레이어 이름 설정하는 함수
// NewUsername : 새로 설정할 플레이어 이름
void ACPP_LobbyPC::ServerSetUsername_Implementation(const FString& NewUsername)
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    if (!ServerConfigSubsystem || ServerConfigSubsystem->IsLobbyTokenVerificationRequired())
    {
        UE_LOG(LogTemp, Warning, TEXT("ServerSetUsername was ignored because lobby token verification is required."));
        return;
    }

    ACPP_LobbyPS* LobbyPS = GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return;
    }

    LobbyPS->SetUsername(NewUsername);
    if (ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr)
    {
        LobbyGMB->NotifyLobbyUserTelemetryChanged();
    }

    UE_LOG(LogTemp, Warning, TEXT("Lobby username set: %s"), *NewUsername);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 로그인 토큰 제출하는 함수
// LoginToken : 로그인 토큰 문자열
void ACPP_LobbyPC::ServerSubmitLoginToken_Implementation(const FString& LoginToken)
{
    if (bLobbyAuthVerifyInFlight)
    {
        return;
    }

    if (LoginToken.IsEmpty())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Login token is empty."));
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    if (!ServerConfigSubsystem)
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Server config subsystem is not available on lobby server."));
        return;
    }

    const FString VerifyUrl = ServerConfigSubsystem->GetSessionVerifyUrl();
    if (VerifyUrl.IsEmpty())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Session verify URL is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), LoginToken);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    bLobbyAuthVerifyInFlight = true;

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(VerifyUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &ACPP_LobbyPC::HandleSessionVerifyResponse);

    if (!HttpRequest->ProcessRequest())
    {
        bLobbyAuthVerifyInFlight = false;
        ClientReceiveLobbyAuthResult(false, TEXT("Failed to start session verify request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 검증 응답 처리 함수
// Request : HTTP 요청 객체
// Response : HTTP 응답 객체
// bWasSuccessful : HTTP 요청이 성공적으로 완료되었는지 여부
void ACPP_LobbyPC::HandleSessionVerifyResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    bLobbyAuthVerifyInFlight = false;

    if (!bWasSuccessful || !Response.IsValid())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Session verify request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        ClientReceiveLobbyAuthResult(false, FString::Printf(TEXT("Invalid session verify response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        ClientReceiveLobbyAuthResult(false, Message.IsEmpty() ? FString::Printf(TEXT("Session verify failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    const TSharedPtr<FJsonObject>* UserObject = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("user"), UserObject) || UserObject == nullptr || !UserObject->IsValid())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Session verify response is missing user data."));
        return;
    }

    double UserIndexNumber = 0.0;
    FString Username;
    (*UserObject)->TryGetNumberField(TEXT("user_Index"), UserIndexNumber);
    (*UserObject)->TryGetStringField(TEXT("username"), Username);

    if (Username.IsEmpty())
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Session verify response username is empty."));
        return;
    }

    ACPP_LobbyPS* LobbyPS = GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        ClientReceiveLobbyAuthResult(false, TEXT("Lobby PlayerState is not available."));
        return;
    }

    LobbyPS->SetAuthenticatedUser(static_cast<int32>(UserIndexNumber), Username);
    if (ACPP_LobbyGSB* LobbyGSB = GetWorld() ? GetWorld()->GetGameState<ACPP_LobbyGSB>() : nullptr)
    {
        LobbyGSB->RestorePartyMemberOnline(LobbyPS);
    }
    if (ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr)
    {
        LobbyGMB->NotifyLobbyUserTelemetryChanged();
    }

    UE_LOG(LogTemp, Warning, TEXT("Lobby auth verified. UserIndex: %d, Username: %s"), static_cast<int32>(UserIndexNumber), *Username);
    ClientReceiveLobbyAuthResult(true, Message.IsEmpty() ? TEXT("Lobby auth verified.") : Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 생성을 요청하는 함수
void ACPP_LobbyPC::RequestCreateParty()
{
    ServerRequestCreateParty();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 생성하는 함수
void ACPP_LobbyPC::ServerRequestCreateParty_Implementation()
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->CreateParty(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 나가기를 요청하는 함수
void ACPP_LobbyPC::RequestLeaveParty()
{
    ServerRequestLeaveParty();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 나가는 함수
void ACPP_LobbyPC::ServerRequestLeaveParty_Implementation()
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->LeaveParty(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 초대를 요청하는 함수
// TargetPlayerState : 초대 대상 플레이어의 PlayerState
void ACPP_LobbyPC::RequestInvitePlayer(APlayerState* TargetPlayerState)
{
    ServerRequestInvitePlayer(TargetPlayerState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 초대 요청을 처리하는 함수
// TargetPlayerState : 초대 대상 플레이어의 PlayerState
void ACPP_LobbyPC::ServerRequestInvitePlayer_Implementation(APlayerState* TargetPlayerState)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->RequestPartyInvite(this, TargetPlayerState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 초대 수락을 요청하는 함수
// PartyId : 수락할 파티 ID
void ACPP_LobbyPC::RequestAcceptPartyInvite(int32 PartyId)
{
    ServerRequestAcceptPartyInvite(PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 초대 수락 요청을 처리하는 함수
// PartyId : 수락할 파티 ID
void ACPP_LobbyPC::ServerRequestAcceptPartyInvite_Implementation(int32 PartyId)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->AcceptPartyInvite(this, PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 초대 거절을 요청하는 함수
// PartyId : 거절할 파티 ID
void ACPP_LobbyPC::RequestDeclinePartyInvite(int32 PartyId)
{
    ServerRequestDeclinePartyInvite(PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 초대 거절 요청을 처리하는 함수
// PartyId : 거절할 파티 ID
void ACPP_LobbyPC::ServerRequestDeclinePartyInvite_Implementation(int32 PartyId)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->DeclinePartyInvite(this, PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 캐릭터 선택 변경을 요청하는 함수
// SelectedCharacterId : 선택한 캐릭터 ID
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 가입 신청을 요청하는 함수 (Reliable)
// PartyId : 가입 신청을 보낼 파티 ID
void ACPP_LobbyPC::RequestJoinParty(int32 PartyId)
{
    ServerRequestJoinParty(PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 가입 신청 요청을 처리하는 함수 (Reliable)
// PartyId : 가입 신청을 보낼 파티 ID
void ACPP_LobbyPC::ServerRequestJoinParty_Implementation(int32 PartyId)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->RequestPartyJoin(this, PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 가입 신청 수락을 요청하는 함수 (Reliable)
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
void ACPP_LobbyPC::RequestAcceptPartyJoinRequest(APlayerState* ApplicantPlayerState)
{
    ServerRequestAcceptPartyJoinRequest(ApplicantPlayerState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 가입 신청 수락 요청을 처리하는 함수 (Reliable)
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
void ACPP_LobbyPC::ServerRequestAcceptPartyJoinRequest_Implementation(APlayerState* ApplicantPlayerState)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->AcceptPartyJoinRequest(this, ApplicantPlayerState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 가입 신청 거절을 요청하는 함수 (Reliable)
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
void ACPP_LobbyPC::RequestDeclinePartyJoinRequest(APlayerState* ApplicantPlayerState)
{
    ServerRequestDeclinePartyJoinRequest(ApplicantPlayerState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 가입 신청 거절 요청을 처리하는 함수 (Reliable)
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
void ACPP_LobbyPC::ServerRequestDeclinePartyJoinRequest_Implementation(APlayerState* ApplicantPlayerState)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->DeclinePartyJoinRequest(this, ApplicantPlayerState);
}

void ACPP_LobbyPC::RequestSelectPartyCharacter(int32 SelectedCharacterId)
{
    ServerRequestSelectPartyCharacter(SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 캐릭터 선택 변경 요청을 처리하는 함수
// SelectedCharacterId : 선택한 캐릭터 ID
void ACPP_LobbyPC::ServerRequestSelectPartyCharacter_Implementation(int32 SelectedCharacterId)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->SelectPartyCharacter(this, SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 로비 입장 캐릭터 선택을 요청하는 함수 (파티 소속 여부와 무관)
// SelectedCharacterId : 선택한 캐릭터 ID
void ACPP_LobbyPC::RequestSelectLobbyCharacter(int32 SelectedCharacterId)
{
    ServerRequestSelectLobbyCharacter(SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 로비 입장 캐릭터 선택 요청을 처리하는 함수
// SelectedCharacterId : 선택한 캐릭터 ID
void ACPP_LobbyPC::ServerRequestSelectLobbyCharacter_Implementation(int32 SelectedCharacterId)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->SelectLobbyCharacter(this, SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 Ready 상태 변경을 요청하는 함수
// bNewIsReady : 새 Ready 상태
void ACPP_LobbyPC::RequestSetPartyReady(bool bNewIsReady)
{
    ServerRequestSetPartyReady(bNewIsReady);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 파티 Ready 상태 변경 요청을 처리하는 함수
// bNewIsReady : 새 Ready 상태
void ACPP_LobbyPC::ServerRequestSetPartyReady_Implementation(bool bNewIsReady)
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->SetPartyReady(this, bNewIsReady);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 요청 결과 메시지를 클라이언트에서 받는 함수
// Message : 클라이언트에 표시할 파티 초대 요청 결과 메시지
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 던전 입장을 요청하는 함수
void ACPP_LobbyPC::RequestEnterDungeon()
{
    ServerRequestEnterDungeon();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 던전 입장 요청을 처리하는 함수
void ACPP_LobbyPC::ServerRequestEnterDungeon_Implementation()
{
    ACPP_LobbyGMB* LobbyGMB = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_LobbyGMB>() : nullptr;
    if (!LobbyGMB)
    {
        return;
    }

    LobbyGMB->EnterDungeon(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 요청 결과 메시지를 클라이언트에서 받는 함수
// Message : 클라이언트에 표시할 파티 초대 요청 결과 메시지
void ACPP_LobbyPC::ClientReceivePartyInviteRequestMessage_Implementation(const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 전송된 파티 초대 정보를 클라이언트에서 받는 함수
// InviteInfo : HUD 팝업에 표시할 파티 초대 정보
void ACPP_LobbyPC::ClientReceivePartyInvite_Implementation(const FLobbyReceivedPartyInviteInfo& InviteInfo)
{
    UE_LOG(LogTemp, Warning, TEXT("Party invite received. Inviter: %s, PartyMember: %d/%d"), *InviteInfo.InviterName, InviteInfo.CurrentPartyMemberCount, InviteInfo.MaxPartyMemberCount);

    if (ULobbyMainWidget* RegisteredLobbyMainWidget = LobbyMainWidget.Get())
    {
        RegisteredLobbyMainWidget->PresentPartyInvite(InviteInfo);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Party invite popup was not shown because LobbyMainWidget is not registered."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 전송된 로비 인증 결과를 클라이언트에서 받는 함수
// bSuccess : 인증 성공 여부 (true: 성공, false: 실패)
// Message : 인증 결과 메시지
void ACPP_LobbyPC::ClientReceiveLobbyAuthResult_Implementation(bool bSuccess, const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("Lobby auth result. Success: %s, Message: %s"), bSuccess ? TEXT("true") : TEXT("false"), *Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 인증이 완료되어 플레이어가 인증된 사용자임을 확인하는 함수
// 반환값: true - 인증 완료, false - 인증 미완료 또는 인증 실패
bool ACPP_LobbyPC::IsLobbyAuthVerified() const
{
    const ACPP_LobbyPS* LobbyPS = GetPlayerState<ACPP_LobbyPS>();
    return LobbyPS && LobbyPS->IsLobbyAuthVerified();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 채널 수신 대상을 수집한다. 자신과 같은 PartyId를 가진 모든 멤버를 모은다.
// 파티가 없으면(PartyId < 0) 대상이 없으므로 아무 것도 채우지 않는다.
// OutRecipients : 수신 대상 컨트롤러 목록(출력)
void ACPP_LobbyPC::GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const
{
    OutRecipients.Reset();

    const ACPP_LobbyPS* MyLobbyPS = GetPlayerState<ACPP_LobbyPS>();
    if (!MyLobbyPS)
    {
        return;
    }

    const int32 MyPartyId = MyLobbyPS->GetPartyId();
    if (MyPartyId < 0)
    {
        // 파티 미소속 → 파티 채팅 수신 대상 없음
        return;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        AMyPlayerController* PC = Cast<AMyPlayerController>(It->Get());
        if (!PC)
        {
            continue;
        }

        const ACPP_LobbyPS* MemberPS = PC->GetPlayerState<ACPP_LobbyPS>();
        if (MemberPS && MemberPS->GetPartyId() == MyPartyId)
        {
            OutRecipients.Add(PC);
        }
    }
}
