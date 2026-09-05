#include "MyHPBarWidget.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Engine/Texture2D.h"

namespace
{
    constexpr float HPBarFillTickInterval = 1.0f / 60.0f;

    //! [채움, 나머지] 두 자식의 Fill 가중치를 Ratio : 1-Ratio 로 배분해 채움 폭을 조절한다.
    void ApplyFillWeights(UHorizontalBox* Box, float Ratio)
    {
        if (!Box || Box->GetChildrenCount() < 2)
        {
            return;
        }

        Ratio = FMath::Clamp(Ratio, 0.0f, 1.0f);

        if (UHorizontalBoxSlot* FillSlot = Cast<UHorizontalBoxSlot>(Box->GetChildAt(0)->Slot))
        {
            FSlateChildSize FillSize(ESlateSizeRule::Fill);
            FillSize.Value = Ratio;
            FillSlot->SetSize(FillSize);
        }

        if (UHorizontalBoxSlot* RestSlot = Cast<UHorizontalBoxSlot>(Box->GetChildAt(1)->Slot))
        {
            FSlateChildSize RestSize(ESlateSizeRule::Fill);
            RestSize.Value = 1.0f - Ratio;
            RestSlot->SetSize(RestSize);
        }

        // 비율 0에서 캡슐 모서리 조각이 남지 않도록 채움 위젯 자체를 숨긴다
        Box->GetChildAt(0)->SetVisibility(Ratio > KINDA_SMALL_NUMBER
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Hidden);
    }
}

void UMyHPBarWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // 이름만 같고 타입이 다른 위젯이 바인딩돼 들어오는 경우 방어 (WBP 마이그레이션 중 크래시 방지).
    if (HB_HP && !HB_HP->IsA(UHorizontalBox::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[HPBar] HB_HP가 HorizontalBox 타입이 아님(%s) - WBP에서 교체 필요"), *GetNameSafe(HB_HP->GetClass()));
        HB_HP = nullptr;
    }

    if (HB_Shield && !HB_Shield->IsA(UHorizontalBox::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[HPBar] HB_Shield가 HorizontalBox 타입이 아님(%s) - WBP에서 교체 필요"), *GetNameSafe(HB_Shield->GetClass()));
        HB_Shield = nullptr;
    }

    if (IsDesignTime())
    {
        CachedCurrentHP = PreviewCurrentHP;
        CachedMaxHP = PreviewMaxHP;
        CachedShield = PreviewShield;
    }

    DisplayedHPRatio = GetHPRatio();
    ApplyConfiguredTextures();
    ApplyDisplayOptions();
    ApplyBars();
}

void UMyHPBarWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FillAnimationTimerHandle);
    }

    Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 체력을 설정한다. bAnimate면 현재 표시 비율에서 목표 비율까지 부드럽게 채운다.
//! \param CurrentHP 현재 체력
//! \param MaxHP 최대 체력
//! \param bAnimate 게이지 보간 애니메이션 재생 여부
void UMyHPBarWidget::SetHP(float CurrentHP, float MaxHP, bool bAnimate)
{
    CachedMaxHP = FMath::Max(MaxHP, 1.0f);
    CachedCurrentHP = FMath::Clamp(CurrentHP, 0.0f, CachedMaxHP);
    RefreshHPFillTexture();

    if (bAnimate)
    {
        StartFillAnimation();
    }
    else
    {
        DisplayedHPRatio = GetHPRatio();
    }

    ApplyBars();
}

void UMyHPBarWidget::SetHPInstant(float CurrentHP, float MaxHP)
{
    SetHP(CurrentHP, MaxHP, false);
}

////////////////////////////
//! \author 준혁
//! \brief 보호막 수치를 설정한다. 체력+보호막이 최대 체력을 넘으면 표시 총량이 (체력+보호막)으로
//!        바뀌므로 체력 게이지 비율도 함께 변한다(왼쪽으로 압축). 그래서 체력 보간 애니메이션을 같이 돌린다.
//! \param InShield 보호막 수치 (0 이하면 보호막 구간 숨김)
//! \param bAnimate 체력 게이지 압축/복원 보간 애니메이션 재생 여부
void UMyHPBarWidget::SetShield(float InShield, bool bAnimate)
{
    CachedShield = FMath::Max(InShield, 0.0f);

    if (bAnimate)
    {
        StartFillAnimation();
    }
    else
    {
        DisplayedHPRatio = GetHPRatio();
    }

    ApplyBars();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// HP 수치 텍스트의 표시 여부를 설정하는 함수
// bVisible : true면 HP 수치 텍스트 표시, false면 숨김
void UMyHPBarWidget::SetValueTextVisible(bool bVisible)
{
    bValueTextVisible = bVisible;
    ApplyDisplayOptions();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// HP와 쉴드 게이지의 표시 여부를 설정하고 배경은 유지하는 함수
// bVisible : true면 HP와 쉴드 게이지 표시, false면 게이지만 숨김
void UMyHPBarWidget::SetGaugeVisible(bool bVisible)
{
    bGaugeVisible = bVisible;
    ApplyDisplayOptions();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// HP 바 인스턴스에서 사용할 텍스처 구성을 설정하는 함수
// InNormalHPTexture : 일반 상태 HP 채움 텍스처
// InDangerHPTexture : 위험 상태 HP 채움 텍스처
// InBackgroundTexture : HP 바 배경 텍스처
// InShieldTexture : 쉴드 채움 텍스처
void UMyHPBarWidget::SetBarTextures(
    UTexture2D* InNormalHPTexture,
    UTexture2D* InDangerHPTexture,
    UTexture2D* InBackgroundTexture,
    UTexture2D* InShieldTexture)
{
    NormalHPTexture = InNormalHPTexture;
    DangerHPTexture = InDangerHPTexture;
    BackgroundTexture = InBackgroundTexture;
    ShieldTexture = InShieldTexture;
    ApplyConfiguredTextures();
}

////////////////////////////
//! \author 준혁
//! \brief 캐시된 수치를 채움 박스와 텍스트에 반영한다.
//!        앞 박스(HB_HP)는 표시 중인 체력 비율, 뒤 박스(HB_Shield)는 체력+보호막 비율로 채운다.
void UMyHPBarWidget::ApplyBars()
{
    ApplyFillWeights(HB_HP, DisplayedHPRatio);

    if (HB_Shield)
    {
        // 보호막은 즉시 반영. 애니메이션 중에도 표시 체력 비율 뒤에 이어 붙어 보이도록 표시 비율 기준으로 계산한다.
        const float ShieldEnd = FMath::Clamp(DisplayedHPRatio + GetShieldRatio(), 0.0f, 1.0f);
        ApplyFillWeights(HB_Shield, CachedShield > 0.0f ? ShieldEnd : 0.0f);
    }

    RefreshText();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// HP 수치 텍스트와 게이지에 저장된 표시 설정을 적용하는 함수
void UMyHPBarWidget::ApplyDisplayOptions()
{
    if (Text_HP)
    {
        Text_HP->SetVisibility(bValueTextVisible
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    if (Overlay_Gauge)
    {
        Overlay_Gauge->SetVisibility(bGaugeVisible
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Hidden);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 설정된 HP 바 텍스처를 각 이미지 위젯에 적용하는 함수
void UMyHPBarWidget::ApplyConfiguredTextures()
{
    if (!NormalHPTexture && IMG_HPFill)
    {
        NormalHPTexture = Cast<UTexture2D>(IMG_HPFill->GetBrush().GetResourceObject());
    }

    if (!BackgroundTexture && IMG_BackgroundHP)
    {
        BackgroundTexture = Cast<UTexture2D>(IMG_BackgroundHP->GetBrush().GetResourceObject());
    }

    if (!ShieldTexture && IMG_ShieldFill)
    {
        ShieldTexture = Cast<UTexture2D>(IMG_ShieldFill->GetBrush().GetResourceObject());
    }

    if (IMG_BackgroundHP && BackgroundTexture)
    {
        IMG_BackgroundHP->SetBrushResourceObject(BackgroundTexture);
    }

    if (IMG_ShieldFill && ShieldTexture)
    {
        IMG_ShieldFill->SetBrushResourceObject(ShieldTexture);
    }

    RefreshHPFillTexture();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 HP 비율과 30퍼센트 위험 기준에 맞는 채움 텍스처를 적용하는 함수
void UMyHPBarWidget::RefreshHPFillTexture()
{
    if (!IMG_HPFill)
    {
        return;
    }

    const float HPRatio = CachedMaxHP > 0.0f
        ? CachedCurrentHP / CachedMaxHP
        : 0.0f;
    UTexture2D* TargetTexture = HPRatio <= DangerThreshold && DangerHPTexture
        ? DangerHPTexture
        : NormalHPTexture;

    if (TargetTexture && IMG_HPFill->GetBrush().GetResourceObject() != TargetTexture)
    {
        IMG_HPFill->SetBrushResourceObject(TargetTexture);
    }
}

void UMyHPBarWidget::StartFillAnimation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        DisplayedHPRatio = GetHPRatio();
        return;
    }

    if (!FillAnimationTimerHandle.IsValid())
    {
        World->GetTimerManager().SetTimer(
            FillAnimationTimerHandle,
            this,
            &ThisClass::HandleFillAnimationTick,
            HPBarFillTickInterval,
            true);
    }
}

void UMyHPBarWidget::HandleFillAnimationTick()
{
    const float TargetRatio = GetHPRatio();
    DisplayedHPRatio = FMath::FInterpConstantTo(DisplayedHPRatio, TargetRatio, HPBarFillTickInterval, FillAnimationSpeed);

    if (FMath::IsNearlyEqual(DisplayedHPRatio, TargetRatio))
    {
        DisplayedHPRatio = TargetRatio;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(FillAnimationTimerHandle);
        }
        FillAnimationTimerHandle.Invalidate();
    }

    ApplyBars();
}

void UMyHPBarWidget::RefreshText()
{
    if (!Text_HP)
    {
        return;
    }

    const int32 Current = FMath::RoundToInt(CachedCurrentHP);
    const int32 Max = FMath::RoundToInt(CachedMaxHP);
    Text_HP->SetText(FText::Format(NSLOCTEXT("ProjectP", "HPFormat", "{0} / {1}"), FText::AsNumber(Current), FText::AsNumber(Max)));
}

////////////////////////////
//! \author 준혁
//! \brief 표시 기준 총량을 구한다. 평소에는 최대 체력, 체력+보호막이 최대를 넘으면 그 합이 바 전체가
//!        되어 게이지 길이는 그대로 두고 내부 비율만 압축된다.
//! \return 게이지 비율 계산의 분모
float UMyHPBarWidget::GetDisplayTotal() const
{
    return FMath::Max(FMath::Max(CachedMaxHP, CachedCurrentHP + CachedShield), 1.0f);
}

float UMyHPBarWidget::GetHPRatio() const
{
    return FMath::Clamp(CachedCurrentHP / GetDisplayTotal(), 0.0f, 1.0f);
}

float UMyHPBarWidget::GetShieldRatio() const
{
    return FMath::Clamp(CachedShield / GetDisplayTotal(), 0.0f, 1.0f);
}
