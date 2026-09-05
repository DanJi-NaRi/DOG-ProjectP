#include "CPP_EntryCharacterSelectWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "../Lobby/CPP_LobbyPC.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 위젯 표시 시 버튼을 바인딩하고 캐릭터를 선택할 때까지 로비 이동 입력을 차단하는 함수
void UCPP_EntryCharacterSelectWidget::NativeConstruct()
{
    InputMode = EMyWidgetInputMode::Menu;
    Super::NativeConstruct();

    BindEntryButtons();
    UpdateConfirmButtonState();
    SetLobbyMoveInputBlocked(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 위젯이 닫힐 때 차단했던 로비 이동 입력을 복구하는 함수
void UCPP_EntryCharacterSelectWidget::NativeDestruct()
{
    SetLobbyMoveInputBlocked(false);

    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 선택한 캐릭터 ID를 반환하는 함수
// Return Value : 선택한 캐릭터 ID, 선택하지 않았으면 -1
int32 UCPP_EntryCharacterSelectWidget::GetSelectedCharacterId() const
{
    return SelectedCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입장 캐릭터 선택 위젯의 버튼 클릭 이벤트를 바인딩하는 함수
void UCPP_EntryCharacterSelectWidget::BindEntryButtons()
{
    if (BTN_Character100)
    {
        BTN_Character100->OnClicked.RemoveDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter100Clicked);
        BTN_Character100->OnClicked.AddDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter100Clicked);
    }

    if (BTN_Character200)
    {
        BTN_Character200->OnClicked.RemoveDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter200Clicked);
        BTN_Character200->OnClicked.AddDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter200Clicked);
    }

    if (BTN_Character300)
    {
        BTN_Character300->OnClicked.RemoveDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter300Clicked);
        BTN_Character300->OnClicked.AddDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleCharacter300Clicked);
    }

    if (BTN_Confirm)
    {
        BTN_Confirm->OnClicked.RemoveDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleConfirmClicked);
        BTN_Confirm->OnClicked.AddDynamic(this, &UCPP_EntryCharacterSelectWidget::HandleConfirmClicked);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 100번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_EntryCharacterSelectWidget::HandleCharacter100Clicked()
{
    HandleCharacterButtonClicked(100);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 200번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_EntryCharacterSelectWidget::HandleCharacter200Clicked()
{
    HandleCharacterButtonClicked(200);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 300번 캐릭터 버튼 클릭을 처리하는 함수
void UCPP_EntryCharacterSelectWidget::HandleCharacter300Clicked()
{
    HandleCharacterButtonClicked(300);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터 버튼 클릭 시 선택한 캐릭터 ID를 로컬에 캐시만 하는 함수
// 실제 서버 선택 요청과 캐릭터 스폰은 선택 완료 버튼에서 처리한다.
// CharacterId : 선택할 캐릭터 ID
void UCPP_EntryCharacterSelectWidget::HandleCharacterButtonClicked(int32 CharacterId)
{
    if (!IsValidCharacterId(CharacterId))
    {
        return;
    }

    SelectedCharacterId = CharacterId;
    OnEntryCharacterSelectionChanged(SelectedCharacterId);
    UpdateConfirmButtonState();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 완료 버튼 클릭 시 서버에 로비 캐릭터 선택을 요청하고 위젯을 닫는 함수
// 서버가 이 요청을 받는 시점에 로비 캐릭터를 스폰한다. 위젯이 닫히면 NativeDestruct에서 이동 입력이 복구된다.
void UCPP_EntryCharacterSelectWidget::HandleConfirmClicked()
{
    if (!IsValidCharacterId(SelectedCharacterId))
    {
        return;
    }

    ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer());
    if (!LobbyPC)
    {
        return;
    }

    LobbyPC->RequestSelectLobbyCharacter(SelectedCharacterId);
    DeactivateWidget();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 완료 버튼의 활성화 상태와 표시 문구를 갱신하는 함수
// 캐릭터를 선택하기 전에는 비활성화한다.
void UCPP_EntryCharacterSelectWidget::UpdateConfirmButtonState()
{
    if (BTN_Confirm)
    {
        BTN_Confirm->SetIsEnabled(IsValidCharacterId(SelectedCharacterId));
    }

    if (TXT_Confirm)
    {
        TXT_Confirm->SetText(ConfirmActionText);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 캐릭터를 선택하는 동안 로비 이동/시점 입력을 차단하거나 복구하는 함수
// bShouldBlock : true이면 입력 차단, false이면 복구
void UCPP_EntryCharacterSelectWidget::SetLobbyMoveInputBlocked(bool bShouldBlock)
{
    if (bLobbyMoveInputBlocked == bShouldBlock)
    {
        return;
    }

    APlayerController* OwningPlayer = GetOwningPlayer();
    if (!OwningPlayer)
    {
        return;
    }

    OwningPlayer->SetIgnoreMoveInput(bShouldBlock);
    OwningPlayer->SetIgnoreLookInput(bShouldBlock);
    bLobbyMoveInputBlocked = bShouldBlock;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입장 캐릭터 선택에서 사용할 수 있는 캐릭터 ID인지 확인하는 함수
// CharacterId : 확인할 캐릭터 ID
// Return Value : 100, 200, 300 중 하나이면 true, 아니면 false
bool UCPP_EntryCharacterSelectWidget::IsValidCharacterId(int32 CharacterId) const
{
    return CharacterId == 100 ||
        CharacterId == 200 ||
        CharacterId == 300;
}
