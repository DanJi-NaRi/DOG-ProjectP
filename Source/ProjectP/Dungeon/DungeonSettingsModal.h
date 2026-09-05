#pragma once

#include "CoreMinimal.h"
#include "Widget/MyActivatableWidget.h"
#include "DungeonSettingsModal.generated.h"

class UButton;
class UDungeonMainUI;
class UWidget;

//! 던전 설정 모달. 항복 투표 시작과 게임 종료(로그아웃)를 선택할 수 있다.
//! 버튼은 전부 BindWidgetOptional이라 WBP에서 자유롭게 디자인한다.
UCLASS()
class PROJECTP_API UDungeonSettingsModal : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UDungeonSettingsModal();

    void SetOwningMainUI(UDungeonMainUI* InMainUI);

    //! 게임 종료 요청 진행 중 중복 클릭을 막기 위해 DungeonMainUI가 호출한다.
    void SetActionButtonsEnabled(bool bShouldEnable);

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Surrender;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ExitGame;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Close;

    TWeakObjectPtr<UDungeonMainUI> OwningMainUI;

    UFUNCTION()
    void HandleSurrenderClicked();

    UFUNCTION()
    void HandleExitGameClicked();

    UFUNCTION()
    void HandleCloseClicked();
};
