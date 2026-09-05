#include "MyExpBarWidget.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Engine/Texture2D.h"

namespace
{
    constexpr float ExpBarFillTickInterval = 1.0f / 60.0f;

    //! [채움, 나머지] 두 자식의 Fill 가중치를 Ratio : 1-Ratio 로 배분해 채움 폭을 조절한다.
    void ApplyExpFillWeights(UHorizontalBox* Box, float Ratio)
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

void UMyExpBarWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // 이름만 같고 타입이 다른 위젯이 바인딩돼 들어오는 경우 방어 (WBP 마이그레이션 중 크래시 방지).
    if (HB_Exp && !HB_Exp->IsA(UHorizontalBox::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("[ExpBar] HB_Exp가 HorizontalBox 타입이 아님(%s) - WBP에서 교체 필요"), *GetNameSafe(HB_Exp->GetClass()));
        HB_Exp = nullptr;
    }

    if (IsDesignTime())
    {
        CachedCurrentExp = PreviewCurrentExp;
        CachedRequiredExp = PreviewRequiredExp;
    }

    DisplayedExpRatio = GetExpRatio();
    ApplyConfiguredTextures();
    ApplyDisplayOptions();
    ApplyBar();
}

void UMyExpBarWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FillAnimationTimerHandle);
    }

    Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 경험치를 설정한다. bAnimate면 현재 표시 비율에서 목표 비율까지 부드럽게 채운다.
//! \param CurrentExp 현재 경험치
//! \param RequiredExp 다음 레벨까지 필요 경험치
//! \param bAnimate 게이지 보간 애니메이션 재생 여부
void UMyExpBarWidget::SetExp(float CurrentExp, float RequiredExp, bool bAnimate)
{
    CachedRequiredExp = FMath::Max(RequiredExp, 1.0f);
    CachedCurrentExp = FMath::Clamp(CurrentExp, 0.0f, CachedRequiredExp);

    if (bAnimate)
    {
        StartFillAnimation();
    }
    else
    {
        DisplayedExpRatio = GetExpRatio();
    }

    ApplyBar();
}

void UMyExpBarWidget::SetExpInstant(float CurrentExp, float RequiredExp)
{
    SetExp(CurrentExp, RequiredExp, false);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 경험치 수치 텍스트의 표시 여부를 설정하는 함수
// bVisible : true면 경험치 수치 텍스트 표시, false면 숨김
void UMyExpBarWidget::SetValueTextVisible(bool bVisible)
{
    bValueTextVisible = bVisible;
    ApplyDisplayOptions();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 경험치 게이지의 표시 여부를 설정하고 배경은 유지하는 함수
// bVisible : true면 경험치 게이지 표시, false면 게이지만 숨김
void UMyExpBarWidget::SetGaugeVisible(bool bVisible)
{
    bGaugeVisible = bVisible;
    ApplyDisplayOptions();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 경험치 바 인스턴스에서 사용할 텍스처 구성을 설정하는 함수
// InFillTexture : 경험치 채움 텍스처
// InBackgroundTexture : 경험치 바 배경 텍스처
void UMyExpBarWidget::SetBarTextures(UTexture2D* InFillTexture, UTexture2D* InBackgroundTexture)
{
    FillTexture = InFillTexture;
    BackgroundTexture = InBackgroundTexture;
    ApplyConfiguredTextures();
}

void UMyExpBarWidget::ApplyBar()
{
    ApplyExpFillWeights(HB_Exp, DisplayedExpRatio);

    RefreshText();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 경험치 수치 텍스트와 게이지에 저장된 표시 설정을 적용하는 함수
void UMyExpBarWidget::ApplyDisplayOptions()
{
    if (Text_Exp)
    {
        Text_Exp->SetVisibility(bValueTextVisible
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
// 설정된 경험치 바 텍스처를 각 이미지 위젯에 적용하는 함수
void UMyExpBarWidget::ApplyConfiguredTextures()
{
    if (!FillTexture && IMG_ExpFill)
    {
        FillTexture = Cast<UTexture2D>(IMG_ExpFill->GetBrush().GetResourceObject());
    }

    if (!BackgroundTexture && IMG_BackgroundEXP)
    {
        BackgroundTexture = Cast<UTexture2D>(IMG_BackgroundEXP->GetBrush().GetResourceObject());
    }

    if (IMG_ExpFill && FillTexture)
    {
        IMG_ExpFill->SetBrushResourceObject(FillTexture);
    }

    if (IMG_BackgroundEXP && BackgroundTexture)
    {
        IMG_BackgroundEXP->SetBrushResourceObject(BackgroundTexture);
    }
}

void UMyExpBarWidget::StartFillAnimation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        DisplayedExpRatio = GetExpRatio();
        return;
    }

    if (!FillAnimationTimerHandle.IsValid())
    {
        World->GetTimerManager().SetTimer(
            FillAnimationTimerHandle,
            this,
            &ThisClass::HandleFillAnimationTick,
            ExpBarFillTickInterval,
            true);
    }
}

void UMyExpBarWidget::HandleFillAnimationTick()
{
    const float TargetRatio = GetExpRatio();
    DisplayedExpRatio = FMath::FInterpConstantTo(DisplayedExpRatio, TargetRatio, ExpBarFillTickInterval, FillAnimationSpeed);

    if (FMath::IsNearlyEqual(DisplayedExpRatio, TargetRatio))
    {
        DisplayedExpRatio = TargetRatio;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(FillAnimationTimerHandle);
        }
        FillAnimationTimerHandle.Invalidate();
    }

    ApplyBar();
}

void UMyExpBarWidget::RefreshText()
{
    if (!Text_Exp)
    {
        return;
    }

    const int32 Current = FMath::RoundToInt(CachedCurrentExp);
    const int32 Required = FMath::RoundToInt(CachedRequiredExp);
    Text_Exp->SetText(FText::Format(NSLOCTEXT("ProjectP", "ExpFormat", "{0} / {1}"), FText::AsNumber(Current), FText::AsNumber(Required)));
}

float UMyExpBarWidget::GetExpRatio() const
{
    return FMath::Clamp(CachedCurrentExp / FMath::Max(CachedRequiredExp, 1.0f), 0.0f, 1.0f);
}
