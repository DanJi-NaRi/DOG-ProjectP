#pragma once

#include "CommonUserWidget.h"
#include "TimerManager.h"
#include "MyExpBarWidget.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UOverlay;
class UTexture2D;

////////////////////////////
//! \class UMyExpBarWidget
//! \brief HUD 경험치 게이지. 가중치 HorizontalBox 방식으로 채움 폭을 조절한다.
//!        ProgressBar는 채움을 클리핑해 우측 끝이 직선이 되므로 쓰지 않는다.
//! \note WBP 구성: HB_Exp = [채움 Image(캡슐 Box 브러시), Spacer] 두 자식을 모두 슬롯 Size=Fill로 배치.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyExpBarWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetExp(float CurrentExp, float RequiredExp, bool bAnimate = true);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetExpInstant(float CurrentExp, float RequiredExp);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetValueTextVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetGaugeVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetBarTextures(UTexture2D* InFillTexture, UTexture2D* InBackgroundTexture);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

private:
    void ApplyBar();
    void ApplyDisplayOptions();
    void ApplyConfiguredTextures();
    void StartFillAnimation();
    void HandleFillAnimationTick();
    void RefreshText();

    float GetExpRatio() const;

private:
    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentExp = 120.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewRequiredExp = 300.0f;

    //! 초당 게이지 비율 변화 속도 (1 = 1초에 바 전체 길이만큼)
    UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.1"))
    float FillAnimationSpeed = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> FillTexture;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> BackgroundTexture;

    //! 경험치 채움 박스. 자식 0 = 채움 Image, 자식 1 = 나머지 Spacer.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UHorizontalBox> HB_Exp;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> Text_Exp;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UOverlay> Overlay_Gauge;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_ExpFill;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_BackgroundEXP;

    float CachedCurrentExp = 0.0f;
    float CachedRequiredExp = 1.0f;
    bool bValueTextVisible = true;
    bool bGaugeVisible = true;

    //! 애니메이션 중 화면에 실제로 그려지는 경험치 비율
    float DisplayedExpRatio = 0.0f;

    FTimerHandle FillAnimationTimerHandle;
};
