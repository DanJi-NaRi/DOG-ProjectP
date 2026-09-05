#include "MyStatusEffectSlot.h"

#include "AbilitySystemComponent.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

void UMyStatusEffectSlot::NativePreConstruct()
{
    Super::NativePreConstruct();

    ApplyDefinitionDisplay();

    if (IsDesignTime())
    {
        if (bPreviewActive)
        {
            ApplyActiveDisplay(PreviewRemainingTime, PreviewDuration, PreviewStackCount);
        }
        else
        {
            ApplyInactiveDisplay();
        }
    }
}

void UMyStatusEffectSlot::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshStatusEffect();
}

void UMyStatusEffectSlot::NativeDestruct()
{
    UnbindFromAbilitySystemComponent();
    Super::NativeDestruct();
}

void UMyStatusEffectSlot::SetStatusEffectDefinition(FGameplayTag InStatusTag, FText InDisplayName, UTexture2D* InIcon)
{
    if (StatusTag == InStatusTag && DisplayName.EqualTo(InDisplayName) && Icon == InIcon)
    {
        return;
    }

    const bool bWasBound = BoundAbilitySystemComponent != nullptr;
    TObjectPtr<UAbilitySystemComponent> PreviousAbilitySystemComponent = BoundAbilitySystemComponent;

    if (bWasBound)
    {
        UnbindFromAbilitySystemComponent();
    }

    StatusTag = InStatusTag;
    DisplayName = MoveTemp(InDisplayName);
    Icon = InIcon;

    ApplyDefinitionDisplay();

    if (bWasBound)
    {
        BindToAbilitySystemComponent(PreviousAbilitySystemComponent.Get());
    }
    else
    {
        RefreshStatusEffect();
    }
}

void UMyStatusEffectSlot::BindToAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
    if (BoundAbilitySystemComponent == InAbilitySystemComponent)
    {
        RefreshStatusEffect();
        return;
    }

    UnbindFromAbilitySystemComponent();
    BoundAbilitySystemComponent = InAbilitySystemComponent;

    if (BoundAbilitySystemComponent && StatusTag.IsValid())
    {
        StatusTagChangedHandle = BoundAbilitySystemComponent
            ->RegisterGameplayTagEvent(StatusTag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &ThisClass::HandleStatusTagChanged);
    }

    RefreshStatusEffect();
}

void UMyStatusEffectSlot::UnbindFromAbilitySystemComponent()
{
    StopDurationTimer();
    StopCategoryPulse();
    ClearStackChangeBindings();

    if (BoundAbilitySystemComponent && StatusTag.IsValid() && StatusTagChangedHandle.IsValid())
    {
        BoundAbilitySystemComponent
            ->RegisterGameplayTagEvent(StatusTag, EGameplayTagEventType::NewOrRemoved)
            .Remove(StatusTagChangedHandle);
    }

    StatusTagChangedHandle.Reset();
    BoundAbilitySystemComponent = nullptr;
}

void UMyStatusEffectSlot::RefreshStatusEffect()
{
    ApplyDefinitionDisplay();

    if (!BoundAbilitySystemComponent || !StatusTag.IsValid())
    {
        ApplyInactiveDisplay();
        return;
    }

    if (!BoundAbilitySystemComponent->HasMatchingGameplayTag(StatusTag))
    {
        ApplyInactiveDisplay();
        return;
    }

    float RemainingTime = 0.0f;
    float Duration = 0.0f;
    int32 StackCount = 0;
    TArray<FActiveGameplayEffectHandle> EffectHandles;
    QueryStatusEffectState(RemainingTime, Duration, StackCount, &EffectHandles);
    RefreshStackChangeBindings(EffectHandles);

    ApplyActiveDisplay(RemainingTime, Duration, FMath::Max(1, StackCount));
    StartDurationTimerIfNeeded(RemainingTime, Duration);
}

void UMyStatusEffectSlot::SetStatusEffectActive(float RemainingTime, float Duration, int32 StackCount)
{
    ApplyDefinitionDisplay();
    ApplyActiveDisplay(RemainingTime, Duration, StackCount);
    StartDurationTimerIfNeeded(RemainingTime, Duration);
}

void UMyStatusEffectSlot::ClearStatusEffect()
{
    StopDurationTimer();
    ApplyInactiveDisplay();
}

void UMyStatusEffectSlot::HandleStatusTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
    RefreshStatusEffect();
}

////////////////////////////
//! \author 장효제
//! \brief 활성 GameplayEffect의 실제 스택 수가 변하면 슬롯 표시를 즉시 갱신한다.
void UMyStatusEffectSlot::HandleStackCountChanged(
    FActiveGameplayEffectHandle EffectHandle,
    int32 NewStackCount,
    int32 PreviousStackCount)
{
    RefreshStatusEffect();
}

void UMyStatusEffectSlot::HandleDurationTick()
{
    RefreshStatusEffect();
}

////////////////////////////
//! \author 장효제
//! \brief CC 슬롯 프레임의 약한 점멸 투명도를 갱신한다.
void UMyStatusEffectSlot::HandleCategoryPulseTick()
{
    if (!IMG_CategoryFrame || !bIsCrowdControl)
    {
        StopCategoryPulse();
        return;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float SafePeriod = FMath::Max(0.2f, CrowdControlPulsePeriod);
    const float Pulse = 0.5f + 0.5f * FMath::Sin(World->GetTimeSeconds() * UE_TWO_PI / SafePeriod);
    IMG_CategoryFrame->SetRenderOpacity(FMath::Lerp(0.65f, 1.0f, Pulse));
}

////////////////////////////
//! \author 장효제
//! \brief 상태 태그를 부여한 활성 GameplayEffect의 시간과 실제 스택 수를 조회한다.
//! \param OutRemainingTime 가장 오래 남은 유한 효과의 남은 시간
//! \param OutDuration 가장 오래 남은 유한 효과의 전체 지속시간
//! \param OutStackCount 활성 효과 Spec의 실제 StackCount 합계
//! \return 태그에 대응하는 활성 GameplayEffect가 하나 이상이면 true
bool UMyStatusEffectSlot::QueryStatusEffectState(
    float& OutRemainingTime,
    float& OutDuration,
    int32& OutStackCount,
    TArray<FActiveGameplayEffectHandle>* OutEffectHandles) const
{
    OutRemainingTime = 0.0f;
    OutDuration = 0.0f;
    OutStackCount = 0;
    if (OutEffectHandles)
    {
        OutEffectHandles->Reset();
    }

    if (!BoundAbilitySystemComponent || !StatusTag.IsValid())
    {
        return false;
    }

    FGameplayTagContainer StatusTags;
    StatusTags.AddTag(StatusTag);

    FGameplayEffectQuery StatusQuery;
    StatusQuery.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(StatusTags);

    const float WorldTime = BoundAbilitySystemComponent->GetWorld()
        ? BoundAbilitySystemComponent->GetWorld()->GetTimeSeconds()
        : 0.0f;
    const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles =
        BoundAbilitySystemComponent->GetActiveEffects(StatusQuery);
    if (OutEffectHandles)
    {
        *OutEffectHandles = ActiveEffectHandles;
    }

    for (const FActiveGameplayEffectHandle& Handle : ActiveEffectHandles)
    {
        const FActiveGameplayEffect* ActiveEffect =
            BoundAbilitySystemComponent->GetActiveGameplayEffect(Handle);
        if (!ActiveEffect)
        {
            continue;
        }

        OutStackCount += FMath::Max(1, ActiveEffect->Spec.GetStackCount());

        const float RemainingTime = ActiveEffect->GetTimeRemaining(WorldTime);
        const float Duration = ActiveEffect->GetDuration();
        if (Duration > 0.0f && RemainingTime > OutRemainingTime)
        {
            OutRemainingTime = FMath::Max(RemainingTime, 0.0f);
            OutDuration = FMath::Max(Duration, 0.0f);
        }
    }

    return !ActiveEffectHandles.IsEmpty();
}

////////////////////////////
//! \author 장효제
//! \brief 현재 상태 태그를 부여한 GameplayEffect의 스택 변경 델리게이트만 유지한다.
//! \param EffectHandles 현재 상태 태그와 일치하는 활성 GameplayEffect 핸들
void UMyStatusEffectSlot::RefreshStackChangeBindings(const TArray<FActiveGameplayEffectHandle>& EffectHandles)
{
    if (!BoundAbilitySystemComponent)
    {
        ClearStackChangeBindings();
        return;
    }

    TArray<FActiveGameplayEffectHandle> HandlesToRemove;
    for (const TPair<FActiveGameplayEffectHandle, FDelegateHandle>& Pair : StackChangeDelegateHandles)
    {
        if (!EffectHandles.Contains(Pair.Key))
        {
            HandlesToRemove.Add(Pair.Key);
        }
    }

    for (const FActiveGameplayEffectHandle& Handle : HandlesToRemove)
    {
        if (FOnActiveGameplayEffectStackChange* Delegate =
            BoundAbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Handle))
        {
            Delegate->Remove(StackChangeDelegateHandles.FindRef(Handle));
        }
        StackChangeDelegateHandles.Remove(Handle);
    }

    for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
    {
        if (StackChangeDelegateHandles.Contains(Handle))
        {
            continue;
        }

        if (FOnActiveGameplayEffectStackChange* Delegate =
            BoundAbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Handle))
        {
            StackChangeDelegateHandles.Add(
                Handle,
                Delegate->AddUObject(this, &ThisClass::HandleStackCountChanged));
        }
    }
}

////////////////////////////
//! \author 장효제
//! \brief 등록한 GameplayEffect 스택 변경 델리게이트를 모두 해제한다.
void UMyStatusEffectSlot::ClearStackChangeBindings()
{
    if (BoundAbilitySystemComponent)
    {
        for (const TPair<FActiveGameplayEffectHandle, FDelegateHandle>& Pair : StackChangeDelegateHandles)
        {
            if (FOnActiveGameplayEffectStackChange* Delegate =
                BoundAbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Pair.Key))
            {
                Delegate->Remove(Pair.Value);
            }
        }
    }

    StackChangeDelegateHandles.Reset();
}

void UMyStatusEffectSlot::ApplyDefinitionDisplay()
{
    if (IMG_Icon && Icon)
    {
        IMG_Icon->SetBrushFromTexture(Icon);
    }

    ApplyCategoryDisplay();
}

////////////////////////////
//! \author 장효제
//! \brief StatusTag의 Buff, Debuff, CC 카테고리에 맞춰 프레임 색과 배지 기호를 적용한다.
void UMyStatusEffectSlot::ApplyCategoryDisplay()
{
    const FGameplayTag BuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff"), false);
    const FGameplayTag CrowdControlRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.CC"), false);
    const FGameplayTag DebuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff"), false);

    FLinearColor CategoryColor = FLinearColor::White;
    FText CategoryBadge = FText::GetEmpty();
    bIsCrowdControl = false;

    if (CrowdControlRoot.IsValid() && StatusTag.MatchesTag(CrowdControlRoot))
    {
        CategoryColor = CrowdControlCategoryColor;
        CategoryBadge = FText::FromString(TEXT("!"));
        bIsCrowdControl = true;
    }
    else if (DebuffRoot.IsValid() && StatusTag.MatchesTag(DebuffRoot))
    {
        CategoryColor = DebuffCategoryColor;
        CategoryBadge = FText::FromString(TEXT("▼"));
    }
    else if (BuffRoot.IsValid() && StatusTag.MatchesTag(BuffRoot))
    {
        CategoryColor = BuffCategoryColor;
        CategoryBadge = FText::FromString(TEXT("▲"));
    }

    if (IMG_CategoryFrame)
    {
        IMG_CategoryFrame->SetColorAndOpacity(CategoryColor);
    }
    if (IMG_CategoryBadge)
    {
        IMG_CategoryBadge->SetColorAndOpacity(CategoryColor);
    }
    if (TXT_CategoryBadge)
    {
        TXT_CategoryBadge->SetText(CategoryBadge);
        TXT_CategoryBadge->SetColorAndOpacity(FLinearColor::White);
    }
}

void UMyStatusEffectSlot::ApplyActiveDisplay(float RemainingTime, float Duration, int32 StackCount)
{
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    const bool bHasDuration = Duration > 0.0f && RemainingTime > 0.0f;
    const float DurationRatio = bHasDuration ? FMath::Clamp(RemainingTime / Duration, 0.0f, 1.0f) : 0.0f;

    if (IMG_DurationOverlay)
    {
        IMG_DurationOverlay->SetColorAndOpacity(DurationOverlayColor);
        IMG_DurationOverlay->SetVisibility(bHasDuration ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        IMG_DurationOverlay->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
        IMG_DurationOverlay->SetRenderScale(FVector2D(1.0f, DurationRatio));
    }

    if (TXT_Duration)
    {
        TXT_Duration->SetVisibility(bHasDuration ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        TXT_Duration->SetText(bHasDuration
            ? FText::AsNumber(FMath::Max(1, FMath::CeilToInt(RemainingTime)))
            : FText::GetEmpty());
    }

    if (TXT_Stack)
    {
        const bool bShowStack = StackCount > 1;
        TXT_Stack->SetVisibility(bShowStack ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        TXT_Stack->SetText(bShowStack ? FText::AsNumber(StackCount) : FText::GetEmpty());
    }

    StartCategoryPulseIfNeeded();
}

void UMyStatusEffectSlot::ApplyInactiveDisplay()
{
    StopDurationTimer();
    StopCategoryPulse();
    ClearStackChangeBindings();

    if (bCollapseWhenInactive && !IsDesignTime())
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    if (IMG_DurationOverlay)
    {
        IMG_DurationOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (TXT_Duration)
    {
        TXT_Duration->SetVisibility(ESlateVisibility::Collapsed);
        TXT_Duration->SetText(FText::GetEmpty());
    }

    if (TXT_Stack)
    {
        TXT_Stack->SetVisibility(ESlateVisibility::Collapsed);
        TXT_Stack->SetText(FText::GetEmpty());
    }
}

void UMyStatusEffectSlot::StartDurationTimerIfNeeded(float RemainingTime, float Duration)
{
    StopDurationTimer();

    if (RemainingTime <= 0.0f || Duration <= 0.0f)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DurationTickTimerHandle,
            this,
            &ThisClass::HandleDurationTick,
            DurationTickInterval,
            true);
    }
}

void UMyStatusEffectSlot::StopDurationTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DurationTickTimerHandle);
    }

    DurationTickTimerHandle.Invalidate();
}

////////////////////////////
//! \author 장효제
//! \brief 활성 CC 슬롯에만 프레임 점멸 타이머를 시작한다.
void UMyStatusEffectSlot::StartCategoryPulseIfNeeded()
{
    if (!bIsCrowdControl || !IMG_CategoryFrame)
    {
        StopCategoryPulse();
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(CategoryPulseTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                CategoryPulseTimerHandle,
                this,
                &ThisClass::HandleCategoryPulseTick,
                0.05f,
                true);
        }
    }
}

////////////////////////////
//! \author 장효제
//! \brief CC 프레임 점멸 타이머를 중지하고 불투명도를 복원한다.
void UMyStatusEffectSlot::StopCategoryPulse()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CategoryPulseTimerHandle);
    }

    CategoryPulseTimerHandle.Invalidate();
    if (IMG_CategoryFrame)
    {
        IMG_CategoryFrame->SetRenderOpacity(1.0f);
    }
}
