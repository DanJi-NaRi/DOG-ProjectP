#include "DungeonSettingsModal.h"

#include "Components/Button.h"
#include "Dungeon/DungeonMainUI.h"
#include "Dungeon/DungeonPC.h"
#include "InputCoreTypes.h"

////////////////////////////
//! \author 준혁
//! \brief 모달 기본 입력 모드를 메뉴 입력(커서 표시)으로 설정하는 생성자
UDungeonSettingsModal::UDungeonSettingsModal()
{
    InputMode = EMyWidgetInputMode::Menu;
    SetIsFocusable(true);
}

////////////////////////////
//! \author 준혁
//! \brief 게임 종료 버튼이 로그아웃 로직을 호출할 수 있게 소유 던전 메인 UI를 연결하는 함수
//! \param InMainUI 이 모달을 띄운 던전 메인 UI
void UDungeonSettingsModal::SetOwningMainUI(UDungeonMainUI* InMainUI)
{
    OwningMainUI = InMainUI;
}

////////////////////////////
//! \author 준혁
//! \brief 로그아웃 요청 중복 실행을 막기 위해 항복/게임 종료 버튼 활성 상태를 변경하는 함수
//! \param bShouldEnable 버튼 활성화 여부
void UDungeonSettingsModal::SetActionButtonsEnabled(bool bShouldEnable)
{
    if (BTN_Surrender)
    {
        BTN_Surrender->SetIsEnabled(bShouldEnable);
    }

    if (BTN_ExitGame)
    {
        BTN_ExitGame->SetIsEnabled(bShouldEnable);
    }
}

void UDungeonSettingsModal::NativeOnActivated()
{
    Super::NativeOnActivated();

    if (BTN_Surrender)
    {
        BTN_Surrender->OnClicked.AddUniqueDynamic(this, &UDungeonSettingsModal::HandleSurrenderClicked);
    }

    if (BTN_ExitGame)
    {
        BTN_ExitGame->OnClicked.AddUniqueDynamic(this, &UDungeonSettingsModal::HandleExitGameClicked);
    }

    if (BTN_Close)
    {
        BTN_Close->OnClicked.AddUniqueDynamic(this, &UDungeonSettingsModal::HandleCloseClicked);
    }

    SetActionButtonsEnabled(true);
    SetFocus();
}

void UDungeonSettingsModal::NativeOnDeactivated()
{
    if (BTN_Surrender)
    {
        BTN_Surrender->OnClicked.RemoveDynamic(this, &UDungeonSettingsModal::HandleSurrenderClicked);
    }

    if (BTN_ExitGame)
    {
        BTN_ExitGame->OnClicked.RemoveDynamic(this, &UDungeonSettingsModal::HandleExitGameClicked);
    }

    if (BTN_Close)
    {
        BTN_Close->OnClicked.RemoveDynamic(this, &UDungeonSettingsModal::HandleCloseClicked);
    }

    Super::NativeOnDeactivated();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ESC 입력을 받을 포커스 대상으로 메인 설정창 자신을 반환하는 함수
// Return Value : 포커스를 받을 메인 설정창 위젯
UWidget* UDungeonSettingsModal::NativeGetDesiredFocusTarget() const
{
    return const_cast<UDungeonSettingsModal*>(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 메인 설정창이 열린 상태에서 ESC 입력 시 설정창을 닫는 함수
// InGeometry : 메인 설정창의 현재 지오메트리
// InKeyEvent : 입력된 키 이벤트
// Return Value : ESC 입력을 처리했으면 Handled
FReply UDungeonSettingsModal::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape)
    {
        DeactivateWidget();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 항복 버튼 클릭 시 서버에 항복 투표 시작을 요청하고 모달을 닫는 함수
void UDungeonSettingsModal::HandleSurrenderClicked()
{
    // 시작 조건 검증은 ADungeonPC::ServerRequestStartSurrenderVote에서 서버가 수행한다.
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer()))
    {
        DungeonPC->RequestStartSurrenderVote();
    }

    DeactivateWidget();
}

////////////////////////////
//! \author 준혁
//! \brief 게임 종료 버튼 클릭 시 던전 메인 UI의 로그아웃 요청을 시작하는 함수
void UDungeonSettingsModal::HandleExitGameClicked()
{
    UDungeonMainUI* MainUI = OwningMainUI.Get();
    if (!MainUI)
    {
        UE_LOG(LogTemp, Warning, TEXT("DungeonSettingsModal has no owning DungeonMainUI. Cannot request logout."));
        return;
    }

    SetActionButtonsEnabled(false);
    MainUI->RequestLogoutFromDungeon();
}

////////////////////////////
//! \author 준혁
//! \brief 닫기 버튼 클릭 시 모달을 닫는 함수
void UDungeonSettingsModal::HandleCloseClicked()
{
    DeactivateWidget();
}
