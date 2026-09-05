#include "CPP_LobbyPartyPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "CPP_CharacterSelectWidget.h"
#include "CPP_PartyInviteUserRow.h"
#include "../Lobby/CPP_LobbyGSB.h"
#include "../Lobby/CPP_LobbyGMB.h"
#include "../Lobby/CPP_LobbyPC.h"
#include "../Lobby/CPP_LobbyPS.h"
#include "../Player/PlayerCharacterBase.h"


namespace
{
    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // 로비 파티 패널에 표시할 파티 멤버 상태 텍스트를 반환하는 함수
    // MemberInfo : 상태 텍스트를 만들 파티 멤버 정보
    // Return Value : offline, online, in game, out game 중 하나의 표시 텍스트
    FString GetLobbyPartyPanelStateText(const FLobbyPartyMemberInfo& MemberInfo)
    {
        if (MemberInfo.ConnectionState == ELobbyPartyConnectionState::Offline)
        {
            return TEXT("offline");
        }

        if (MemberInfo.ConnectionState == ELobbyPartyConnectionState::Online)
        {
            if (MemberInfo.bIsReady)
            {
                return TEXT("Ready");
            }

            return TEXT("online");
        }

        if (MemberInfo.ConnectionState == ELobbyPartyConnectionState::InGame)
        {
            return TEXT("in game");
        }

        if (MemberInfo.ConnectionState == ELobbyPartyConnectionState::OutGame)
        {
            return TEXT("out game");
        }

        return TEXT("error");
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // 파티 멤버 Row 데이터가 바뀌었는지 비교하기 위한 캐시 키를 만드는 함수
    // MemberRow : 캐시 키를 만들 파티 멤버 Row 데이터
    // Return Value : Row 갱신 비교에 사용할 문자열 키
    FString BuildLobbyPartyMemberRowCacheKey(const FLobbyPartyMemberRowData& MemberRow)
    {
        return FString::Printf(
            TEXT("%s|%s|%d|%d|%d"),
            *MemberRow.DisplayName,
            *MemberRow.ConnectionStateText,
            MemberRow.bIsReady ? 1 : 0,
            MemberRow.SelectedCharacterId,
            MemberRow.bIsPartyLeader ? 1 : 0);
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // 로비 파티에서 선택 가능한 캐릭터 ID인지 확인하는 함수
    // CharacterId : 확인할 캐릭터 ID
    // Return Value : 선택 가능한 캐릭터 ID이면 true, 아니면 false
    bool IsLobbyPartyCharacterId(int32 CharacterId)
    {
        return CharacterId == 100 ||
            CharacterId == 200 ||
            CharacterId == 300;
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 패널이 열려 있는 동안 로비 이동과 UI 조작을 함께 허용하도록 입력 정책을 설정하는 생성자
UCPP_LobbyPartyPanel::UCPP_LobbyPartyPanel()
{
    InputMode = EMyWidgetInputMode::GameAndMenu;
    bUseCommonUIBackHandler = true;
}


void UCPP_LobbyPartyPanel::NativeConstruct()
{
    Super::NativeConstruct();

    ValidateWidgetBindings();
    BindMenuButtons();
    BindLocalPlayerState();
    CacheMemberTextBlocks();
    SetCurrentMenu(ELobbyPartyPanelMenu::MyParty);
    RefreshPartyMemberList();
    UpdatePartyActionButtonState();
    UpdatePartyDisplayText();
    UpdateEnterDungeonButtonState();
    UpdateSelectCharacterButtonState();
    UpdateReadyButtonState();
}


void UCPP_LobbyPartyPanel::NativeDestruct()
{
    UnbindLocalPlayerState();

    Super::NativeDestruct();
}


void UCPP_LobbyPartyPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    BindLocalPlayerState();
    RefreshPartyMemberList();
    UpdatePartyActionButtonState();
    UpdatePartyDisplayText();
    UpdateEnterDungeonButtonState();
    UpdateSelectCharacterButtonState();
    UpdateReadyButtonState();
    RefreshCharacterSelectWidgetState();
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버 목록을 새로고침하는 함수
void UCPP_LobbyPartyPanel::RefreshPartyMemberList()
{
    TArray<FString> CurrentMemberNames;
    int32 CurrentPartyId = -1;
    BuildCurrentMenuNames(CurrentMemberNames, CurrentPartyId);

    if (bHasCachedMemberList &&
        CachedMenu == CurrentMenu &&
        CachedPartyId == CurrentPartyId &&
        CachedMemberNames == CurrentMemberNames)
    {
        UpdateEnterDungeonButtonState();
        UpdateSelectCharacterButtonState();
        UpdateReadyButtonState();
        RefreshCharacterSelectWidgetState();
        return;
    }

    CachedMenu = CurrentMenu;
    CachedPartyId = CurrentPartyId;
    CachedMemberNames = CurrentMemberNames;
    bHasCachedMemberList = true;

    ApplyCurrentMenuRows(CurrentMemberNames);
    UpdateEnterDungeonButtonState();
    UpdateSelectCharacterButtonState();
    UpdateReadyButtonState();
    RefreshCharacterSelectWidgetState();
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 던전에 입장할 수 있는 파티 상태인지 확인하는 함수
// Return Value : 파티장이며 모든 파티원이 온라인, 캐릭터 선택, Ready 상태이면 true, 아니면 false
bool UCPP_LobbyPartyPanel::CanEnterDungeon() const
{
    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || !LocalLobbyPS->IsPartyLeader())
    {
        return false;
    }

    const int32 PartyId = LocalLobbyPS->GetPartyId();
    if (PartyId == -1)
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return false;
    }

    return LobbyGSB->CanPartyEnterDungeon(PartyId, ACPP_LobbyGMB::MaxPartyMemberCount);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 파티에 속해 있는지 확인하는 함수
// Return Value : 로컬 플레이어의 파티 ID가 있으면 true, 없으면 false
bool UCPP_LobbyPartyPanel::IsLocalPlayerInParty() const
{
    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    return LocalLobbyPS && LocalLobbyPS->GetPartyId() != -1;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 파티에서 Ready 상태인지 확인하는 함수
// Return Value : Ready 상태이면 true, 아니면 false
bool UCPP_LobbyPartyPanel::IsLocalPlayerReady() const
{
    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || LocalLobbyPS->GetPartyId() == -1)
    {
        return false;
    }

    const int32 LocalUserIndex = LocalLobbyPS->GetUserIndex() > 0 ? LocalLobbyPS->GetUserIndex() : LocalLobbyPS->GetPlayerId();
    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return false;
    }

    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId != LocalLobbyPS->GetPartyId())
        {
            continue;
        }

        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.UserIndex == LocalUserIndex)
            {
                return MemberInfo.bIsReady;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 선택한 캐릭터로 Ready를 확정할 수 있는지 확인하는 함수
// Return Value : 파티에 속해 있고 유효한 캐릭터를 선택했으며 다른 파티원이 확정한 캐릭터와 겹치지 않으면 true, 아니면 false
bool UCPP_LobbyPartyPanel::CanLocalPlayerSetReady() const
{
    const int32 SelectedCharacterId = GetLocalSelectedCharacterId();
    if (!IsLocalPlayerInParty() || !IsLobbyPartyCharacterId(SelectedCharacterId))
    {
        return false;
    }

    // 이미 Ready 상태면 확정 목록에 본인 캐릭터가 포함되므로 중복 검사를 건너뛴다.
    if (IsLocalPlayerReady())
    {
        return true;
    }

    // 중복 선택 자체는 가능하지만, 다른 파티원이 Ready로 확정한 캐릭터와 겹치면 Ready할 수 없다.
    TArray<int32> ConfirmedCharacterIds;
    GetConfirmedCharacterIds(ConfirmedCharacterIds);
    return !ConfirmedCharacterIds.Contains(SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어의 파티 캐릭터 선택 변경을 서버에 요청하는 함수
// SelectedCharacterId : 선택할 캐릭터 ID
void UCPP_LobbyPartyPanel::RequestSelectCharacter(int32 SelectedCharacterId)
{
    if (!IsLocalPlayerInParty() || !IsLobbyPartyCharacterId(SelectedCharacterId))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    if (APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(LobbyPC->GetPawn()))
    {
        PlayerCharacter->PreviewSelectedCharacterMaterial(SelectedCharacterId);
    }

    LobbyPC->RequestSelectPartyCharacter(SelectedCharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 파티에서 선택한 캐릭터 ID를 반환하는 함수
// Return Value : 선택한 캐릭터 ID, 선택하지 않았으면 -1
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어의 파티 Ready 상태 변경을 서버에 요청하는 함수
// bNewIsReady : 새 Ready 상태
// Return Value : Ready 변경 요청을 보냈으면 true, 요청 전에 차단되었으면 false
bool UCPP_LobbyPartyPanel::RequestSetReady(bool bNewIsReady)
{
    const bool bIsInParty = IsLocalPlayerInParty();
    const bool bCurrentIsReady = IsLocalPlayerReady();
    const int32 CurrentSelectedCharacterId = GetLocalSelectedCharacterId();
    const bool bCanSetReady = CanLocalPlayerSetReady();

    UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel Ready request. IsInParty=%s, CurrentReady=%s, NewReady=%s, SelectedCharacterId=%d, CanSetReady=%s"),
        bIsInParty ? TEXT("true") : TEXT("false"),
        bCurrentIsReady ? TEXT("true") : TEXT("false"),
        bNewIsReady ? TEXT("true") : TEXT("false"),
        CurrentSelectedCharacterId,
        bCanSetReady ? TEXT("true") : TEXT("false"));

    if (!bIsInParty)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel Ready request blocked. Local player is not in party."));
        return false;
    }

    if (bNewIsReady && !bCanSetReady)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel Ready request blocked. Select a valid character before ready."));
        return false;
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel Ready request blocked. Owning player is not ACPP_LobbyPC."));
        return false;
    }

    LobbyPC->RequestSetPartyReady(bNewIsReady);
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 파티에서 선택한 캐릭터 ID를 반환하는 함수
// Return Value : 선택한 캐릭터 ID, 선택하지 않았다면 -1
int32 UCPP_LobbyPartyPanel::GetLocalSelectedCharacterId() const
{
    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || LocalLobbyPS->GetPartyId() == -1)
    {
        return -1;
    }

    const int32 LocalUserIndex = LocalLobbyPS->GetUserIndex() > 0 ? LocalLobbyPS->GetUserIndex() : LocalLobbyPS->GetPlayerId();
    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return -1;
    }

    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId != LocalLobbyPS->GetPartyId())
        {
            continue;
        }

        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.UserIndex == LocalUserIndex)
            {
                return MemberInfo.SelectedCharacterId;
            }
        }
    }

    return -1;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티에서 Ready로 확정된 캐릭터 ID 목록을 반환하는 함수
// OutCharacterIds : Ready 상태인 파티원이 확정한 캐릭터 ID 배열
void UCPP_LobbyPartyPanel::GetConfirmedCharacterIds(TArray<int32>& OutCharacterIds) const
{
    OutCharacterIds.Reset();

    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || LocalLobbyPS->GetPartyId() == -1)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return;
    }

    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId != LocalLobbyPS->GetPartyId())
        {
            continue;
        }

        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.bIsReady &&
                IsLobbyPartyCharacterId(MemberInfo.SelectedCharacterId) &&
                !OutCharacterIds.Contains(MemberInfo.SelectedCharacterId))
            {
                OutCharacterIds.Add(MemberInfo.SelectedCharacterId);
            }
        }

        return;
    }
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// "My Party" 메뉴가 클릭되었을 때 호출되는 함수
void UCPP_LobbyPartyPanel::HandleMyPartyMenuClicked()
{
    SetCurrentMenu(ELobbyPartyPanelMenu::MyParty);
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// "Party Invite" 메뉴가 클릭되었을 때 호출되는 함수
void UCPP_LobbyPartyPanel::HandlePartyInviteMenuClicked()
{
    SetCurrentMenu(ELobbyPartyPanelMenu::PartyInvite);
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// "파티 가입" 메뉴가 클릭되었을 때 호출되는 함수
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// "파티 가입" 메뉴가 클릭되었을 때 호출되는 함수
void UCPP_LobbyPartyPanel::HandlePartyJoinMenuClicked()
{
    SetCurrentMenu(ELobbyPartyPanelMenu::PartyJoin);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 초대 대상 버튼이 클릭되었을 때 서버에 초대 요청을 보내는 함수
// TargetPlayerState : 초대 대상 플레이어의 PlayerState
void UCPP_LobbyPartyPanel::HandleInviteTargetButtonClicked(APlayerState* TargetPlayerState)
{
    if (!TargetPlayerState)
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestInvitePlayer(TargetPlayerState);
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 버튼이 클릭되었을 때 서버로 가입 신청을 보내는 함수
// PartyId : 가입 신청을 보낼 파티 ID
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 버튼이 클릭되었을 때 서버로 가입 신청을 보내는 함수
// PartyId : 가입 신청을 보낼 파티 ID
void UCPP_LobbyPartyPanel::HandleJoinPartyButtonClicked(int32 PartyId)
{
    if (PartyId == -1)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        PartyJoinCooldownEndTimes.Add(PartyId, World->GetTimeSeconds() + 5.0f);
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestJoinParty(PartyId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 입장 버튼이 클릭되었을 때 서버에 던전 입장을 요청하는 함수
void UCPP_LobbyPartyPanel::HandleEnterDungeonButtonClicked()
{
    if (!CanEnterDungeon())
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestEnterDungeon();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 버튼이 클릭되었을 때 캐릭터 선택 UI 표시를 블루프린트에 요청하는 함수
void UCPP_LobbyPartyPanel::HandleSelectCharacterButtonClicked()
{
    if (!IsLocalPlayerInParty())
    {
        return;
    }

    TArray<int32> ConfirmedCharacterIds;
    GetConfirmedCharacterIds(ConfirmedCharacterIds);
    ShowCharacterSelectWidget(ConfirmedCharacterIds, GetLocalSelectedCharacterId());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Ready 버튼이 클릭되었을 때 선택한 캐릭터를 확정하거나 Ready를 취소하는 함수
void UCPP_LobbyPartyPanel::HandleReadyButtonClicked()
{
    RequestSetReady(!IsLocalPlayerReady());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티 상태에 맞게 파티 생성 또는 파티 탈퇴를 요청하는 단일 액션 버튼 처리 함수
void UCPP_LobbyPartyPanel::HandlePartyActionButtonClicked()
{
    if (IsLocalPlayerInParty())
    {
        HandleLeavePartyButtonClicked();
        return;
    }

    HandleCreatePartyButtonClicked();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 생성 버튼이 클릭되었을 때 서버에 파티 생성을 요청하는 함수
void UCPP_LobbyPartyPanel::HandleCreatePartyButtonClicked()
{
    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestCreateParty();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 탈퇴 버튼이 클릭되었을 때 서버에 파티 탈퇴를 요청하는 함수
void UCPP_LobbyPartyPanel::HandleLeavePartyButtonClicked()
{
    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestLeaveParty();
}


//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어의 파티 정보가 변경되었을 때 파티 패널을 갱신하는 함수
// NewPartyId : 변경된 파티 ID
// bNewIsPartyLeader : 변경된 파티장 여부
void UCPP_LobbyPartyPanel::HandleLocalPartyInfoChanged(int32 NewPartyId, bool bNewIsPartyLeader)
{
    bHasCachedMemberList = false;
    RefreshPartyMemberList();
    UpdatePartyActionButtonState();
    UpdatePartyDisplayText();
    UpdateEnterDungeonButtonState();
    UpdateSelectCharacterButtonState();
    UpdateReadyButtonState();
    RefreshCharacterSelectWidgetState();
    OnLocalPartyInfoChanged(NewPartyId, bNewIsPartyLeader);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// "My Party"와 "Party Invite" 메뉴 버튼의 클릭 이벤트를 바인딩하는 함수
void UCPP_LobbyPartyPanel::BindMenuButtons()
{
    if (BTN_MyParty)
    {
        BTN_MyParty->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleMyPartyMenuClicked);
        BTN_MyParty->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleMyPartyMenuClicked);
    }

    if (BTN_PartyInvite)
    {
        BTN_PartyInvite->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyInviteMenuClicked);
        BTN_PartyInvite->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyInviteMenuClicked);
    }

    if (BTN_Menu3)
    {
        BTN_Menu3->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyJoinMenuClicked);
        BTN_Menu3->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyJoinMenuClicked);
    }

    if (BTN_EnterDungeon)
    {
        BTN_EnterDungeon->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleEnterDungeonButtonClicked);
        BTN_EnterDungeon->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleEnterDungeonButtonClicked);
    }

    if (BTN_SelectCharacter)
    {
        BTN_SelectCharacter->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleSelectCharacterButtonClicked);
        BTN_SelectCharacter->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleSelectCharacterButtonClicked);
    }

    if (BTN_Ready)
    {
        BTN_Ready->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleReadyButtonClicked);
        BTN_Ready->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleReadyButtonClicked);
    }

    if (BTN_PartyAction)
    {
        BTN_PartyAction->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyActionButtonClicked);
        BTN_PartyAction->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandlePartyActionButtonClicked);
    }

    if (BTN_CreateParty)
    {
        BTN_CreateParty->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleCreatePartyButtonClicked);
        BTN_CreateParty->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleCreatePartyButtonClicked);
    }

    if (BTN_LeaveParty)
    {
        BTN_LeaveParty->OnClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleLeavePartyButtonClicked);
        BTN_LeaveParty->OnClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleLeavePartyButtonClicked);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 파티 패널 Widget Blueprint의 기능 연결 이름이 맞는지 확인하는 함수
void UCPP_LobbyPartyPanel::ValidateWidgetBindings() const
{
    if (!Panel_PartyRows && !VerticalBox_PartyMemberlist)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Create a PanelWidget named Panel_PartyRows. VerticalBox_PartyMemberlist is only a legacy fallback."));
    }

    if (!BTN_MyParty)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. My Party menu button name should be BTN_MyParty."));
    }

    if (!BTN_PartyInvite)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Party Invite menu button name should be BTN_PartyInvite."));
    }

    if (!BTN_Menu3)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Party Join menu button name should be BTN_Menu3."));
    }

    if (!BTN_EnterDungeon)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Enter dungeon button name should be BTN_EnterDungeon."));
    }

    if (!BTN_Ready && !CharacterSelectWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Ready button name should be BTN_Ready."));
    }

    if (!TXT_PartyAction)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Party action text widget name should be TXT_PartyAction."));
    }

    if (!TXT_PartyStatus)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Party status text widget name should be TXT_PartyStatus."));
    }

    if (!BTN_PartyAction && !BTN_CreateParty && !BTN_LeaveParty)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel binding missing. Use BTN_PartyAction for a single create/leave button, or BTN_CreateParty and BTN_LeaveParty for split buttons."));
    }

    if (!PartyInviteUserRowClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyPartyPanel setup warning. PartyInviteUserRowClass is not set, so invite and join rows cannot use the designed row widget."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어의 PlayerState를 찾아 파티 정보 변경 이벤트를 바인딩하는 함수
void UCPP_LobbyPartyPanel::BindLocalPlayerState()
{
    const APlayerController* PlayerController = GetOwningPlayer();
    ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || BoundLocalLobbyPS == LocalLobbyPS)
    {
        return;
    }

    UnbindLocalPlayerState();

    BoundLocalLobbyPS = LocalLobbyPS;
    BoundLocalLobbyPS->OnPartyInfoChanged.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleLocalPartyInfoChanged);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 바인딩된 로컬 PlayerState의 파티 정보 변경 이벤트를 해제하는 함수
void UCPP_LobbyPartyPanel::UnbindLocalPlayerState()
{
    if (!BoundLocalLobbyPS)
    {
        return;
    }

    BoundLocalLobbyPS->OnPartyInfoChanged.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleLocalPartyInfoChanged);
    BoundLocalLobbyPS = nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 생성/나가기 버튼의 활성화 상태를 현재 파티 가입 여부에 맞게 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdatePartyActionButtonState()
{
    const bool bIsInParty = IsLocalPlayerInParty();

    if (BTN_PartyAction)
    {
        BTN_PartyAction->SetIsEnabled(Cast<ACPP_LobbyPC>(GetOwningPlayer()) != nullptr);
    }

    if (BTN_CreateParty)
    {
        BTN_CreateParty->SetIsEnabled(!bIsInParty);
    }

    if (BTN_LeaveParty)
    {
        BTN_LeaveParty->SetIsEnabled(bIsInParty);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티 상태에 맞게 파티 액션/상태 표시 텍스트를 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdatePartyDisplayText()
{
    UpdatePartyActionText();
    UpdatePartyStatusText();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티 가입 여부에 맞게 단일 파티 액션 버튼 텍스트를 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdatePartyActionText()
{
    if (!TXT_PartyAction)
    {
        return;
    }

    TXT_PartyAction->SetText(IsLocalPlayerInParty() ? LeavePartyActionText : CreatePartyActionText);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 파티 가입 여부와 파티 ID, 파티장 이름에 맞게 파티 상태 텍스트를 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdatePartyStatusText()
{
    if (!TXT_PartyStatus)
    {
        return;
    }

    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS || LocalLobbyPS->GetPartyId() == -1)
    {
        TXT_PartyStatus->SetText(NoPartyStatusText);
        return;
    }

    const int32 PartyId = LocalLobbyPS->GetPartyId();
    FString LeaderName = TEXT("Unknown");

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (LobbyGSB)
    {
        for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
        {
            if (PartyInfo.PartyId != PartyId)
            {
                continue;
            }

            for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
            {
                if (MemberInfo.UserIndex != PartyInfo.LeaderUserIndex)
                {
                    continue;
                }

                LeaderName = MemberInfo.Username;
                if (LeaderName.IsEmpty() && MemberInfo.PlayerState)
                {
                    LeaderName = MemberInfo.PlayerState->GetPlayerName();
                }
                break;
            }
            break;
        }
    }

    if (LeaderName.IsEmpty())
    {
        LeaderName = LocalLobbyPS->GetUsername().IsEmpty() ? LocalLobbyPS->GetPlayerName() : LocalLobbyPS->GetUsername();
    }

    FFormatNamedArguments FormatArguments;
    FormatArguments.Add(TEXT("PartyId"), FText::AsNumber(PartyId));
    FormatArguments.Add(TEXT("LeaderName"), FText::FromString(LeaderName));
    TXT_PartyStatus->SetText(FText::Format(PartyStatusFormatText, FormatArguments));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 입장 버튼의 노출/활성화 상태를 현재 파티 상태에 맞게 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdateEnterDungeonButtonState()
{
    if (!BTN_EnterDungeon)
    {
        return;
    }

    // 게임 시작(던전 입장) 버튼은 파티장에게만 노출한다.
    // Collapsed는 레이아웃 자리를 없애 HorizontalBox의 다른 버튼이 밀리므로, 자리를 유지하는 Hidden을 쓴다.
    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    const bool bIsPartyLeader = LocalLobbyPS && LocalLobbyPS->IsPartyLeader();

    BTN_EnterDungeon->SetVisibility(bIsPartyLeader ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    BTN_EnterDungeon->SetIsEnabled(CanEnterDungeon());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 버튼의 활성화 상태를 현재 파티 가입 여부에 맞게 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdateSelectCharacterButtonState()
{
    if (!BTN_SelectCharacter)
    {
        return;
    }

    BTN_SelectCharacter->SetIsEnabled(IsLocalPlayerInParty());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Ready 버튼의 활성화 상태와 표시 문구를 현재 선택/Ready 상태에 맞게 갱신하는 함수
void UCPP_LobbyPartyPanel::UpdateReadyButtonState()
{
    const bool bIsReady = IsLocalPlayerReady();

    if (BTN_Ready)
    {
        BTN_Ready->SetIsEnabled(bIsReady || CanLocalPlayerSetReady());
    }

    if (TXT_Ready)
    {
        TXT_Ready->SetText(bIsReady ? CancelReadyActionText : ReadyActionText);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 위젯을 생성하거나 갱신해서 화면에 표시하는 함수
// ConfirmedCharacterIds : Ready로 확정되어 선택할 수 없는 캐릭터 ID 목록
// CurrentSelectedCharacterId : 로컬 플레이어가 현재 선택한 캐릭터 ID
void UCPP_LobbyPartyPanel::ShowCharacterSelectWidget(const TArray<int32>& ConfirmedCharacterIds, int32 CurrentSelectedCharacterId)
{
    if (!CharacterSelectWidgetClass)
    {
        OnSelectCharacterRequested(ConfirmedCharacterIds, CurrentSelectedCharacterId);
        return;
    }

    if (!CharacterSelectWidget)
    {
        CharacterSelectWidget = CreateWidget<UCPP_CharacterSelectWidget>(GetOwningPlayer(), CharacterSelectWidgetClass);
    }

    if (!CharacterSelectWidget)
    {
        OnSelectCharacterRequested(ConfirmedCharacterIds, CurrentSelectedCharacterId);
        return;
    }

    CharacterSelectWidget->SetupCharacterSelect(this, ConfirmedCharacterIds, CurrentSelectedCharacterId);
    if (!CharacterSelectWidget->IsInViewport())
    {
        CharacterSelectWidget->AddToViewport(1000);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 열려 있는 캐릭터 선택 위젯에 최신 선택/확정 상태를 다시 전달하는 함수
void UCPP_LobbyPartyPanel::RefreshCharacterSelectWidgetState()
{
    if (!CharacterSelectWidget || !CharacterSelectWidget->IsInViewport())
    {
        return;
    }

    TArray<int32> ConfirmedCharacterIds;
    GetConfirmedCharacterIds(ConfirmedCharacterIds);
    CharacterSelectWidget->SetupCharacterSelect(this, ConfirmedCharacterIds, GetLocalSelectedCharacterId());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 선택된 메뉴를 설정하는 함수
// NewMenu : 설정할 새로운 메뉴
void UCPP_LobbyPartyPanel::SetCurrentMenu(ELobbyPartyPanelMenu NewMenu)
{
    if (CurrentMenu == NewMenu && bHasCachedMemberList)
    {
        return;
    }

    CurrentMenu = NewMenu;
    bHasCachedMemberList = false;
    OnPartyPanelMenuChanged(CurrentMenu);

    const ESlateVisibility ActionVisibility = CurrentMenu == ELobbyPartyPanelMenu::MyParty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

    if (SizeBox_PartyAction)
    {
        SizeBox_PartyAction->SetVisibility(ActionVisibility);
    }

    if (SizeBox_EnterDungeon)
    {
        SizeBox_EnterDungeon->SetVisibility(ActionVisibility);
    }

    if (SizeBox_SelectCharacter)
    {
        SizeBox_SelectCharacter->SetVisibility(ActionVisibility);
    }

    if (SizeBox_Ready)
    {
        SizeBox_Ready->SetVisibility(ActionVisibility);
    }

    RefreshPartyMemberList();
    UpdateEnterDungeonButtonState();
    UpdateSelectCharacterButtonState();
    UpdateReadyButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 선택된 메뉴에 따라 표시할 이름 목록과 파티 ID를 빌드하는 함수
// OutNames : 표시할 이름 목록이 담길 배열 (참조로 전달, 함수 안에서 초기화됨)
// OutPartyId : 현재 파티의 ID (참조로 전달, 함수 안에서 초기화됨)
void UCPP_LobbyPartyPanel::BuildCurrentMenuNames(TArray<FString>& OutNames, int32& OutPartyId) const
{
    switch (CurrentMenu)
    {
    case ELobbyPartyPanelMenu::PartyJoin:
        BuildPartyJoinTargetNames(OutNames, OutPartyId);
        break;
    case ELobbyPartyPanelMenu::PartyInvite:
        BuildPartyInviteTargetNames(OutNames, OutPartyId);
        break;
    case ELobbyPartyPanelMenu::MyParty:
    default:
        BuildCurrentPartyMemberNames(OutNames, OutPartyId);
        break;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 각 멤버 텍스트 블록을 초기화하는 함수
void UCPP_LobbyPartyPanel::CacheMemberTextBlocks()
{
    MemberTextBlocks.Reset();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티원 목록을 표시하는 스크롤 박스가 존재하는지 확인하고, 존재하지 않는 경우 생성하여 연결하는 함수
void UCPP_LobbyPartyPanel::EnsurePartyMemberListScrollBox()
{
    if (ScrollBox_PartyMemberlist || !VerticalBox_PartyMemberlist || !WidgetTree)
    {
        return;
    }

    UContentWidget* ParentContentWidget = Cast<UContentWidget>(VerticalBox_PartyMemberlist->GetParent());
    if (!ParentContentWidget)
    {
        return;
    }

    ScrollBox_PartyMemberlist = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_PartyMemberlist_Runtime"));
    if (!ScrollBox_PartyMemberlist)
    {
        return;
    }

    ScrollBox_PartyMemberlist->SetOrientation(EOrientation::Orient_Vertical);
    ScrollBox_PartyMemberlist->SetScrollBarVisibility(ESlateVisibility::Visible);

    VerticalBox_PartyMemberlist->RemoveFromParent();
    ParentContentWidget->SetContent(ScrollBox_PartyMemberlist);
    ScrollBox_PartyMemberlist->AddChild(VerticalBox_PartyMemberlist);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 Row를 추가할 패널을 반환하는 함수
// Return Value : 새 디자인용 Panel_PartyRows가 있으면 그것을 반환하고, 없으면 기존 VerticalBox_PartyMemberlist를 반환함
UPanelWidget* UCPP_LobbyPartyPanel::GetPartyRowsPanel() const
{
    if (Panel_PartyRows)
    {
        return Panel_PartyRows;
    }

    return VerticalBox_PartyMemberlist;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 플레이어의 파티에 속한 멤버들의 이름을 가져오는 함수
// OutMemberNames : 현재 파티 멤버들의 이름이 담길 배열 (참조로 전달, 함수 안에서 초기화됨)
// OutPartyId : 현재 플레이어가 속한 파티의 ID (참조로 전달, 함수 안에서 초기화됨)
void UCPP_LobbyPartyPanel::BuildCurrentPartyMemberNames(TArray<FString>& OutMemberNames, int32& OutPartyId) const
{
    OutMemberNames.Reset();

    TArray<FLobbyPartyMemberRowData> MemberRows;
    BuildCurrentPartyMemberRows(MemberRows, OutPartyId);

    for (const FLobbyPartyMemberRowData& MemberRow : MemberRows)
    {
        OutMemberNames.Add(BuildLobbyPartyMemberRowCacheKey(MemberRow));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 플레이어가 속한 파티의 멤버 Row 데이터를 가져오는 함수
// OutMemberRows : 현재 파티 멤버 Row 데이터 배열
// OutPartyId : 현재 로컬 플레이어가 속한 파티 ID
void UCPP_LobbyPartyPanel::BuildCurrentPartyMemberRows(TArray<FLobbyPartyMemberRowData>& OutMemberRows, int32& OutPartyId) const
{
    OutMemberRows.Reset();
    OutPartyId = -1;

    APlayerController* PlayerController = GetOwningPlayer();
    ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS)
    {
        return;
    }

    OutPartyId = LocalLobbyPS->GetPartyId();
    if (OutPartyId == -1)
    {
        return;
    }

    bool bFoundParty = false;

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (LobbyGSB)
    {
        for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
        {
            if (PartyInfo.PartyId != OutPartyId)
            {
                continue;
            }

            bFoundParty = true;

            for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
            {
                FString DisplayName = MemberInfo.Username;
                if (DisplayName.IsEmpty() && MemberInfo.PlayerState)
                {
                    DisplayName = MemberInfo.PlayerState->GetPlayerName();
                }

                if (DisplayName.IsEmpty())
                {
                    DisplayName = TEXT("Unknown");
                }

                FLobbyPartyMemberRowData MemberRow;
                MemberRow.DisplayName = DisplayName;
                MemberRow.ConnectionState = MemberInfo.ConnectionState;
                MemberRow.ConnectionStateText = GetLobbyPartyPanelStateText(MemberInfo);
                MemberRow.bIsReady = MemberInfo.bIsReady;
                MemberRow.bIsPartyLeader = MemberInfo.UserIndex == PartyInfo.LeaderUserIndex;
                MemberRow.SelectedCharacterId = MemberInfo.SelectedCharacterId;
                MemberRow.PlayerState = MemberInfo.PlayerState.Get();

                OutMemberRows.Add(MemberRow);
            }

            break;
        }
    }

    if (!bFoundParty || OutMemberRows.IsEmpty())
    {
        const FString& Username = LocalLobbyPS->GetUsername();

        FLobbyPartyMemberRowData MemberRow;
        MemberRow.DisplayName = Username.IsEmpty() ? LocalLobbyPS->GetPlayerName() : Username;
        MemberRow.ConnectionState = ELobbyPartyConnectionState::Online;
        MemberRow.ConnectionStateText = TEXT("online");
        MemberRow.bIsPartyLeader = LocalLobbyPS->IsPartyLeader();
        MemberRow.PlayerState = LocalLobbyPS;

        OutMemberRows.Add(MemberRow);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 플레이어가 초대할 수 있는 대상들의 이름을 가져오는 함수
// OutInviteTargetNames : 초대할 수 있는 대상들의 이름이 담길 배열
// OutPartyId : 현재 플레이어가 속한 파티의 ID (참조로 전달, 함수 안에서 초기화됨)
void UCPP_LobbyPartyPanel::BuildPartyInviteTargetNames(TArray<FString>& OutInviteTargetNames, int32& OutPartyId) const
{
    OutInviteTargetNames.Reset();

    TArray<FLobbyOnlineUserInfo> InviteTargets;
    BuildPartyInviteTargets(InviteTargets, OutPartyId);

    for (const FLobbyOnlineUserInfo& InviteTarget : InviteTargets)
    {
        OutInviteTargetNames.Add(InviteTarget.Username);
    }

    if (OutInviteTargetNames.IsEmpty())
    {
        OutInviteTargetNames.Add(TEXT("초대 가능한 유저 없음"));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 플레이어가 초대할 수 있는 대상 정보를 가져오는 함수
// OutInviteTargets : 초대할 수 있는 대상 정보가 담길 배열
// OutPartyId : 현재 플레이어가 속한 파티의 ID (참조로 전달, 함수 안에서 초기화됨)
void UCPP_LobbyPartyPanel::BuildPartyInviteTargets(TArray<FLobbyOnlineUserInfo>& OutInviteTargets, int32& OutPartyId) const
{
    OutInviteTargets.Reset();
    OutPartyId = -1;

    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS)
    {
        return;
    }

    OutPartyId = LocalLobbyPS->GetPartyId();

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return;
    }

    TArray<FLobbyOnlineUserInfo> OnlineUsers;
    LobbyGSB->GetOnlineUsers(OnlineUsers);

    for (const FLobbyOnlineUserInfo& OnlineUser : OnlineUsers)
    {
        const ACPP_LobbyPS* TargetLobbyPS = Cast<ACPP_LobbyPS>(OnlineUser.PlayerState.Get());
        if (!TargetLobbyPS || TargetLobbyPS == LocalLobbyPS)
        {
            continue;
        }

        if (OnlineUser.PartyId != -1)
        {
            continue;
        }

        if (!OnlineUser.Username.IsEmpty())
        {
            OutInviteTargets.Add(OnlineUser);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 메뉴에 맞는 행을 목록에 적용하는 함수
// MemberNames : 텍스트 행으로 표시할 이름 목록
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 서버에 존재하는 파티 가입 대상 목록의 표시 이름을 가져오는 함수
// OutPartyJoinTargetNames : 파티 가입 탭에 표시할 파티 이름 목록
// OutPartyId : 현재 로컬 플레이어가 속한 파티 ID
void UCPP_LobbyPartyPanel::BuildPartyJoinTargetNames(TArray<FString>& OutPartyJoinTargetNames, int32& OutPartyId) const
{
    OutPartyJoinTargetNames.Reset();

    TArray<FLobbyJoinablePartyEntry> PartyJoinTargets;
    BuildPartyJoinTargets(PartyJoinTargets, OutPartyId);

    for (const FLobbyJoinablePartyEntry& PartyJoinTarget : PartyJoinTargets)
    {
        OutPartyJoinTargetNames.Add(PartyJoinTarget.DisplayName);
    }

    if (OutPartyJoinTargetNames.IsEmpty())
    {
        OutPartyJoinTargetNames.Add(TEXT("가입 신청할 파티 없음"));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 서버에 존재하는 파티 중 로컬 플레이어 본인의 파티를 제외한 가입 대상 정보를 가져오는 함수
// OutPartyJoinTargets : 파티 가입 대상 정보 배열
// OutPartyId : 현재 로컬 플레이어가 속한 파티 ID
void UCPP_LobbyPartyPanel::BuildPartyJoinTargets(TArray<FLobbyJoinablePartyEntry>& OutPartyJoinTargets, int32& OutPartyId) const
{
    OutPartyJoinTargets.Reset();
    OutPartyId = -1;

    const APlayerController* PlayerController = GetOwningPlayer();
    const ACPP_LobbyPS* LocalLobbyPS = PlayerController ? PlayerController->GetPlayerState<ACPP_LobbyPS>() : nullptr;
    if (!LocalLobbyPS)
    {
        return;
    }

    OutPartyId = LocalLobbyPS->GetPartyId();
    const bool bLocalPlayerHasParty = OutPartyId != -1;

    const UWorld* World = GetWorld();
    const ACPP_LobbyGSB* LobbyGSB = World ? World->GetGameState<ACPP_LobbyGSB>() : nullptr;
    if (!LobbyGSB)
    {
        return;
    }

    for (const FLobbyPartyInfo& PartyInfo : LobbyGSB->GetParties())
    {
        if (PartyInfo.PartyId == OutPartyId)
        {
            continue;
        }

        FString LeaderName = TEXT("Unknown");
        for (const FLobbyPartyMemberInfo& MemberInfo : PartyInfo.Members)
        {
            if (MemberInfo.UserIndex != PartyInfo.LeaderUserIndex)
            {
                continue;
            }

            LeaderName = MemberInfo.Username;
            if (LeaderName.IsEmpty() && MemberInfo.PlayerState)
            {
                LeaderName = MemberInfo.PlayerState->GetPlayerName();
            }
            break;
        }

        const int32 CurrentMemberCount = PartyInfo.Members.Num();
        FLobbyJoinablePartyEntry PartyJoinTarget;
        PartyJoinTarget.PartyId = PartyInfo.PartyId;
        PartyJoinTarget.CurrentMemberCount = CurrentMemberCount;
        PartyJoinTarget.MaxMemberCount = ACPP_LobbyGMB::MaxPartyMemberCount;
        PartyJoinTarget.bCanRequestJoin = !bLocalPlayerHasParty && CurrentMemberCount < ACPP_LobbyGMB::MaxPartyMemberCount;
        PartyJoinTarget.RemainingCooldownSeconds = GetPartyJoinCooldownRemainingSeconds(PartyInfo.PartyId);
        PartyJoinTarget.DisplayName = FString::Printf(TEXT("파티 %d | %s | %d/%d"), PartyInfo.PartyId, *LeaderName, CurrentMemberCount, ACPP_LobbyGMB::MaxPartyMemberCount);

        OutPartyJoinTargets.Add(PartyJoinTarget);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 선택된 메뉴에 맞는 Row 목록을 패널에 적용하는 함수
// MemberNames : 텍스트 Row로 표시할 이름 목록
void UCPP_LobbyPartyPanel::ApplyCurrentMenuRows(const TArray<FString>& /*MemberNames*/)
{
    if (CurrentMenu == ELobbyPartyPanelMenu::PartyJoin)
    {
        TArray<FLobbyJoinablePartyEntry> PartyJoinTargets;
        int32 CurrentPartyId = -1;
        BuildPartyJoinTargets(PartyJoinTargets, CurrentPartyId);
        ApplyPartyJoinTargets(PartyJoinTargets);
        return;
    }

    if (CurrentMenu == ELobbyPartyPanelMenu::PartyInvite)
    {
        TArray<FLobbyOnlineUserInfo> InviteTargets;
        int32 CurrentPartyId = -1;
        BuildPartyInviteTargets(InviteTargets, CurrentPartyId);
        ApplyPartyInviteTargets(InviteTargets);
        return;
    }

    TArray<FLobbyPartyMemberRowData> MemberRows;
    int32 CurrentPartyId = -1;
    BuildCurrentPartyMemberRows(MemberRows, CurrentPartyId);
    ApplyPartyMemberRows(MemberRows);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 각 텍스트 블록에 파티 멤버들의 이름을 적용하는 함수
// MemberNames : 각 텍스트 블록에 적용할 파티 멤버들의 이름이 담긴 배열 (인자로 전달, 함수 안에서 사용)
void UCPP_LobbyPartyPanel::ApplyPartyMemberRows(const TArray<FLobbyPartyMemberRowData>& MemberRows)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel)
    {
        return;
    }

    RowsPanel->ClearChildren();
    MemberTextBlocks.Reset();
    InviteUserRows.Reset();

    for (const FLobbyPartyMemberRowData& MemberRow : MemberRows)
    {
        if (MemberRow.DisplayName.IsEmpty())
        {
            continue;
        }

        AddPartyMemberRow(MemberRow);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버 Row 위젯을 생성해 목록에 추가하는 함수
// MemberRow : 추가할 파티 멤버 Row 데이터
void UCPP_LobbyPartyPanel::AddPartyMemberRow(const FLobbyPartyMemberRowData& MemberRow)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel)
    {
        return;
    }

    TSubclassOf<UCPP_PartyInviteUserRow> MemberRowWidgetClass = PartyInviteUserRowClass;
    if (!MemberRowWidgetClass)
    {
        const FString DisplayName = MemberRow.bIsPartyLeader
            ? FString::Printf(TEXT("(Master) %s"), *MemberRow.DisplayName)
            : MemberRow.DisplayName;
        AddPartyMemberTextRow(MemberRow.ConnectionStateText.IsEmpty()
            ? DisplayName
            : FString::Printf(TEXT("%s - %s"), *DisplayName, *MemberRow.ConnectionStateText));
        return;
    }

    UCPP_PartyInviteUserRow* MemberRowWidget = CreateWidget<UCPP_PartyInviteUserRow>(GetOwningPlayer(), MemberRowWidgetClass);
    if (!MemberRowWidget)
    {
        return;
    }

    MemberRowWidget->SetupPartyMember(MemberRow);

    UPanelSlot* PanelSlot = RowsPanel->AddChild(MemberRowWidget);
    if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
    {
        VerticalBoxSlot->SetPadding(FMargin(6.0f, 4.0f));
    }

    InviteUserRows.Add(MemberRowWidget);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 멤버의 이름이 담긴 텍스트 블록을 파티 멤버 목록에 추가하는 함수
// MemberName : 추가할 파티 멤버의 이름 (인자로 전달, 함수 안에서 사용)
void UCPP_LobbyPartyPanel::AddPartyMemberTextRow(const FString& MemberName)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel || !WidgetTree)
    {
        return;
    }

    UTextBlock* MemberTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    if (!MemberTextBlock)
    {
        return;
    }

    MemberTextBlock->SetText(FText::FromString(MemberName));
    MemberTextBlock->SetAutoWrapText(true);

    UPanelSlot* PanelSlot = RowsPanel->AddChild(MemberTextBlock);
    if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
    {
        VerticalBoxSlot->SetPadding(FMargin(6.0f, 4.0f));
    }

    MemberTextBlocks.Add(MemberTextBlock);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 초대 대상 버튼 행을 목록에 적용하는 함수
// InviteTargets : 초대 대상 정보가 담긴 배열
void UCPP_LobbyPartyPanel::ApplyPartyInviteTargets(const TArray<FLobbyOnlineUserInfo>& InviteTargets)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel)
    {
        return;
    }

    RowsPanel->ClearChildren();
    MemberTextBlocks.Reset();
    InviteUserRows.Reset();

    if (InviteTargets.IsEmpty())
    {
        AddPartyMemberTextRow(TEXT("초대 가능한 유저 없음"));
        return;
    }

    for (const FLobbyOnlineUserInfo& InviteTarget : InviteTargets)
    {
        AddPartyInviteTargetRow(InviteTarget);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 초대 대상의 이름과 버튼이 담긴 Row 위젯을 초대 목록에 추가하는 함수
// InviteTarget : 추가할 초대 대상 정보
void UCPP_LobbyPartyPanel::AddPartyInviteTargetRow(const FLobbyOnlineUserInfo& InviteTarget)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel || !InviteTarget.PlayerState)
    {
        return;
    }

    TSubclassOf<UCPP_PartyInviteUserRow> InviteUserRowWidgetClass = PartyInviteUserRowClass;
    if (!InviteUserRowWidgetClass)
    {
        InviteUserRowWidgetClass = UCPP_PartyInviteUserRow::StaticClass();
    }

    UCPP_PartyInviteUserRow* InviteUserRow = CreateWidget<UCPP_PartyInviteUserRow>(GetOwningPlayer(), InviteUserRowWidgetClass);
    if (!InviteUserRow)
    {
        return;
    }

    InviteUserRow->SetupInviteTarget(InviteTarget.PlayerState.Get(), InviteTarget.Username);
    InviteUserRow->OnInviteButtonClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleInviteTargetButtonClicked);
    InviteUserRow->OnInviteButtonClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleInviteTargetButtonClicked);

    UPanelSlot* PanelSlot = RowsPanel->AddChild(InviteUserRow);
    if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
    {
        VerticalBoxSlot->SetPadding(FMargin(6.0f, 4.0f));
    }

    InviteUserRows.Add(InviteUserRow);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 대상 파티 Row들을 목록에 적용하는 함수
// PartyJoinTargets : 파티 가입 탭에 표시할 파티 정보 배열
void UCPP_LobbyPartyPanel::ApplyPartyJoinTargets(const TArray<FLobbyJoinablePartyEntry>& PartyJoinTargets)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel)
    {
        return;
    }

    RowsPanel->ClearChildren();
    MemberTextBlocks.Reset();
    InviteUserRows.Reset();

    if (PartyJoinTargets.IsEmpty())
    {
        AddPartyMemberTextRow(TEXT("가입 신청할 파티 없음"));
        return;
    }

    for (const FLobbyJoinablePartyEntry& PartyJoinTarget : PartyJoinTargets)
    {
        AddPartyJoinTargetRow(PartyJoinTarget);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 가입 신청 대상 파티 정보를 Row 위젯으로 생성해 목록에 추가하는 함수
// PartyJoinTarget : 추가할 파티 가입 대상 정보
void UCPP_LobbyPartyPanel::AddPartyJoinTargetRow(const FLobbyJoinablePartyEntry& PartyJoinTarget)
{
    UPanelWidget* RowsPanel = GetPartyRowsPanel();
    if (!RowsPanel)
    {
        return;
    }

    TSubclassOf<UCPP_PartyInviteUserRow> PartyJoinRowWidgetClass = PartyInviteUserRowClass;
    if (!PartyJoinRowWidgetClass)
    {
        PartyJoinRowWidgetClass = UCPP_PartyInviteUserRow::StaticClass();
    }

    UCPP_PartyInviteUserRow* PartyJoinRow = CreateWidget<UCPP_PartyInviteUserRow>(GetOwningPlayer(), PartyJoinRowWidgetClass);
    if (!PartyJoinRow)
    {
        return;
    }

    PartyJoinRow->SetupPartyJoinTargetEntry(PartyJoinTarget);
    PartyJoinRow->OnJoinPartyButtonClicked.RemoveDynamic(this, &UCPP_LobbyPartyPanel::HandleJoinPartyButtonClicked);
    PartyJoinRow->OnJoinPartyButtonClicked.AddDynamic(this, &UCPP_LobbyPartyPanel::HandleJoinPartyButtonClicked);

    UPanelSlot* PanelSlot = RowsPanel->AddChild(PartyJoinRow);
    if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
    {
        VerticalBoxSlot->SetPadding(FMargin(6.0f, 4.0f));
    }

    InviteUserRows.Add(PartyJoinRow);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 파티 가입 신청 버튼의 남은 클라이언트 쿨타임을 반환하는 함수
// PartyId : 쿨타임을 확인할 파티 ID
// Return Value : 남은 쿨타임 시간, 없으면 0
float UCPP_LobbyPartyPanel::GetPartyJoinCooldownRemainingSeconds(int32 PartyId) const
{
    if (PartyId == -1)
    {
        return 0.0f;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }

    const float* CooldownEndTime = PartyJoinCooldownEndTimes.Find(PartyId);
    if (!CooldownEndTime)
    {
        return 0.0f;
    }

    return FMath::Max(0.0f, *CooldownEndTime - World->GetTimeSeconds());
}
