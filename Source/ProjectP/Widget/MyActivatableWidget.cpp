////////////////////////////
//! \page MyActivatableWidget.cpp
//! 
#include "MyActivatableWidget.h"

UMyActivatableWidget::UMyActivatableWidget()
{
    bIsBackHandler = false;
    bAutoActivate = false;   // 생성은 하되 활성화는 안 함. 
}

void UMyActivatableWidget::NativeConstruct()
{
    bIsBackHandler = bUseCommonUIBackHandler;
    Super::NativeConstruct();
}

//! \brief 위젯 활성화 시 적용할 CommonUI 입력 설정을 반환함
//! \details 에디터에서 설정한 InputMode와 MouseCaptureMode를 기반으로 FUIInputConfig를 생성함
//! \return 입력 설정 정보 (Default 모드일 경우 빈 값 반환)
TOptional<FUIInputConfig> UMyActivatableWidget::GetDesiredInputConfig() const
{
    switch (InputMode)
    {
    case EMyWidgetInputMode::Game:
        return FUIInputConfig(ECommonInputMode::Game, MouseCaptureMode);
    case EMyWidgetInputMode::GameAndMenu:
        return FUIInputConfig(ECommonInputMode::All, MouseCaptureMode);
    case EMyWidgetInputMode::Menu:
        return FUIInputConfig(ECommonInputMode::Menu, MouseCaptureMode);
    default:
        return TOptional<FUIInputConfig>();
    }
}

void UMyActivatableWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
}

void UMyActivatableWidget::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
}
