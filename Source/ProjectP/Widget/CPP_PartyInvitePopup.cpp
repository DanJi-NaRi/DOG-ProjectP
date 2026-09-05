#include "CPP_PartyInvitePopup.h"

#include "../Lobby/CPP_LobbyPC.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 초대 팝업이 활성화된 동안 UI 입력만 사용하도록 입력 정책을 설정하는 생성자
UCPP_PartyInvitePopup::UCPP_PartyInvitePopup()
{
    InputMode = EMyWidgetInputMode::Menu;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 팝업이 활성화되기 전에 표시할 파티 초대 또는 가입 신청 정보를 저장하는 함수
// InviteInfo : 서버에서 전달받은 파티 요청 정보
void UCPP_PartyInvitePopup::InitializeInviteInfo(const FLobbyReceivedPartyInviteInfo& InviteInfo)
{
    CachedInviteInfo = InviteInfo;
    RefreshInviteText();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 초대 팝업 생성 시 버튼 이벤트를 연결하고 현재 요청 정보를 화면에 반영하는 함수
void UCPP_PartyInvitePopup::NativeConstruct()
{
    Super::NativeConstruct();

    BindPopupButtons();
    RefreshInviteText();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 초대 팝업 제거 시 버튼 이벤트 연결을 정리하는 함수
void UCPP_PartyInvitePopup::NativeDestruct()
{
    UnbindPopupButtons();

    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 수락 버튼 클릭 시 요청 종류에 맞는 서버 RPC를 호출하고 팝업을 닫는 함수
void UCPP_PartyInvitePopup::HandleYesClicked()
{
    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    if (CachedInviteInfo.RequestType == ELobbyPartyPopupRequestType::PartyJoinRequest)
    {
        LobbyPC->RequestAcceptPartyJoinRequest(CachedInviteInfo.ApplicantPlayerState.Get());
    }
    else
    {
        LobbyPC->RequestAcceptPartyInvite(CachedInviteInfo.PartyId);
    }

    ClosePopup();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 거절 버튼 클릭 시 요청 종류에 맞는 서버 RPC를 호출하고 팝업을 닫는 함수
void UCPP_PartyInvitePopup::HandleNoClicked()
{
    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    if (CachedInviteInfo.RequestType == ELobbyPartyPopupRequestType::PartyJoinRequest)
    {
        LobbyPC->RequestDeclinePartyJoinRequest(CachedInviteInfo.ApplicantPlayerState.Get());
    }
    else
    {
        LobbyPC->RequestDeclinePartyInvite(CachedInviteInfo.PartyId);
    }

    ClosePopup();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 초대 팝업의 수락 및 거절 버튼 이벤트를 중복 없이 연결하는 함수
void UCPP_PartyInvitePopup::BindPopupButtons()
{
    if (BTN_YES)
    {
        BTN_YES->OnClicked.RemoveDynamic(this, &UCPP_PartyInvitePopup::HandleYesClicked);
        BTN_YES->OnClicked.AddDynamic(this, &UCPP_PartyInvitePopup::HandleYesClicked);
    }

    if (BTN_NO)
    {
        BTN_NO->OnClicked.RemoveDynamic(this, &UCPP_PartyInvitePopup::HandleNoClicked);
        BTN_NO->OnClicked.AddDynamic(this, &UCPP_PartyInvitePopup::HandleNoClicked);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 초대 팝업 제거 전에 수락 및 거절 버튼 이벤트 연결을 해제하는 함수
void UCPP_PartyInvitePopup::UnbindPopupButtons()
{
    if (BTN_YES)
    {
        BTN_YES->OnClicked.RemoveDynamic(this, &UCPP_PartyInvitePopup::HandleYesClicked);
    }

    if (BTN_NO)
    {
        BTN_NO->OnClicked.RemoveDynamic(this, &UCPP_PartyInvitePopup::HandleNoClicked);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 요청 종류와 파티 인원 정보를 팝업 텍스트에 반영하는 함수
void UCPP_PartyInvitePopup::RefreshInviteText()
{
    if (TXT_InviteMessage)
    {
        const FString RequestMessage = CachedInviteInfo.RequestType == ELobbyPartyPopupRequestType::PartyJoinRequest
            ? FString::Printf(TEXT("%s님이 파티 가입을 요청했습니다."), *CachedInviteInfo.InviterName)
            : FString::Printf(TEXT("%s님이 파티에 초대했습니다."), *CachedInviteInfo.InviterName);
        TXT_InviteMessage->SetText(FText::FromString(RequestMessage));
    }

    if (TXT_PartyNum)
    {
        TXT_PartyNum->SetText(FText::FromString(FString::Printf(
            TEXT("%d / %d"),
            CachedInviteInfo.CurrentPartyMemberCount,
            CachedInviteInfo.MaxPartyMemberCount)));
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 파티 초대 팝업을 Modal 레이어 스택에서 비활성화해 닫는 함수
void UCPP_PartyInvitePopup::ClosePopup()
{
    DeactivateWidget();
}
