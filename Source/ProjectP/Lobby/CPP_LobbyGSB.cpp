// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_LobbyGSB.h"

#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "CPP_LobbyPS.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

void ACPP_LobbyGSB::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_LobbyGSB, Parties);
    DOREPLIFETIME(ACPP_LobbyGSB, PendingPartyInvites);
    DOREPLIFETIME(ACPP_LobbyGSB, PendingPartyJoinRequests);
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 정보를 반환하는 함수
// Return Value : 현재 존재하는 모든 파티 정보가 담긴 배열
const TArray<FLobbyPartyInfo>& ACPP_LobbyGSB::GetParties() const
{
    return Parties;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 로비에 접속해 있는 유저 정보를 반환하는 함수
// OutOnlineUsers : 현재 접속 유저 정보가 담길 배열
void ACPP_LobbyGSB::GetOnlineUsers(TArray<FLobbyOnlineUserInfo>& OutOnlineUsers) const
{
    OutOnlineUsers.Reset();

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const bool bRequireLobbyAuth = !ServerConfigSubsystem || ServerConfigSubsystem->IsLobbyTokenVerificationRequired();

    for (APlayerState* PlayerState : PlayerArray)
    {
        const ACPP_LobbyPS* LobbyPS = Cast<ACPP_LobbyPS>(PlayerState);
        if (!LobbyPS)
        {
            continue;
        }

        if (bRequireLobbyAuth && !LobbyPS->IsLobbyAuthVerified())
        {
            continue;
        }

        FLobbyOnlineUserInfo OnlineUser;
        const FString& Username = LobbyPS->GetUsername();
        OnlineUser.Username = Username.IsEmpty() ? LobbyPS->GetPlayerName() : Username;
        OnlineUser.PartyId = LobbyPS->GetPartyId();
        OnlineUser.bIsPartyLeader = LobbyPS->IsPartyLeader();
        OnlineUser.PlayerState = PlayerState;

        OutOnlineUsers.Add(OnlineUser);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 대기 중인 파티 초대 목록을 반환하는 함수
// Return Value : 아직 처리되지 않은 파티 초대 정보 배열
const TArray<FLobbyPartyInviteInfo>& ACPP_LobbyGSB::GetPendingPartyInvites() const
{
    return PendingPartyInvites;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 대기 중인 파티 가입 신청 목록을 반환하는 함수
// Return Value : 아직 처리되지 않은 파티 가입 신청 정보 배열
const TArray<FLobbyPartyJoinRequestInfo>& ACPP_LobbyGSB::GetPendingPartyJoinRequests() const
{
    return PendingPartyJoinRequests;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 파티의 현재 인원 수를 반환하는 함수
// PartyId : 인원 수를 확인할 파티 ID
// Return Value : 해당 파티의 현재 멤버 수
int32 ACPP_LobbyGSB::GetPartyMemberCount(int32 PartyId) const
{
    for (const FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId == PartyId)
        {
            return PartyInfo.Members.Num();
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티에서 사용할 안정적인 유저 식별자를 반환하는 함수
// LobbyPS : 식별자를 가져올 로비 PlayerState
// Return Value : 로그인 유저 DB Index, 인증 정보가 없으면 현재 PlayerState의 PlayerId
int32 ACPP_LobbyGSB::GetStablePartyUserIndex(const ACPP_LobbyPS* LobbyPS) const
{
    if (!LobbyPS)
    {
        return -1;
    }

    const int32 AuthenticatedUserIndex = LobbyPS->GetUserIndex();
    if (AuthenticatedUserIndex > 0)
    {
        return AuthenticatedUserIndex;
    }

    return LobbyPS->GetPlayerId();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerState 정보를 기준으로 파티 멤버 데이터를 생성하는 함수
// LobbyPS : 파티 멤버로 저장할 로비 PlayerState
// InitialConnectionState : 처음 저장할 접속 상태
// Return Value : 파티 멤버 데이터
FLobbyPartyMemberInfo ACPP_LobbyGSB::MakePartyMemberInfo(ACPP_LobbyPS* LobbyPS, ELobbyPartyConnectionState InitialConnectionState) const
{
    FLobbyPartyMemberInfo MemberInfo;
    if (!LobbyPS)
    {
        return MemberInfo;
    }

    const FString& Username = LobbyPS->GetUsername();
    MemberInfo.UserIndex = GetStablePartyUserIndex(LobbyPS);
    MemberInfo.Username = Username.IsEmpty() ? LobbyPS->GetPlayerName() : Username;
    MemberInfo.ConnectionState = InitialConnectionState;
    MemberInfo.bIsReady = false;

    // 로비 입장 시 선택한 캐릭터를 파티 기본 선택 캐릭터로 사용한다. (아직 선택 전이면 -1 유지)
    const int32 EntrySelectedCharacterId = LobbyPS->GetSelectedCharacterId();
    MemberInfo.SelectedCharacterId = IsValidSelectedCharacterId(EntrySelectedCharacterId) ? EntrySelectedCharacterId : -1;

    MemberInfo.PlayerState = LobbyPS;

    return MemberInfo;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 안에서 특정 유저 식별자를 가진 멤버 데이터를 찾는 함수
// PartyInfo : 검색할 파티 정보
// UserIndex : 찾을 유저 식별자
// Return Value : 찾은 파티 멤버 데이터 포인터, 없으면 nullptr
FLobbyPartyMemberInfo* ACPP_LobbyGSB::FindPartyMemberByUserIndex(FLobbyPartyInfo& PartyInfo, int32 UserIndex)
{
    for (FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (MemberInfo.UserIndex == UserIndex)
        {
            return &MemberInfo;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 안에서 특정 유저 식별자를 가진 멤버 데이터를 읽기 전용으로 찾는 함수
// PartyInfo : 검색할 파티 정보
// UserIndex : 찾을 유저 식별자
// Return Value : 찾은 파티 멤버 데이터 포인터, 없으면 nullptr
const FLobbyPartyMemberInfo* ACPP_LobbyGSB::FindPartyMemberByUserIndex(const FLobbyPartyInfo& PartyInfo, int32 UserIndex) const
{
    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (MemberInfo.UserIndex == UserIndex)
        {
            return &MemberInfo;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티에 접속 중인 PlayerState들에게 파티 ID와 파티장 여부를 동기화하는 함수
// PartyInfo : 동기화할 파티 정보
void ACPP_LobbyGSB::SyncPartyInfoToConnectedMembers(FLobbyPartyInfo& PartyInfo) const
{
    for (FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberInfo.PlayerState.Get());
        if (!MemberLobbyPS)
        {
            continue;
        }

        const bool bIsLeader = MemberInfo.UserIndex == PartyInfo.LeaderUserIndex;
        MemberLobbyPS->SetPartyInfo(PartyInfo.PartyId, bIsLeader);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 접속 중인 파티원 중 조건에 맞는 첫 번째 멤버에게 파티장을 양도하는 함수
// PartyInfo : 파티장 정보를 변경할 파티
// RequiredConnectionState : 파티장을 받을 수 있는 접속 상태
// Return Value : 파티장 양도 성공 여부
bool ACPP_LobbyGSB::TransferPartyLeaderToFirstConnectedMember(FLobbyPartyInfo& PartyInfo, ELobbyPartyConnectionState RequiredConnectionState)
{
    for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
    {
        if (MemberInfo.ConnectionState != RequiredConnectionState || !MemberInfo.PlayerState)
        {
            continue;
        }

        PartyInfo.LeaderUserIndex = MemberInfo.UserIndex;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 PlayerState와 연결된 대기 중인 파티 초대 정보를 제거하는 함수
// PlayerState : 초대 정보에서 제거 기준으로 사용할 PlayerState
void ACPP_LobbyGSB::RemovePendingPartyInvitesByPlayerState(APlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    PendingPartyInvites.RemoveAll(
        [PlayerState](const FLobbyPartyInviteInfo& PendingInvite)
        {
            return PendingInvite.InviterPlayerState == PlayerState ||
                PendingInvite.TargetPlayerState == PlayerState ||
                (!PendingInvite.InviterPlayerState || !PendingInvite.TargetPlayerState);
        });
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택된 캐릭터 ID가 유효한지 확인하는 함수
// SelectedCharacterId : 확인할 캐릭터 ID
// Return Value : 유효한 캐릭터 ID이면 true, 아니면 false
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 PlayerState와 연결된 대기 중인 파티 가입 신청 정보를 제거하는 함수
// PlayerState : 가입 신청 정보에서 제거 기준으로 사용할 PlayerState
void ACPP_LobbyGSB::RemovePendingPartyJoinRequestsByPlayerState(APlayerState* PlayerState)
{
    if (!PlayerState)
    {
        return;
    }

    PendingPartyJoinRequests.RemoveAll(
        [PlayerState](const FLobbyPartyJoinRequestInfo& PendingJoinRequest)
        {
            return PendingJoinRequest.ApplicantPlayerState == PlayerState ||
                PendingJoinRequest.LeaderPlayerState == PlayerState ||
                (!PendingJoinRequest.ApplicantPlayerState || !PendingJoinRequest.LeaderPlayerState);
        });
}

bool ACPP_LobbyGSB::IsValidSelectedCharacterId(int32 SelectedCharacterId) const
{
    return SelectedCharacterId == 100 ||
        SelectedCharacterId == 200 ||
        SelectedCharacterId == 300;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 새로운 파티를 추가하는 함수
// LeaderPlayerState : 파티 리더가 될 플레이어의 PlayerState
// Return Value : 새로 생성된 파티의 ID (성공 시), -1 (실패 시)
int32 ACPP_LobbyGSB::AddParty(APlayerState* LeaderPlayerState)
{
    ACPP_LobbyPS* LeaderLobbyPS = Cast<ACPP_LobbyPS>(LeaderPlayerState);
    if (!LeaderLobbyPS)
    {
        return -1;
    }

    const int32 LeaderUserIndex = GetStablePartyUserIndex(LeaderLobbyPS);
    if (LeaderUserIndex == -1)
    {
        return -1;
    }

    int32 NewPartyId = 0;
    for (const FLobbyPartyInfo& PartyInfo : Parties)
    {
        NewPartyId = FMath::Max(NewPartyId, PartyInfo.PartyId + 1);
    }

    FLobbyPartyInfo NewParty;
    NewParty.PartyId = NewPartyId;
    NewParty.LeaderUserIndex = LeaderUserIndex;
    NewParty.Members.Add(MakePartyMemberInfo(LeaderLobbyPS, ELobbyPartyConnectionState::Online));

    Parties.Add(NewParty);

    return NewPartyId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기존 파티에 플레이어를 추가하는 함수
// JoiningPlayerState : 파티에 참가하려는 플레이어의 PlayerState
// PartyId : 참가할 파티 ID
// MaxPartyMemberCount : 파티 최대 인원 수
// Return Value : 파티 참가 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGSB::AddPlayerToParty(APlayerState* JoiningPlayerState, int32 PartyId, int32 MaxPartyMemberCount)
{
    ACPP_LobbyPS* JoiningLobbyPS = Cast<ACPP_LobbyPS>(JoiningPlayerState);
    if (!JoiningLobbyPS || PartyId == -1)
    {
        return false;
    }

    const int32 JoiningUserIndex = GetStablePartyUserIndex(JoiningLobbyPS);
    if (JoiningUserIndex == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        if (FindPartyMemberByUserIndex(PartyInfo, JoiningUserIndex) || PartyInfo.Members.Num() >= MaxPartyMemberCount)
        {
            return false;
        }

        // 캐릭터 중복 선택 자체는 허용되므로 기본 선택 캐릭터가 다른 멤버와 겹쳐도 그대로 둔다. (중복 검증은 Ready 시점에만)
        PartyInfo.Members.Add(MakePartyMemberInfo(JoiningLobbyPS, ELobbyPartyConnectionState::Online));
        SyncPartyInfoToConnectedMembers(PartyInfo);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티에서 플레이어를 제거하는 함수
// LeavingPlayerState : 파티에서 나가려는 플레이어의 PlayerState
// Return Value : 플레이어 제거 성공 여부 (true: 성공, false: 실패)
bool ACPP_LobbyGSB::RemovePlayerFromParty(APlayerState* LeavingPlayerState)
{
    ACPP_LobbyPS* LeavingLobbyPS = Cast<ACPP_LobbyPS>(LeavingPlayerState);
    if (!LeavingLobbyPS)
    {
        return false;
    }

    const int32 LeavingUserIndex = GetStablePartyUserIndex(LeavingLobbyPS);
    if (LeavingUserIndex == -1)
    {
        return false;
    }

    for (int32 PartyIndex = Parties.Num() - 1; PartyIndex >= 0; --PartyIndex)
    {
        FLobbyPartyInfo& Party = Parties[PartyIndex];

        FLobbyPartyMemberInfo* LeavingMemberInfo = FindPartyMemberByUserIndex(Party, LeavingUserIndex);
        if (!LeavingMemberInfo)
        {
            continue;
        }

        const int32 RemovedPartyId = Party.PartyId;

        Party.Members.RemoveAll(
            [LeavingUserIndex](const FLobbyPartyMemberInfo& MemberInfo)
            {
                return MemberInfo.UserIndex == LeavingUserIndex;
            });

        RemovePendingPartyInvitesByPlayerState(LeavingPlayerState);
        RemovePendingPartyJoinRequestsByPlayerState(LeavingPlayerState);

        if (Party.LeaderUserIndex == LeavingUserIndex || Party.Members.Num() == 0)
        {
            for (const FLobbyPartyMemberInfo& MemberInfo : Party.Members)
            {
                ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberInfo.PlayerState.Get());
                if (MemberLobbyPS)
                {
                    MemberLobbyPS->SetPartyInfo(-1, false);
                }
            }

            PendingPartyInvites.RemoveAll(
                [RemovedPartyId](const FLobbyPartyInviteInfo& PendingInvite)
                {
                    return PendingInvite.PartyId == RemovedPartyId;
                });

            PendingPartyJoinRequests.RemoveAll(
                [RemovedPartyId](const FLobbyPartyJoinRequestInfo& PendingJoinRequest)
                {
                    return PendingJoinRequest.PartyId == RemovedPartyId;
                });

            Parties.RemoveAt(PartyIndex);
        }
        else
        {
            SyncPartyInfoToConnectedMembers(Party);
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 접속 상태를 변경하는 함수
// MemberPlayerState : 상태를 변경할 파티 멤버 PlayerState
// NewConnectionState : 새로 적용할 접속 상태
// Return Value : 접속 상태 변경 성공 여부
bool ACPP_LobbyGSB::SetPartyMemberConnectionState(APlayerState* MemberPlayerState, ELobbyPartyConnectionState NewConnectionState)
{
    ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS)
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo)
        {
            continue;
        }

        MemberInfo->ConnectionState = NewConnectionState;
        MemberInfo->bIsReady = false;

        if (NewConnectionState == ELobbyPartyConnectionState::Offline ||
            NewConnectionState == ELobbyPartyConnectionState::InGame ||
            NewConnectionState == ELobbyPartyConnectionState::OutGame)
        {
            MemberInfo->PlayerState = nullptr;
            RemovePendingPartyInvitesByPlayerState(MemberPlayerState);
            RemovePendingPartyJoinRequestsByPlayerState(MemberPlayerState);
        }
        else
        {
            const FString& Username = MemberLobbyPS->GetUsername();
            MemberInfo->Username = Username.IsEmpty() ? MemberLobbyPS->GetPlayerName() : Username;
            MemberInfo->PlayerState = MemberLobbyPS;
        }

        if (PartyInfo.LeaderUserIndex == MemberUserIndex &&
            NewConnectionState == ELobbyPartyConnectionState::Offline)
        {
            TransferPartyLeaderToFirstConnectedMember(PartyInfo, ELobbyPartyConnectionState::Online);
        }

        SyncPartyInfoToConnectedMembers(PartyInfo);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 현재 접속 상태를 조회하는 함수
// MemberPlayerState : 상태를 조회할 파티 멤버 PlayerState
// OutConnectionState : 조회된 접속 상태가 담길 변수
// Return Value : 접속 상태 조회 성공 여부
bool ACPP_LobbyGSB::GetPartyMemberConnectionState(APlayerState* MemberPlayerState, ELobbyPartyConnectionState& OutConnectionState) const
{
    const ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS)
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (const FLobbyPartyInfo& PartyInfo : Parties)
    {
        const FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo)
        {
            continue;
        }

        OutConnectionState = MemberInfo->ConnectionState;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 파티의 모든 멤버 접속 상태를 변경하는 함수
// PartyId : 접속 상태를 변경할 파티 ID
// NewConnectionState : 새로 적용할 접속 상태
// Return Value : 접속 상태 변경 성공 여부
bool ACPP_LobbyGSB::SetPartyMembersConnectionState(int32 PartyId, ELobbyPartyConnectionState NewConnectionState)
{
    if (PartyId == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        for (FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            MemberInfo.ConnectionState = NewConnectionState;
            MemberInfo.bIsReady = false;

            if (NewConnectionState != ELobbyPartyConnectionState::Online)
            {
                MemberInfo.PlayerState = nullptr;
            }
        }

        if (NewConnectionState == ELobbyPartyConnectionState::InGame)
        {
            PartyInfo.PartyState = ELobbyPartyState::Dungeon;
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티가 현재 연결된 던전 세션 ID를 설정하는 함수
// PartyId : 던전 세션 ID를 설정할 파티 ID
// NewDungeonSessionId : 파티와 연결할 던전 세션 ID
// Return Value : 던전 세션 ID 설정 성공 여부
bool ACPP_LobbyGSB::SetPartyDungeonSession(int32 PartyId, const FString& NewDungeonSessionId)
{
    if (PartyId == -1 || NewDungeonSessionId.IsEmpty())
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        PartyInfo.PartyState = ELobbyPartyState::Dungeon;
        PartyInfo.CurrentDungeonSessionId = NewDungeonSessionId;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티의 던전 세션 연결 정보를 로비 상태로 초기화하는 함수
// PartyId : 던전 세션 연결 정보를 초기화할 파티 ID
// Return Value : 던전 세션 연결 정보 초기화 성공 여부
bool ACPP_LobbyGSB::ClearPartyDungeonSession(int32 PartyId)
{
    if (PartyId == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        PartyInfo.PartyState = ELobbyPartyState::Lobby;
        PartyInfo.CurrentDungeonSessionId.Empty();
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속한 플레이어를 기존 파티 멤버 데이터에 다시 연결하는 함수
// MemberPlayerState : 재접속한 플레이어의 PlayerState
// Return Value : 기존 파티 멤버 복구 성공 여부
bool ACPP_LobbyGSB::RestorePartyMemberOnline(APlayerState* MemberPlayerState)
{
    ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS)
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo)
        {
            continue;
        }

        const bool bNeedsDungeonStateCleanup = PartyInfo.PartyState == ELobbyPartyState::Dungeon ||
            !PartyInfo.CurrentDungeonSessionId.IsEmpty() ||
            MemberInfo->ConnectionState == ELobbyPartyConnectionState::InGame ||
            MemberInfo->ConnectionState == ELobbyPartyConnectionState::OutGame;

        const FString& Username = MemberLobbyPS->GetUsername();
        MemberInfo->Username = Username.IsEmpty() ? MemberLobbyPS->GetPlayerName() : Username;
        MemberInfo->ConnectionState = ELobbyPartyConnectionState::Online;
        MemberInfo->PlayerState = MemberLobbyPS;

        if (bNeedsDungeonStateCleanup)
        {
            for (FLobbyPartyMemberInfo& PartyMemberInfo : PartyInfo.Members)
            {
                if (PartyMemberInfo.UserIndex == MemberUserIndex)
                {
                    continue;
                }

                if (PartyMemberInfo.ConnectionState == ELobbyPartyConnectionState::InGame ||
                    PartyMemberInfo.ConnectionState == ELobbyPartyConnectionState::OutGame)
                {
                    PartyMemberInfo.ConnectionState = ELobbyPartyConnectionState::Offline;
                    PartyMemberInfo.bIsReady = false;
                    PartyMemberInfo.PlayerState = nullptr;
                }
            }

            PartyInfo.PartyState = ELobbyPartyState::Lobby;
            PartyInfo.CurrentDungeonSessionId.Empty();
        }
        else
        {
            bool bAllMembersOnline = true;
            for (const FLobbyPartyMemberInfo& PartyMemberInfo : PartyInfo.Members)
            {
                if (PartyMemberInfo.ConnectionState != ELobbyPartyConnectionState::Online)
                {
                    bAllMembersOnline = false;
                    break;
                }
            }

            if (bAllMembersOnline)
            {
                PartyInfo.PartyState = ELobbyPartyState::Lobby;
                PartyInfo.CurrentDungeonSessionId.Empty();
            }
        }

        SyncPartyInfoToConnectedMembers(PartyInfo);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 선택 캐릭터 ID를 변경하는 함수
// MemberPlayerState : 선택 캐릭터를 변경할 파티 멤버 PlayerState
// NewSelectedCharacterId : 새로 선택한 캐릭터 ID
// Return Value : 선택 캐릭터 변경 성공 여부
bool ACPP_LobbyGSB::SetPartyMemberSelectedCharacter(APlayerState* MemberPlayerState, int32 NewSelectedCharacterId)
{
    ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS || !IsValidSelectedCharacterId(NewSelectedCharacterId))
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo || MemberInfo->ConnectionState != ELobbyPartyConnectionState::Online)
        {
            continue;
        }

        // 파티 내 캐릭터 중복 선택 자체는 항상 허용한다. 중복 검증은 Ready 시점(SetPartyMemberReady)에만 한다.
        if (MemberInfo->SelectedCharacterId != NewSelectedCharacterId)
        {
            MemberInfo->SelectedCharacterId = NewSelectedCharacterId;
            MemberInfo->bIsReady = false;
        }
        MemberInfo->PlayerState = MemberLobbyPS;

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 Ready 상태를 변경하는 함수
// MemberPlayerState : Ready 상태를 변경할 파티 멤버 PlayerState
// bNewIsReady : 새 Ready 상태
// Return Value : Ready 상태 변경 성공 여부
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버가 현재 선택해 둔 캐릭터 ID를 조회하는 함수
// MemberPlayerState : 선택 캐릭터 ID를 조회할 파티 멤버 PlayerState
// OutSelectedCharacterId : 조회된 선택 캐릭터 ID가 담길 변수
// Return Value : 선택 캐릭터 ID 조회 성공 여부
bool ACPP_LobbyGSB::GetPartyMemberSelectedCharacter(APlayerState* MemberPlayerState, int32& OutSelectedCharacterId) const
{
    OutSelectedCharacterId = -1;

    const ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS)
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (const FLobbyPartyInfo& PartyInfo : Parties)
    {
        const FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo)
        {
            continue;
        }

        OutSelectedCharacterId = MemberInfo->SelectedCharacterId;
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 Ready 상태를 변경하는 함수
// MemberPlayerState : Ready 상태를 변경할 파티 멤버 PlayerState
// bNewIsReady : 새 Ready 상태
// Return Value : Ready 상태 변경 성공 여부
bool ACPP_LobbyGSB::SetPartyMemberReady(APlayerState* MemberPlayerState, bool bNewIsReady)
{
    ACPP_LobbyPS* MemberLobbyPS = Cast<ACPP_LobbyPS>(MemberPlayerState);
    if (!MemberLobbyPS)
    {
        return false;
    }

    const int32 MemberUserIndex = GetStablePartyUserIndex(MemberLobbyPS);
    if (MemberUserIndex == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        FLobbyPartyMemberInfo* MemberInfo = FindPartyMemberByUserIndex(PartyInfo, MemberUserIndex);
        if (!MemberInfo || MemberInfo->ConnectionState != ELobbyPartyConnectionState::Online)
        {
            continue;
        }

        if (bNewIsReady)
        {
            if (!IsValidSelectedCharacterId(MemberInfo->SelectedCharacterId))
            {
                return false;
            }

            // 레디 시점에 다른 멤버가 이미 같은 캐릭터로 Ready 확정한 상태면 레디 불가. (중복 선택 자체는 허용)
            for (const FLobbyPartyMemberInfo& PartyMemberInfo : PartyInfo.Members)
            {
                if (PartyMemberInfo.UserIndex != MemberUserIndex &&
                    PartyMemberInfo.bIsReady &&
                    PartyMemberInfo.SelectedCharacterId == MemberInfo->SelectedCharacterId)
                {
                    return false;
                }
            }
        }

        MemberInfo->bIsReady = bNewIsReady;
        MemberInfo->PlayerState = MemberLobbyPS;

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티가 던전에 입장할 수 있는 상태인지 확인하는 함수
// PartyId : 확인할 파티 ID
// RequiredMemberCount : 던전 입장에 필요한 파티원 수
// Return Value : 모든 파티원이 Online 상태이고 캐릭터 선택 후 Ready 상태이면 true, 아니면 false
bool ACPP_LobbyGSB::CanPartyEnterDungeon(int32 PartyId, int32 RequiredMemberCount) const
{
    if (PartyId == -1 || RequiredMemberCount <= 0)
    {
        return false;
    }

    for (const FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        if (PartyInfo.Members.Num() < RequiredMemberCount)
        {
            return false;
        }

        TSet<int32> ReadyCharacterIds;
        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.ConnectionState != ELobbyPartyConnectionState::Online ||
                !MemberInfo.PlayerState ||
                !MemberInfo.bIsReady ||
                !IsValidSelectedCharacterId(MemberInfo.SelectedCharacterId))
            {
                return false;
            }

            if (ReadyCharacterIds.Contains(MemberInfo.SelectedCharacterId))
            {
                return false;
            }

            ReadyCharacterIds.Add(MemberInfo.SelectedCharacterId);
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 파티의 모든 Ready 상태를 false로 초기화하는 함수
// PartyId : Ready 상태를 초기화할 파티 ID
// Return Value : Ready 상태 초기화 성공 여부
bool ACPP_LobbyGSB::ClearPartyReadyState(int32 PartyId)
{
    if (PartyId == -1)
    {
        return false;
    }

    for (FLobbyPartyInfo& PartyInfo : Parties)
    {
        if (PartyInfo.PartyId != PartyId)
        {
            continue;
        }

        for (FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            MemberInfo.bIsReady = false;
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 초대 정보를 추가하거나 기존 초대 시간을 갱신하는 함수
// InviterPlayerState : 초대한 플레이어의 PlayerState
// TargetPlayerState : 초대 대상 플레이어의 PlayerState
// PartyId : 초대할 파티 ID
// CurrentServerTime : 현재 서버 시간
// CooldownSeconds : 동일 대상 재초대 제한 시간
// OutRemainingCooldownSeconds : 쿨타임이 남아 있을 경우 남은 시간
// Return Value : 초대 추가 또는 갱신 성공 여부 (true: 성공, false: 쿨타임 또는 실패)
bool ACPP_LobbyGSB::AddPartyInvite(APlayerState* InviterPlayerState, APlayerState* TargetPlayerState, int32 PartyId, float CurrentServerTime, float CooldownSeconds, float& OutRemainingCooldownSeconds)
{
    OutRemainingCooldownSeconds = 0.0f;

    if (!InviterPlayerState || !TargetPlayerState || PartyId == -1)
    {
        return false;
    }

    for (int32 InviteIndex = PendingPartyInvites.Num() - 1; InviteIndex >= 0; --InviteIndex)
    {
        FLobbyPartyInviteInfo& PendingInvite = PendingPartyInvites[InviteIndex];
        if (!PendingInvite.InviterPlayerState || !PendingInvite.TargetPlayerState)
        {
            PendingPartyInvites.RemoveAt(InviteIndex);
            continue;
        }

        if (PendingInvite.InviterPlayerState != InviterPlayerState || PendingInvite.TargetPlayerState != TargetPlayerState)
        {
            continue;
        }

        const float ElapsedTime = CurrentServerTime - PendingInvite.SentServerTime;
        if (ElapsedTime < CooldownSeconds)
        {
            OutRemainingCooldownSeconds = CooldownSeconds - ElapsedTime;
            return false;
        }

        PendingInvite.PartyId = PartyId;
        PendingInvite.SentServerTime = CurrentServerTime;
        return true;
    }

    FLobbyPartyInviteInfo NewInvite;
    NewInvite.PartyId = PartyId;
    NewInvite.InviterPlayerState = InviterPlayerState;
    NewInvite.TargetPlayerState = TargetPlayerState;
    NewInvite.SentServerTime = CurrentServerTime;

    PendingPartyInvites.Add(NewInvite);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 플레이어에게 도착한 특정 파티 초대가 대기 중인지 확인하는 함수
// TargetPlayerState : 초대를 받은 플레이어의 PlayerState
// PartyId : 확인할 파티 ID
// Return Value : 대기 중인 초대 존재 여부 (true: 존재, false: 없음)
bool ACPP_LobbyGSB::HasPendingPartyInvite(APlayerState* TargetPlayerState, int32 PartyId) const
{
    if (!TargetPlayerState || PartyId == -1)
    {
        return false;
    }

    for (const FLobbyPartyInviteInfo& PendingInvite : PendingPartyInvites)
    {
        if (PendingInvite.TargetPlayerState == TargetPlayerState && PendingInvite.PartyId == PartyId)
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 플레이어에게 도착한 파티 초대를 제거하는 함수
// TargetPlayerState : 초대를 받은 플레이어의 PlayerState
// PartyId : 제거할 파티 ID, -1이면 해당 플레이어의 모든 초대 제거
// Return Value : 제거된 초대 존재 여부 (true: 제거됨, false: 제거할 초대 없음)
bool ACPP_LobbyGSB::RemovePendingPartyInvites(APlayerState* TargetPlayerState, int32 PartyId)
{
    if (!TargetPlayerState)
    {
        return false;
    }

    const int32 RemovedCount = PendingPartyInvites.RemoveAll(
        [TargetPlayerState, PartyId](const FLobbyPartyInviteInfo& PendingInvite)
        {
            const bool bIsTargetInvite = PendingInvite.TargetPlayerState == TargetPlayerState;
            const bool bMatchesParty = PartyId == -1 || PendingInvite.PartyId == PartyId;
            return bIsTargetInvite && bMatchesParty;
        });

    return RemovedCount > 0;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 정보를 추가하거나 기존 가입 신청 시간을 갱신하는 함수
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
// LeaderPlayerState : 가입 신청을 받을 파티장의 PlayerState
// PartyId : 가입 신청을 보낼 파티 ID
// CurrentServerTime : 현재 서버 시간
// CooldownSeconds : 동일 파티 재신청 제한 시간
// OutRemainingCooldownSeconds : 쿨타임이 남아 있을 경우 남은 시간
// Return Value : 가입 신청 추가 또는 갱신 성공 여부 (true: 성공, false: 쿨타임 또는 실패)
bool ACPP_LobbyGSB::AddPartyJoinRequest(APlayerState* ApplicantPlayerState, APlayerState* LeaderPlayerState, int32 PartyId, float CurrentServerTime, float CooldownSeconds, float& OutRemainingCooldownSeconds)
{
    OutRemainingCooldownSeconds = 0.0f;

    if (!ApplicantPlayerState || !LeaderPlayerState || PartyId == -1)
    {
        return false;
    }

    for (int32 RequestIndex = PendingPartyJoinRequests.Num() - 1; RequestIndex >= 0; --RequestIndex)
    {
        FLobbyPartyJoinRequestInfo& PendingJoinRequest = PendingPartyJoinRequests[RequestIndex];
        if (!PendingJoinRequest.ApplicantPlayerState || !PendingJoinRequest.LeaderPlayerState)
        {
            PendingPartyJoinRequests.RemoveAt(RequestIndex);
            continue;
        }

        if (PendingJoinRequest.ApplicantPlayerState != ApplicantPlayerState || PendingJoinRequest.PartyId != PartyId)
        {
            continue;
        }

        const float ElapsedTime = CurrentServerTime - PendingJoinRequest.SentServerTime;
        if (ElapsedTime < CooldownSeconds)
        {
            OutRemainingCooldownSeconds = CooldownSeconds - ElapsedTime;
            return false;
        }

        PendingJoinRequest.LeaderPlayerState = LeaderPlayerState;
        PendingJoinRequest.SentServerTime = CurrentServerTime;
        return true;
    }

    FLobbyPartyJoinRequestInfo NewJoinRequest;
    NewJoinRequest.PartyId = PartyId;
    NewJoinRequest.ApplicantPlayerState = ApplicantPlayerState;
    NewJoinRequest.LeaderPlayerState = LeaderPlayerState;
    NewJoinRequest.SentServerTime = CurrentServerTime;

    PendingPartyJoinRequests.Add(NewJoinRequest);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 플레이어가 특정 파티에 가입 신청을 대기 중인지 확인하는 함수
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
// PartyId : 확인할 파티 ID
// Return Value : 대기 중인 가입 신청 존재 여부 (true: 존재, false: 없음)
bool ACPP_LobbyGSB::HasPendingPartyJoinRequest(APlayerState* ApplicantPlayerState, int32 PartyId) const
{
    if (!ApplicantPlayerState || PartyId == -1)
    {
        return false;
    }

    for (const FLobbyPartyJoinRequestInfo& PendingJoinRequest : PendingPartyJoinRequests)
    {
        if (PendingJoinRequest.ApplicantPlayerState == ApplicantPlayerState && PendingJoinRequest.PartyId == PartyId)
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 플레이어의 파티 가입 신청을 제거하는 함수
// ApplicantPlayerState : 가입 신청을 보낸 플레이어의 PlayerState
// PartyId : 제거할 파티 ID, -1이면 해당 플레이어의 모든 가입 신청 제거
// Return Value : 제거된 가입 신청 존재 여부 (true: 제거됨, false: 제거할 가입 신청 없음)
bool ACPP_LobbyGSB::RemovePendingPartyJoinRequests(APlayerState* ApplicantPlayerState, int32 PartyId)
{
    if (!ApplicantPlayerState)
    {
        return false;
    }

    const int32 RemovedCount = PendingPartyJoinRequests.RemoveAll(
        [ApplicantPlayerState, PartyId](const FLobbyPartyJoinRequestInfo& PendingJoinRequest)
        {
            const bool bIsApplicantRequest = PendingJoinRequest.ApplicantPlayerState == ApplicantPlayerState;
            const bool bMatchesParty = PartyId == -1 || PendingJoinRequest.PartyId == PartyId;
            return bIsApplicantRequest && bMatchesParty;
        });

    return RemovedCount > 0;
}
