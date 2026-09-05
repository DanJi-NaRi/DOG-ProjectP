#include "CPP_CharacterSelectWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "CPP_LobbyPartyPanel.h"

void UCPP_CharacterSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BindCharacterButtons();
    UpdateCharacterButtonStates();
    UpdateReadyButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 위젯에 현재 파티 캐릭터 선택 상태를 설정하는 함수
// InLobbyPartyPanel : 캐릭터 선택 요청을 전달할 로비 파티 패널
// InConfirmedCharacterIds : Ready로 확정된 캐릭터 ID 목록 (선택은 가능하나 중복 Ready 불가, 표시용)
// InCurrentSelectedCharacterId : 로컬 플레이어가 현재 선택한 캐릭터 ID
void UCPP_CharacterSelectWidget::SetupCharacterSelect(UCPP_LobbyPartyPanel* InLobbyPartyPanel, const TArray<int32>& InConfirmedCharacterIds, int32 InCurrentSelectedCharacterId)
{
    const bool bStateChanged = !bHasCharacterSelectState ||
        CachedConfirmedCharacterIds != InConfirmedCharacterIds ||
        CachedCurrentSelectedCharacterId != InCurrentSelectedCharacterId;

    LobbyPartyPanel = InLobbyPartyPanel;
    CachedConfirmedCharacterIds = InConfirmedCharacterIds;
    CachedCurrentSelectedCharacterId = InCurrentSelectedCharacterId;
    bHasCharacterSelectState = true;

    UpdateCharacterButtonStates();
    UpdateReadyButtonState();
    if (bStateChanged)
    {
        OnCharacterSelectStateChanged(CachedConfirmedCharacterIds, CachedCurrentSelectedCharacterId);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 캐릭터가 다른 파티원에 의해 Ready 확정되었는지 확인하는 함수
// CharacterId : 확인할 캐릭터 ID
// Return Value : 이미 확정된 캐릭터이면 true, 아니면 false
bool UCPP_CharacterSelectWidget::IsCharacterConfirmed(int32 CharacterId) const
{
    return CachedConfirmedCharacterIds.Contains(CharacterId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 특정 캐릭터가 로컬 플레이어의 현재 선택 캐릭터인지 확인하는 함수
// CharacterId : 확인할 캐릭터 ID
// Return Value : 현재 선택한 캐릭터이면 true, 아니면 false
bool UCPP_CharacterSelectWidget::IsCurrentSelectedCharacter(int32 CharacterId) const
{
    return CachedCurrentSelectedCharacterId == CharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 위젯의 버튼 클릭 이벤트를 바인딩하는 함수
void UCPP_CharacterSelectWidget::BindCharacterButtons()
{
    if (BTN_Character100)
    {
        BTN_Character100->OnClicked.RemoveDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter100Clicked);
        BTN_Character100->OnClicked.AddDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter100Clicked);
    }

    if (BTN_Character200)
    {
        BTN_Character200->OnClicked.RemoveDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter200Clicked);
        BTN_Character200->OnClicked.AddDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter200Clicked);
    }

    if (BTN_Character300)
    {
        BTN_Character300->OnClicked.RemoveDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter300Clicked);
        BTN_Character300->OnClicked.AddDynamic(this, &UCPP_CharacterSelectWidget::HandleCharacter300Clicked);
    }

    if (BTN_Close)
    {
        BTN_Close->OnClicked.RemoveDynamic(this, &UCPP_CharacterSelectWidget::HandleCloseClicked);
        BTN_Close->OnClicked.AddDynamic(this, &UCPP_CharacterSelectWidget::HandleCloseClicked);
    }

    if (BTN_Ready)
    {
        BTN_Ready->OnClicked.RemoveDynamic(this, &UCPP_CharacterSelectWidget::HandleReadyClicked);
        BTN_Ready->OnClicked.AddDynamic(this, &UCPP_CharacterSelectWidget::HandleReadyClicked);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 100번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_CharacterSelectWidget::HandleCharacter100Clicked()
{
    HandleCharacterButtonClicked(100);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 200번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_CharacterSelectWidget::HandleCharacter200Clicked()
{
    HandleCharacterButtonClicked(200);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 300번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_CharacterSelectWidget::HandleCharacter300Clicked()
{
    HandleCharacterButtonClicked(300);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 닫기 버튼 클릭을 처리하는 함수
void UCPP_CharacterSelectWidget::HandleCloseClicked()
{
    RemoveFromParent();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Ready 버튼 클릭 시 로비 파티 패널에 Ready 상태 변경 요청을 전달하는 함수
void UCPP_CharacterSelectWidget::HandleReadyClicked()
{
    if (!LobbyPartyPanel)
    {
        UE_LOG(LogTemp, Warning, TEXT("CharacterSelectWidget Ready request blocked. LobbyPartyPanel is not set."));
        return;
    }

    const bool bCurrentIsReady = LobbyPartyPanel->IsLocalPlayerReady();
    const bool bNewIsReady = !bCurrentIsReady;

    UE_LOG(LogTemp, Warning, TEXT("CharacterSelectWidget Ready clicked. CurrentReady=%s, NewReady=%s"),
        bCurrentIsReady ? TEXT("true") : TEXT("false"),
        bNewIsReady ? TEXT("true") : TEXT("false"));

    if (LobbyPartyPanel->RequestSetReady(bNewIsReady) && TXT_Ready)
    {
        TXT_Ready->SetText(bNewIsReady ? CancelReadyActionText : ReadyActionText);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 버튼 클릭 시 선택 가능 여부를 확인하고 로비 파티 패널에 선택 요청을 전달하는 함수
// 중복 선택 자체는 허용되므로 확정된 캐릭터도 선택할 수 있다. (중복 차단은 Ready 시점에만)
// CharacterId : 선택 요청할 캐릭터 ID
void UCPP_CharacterSelectWidget::HandleCharacterButtonClicked(int32 CharacterId)
{
    if (!LobbyPartyPanel || !IsValidCharacterId(CharacterId))
    {
        return;
    }

    CachedCurrentSelectedCharacterId = CharacterId;
    LobbyPartyPanel->RequestSelectCharacter(CharacterId);
    OnCharacterSelectStateChanged(CachedConfirmedCharacterIds, CachedCurrentSelectedCharacterId);
    UpdateReadyButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 버튼 활성화 상태를 갱신하는 함수
// 중복 선택 자체는 허용되므로 확정 여부와 무관하게 항상 활성화한다. (확정 표시는 OnCharacterSelectStateChanged에서 처리)
void UCPP_CharacterSelectWidget::UpdateCharacterButtonStates()
{
    if (BTN_Character100)
    {
        BTN_Character100->SetIsEnabled(true);
    }

    if (BTN_Character200)
    {
        BTN_Character200->SetIsEnabled(true);
    }

    if (BTN_Character300)
    {
        BTN_Character300->SetIsEnabled(true);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 UI에서 사용할 수 있는 캐릭터 ID인지 확인하는 함수
// CharacterId : 확인할 캐릭터 ID
// Return Value : 100, 200, 300 중 하나이면 true, 아니면 false
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Ready 버튼의 활성화 상태와 표시 문구를 현재 선택/Ready 상태에 맞게 갱신하는 함수
void UCPP_CharacterSelectWidget::UpdateReadyButtonState()
{
    const bool bIsReady = LobbyPartyPanel && LobbyPartyPanel->IsLocalPlayerReady();
    const bool bCanSetReady = LobbyPartyPanel && LobbyPartyPanel->CanLocalPlayerSetReady();

    if (BTN_Ready)
    {
        BTN_Ready->SetIsEnabled(bIsReady || bCanSetReady);
    }

    if (TXT_Ready)
    {
        TXT_Ready->SetText(bIsReady ? CancelReadyActionText : ReadyActionText);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 선택 UI에서 사용할 수 있는 캐릭터 ID인지 확인하는 함수
// CharacterId : 확인할 캐릭터 ID
// Return Value : 100, 200, 300 중 하나이면 true, 아니면 false
bool UCPP_CharacterSelectWidget::IsValidCharacterId(int32 CharacterId) const
{
    return CharacterId == 100 ||
        CharacterId == 200 ||
        CharacterId == 300;
}
