#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MyNoticeTypes.h"
#include "MyNoticeWidget.generated.h"

class UBorder;
class URichTextBlock;

//! 인게임 시스템 메시지를 순차 표시하고 서버 시간 기준 카운트다운을 처리하는 위젯
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyNoticeWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowNotice(const FText& Message, float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowNoticeData(const FMyNoticeData& NoticeData);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowCountdownNotice(const FText& MessageFormat, float EndServerTime);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ClearNotice();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<URichTextBlock> TXT_Notice;

    //! Donation에서만 표시하며 Default Notice에서는 Hidden으로 두어 레이아웃 크기를 유지한다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BDR_Halo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Notice", meta = (ClampMin = "0.1"))
    float DefaultNoticeDuration = 2.5f;

    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Notice")
    void OnNoticeShown();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Notice")
    void OnNoticeHidden();

    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Notice")
    void OnNoticePresentationChanged(EMyNoticePresentationType PresentationType);

    UPROPERTY(BlueprintReadOnly, Category = "UI|Notice")
    EMyNoticePresentationType ActiveNoticePresentationType = EMyNoticePresentationType::Default;

private:
    TArray<FMyNoticeData> PendingNotices;

    FTimerHandle NoticeTimerHandle;

    FText ActiveCountdownMessageFormat;

    float CountdownEndServerTime = 0.0f;

    int32 LastDisplayedCountdownSeconds = INDEX_NONE;

    bool bNoticeVisible = false;

    bool bCountdownActive = false;

    void DisplayNotice(const FMyNoticeData& NoticeData);
    void HandleNoticeDurationElapsed();
    void DisplayNextPendingNotice();
    void UpdateCountdownNotice();
    void HideNoticeVisual();
    void SetNoticeText(const FText& Message);
    void ApplyNoticePresentation(EMyNoticePresentationType PresentationType);
    float GetSyncedServerWorldTimeSeconds() const;
};
