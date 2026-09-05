////////////////////////////
//! \page MyActivatableWidget.h
//! 
#pragma once

#include "CommonActivatableWidget.h"
#include "Input/CommonUIInputTypes.h"
#include "MyActivatableWidget.generated.h"

//! \enum EMyWidgetInputMode UI가 활성화됐을 때 원하는 입력 모드
UENUM(BlueprintType)
enum class EMyWidgetInputMode : uint8
{
    Default,     //! CommonUI 기본 정책을 따름
    Game,        //! 게임 입력 중심
    GameAndMenu, //! 게임 입력 + UI 입력 둘 다 허용
    Menu         //! 메뉴/UI 입력 중심
};

UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyActivatableWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UMyActivatableWidget();

public:
    //~UCommonActivatableWidget interface
    virtual void NativeConstruct() override;
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    //~End of UCommonActivatableWidget interface

    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;


protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    EMyWidgetInputMode InputMode = EMyWidgetInputMode::Default;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::NoCapture;

    // CommonInputData/Back 액션 세팅 전에는 켜지 않는다. Menu/Modal 계열에서만 opt-in한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Back")
    bool bUseCommonUIBackHandler = false;
};
