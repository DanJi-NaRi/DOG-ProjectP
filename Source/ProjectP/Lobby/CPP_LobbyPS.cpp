// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_LobbyPS.h"
#include "Net/UnrealNetwork.h"

void ACPP_LobbyPS::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_LobbyPS, PartyId);
    DOREPLIFETIME(ACPP_LobbyPS, bIsPartyLeader);
    DOREPLIFETIME(ACPP_LobbyPS, LobbyConnectedAt);
    DOREPLIFETIME(ACPP_LobbyPS, LobbyConnectedUnixTimestamp);
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어의 파티 ID를 반환하는 함수
// Return Value : 플레이어가 속한 파티의 ID (속해있지 않으면 -1)
int32 ACPP_LobbyPS::GetPartyId() const
{
    return PartyId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 파티 리더인지 여부를 반환하는 함수
// Return Value : true (파티 리더), false (파티 리더 아님)
bool ACPP_LobbyPS::IsPartyLeader() const
{
    return bIsPartyLeader;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어의 파티 정보를 설정하는 함수
// NewPartyId : 플레이어가 속한 파티의 ID
// bNewIsPartyLeader : 플레이어가 파티 리더인지 여부 (true: 파티 리더, false: 파티 리더 아님)
void ACPP_LobbyPS::SetPartyInfo(int32 NewPartyId, bool bNewIsPartyLeader)
{
    const bool bWasChanged = PartyId != NewPartyId || bIsPartyLeader != bNewIsPartyLeader;

    PartyId = NewPartyId;
    bIsPartyLeader = bNewIsPartyLeader;

    if (bWasChanged)
    {
        BroadcastPartyInfoChanged();
    }
}

const FString& ACPP_LobbyPS::GetLobbyConnectedAt() const
{
    return LobbyConnectedAt;
}

int64 ACPP_LobbyPS::GetLobbyConnectedUnixTimestamp() const
{
    return LobbyConnectedUnixTimestamp;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 인증이 완료되었는지 확인하는 함수
// Return Value : 인증 완료 여부
bool ACPP_LobbyPS::IsLobbyAuthVerified() const
{
    return IsAuthVerified();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어가 로비 서버에 접속한 시간을 설정하는 함수
// NewConnectedAtUtc : UTC 기준 로비 접속 시간
void ACPP_LobbyPS::SetLobbyConnectedAt(const FDateTime& NewConnectedAtUtc)
{
    LobbyConnectedAt = NewConnectedAtUtc.ToIso8601();
    LobbyConnectedUnixTimestamp = NewConnectedAtUtc.ToUnixTimestamp();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어의 파티 정보가 변경되었을 때 호출되는 함수
void ACPP_LobbyPS::OnRep_PartyInfo()
{
    BroadcastPartyInfoChanged();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어의 파티 정보가 변경되었을 때, 이를 바인딩된 함수들에게 알리는 함수
void ACPP_LobbyPS::BroadcastPartyInfoChanged()
{
    OnPartyInfoChanged.Broadcast(PartyId, bIsPartyLeader);
}
