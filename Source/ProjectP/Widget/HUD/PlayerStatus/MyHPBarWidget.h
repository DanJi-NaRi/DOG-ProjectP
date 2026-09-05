#pragma once

#include "CommonUserWidget.h"
#include "TimerManager.h"
#include "MyHPBarWidget.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UOverlay;
class UTexture2D;

////////////////////////////
//! \class UMyHPBarWidget
//! \brief HUD 체력 게이지. 가중치 HorizontalBox 방식으로 채움 폭을 조절한다.
//!        ProgressBar는 채움을 클리핑해 우측 끝이 직선이 되므로 쓰지 않는다.
//!        박스 안 채움 Image가 Box(캡슐) 브러시로 늘어나 양 끝이 항상 라운드로 유지된다.
//! \note WBP 구성: Overlay [ HB_Shield(뒤), HB_HP(앞) ]. 각 HBox는 [채움 Image, Spacer] 두 자식을
//!       모두 슬롯 Size=Fill로 배치한다(가중치는 C++이 관리). HB_Shield는 없어도 된다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyHPBarWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetHP(float CurrentHP, float MaxHP, bool bAnimate = true);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetHPInstant(float CurrentHP, float MaxHP);

    //! 보호막 수치를 설정한다. 체력 끝에 이어 붙는 보호막 구간으로 표시된다.
    //! 체력+보호막이 최대 체력을 넘으면 바 전체 길이는 고정한 채 (체력+보호막) 기준으로 정규화해
    //! 체력 게이지가 왼쪽으로 압축되고 그 오른쪽에 보호막이 표시된다.
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetShield(float InShield, bool bAnimate = true);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetValueTextVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetGaugeVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetBarTextures(
        UTexture2D* InNormalHPTexture,
        UTexture2D* InDangerHPTexture,
        UTexture2D* InBackgroundTexture,
        UTexture2D* InShieldTexture);

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

private:
    void ApplyBars();
    void ApplyDisplayOptions();
    void ApplyConfiguredTextures();
    void RefreshHPFillTexture();
    void StartFillAnimation();
    void HandleFillAnimationTick();
    void RefreshText();

    //! 표시 기준 총량. 보호막이 최대 체력을 넘치게 하면 (체력+보호막)이 바 전체가 된다.
    float GetDisplayTotal() const;
    float GetHPRatio() const;
    float GetShieldRatio() const;

private:
    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentHP = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewMaxHP = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewShield = 20.0f;

    //! 초당 게이지 비율 변화 속도 (1 = 1초에 바 전체 길이만큼)
    UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.1"))
    float FillAnimationSpeed = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DangerThreshold = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> NormalHPTexture;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> DangerHPTexture;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> BackgroundTexture;

    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> ShieldTexture;

    //! 체력 채움 박스. 자식 0 = 채움 Image, 자식 1 = 나머지 Spacer.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UHorizontalBox> HB_HP;

    //! 보호막 표시용. 뒤에 깔려 (체력+보호막)/최대 비율로 채워진다. 구성은 HB_HP와 동일.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UHorizontalBox> HB_Shield;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> Text_HP;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UOverlay> Overlay_Gauge;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_HPFill;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_ShieldFill;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_BackgroundHP;

    float CachedCurrentHP = 0.0f;
    float CachedMaxHP = 1.0f;
    float CachedShield = 0.0f;
    bool bValueTextVisible = true;
    bool bGaugeVisible = true;

    //! 애니메이션 중 화면에 실제로 그려지는 체력 비율
    float DisplayedHPRatio = 0.0f;

    FTimerHandle FillAnimationTimerHandle;
};
