// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_LobbyGMB.h"

#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// GameInstance
#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"

// Player
#include "../Player/PlayerCharacterBase.h"

// Lobby
#include "CPP_LobbyGSB.h"
#include "CPP_LobbyPC.h"
#include "CPP_LobbyPS.h"

namespace
{
    constexpr float DungeonAllocateHttpTimeoutSeconds = 105.0f;
}



//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 입장 시 캐릭터를 선택하기 전까지 Pawn을 스폰하지 않도록 관전자 시작을 설정하는 생성자
// 캐릭터 선택이 완료되면 SelectLobbyCharacter에서 RestartPlayer로 로비 캐릭터를 스폰한다.
ACPP_LobbyGMB::ACPP_LobbyGMB()
{
    bStartPlayersAsSpectators = true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버가 시작되었을 때 현재 접속 유저 텔레메트리 주기 보고를 시작하는 함수
void ACPP_LobbyGMB::BeginPlay()
{
    Super::BeginPlay();

    ReportLobbyTelemetry();
    GetWorldTimerManager().SetTimer(
        LobbyTelemetryTimerHandle,
        this,
        &ACPP_LobbyGMB::ReportLobbyTelemetry,
        LobbyTelemetryIntervalSeconds,
        true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버가 종료될 때 텔레메트리 주기 보고 타이머를 정리하는 함수
void ACPP_LobbyGMB::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(LobbyTelemetryTimerHandle);

    Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비에 플레이어가 접속했을 때 현재 접속 수를 갱신하고 GameBackend에 보고하는 함수
// NewPlayer : 새로 접속한 플레이어 컨트롤러
void ACPP_LobbyGMB::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    CurrentLobbyClientCount = FMath::Max(0, CurrentLobbyClientCount + 1);
    ++TotalLobbyClientConnectCount;

    if (ACPP_LobbyPS* LobbyPS = NewPlayer ? NewPlayer->GetPlayerState<ACPP_LobbyPS>() : nullptr)
    {
        LobbyPS->SetLobbyConnectedAt(FDateTime::UtcNow());
    }

    ReportLobbyTelemetry();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비에서 Online 상태의 플레이어 연결이 끊겼을 때 파티 소속은 유지하고 접속 상태만 Offline으로 변경하는 함수
// Exiting : 연결이 끊긴 플레이어 컨트롤러
void ACPP_LobbyGMB::Logout(AController* Exiting)
{
    APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
    ACPP_LobbyPS* ExitingLobbyPS = Cast<ACPP_LobbyPS>(ExitingPlayerState);
    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();

    if (ExitingLobbyPS && LobbyGSB && ExitingLobbyPS->GetPartyId() != -1)
    {
        ELobbyPartyConnectionState CurrentConnectionState = ELobbyPartyConnectionState::Offline;
        if (LobbyGSB->GetPartyMemberConnectionState(ExitingLobbyPS, CurrentConnectionState) &&
            CurrentConnectionState == ELobbyPartyConnectionState::Online)
        {
            LobbyGSB->SetPartyMemberConnectionState(ExitingLobbyPS, ELobbyPartyConnectionState::Offline);
        }
    }

    Super::Logout(Exiting);

    CurrentLobbyClientCount = FMath::Max(0, CurrentLobbyClientCount - 1);
    ++TotalLobbyClientDisconnectCount;

    ReportLobbyTelemetry();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 접속 유저 정보가 변경되었을 때 GameBackend에 최신 텔레메트리를 다시 보고하는 함수
void ACPP_LobbyGMB::NotifyLobbyUserTelemetryChanged()
{
    ReportLobbyTelemetry();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 생성하는 함수
// RequestingController : 파티 생성을 요청한 플레이어의 PlayerController
// Return Value : 파티 생성 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::CreateParty(APlayerController* RequestingController)
{
    if (!CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    if (LobbyPS->GetPartyId() != -1)
    {
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        return false;
    }

    const int32 NewPartyId = LobbyGSB->AddParty(LobbyPS);
    if (NewPartyId == -1)
    {
        return false;
    }

    LobbyPS->SetPartyInfo(NewPartyId, true);

    UE_LOG(LogTemp, Warning, TEXT("Party created. PartyId: %d"), NewPartyId);

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 나가는 함수
// RequestingController : 파티에서 나가려는 플레이어의 PlayerController
// Return Value : 파티 나가기 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::LeaveParty(APlayerController* RequestingController)
{
    if (!CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    if (LobbyPS->GetPartyId() == -1)
    {
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        return false;
    }

    const bool bRemoved = LobbyGSB->RemovePlayerFromParty(LobbyPS);
    if (!bRemoved)
    {
        return false;
    }

    LobbyPS->SetPartyInfo(-1, false);

    UE_LOG(LogTemp, Warning, TEXT("Party left."));

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 요청을 처리하는 함수
// RequestingController : 파티 초대를 요청한 플레이어의 PlayerController
// TargetPlayerState : 초대 대상 플레이어의 PlayerState
// Return Value : 파티 초대 요청 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::RequestPartyInvite(APlayerController* RequestingController, APlayerState* TargetPlayerState)
{
    if (!CanUsePartyFeature(RequestingController) || !TargetPlayerState)
    {
        return false;
    }

    ACPP_LobbyPS* RequestingLobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    ACPP_LobbyPS* TargetLobbyPS = Cast<ACPP_LobbyPS>(TargetPlayerState);
    if (!RequestingLobbyPS || !TargetLobbyPS)
    {
        return false;
    }

    if (IsLobbyTokenVerificationRequired() && !TargetLobbyPS->IsLobbyAuthVerified())
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("Target player has not completed lobby auth."));
        return false;
    }

    if (RequestingLobbyPS == TargetLobbyPS)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("자기 자신은 초대할 수 없습니다."));
        return false;
    }

    int32 RequestingPartyId = RequestingLobbyPS->GetPartyId();
    if (RequestingPartyId == -1)
    {
        const bool bCreatedParty = CreateParty(RequestingController);
        if (!bCreatedParty)
        {
            SendPartyInviteRequestMessage(RequestingController, TEXT("파티를 생성할 수 없습니다."));
            return false;
        }

        RequestingPartyId = RequestingLobbyPS->GetPartyId();
        if (RequestingPartyId == -1)
        {
            SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
            return false;
        }
    }

    if (TargetLobbyPS->GetPartyId() != -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 파티가 있는 유저입니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    if (LobbyGSB->GetPartyMemberCount(RequestingPartyId) >= MaxPartyMemberCount)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정원이 가득찼습니다."));
        return false;
    }

    float RemainingInviteCooldownSeconds = 0.0f;
    const float CurrentServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bAddedInvite = LobbyGSB->AddPartyInvite(RequestingLobbyPS, TargetLobbyPS, RequestingPartyId, CurrentServerTime, PartyInviteCooldownSeconds, RemainingInviteCooldownSeconds);
    if (!bAddedInvite)
    {
        const int32 RemainingSeconds = FMath::Max(1, FMath::CeilToInt(RemainingInviteCooldownSeconds));
        SendPartyInviteRequestMessage(RequestingController, FString::Printf(TEXT("%d초 뒤에 다시 초대할 수 있습니다."), RemainingSeconds));
        return false;
    }

    const FString& RequestingUsername = RequestingLobbyPS->GetUsername();
    const FString& TargetUsername = TargetLobbyPS->GetUsername();
    const FString RequestingDisplayName = RequestingUsername.IsEmpty() ? RequestingLobbyPS->GetPlayerName() : RequestingUsername;
    const FString TargetDisplayName = TargetUsername.IsEmpty() ? TargetLobbyPS->GetPlayerName() : TargetUsername;

    UE_LOG(LogTemp, Warning, TEXT("Party invite requested. From: %s, To: %s, PartyId: %d"), *RequestingDisplayName, *TargetDisplayName, RequestingPartyId);
    SendPartyInviteRequestMessage(RequestingController, FString::Printf(TEXT("%s님에게 파티 초대를 전송했습니다."), *TargetDisplayName));

    ACPP_LobbyPC* TargetLobbyPC = nullptr;
    if (UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PlayerController = It->Get();
            if (!PlayerController || PlayerController->PlayerState != TargetPlayerState)
            {
                continue;
            }

            TargetLobbyPC = Cast<ACPP_LobbyPC>(PlayerController);
            break;
        }
    }

    if (TargetLobbyPC)
    {
        FLobbyReceivedPartyInviteInfo InviteInfo;
        InviteInfo.InviterName = RequestingDisplayName;
        InviteInfo.PartyId = RequestingPartyId;
        InviteInfo.CurrentPartyMemberCount = LobbyGSB->GetPartyMemberCount(RequestingPartyId);
        InviteInfo.MaxPartyMemberCount = MaxPartyMemberCount;
        TargetLobbyPC->ClientReceivePartyInvite(InviteInfo);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 수락 요청을 처리하는 함수
// RequestingController : 파티 초대를 수락한 플레이어의 PlayerController
// PartyId : 수락할 파티 ID
// Return Value : 파티 초대 수락 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::AcceptPartyInvite(APlayerController* RequestingController, int32 PartyId)
{
    if (!CanUsePartyFeature(RequestingController) || PartyId == -1)
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    if (LobbyPS->GetPartyId() != -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 파티에 속해 있습니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    if (!LobbyGSB->HasPendingPartyInvite(LobbyPS, PartyId))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 만료되었거나 처리된 초대입니다."));
        return false;
    }

    if (LobbyGSB->GetPartyMemberCount(PartyId) >= MaxPartyMemberCount)
    {
        LobbyGSB->RemovePendingPartyInvites(LobbyPS, PartyId);
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정원이 가득 찼습니다."));
        return false;
    }

    if (!LobbyGSB->AddPlayerToParty(LobbyPS, PartyId, MaxPartyMemberCount))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티에 참가할 수 없습니다."));
        return false;
    }

    LobbyPS->SetPartyInfo(PartyId, false);
    LobbyGSB->RemovePendingPartyInvites(LobbyPS, -1);

    UE_LOG(LogTemp, Warning, TEXT("Party invite accepted. PartyId: %d"), PartyId);
    SendPartyInviteRequestMessage(RequestingController, TEXT("파티에 참가했습니다."));

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 거절 요청을 처리하는 함수
// RequestingController : 파티 초대를 거절한 플레이어의 PlayerController
// PartyId : 거절할 파티 ID
// Return Value : 파티 초대 거절 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::DeclinePartyInvite(APlayerController* RequestingController, int32 PartyId)
{
    if (!CanUsePartyFeature(RequestingController) || PartyId == -1)
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    const bool bRemovedInvite = LobbyGSB->RemovePendingPartyInvites(LobbyPS, PartyId);
    if (!bRemovedInvite)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 만료되었거나 처리된 초대입니다."));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Party invite declined. PartyId: %d"), PartyId);
    SendPartyInviteRequestMessage(RequestingController, TEXT("파티 초대를 거절했습니다."));

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 요청을 처리하고 대상 파티장에게 가입 신청 팝업 정보를 전송하는 함수
// RequestingController : 파티 가입 신청을 요청한 플레이어의 PlayerController
// PartyId : 가입 신청을 보낼 파티 ID
// Return Value : 파티 가입 신청 처리 성공 여부 (true: 성공, false: 실패)
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 요청을 처리하고 대상 파티장에게 가입 신청 팝업 정보를 전송하는 함수
// RequestingController : 파티 가입 신청을 요청한 플레이어의 PlayerController
// PartyId : 가입 신청을 보낼 파티 ID
// Return Value : 파티 가입 신청 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::RequestPartyJoin(APlayerController* RequestingController, int32 PartyId)
{
    if (!CanUsePartyFeature(RequestingController) || PartyId == -1)
    {
        return false;
    }

    ACPP_LobbyPS* ApplicantLobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!ApplicantLobbyPS)
    {
        return false;
    }

    if (ApplicantLobbyPS->GetPartyId() != -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 파티에 속해 있어 가입 신청을 보낼 수 없습니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    const FLobbyPartyInfo* TargetPartyInfo = nullptr;
    const FLobbyPartyMemberInfo* LeaderMemberInfo = nullptr;
    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        TargetPartyInfo = &PartyInfo;
        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.UserIndex == PartyInfo.LeaderUserIndex)
            {
                LeaderMemberInfo = &MemberInfo;
                break;
            }
        }
        break;
    }

    if (!TargetPartyInfo || !LeaderMemberInfo || !LeaderMemberInfo->PlayerState)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("가입 신청을 보낼 파티장을 찾을 수 없습니다."));
        return false;
    }

    if (TargetPartyInfo->Members.Num() >= MaxPartyMemberCount)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정원이 가득 찼습니다."));
        return false;
    }

    APlayerController* LeaderController = FindPlayerControllerByPlayerState(LeaderMemberInfo->PlayerState.Get());
    ACPP_LobbyPC* LeaderLobbyPC = Cast<ACPP_LobbyPC>(LeaderController);
    if (!LeaderLobbyPC)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티장이 현재 응답할 수 없습니다."));
        return false;
    }

    float RemainingJoinCooldownSeconds = 0.0f;
    const float CurrentServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bAddedJoinRequest = LobbyGSB->AddPartyJoinRequest(ApplicantLobbyPS, LeaderMemberInfo->PlayerState.Get(), PartyId, CurrentServerTime, PartyInviteCooldownSeconds, RemainingJoinCooldownSeconds);
    if (!bAddedJoinRequest)
    {
        const int32 RemainingSeconds = FMath::Max(1, FMath::CeilToInt(RemainingJoinCooldownSeconds));
        SendPartyInviteRequestMessage(RequestingController, FString::Printf(TEXT("%d초 뒤에 다시 가입 신청할 수 있습니다."), RemainingSeconds));
        return false;
    }

    const FString& ApplicantUsername = ApplicantLobbyPS->GetUsername();
    const FString ApplicantDisplayName = ApplicantUsername.IsEmpty() ? ApplicantLobbyPS->GetPlayerName() : ApplicantUsername;
    const FString LeaderDisplayName = LeaderMemberInfo->Username.IsEmpty() ? LeaderMemberInfo->PlayerState->GetPlayerName() : LeaderMemberInfo->Username;

    FLobbyReceivedPartyInviteInfo JoinRequestInfo;
    JoinRequestInfo.InviterName = ApplicantDisplayName;
    JoinRequestInfo.PartyId = PartyId;
    JoinRequestInfo.CurrentPartyMemberCount = LobbyGSB->GetPartyMemberCount(PartyId);
    JoinRequestInfo.MaxPartyMemberCount = MaxPartyMemberCount;
    JoinRequestInfo.RequestType = ELobbyPartyPopupRequestType::PartyJoinRequest;
    JoinRequestInfo.ApplicantPlayerState = ApplicantLobbyPS;
    LeaderLobbyPC->ClientReceivePartyInvite(JoinRequestInfo);

    UE_LOG(LogTemp, Warning, TEXT("Party join requested. Applicant: %s, PartyId: %d"), *ApplicantDisplayName, PartyId);
    SendPartyInviteRequestMessage(RequestingController, FString::Printf(TEXT("%s 파티에 가입 신청을 전송했습니다."), *LeaderDisplayName));

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티장이 받은 파티 가입 신청 수락 요청을 처리하는 함수
// RequestingController : 파티 가입 신청을 수락한 파티장의 PlayerController
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
// Return Value : 파티 가입 신청 수락 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::AcceptPartyJoinRequest(APlayerController* RequestingController, APlayerState* ApplicantPlayerState)
{
    if (!CanUsePartyFeature(RequestingController) || !ApplicantPlayerState)
    {
        return false;
    }

    ACPP_LobbyPS* LeaderLobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    ACPP_LobbyPS* ApplicantLobbyPS = Cast<ACPP_LobbyPS>(ApplicantPlayerState);
    if (!LeaderLobbyPS || !ApplicantLobbyPS)
    {
        return false;
    }

    if (!LeaderLobbyPS->IsPartyLeader() || LeaderLobbyPS->GetPartyId() == -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티장만 가입 신청을 수락할 수 있습니다."));
        return false;
    }

    const int32 PartyId = LeaderLobbyPS->GetPartyId();
    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    if (!LobbyGSB->HasPendingPartyJoinRequest(ApplicantLobbyPS, PartyId))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 만료되었거나 처리된 가입 신청입니다."));
        return false;
    }

    if (ApplicantLobbyPS->GetPartyId() != -1)
    {
        LobbyGSB->RemovePendingPartyJoinRequests(ApplicantLobbyPS, PartyId);
        SendPartyInviteRequestMessage(RequestingController, TEXT("신청자가 이미 다른 파티에 속해 있습니다."));
        return false;
    }

    if (LobbyGSB->GetPartyMemberCount(PartyId) >= MaxPartyMemberCount)
    {
        LobbyGSB->RemovePendingPartyJoinRequests(ApplicantLobbyPS, PartyId);
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정원이 가득 찼습니다."));
        return false;
    }

    if (!LobbyGSB->AddPlayerToParty(ApplicantLobbyPS, PartyId, MaxPartyMemberCount))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("신청자를 파티에 참가시킬 수 없습니다."));
        return false;
    }

    ApplicantLobbyPS->SetPartyInfo(PartyId, false);
    LobbyGSB->RemovePendingPartyJoinRequests(ApplicantLobbyPS, -1);
    LobbyGSB->RemovePendingPartyInvites(ApplicantLobbyPS, -1);

    APlayerController* ApplicantController = FindPlayerControllerByPlayerState(ApplicantLobbyPS);
    SendPartyInviteRequestMessage(RequestingController, TEXT("파티 가입 신청을 수락했습니다."));
    SendPartyInviteRequestMessage(ApplicantController, TEXT("파티 가입 신청이 수락되었습니다."));

    UE_LOG(LogTemp, Warning, TEXT("Party join request accepted. PartyId: %d"), PartyId);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티장이 받은 파티 가입 신청 거절 요청을 처리하는 함수
// RequestingController : 파티 가입 신청을 거절한 파티장의 PlayerController
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
// Return Value : 파티 가입 신청 거절 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::DeclinePartyJoinRequest(APlayerController* RequestingController, APlayerState* ApplicantPlayerState)
{
    if (!CanUsePartyFeature(RequestingController) || !ApplicantPlayerState)
    {
        return false;
    }

    ACPP_LobbyPS* LeaderLobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    ACPP_LobbyPS* ApplicantLobbyPS = Cast<ACPP_LobbyPS>(ApplicantPlayerState);
    if (!LeaderLobbyPS || !ApplicantLobbyPS)
    {
        return false;
    }

    if (!LeaderLobbyPS->IsPartyLeader() || LeaderLobbyPS->GetPartyId() == -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티장만 가입 신청을 거절할 수 있습니다."));
        return false;
    }

    const int32 PartyId = LeaderLobbyPS->GetPartyId();
    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    const bool bRemovedJoinRequest = LobbyGSB->RemovePendingPartyJoinRequests(ApplicantLobbyPS, PartyId);
    if (!bRemovedJoinRequest)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("이미 만료되었거나 처리된 가입 신청입니다."));
        return false;
    }

    APlayerController* ApplicantController = FindPlayerControllerByPlayerState(ApplicantLobbyPS);
    SendPartyInviteRequestMessage(RequestingController, TEXT("파티 가입 신청을 거절했습니다."));
    SendPartyInviteRequestMessage(ApplicantController, TEXT("파티 가입 신청이 거절되었습니다."));

    UE_LOG(LogTemp, Warning, TEXT("Party join request declined. PartyId: %d"), PartyId);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 캐릭터 선택 변경 요청을 서버에서 처리하고 선택 즉시 로비 캐릭터 머티리얼을 갱신하는 함수
// RequestingController : 캐릭터 선택을 요청한 플레이어의 PlayerController
// SelectedCharacterId : 선택한 캐릭터 ID
// Return Value : 캐릭터 선택 변경 성공 여부
bool ACPP_LobbyGMB::SelectPartyCharacter(APlayerController* RequestingController, int32 SelectedCharacterId)
{
    if (!CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS || LobbyPS->GetPartyId() == -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보가 없습니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    if (!LobbyGSB->SetPartyMemberSelectedCharacter(LobbyPS, SelectedCharacterId))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("캐릭터 선택을 변경할 수 없습니다."));
        return false;
    }

    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(RequestingController->GetPawn());
    if (!PlayerCharacter)
    {
        // 입장 관전자 상태에서 파티 경로로 캐릭터를 선택한 경우(Gauntlet 등)에도 이 시점에 로비 캐릭터를 스폰한다.
        RestartPlayer(RequestingController);
        PlayerCharacter = Cast<APlayerCharacterBase>(RequestingController->GetPawn());
    }

    if (PlayerCharacter)
    {
        PlayerCharacter->SetSelectedCharacterId(SelectedCharacterId);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 입장 캐릭터 선택 요청을 서버에서 처리하는 함수
// 로비 입장 직후에는 관전자 상태라 Pawn이 없으므로, 첫 캐릭터 선택 시점에 로비 캐릭터를 스폰하고
// 선택한 ID에 맞는 외형(임시 헤일로 머티리얼)을 적용한다. 파티에 속해 있으면 파티 규칙 검증을 먼저 거친다.
// RequestingController : 캐릭터 선택을 요청한 플레이어의 PlayerController
// SelectedCharacterId : 선택한 캐릭터 ID (100, 200, 300)
// Return Value : 캐릭터 선택 성공 여부
bool ACPP_LobbyGMB::SelectLobbyCharacter(APlayerController* RequestingController, int32 SelectedCharacterId)
{
    if (!RequestingController)
    {
        return false;
    }

    // Standalone과 PIE 네트워크 테스트에서는 로그인 토큰 없이도 입장 캐릭터 선택을 허용한다.
    // 패키징된 로비 서버에서는 기존 토큰 인증 검증을 그대로 유지한다.
    bool bCanSkipLobbyAuthForLocalTest = GetNetMode() == NM_Standalone;
#if WITH_EDITOR
    bCanSkipLobbyAuthForLocalTest |= GetWorld() && GetWorld()->WorldType == EWorldType::PIE;
#endif

    if (!bCanSkipLobbyAuthForLocalTest && !CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    if (SelectedCharacterId != 100 &&
        SelectedCharacterId != 200 &&
        SelectedCharacterId != 300)
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    if (LobbyPS->GetPartyId() != -1)
    {
        // 파티 상태(중복 캐릭터 확정, Ready 잠금)와 어긋나지 않게 기존 파티 캐릭터 선택 경로로 먼저 검증한다.
        if (!SelectPartyCharacter(RequestingController, SelectedCharacterId))
        {
            return false;
        }
    }

    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(RequestingController->GetPawn());
    if (!PlayerCharacter)
    {
        // 첫 캐릭터 선택 시점에 관전자 상태를 끝내고 로비 캐릭터를 스폰한다.
        RestartPlayer(RequestingController);
        PlayerCharacter = Cast<APlayerCharacterBase>(RequestingController->GetPawn());
    }

    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("Lobby character selection failed because RestartPlayer did not create APlayerCharacterBase. Controller: %s, DefaultPawnClass: %s"),
            *GetNameSafe(RequestingController),
            *GetNameSafe(GetDefaultPawnClassForController(RequestingController)));
        return false;
    }

    PlayerCharacter->SetSelectedCharacterId(SelectedCharacterId);

    // 재입장/위젯 재표시 판단에 사용할 수 있도록 PlayerState에도 선택 결과를 보존한다.
    LobbyPS->SetSelectedCharacterId(SelectedCharacterId);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 Ready 변경 요청을 서버에서 처리하는 함수
// RequestingController : Ready 변경을 요청한 플레이어의 PlayerController
// bNewIsReady : 새 Ready 상태
// Return Value : Ready 변경 성공 여부
bool ACPP_LobbyGMB::SetPartyReady(APlayerController* RequestingController, bool bNewIsReady)
{
    if (!CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS || LobbyPS->GetPartyId() == -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보가 없습니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    APlayerCharacterBase* PlayerCharacter = nullptr;
    if (bNewIsReady)
    {
        PlayerCharacter = Cast<APlayerCharacterBase>(RequestingController->GetPawn());
        if (!PlayerCharacter)
        {
            SendPartyInviteRequestMessage(RequestingController, TEXT("로비 캐릭터 정보를 확인할 수 없습니다."));
            return false;
        }
    }

    if (!LobbyGSB->SetPartyMemberReady(LobbyPS, bNewIsReady))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("캐릭터를 선택하지 않았거나 다른 파티원이 이미 확정한 캐릭터입니다."));
        return false;
    }

    if (bNewIsReady && PlayerCharacter)
    {
        int32 SelectedCharacterId = -1;
        if (LobbyGSB->GetPartyMemberSelectedCharacter(LobbyPS, SelectedCharacterId))
        {
            PlayerCharacter->SetSelectedCharacterId(SelectedCharacterId);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 요청 결과 메시지를 요청한 클라이언트에게 전달하는 함수
// TargetController : 메시지를 받을 PlayerController
// Message : 전달할 메시지
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티장이 던전 입장을 요청했을 때 파티 상태를 검증하고 파티원들을 던전 서버로 ClientTravel시키는 함수
// RequestingController : 던전 입장을 요청한 플레이어의 PlayerController
// Return Value : 던전 이동 요청 처리 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGMB::EnterDungeon(APlayerController* RequestingController)
{
    if (!CanUsePartyFeature(RequestingController))
    {
        return false;
    }

    ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (!LobbyPS)
    {
        return false;
    }

    if (!LobbyPS->IsPartyLeader())
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티장만 던전에 입장할 수 있습니다."));
        return false;
    }

    const int32 PartyId = LobbyPS->GetPartyId();
    if (PartyId == -1)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보가 없습니다."));
        return false;
    }

    if (PendingDungeonAllocationPartyIds.Contains(PartyId))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("던전 서버를 준비 중입니다."));
        return false;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    const int32 CurrentPartyMemberCount = LobbyGSB->GetPartyMemberCount(PartyId);
    if (CurrentPartyMemberCount < MaxPartyMemberCount)
    {
        SendPartyInviteRequestMessage(RequestingController, FString::Printf(TEXT("던전 입장에는 파티원 %d명이 필요합니다. 현재 인원: %d명"), MaxPartyMemberCount, CurrentPartyMemberCount));
        return false;
    }

    if (!LobbyGSB->CanPartyEnterDungeon(PartyId, MaxPartyMemberCount))
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("모든 파티원이 온라인 상태에서 캐릭터를 선택하고 준비 완료해야 합니다."));
        return false;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString DungeonAllocateUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonAllocateUrl() : TEXT("");
    const FString ServerAuthKey = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonStateServerAuthKey() : TEXT("");
    if (DungeonAllocateUrl.IsEmpty() || ServerAuthKey.IsEmpty())
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("던전 서버 주소가 설정되지 않았습니다."));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("월드 정보를 확인할 수 없습니다."));
        return false;
    }

    const FLobbyPartyInfo* EnteringPartyInfo = nullptr;
    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId == PartyId)
        {
            EnteringPartyInfo = &PartyInfo;
            break;
        }
    }

    if (!EnteringPartyInfo)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("파티 정보를 확인할 수 없습니다."));
        return false;
    }

    TArray<APlayerController*> PartyControllers;
    for (const FLobbyPartyMemberInfo& MemberInfo : EnteringPartyInfo->Members)
    {
        if (MemberInfo.ConnectionState != ELobbyPartyConnectionState::Online || !MemberInfo.PlayerState)
        {
            continue;
        }

        APlayerController* MemberController = nullptr;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PlayerController = It->Get();
            if (PlayerController && PlayerController->PlayerState == MemberInfo.PlayerState.Get())
            {
                MemberController = PlayerController;
                break;
            }
        }

        if (MemberController)
        {
            PartyControllers.Add(MemberController);
        }
    }

    if (PartyControllers.Num() < MaxPartyMemberCount)
    {
        SendPartyInviteRequestMessage(RequestingController, TEXT("던전으로 이동할 파티원 접속 정보를 확인할 수 없습니다."));
        return false;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("partyId"), PartyId);

    TArray<TSharedPtr<FJsonValue>> MemberUserIndexValues;
    TArray<TSharedPtr<FJsonValue>> MemberValues;
    for (const FLobbyPartyMemberInfo& MemberInfo : EnteringPartyInfo->Members)
    {
        if (MemberInfo.UserIndex > 0)
        {
            MemberUserIndexValues.Add(MakeShared<FJsonValueNumber>(MemberInfo.UserIndex));

            TSharedRef<FJsonObject> MemberObject = MakeShared<FJsonObject>();
            MemberObject->SetNumberField(TEXT("userIndex"), MemberInfo.UserIndex);
            MemberObject->SetNumberField(TEXT("characterId"), MemberInfo.SelectedCharacterId);
            MemberValues.Add(MakeShared<FJsonValueObject>(MemberObject));
        }
    }
    JsonObject->SetArrayField(TEXT("memberUserIndexes"), MemberUserIndexValues);
    JsonObject->SetArrayField(TEXT("members"), MemberValues);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TArray<TWeakObjectPtr<APlayerController>> WeakPartyControllers;
    for (APlayerController* PartyController : PartyControllers)
    {
        WeakPartyControllers.Add(PartyController);
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(DungeonAllocateUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->SetTimeout(DungeonAllocateHttpTimeoutSeconds);
    HttpRequest->SetActivityTimeout(DungeonAllocateHttpTimeoutSeconds);

    TWeakObjectPtr<APlayerController> WeakRequestingController(RequestingController);
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, PartyId, WeakRequestingController, WeakPartyControllers](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleDungeonServerAllocationResponse(Request, Response, bWasSuccessful, PartyId, WeakRequestingController, WeakPartyControllers);
        });

    PendingDungeonAllocationPartyIds.Add(PartyId);
    if (!HttpRequest->ProcessRequest())
    {
        PendingDungeonAllocationPartyIds.Remove(PartyId);
        SendPartyInviteRequestMessage(RequestingController, TEXT("던전 서버 할당 요청을 시작할 수 없습니다."));
        return false;
    }

    SendPartyInviteRequestMessage(RequestingController, TEXT("던전 서버를 준비 중입니다."));
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 할당 요청 응답을 처리하고 파티원들을 할당된 던전 서버로 이동시키는 함수
// Request : 완료된 HTTP 요청
// Response : HTTP 응답
// bWasSuccessful : HTTP 요청 성공 여부
// PartyId : 던전으로 이동할 파티 ID
// RequestingController : 던전 입장을 요청한 파티장 컨트롤러
// PartyControllers : 던전으로 이동할 파티원 컨트롤러 목록
void ACPP_LobbyGMB::HandleDungeonServerAllocationResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bWasSuccessful,
    int32 PartyId,
    TWeakObjectPtr<APlayerController> RequestingController,
    TArray<TWeakObjectPtr<APlayerController>> PartyControllers)
{
    PendingDungeonAllocationPartyIds.Remove(PartyId);

    APlayerController* RequestingPlayerController = RequestingController.Get();
    if (!bWasSuccessful || !Response.IsValid())
    {
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("던전 서버 할당 요청에 실패했습니다."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        SendPartyInviteRequestMessage(RequestingPlayerController, FString::Printf(TEXT("던전 서버 할당 응답이 올바르지 않습니다. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);
    if (!bOk || ResponseCode < 200 || ResponseCode >= 300)
    {
        SendPartyInviteRequestMessage(RequestingPlayerController, Message.IsEmpty() ? FString::Printf(TEXT("던전 서버 할당에 실패했습니다. HTTP %d"), ResponseCode) : Message);
        return;
    }

    const TSharedPtr<FJsonObject>* SessionObject = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("session"), SessionObject) || !SessionObject || !SessionObject->IsValid())
    {
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("던전 서버 세션 정보가 없습니다."));
        return;
    }

    FString DungeonServerAddress;
    FString DungeonSessionId;
    (*SessionObject)->TryGetStringField(TEXT("address"), DungeonServerAddress);
    (*SessionObject)->TryGetStringField(TEXT("dungeonSessionId"), DungeonSessionId);
    DungeonServerAddress.TrimStartAndEndInline();
    DungeonSessionId.TrimStartAndEndInline();

    if (DungeonServerAddress.IsEmpty())
    {
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("할당된 던전 서버 주소가 비어 있습니다."));
        return;
    }

    if (DungeonSessionId.IsEmpty())
    {
        DungeonSessionId = DungeonServerAddress;
    }

    ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>();
    if (!LobbyGSB)
    {
        RequestDungeonServerShutdown(DungeonSessionId);
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("파티 정보를 확인할 수 없습니다."));
        return;
    }

    if (!LobbyGSB->SetPartyDungeonSession(PartyId, DungeonSessionId))
    {
        RequestDungeonServerShutdown(DungeonSessionId);
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("파티 던전 세션 정보를 설정할 수 없습니다."));
        return;
    }

    if (!LobbyGSB->SetPartyMembersConnectionState(PartyId, ELobbyPartyConnectionState::InGame))
    {
        LobbyGSB->ClearPartyDungeonSession(PartyId);
        RequestDungeonServerShutdown(DungeonSessionId);
        SendPartyInviteRequestMessage(RequestingPlayerController, TEXT("파티 접속 상태를 던전 입장 상태로 변경할 수 없습니다."));
        return;
    }

    LobbyGSB->ClearPartyReadyState(PartyId);

    UE_LOG(LogTemp, Warning, TEXT("Entering allocated dungeon server. PartyId: %d, MemberCount: %d, Address: %s"), PartyId, PartyControllers.Num(), *DungeonServerAddress);
    for (const TWeakObjectPtr<APlayerController>& PartyControllerPtr : PartyControllers)
    {
        if (APlayerController* PartyController = PartyControllerPtr.Get())
        {
            PartyController->ClientTravel(DungeonServerAddress, TRAVEL_Absolute);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 할당되었지만 사용하지 못한 던전 서버 종료를 GameBackend에 요청하는 함수
// DungeonSessionId : 종료할 던전 서버 세션 ID
void ACPP_LobbyGMB::RequestDungeonServerShutdown(const FString& DungeonSessionId) const
{
    if (DungeonSessionId.IsEmpty())
    {
        return;
    }

    const UServerConfigSubsystem* ServerConfigSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString ShutdownUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonShutdownUrl() : TEXT("");
    const FString ServerAuthKey = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonStateServerAuthKey() : TEXT("");
    if (ShutdownUrl.IsEmpty() || ServerAuthKey.IsEmpty())
    {
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("dungeonSessionId"), DungeonSessionId);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(ShutdownUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->ProcessRequest();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 로비 서버 접속 수를 GameBackend 모니터링 API에 보고하는 함수
void ACPP_LobbyGMB::ReportLobbyTelemetry()
{
    const UServerConfigSubsystem* ServerConfigSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString TelemetryUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetLobbyTelemetryUrl() : TEXT("");
    const FString ServerAuthKey = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonStateServerAuthKey() : TEXT("");
    if (TelemetryUrl.IsEmpty() || ServerAuthKey.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot report lobby telemetry because telemetry URL or server auth key is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("currentClientCount"), CurrentLobbyClientCount);
    JsonObject->SetNumberField(TEXT("totalConnected"), TotalLobbyClientConnectCount);
    JsonObject->SetNumberField(TEXT("totalDisconnected"), TotalLobbyClientDisconnectCount);
    JsonObject->SetStringField(TEXT("reportedAt"), FDateTime::UtcNow().ToIso8601());

    TArray<TSharedPtr<FJsonValue>> ConnectedUserValues;
    if (const ACPP_LobbyGSB* LobbyGSB = GetGameState<ACPP_LobbyGSB>())
    {
        const FDateTime CurrentUtcTime = FDateTime::UtcNow();
        const int64 CurrentUnixTimestamp = CurrentUtcTime.ToUnixTimestamp();
        for (APlayerState* PlayerState : LobbyGSB->PlayerArray)
        {
            const ACPP_LobbyPS* LobbyPS = Cast<ACPP_LobbyPS>(PlayerState);
            if (!LobbyPS)
            {
                continue;
            }

            const FString& Username = LobbyPS->GetUsername();
            const int64 ConnectedUnixTimestamp = LobbyPS->GetLobbyConnectedUnixTimestamp();
            const int64 ConnectedSeconds = ConnectedUnixTimestamp > 0
                ? FMath::Max<int64>(0, CurrentUnixTimestamp - ConnectedUnixTimestamp)
                : 0;

            TSharedRef<FJsonObject> UserObject = MakeShared<FJsonObject>();
            UserObject->SetNumberField(TEXT("userIndex"), LobbyPS->GetUserIndex());
            UserObject->SetNumberField(TEXT("playerId"), LobbyPS->GetPlayerId());
            UserObject->SetStringField(TEXT("username"), Username.IsEmpty() ? LobbyPS->GetPlayerName() : Username);
            UserObject->SetStringField(TEXT("loginAt"), LobbyPS->GetLobbyConnectedAt());
            UserObject->SetNumberField(TEXT("connectedSeconds"), static_cast<double>(ConnectedSeconds));
            UserObject->SetBoolField(TEXT("authVerified"), LobbyPS->IsLobbyAuthVerified());
            ConnectedUserValues.Add(MakeShared<FJsonValueObject>(UserObject));
        }
    }
    JsonObject->SetArrayField(TEXT("connectedUsers"), ConnectedUserValues);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(TelemetryUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &ACPP_LobbyGMB::HandleLobbyTelemetryResponse);

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to start lobby telemetry report. CurrentClientCount: %d"), CurrentLobbyClientCount);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 서버 접속 수 보고 응답을 처리하는 함수
// Request : HTTP 요청 객체
// Response : HTTP 응답 객체
// bWasSuccessful : HTTP 요청이 성공적으로 완료되었는지 여부
void ACPP_LobbyGMB::HandleLobbyTelemetryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Lobby telemetry report failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        UE_LOG(LogTemp, Warning, TEXT("Lobby telemetry report failed. HTTP %d"), ResponseCode);
        return;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 요청 결과 메시지를 요청한 클라이언트에게 전달하는 함수
// TargetController : 메시지를 받을 PlayerController
// Message : 전달할 메시지
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerState를 가진 PlayerController를 현재 월드에서 찾는 함수
// TargetPlayerState : 찾을 PlayerController가 소유한 PlayerState
// Return Value : TargetPlayerState를 소유한 PlayerController, 없으면 nullptr
APlayerController* ACPP_LobbyGMB::FindPlayerControllerByPlayerState(APlayerState* TargetPlayerState) const
{
    if (!TargetPlayerState)
    {
        return nullptr;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (PlayerController && PlayerController->PlayerState == TargetPlayerState)
        {
            return PlayerController;
        }
    }

    return nullptr;
}

void ACPP_LobbyGMB::SendPartyInviteRequestMessage(APlayerController* TargetController, const FString& Message) const
{
    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(TargetController);
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->ClientReceivePartyInviteRequestMessage(Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 토큰 검증이 필요한지 여부를 반환하는 함수
// Return Value : 로비 토큰 검증이 필요한 경우 true, 그렇지 않은 경우 false
bool ACPP_LobbyGMB::IsLobbyTokenVerificationRequired() const
{
    const UServerConfigSubsystem* ServerConfigSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    return !ServerConfigSubsystem || ServerConfigSubsystem->IsLobbyTokenVerificationRequired();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 기능을 사용할 수 있는지 여부를 반환하는 함수
// RequestingController : 파티 기능 사용을 요청한 플레이어의 PlayerController
// Return Value : 파티 기능을 사용할 수 있는 경우 true, 그렇지 않은 경우 false
bool ACPP_LobbyGMB::CanUsePartyFeature(APlayerController* RequestingController) const
{
    if (!RequestingController)
    {
        return false;
    }

    if (!IsLobbyTokenVerificationRequired())
    {
        return true;
    }

    const ACPP_LobbyPS* LobbyPS = RequestingController->GetPlayerState<ACPP_LobbyPS>();
    if (LobbyPS && LobbyPS->IsLobbyAuthVerified())
    {
        return true;
    }

    SendPartyInviteRequestMessage(RequestingController, TEXT("Lobby auth is not complete."));
    return false;
}
