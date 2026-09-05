#include "MyPlayerStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "../../../GAS/MyAttributeSet.h"
#include "../../../GAS/MyPlayerState.h"
#include "MyExpBarWidget.h"
#include "MyHPBarWidget.h"
#include "MyPlayerProfileWidget.h"
#include "Widget/HUD/StatusEffects/MyStatusEffectPanelWidget.h"
#include "../../../Player/PlayerCharacterBase.h"

namespace
{
    constexpr float AttributeBindRetryInterval = 0.2f;
    constexpr int32 MaxAttributeBindRetryCount = 25;
}

void UMyPlayerStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus NativeConstruct - Widget: %s, OwningPlayer: %s, OwningPawn: %s"),
        *GetNameSafe(this),
        *GetNameSafe(GetOwningPlayer()),
        *GetNameSafe(GetOwningPlayerPawn()));

    const bool bBound = BindToOwningPlayerAttributes();
    UE_LOG(LogTemp, Log, TEXT("PlayerStatus BindToOwningPlayerAttributes result - Widget: %s, Bound: %s"),
        *GetNameSafe(this),
        bBound ? TEXT("true") : TEXT("false"));
}

void UMyPlayerStatusWidget::NativeDestruct()
{
    UE_LOG(LogTemp, Log, TEXT("PlayerStatus NativeDestruct - Widget: %s"), *GetNameSafe(this));
    CancelAttributeBindRetry();
    UnbindFromAttributes();

    Super::NativeDestruct();
}

void UMyPlayerStatusWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (IsDesignTime())
    {
        ApplyPreviewStatus();
    }
}

void UMyPlayerStatusWidget::SetPlayerStatus(
    float CurrentHP,
    float MaxHP,
    float CurrentExp,
    float RequiredExp,
    int32 Level,
    UTexture2D* CharacterIcon,
    bool bAnimate)
{
    if (HPBar)
    {
        HPBar->SetHP(CurrentHP, MaxHP, bAnimate);
    }

    if (ExpBar)
    {
        ExpBar->SetExp(CurrentExp, RequiredExp, bAnimate);
    }

    if (PlayerProfile)
    {
        PlayerProfile->SetProfile(Level, CharacterIcon);
    }
}

void UMyPlayerStatusWidget::ApplyPreviewStatus()
{
    SetPlayerStatus(
        PreviewCurrentHP,
        PreviewMaxHP,
        PreviewCurrentExp,
        PreviewRequiredExp,
        PreviewLevel,
        PreviewCharacterIcon,
        false);
}

bool UMyPlayerStatusWidget::BindToOwningPlayerAttributes()
{
    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(GetOwningPlayerPawn());
    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus bind failed - OwningPawn is not APlayerCharacterBase. Widget: %s, OwningPawn: %s"),
            *GetNameSafe(this),
            *GetNameSafe(GetOwningPlayerPawn()));
        ApplyPreviewStatus();
        ScheduleAttributeBindRetry();
        return false;
    }

    UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus bind failed - ASC is null. Widget: %s, PlayerCharacter: %s"),
            *GetNameSafe(this),
            *GetNameSafe(PlayerCharacter));
        ApplyPreviewStatus();
        ScheduleAttributeBindRetry();
        return false;
    }

    if (!HPBar)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus bind warning - HPBar BindWidget is null. Widget: %s"), *GetNameSafe(this));
    }

    if (BoundAbilitySystemComponent == ASC)
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerStatus bind reused - Widget: %s, ASC: %s"),
            *GetNameSafe(this),
            *GetNameSafe(ASC));
        if (StatusEffectPanel)
        {
            StatusEffectPanel->BindToAbilitySystemComponent(ASC);
        }
        BindToLevelData(PlayerCharacter->GetPlayerState<AMyPlayerState>());
        RefreshHPFromAttribute(false);
        RefreshLevelDataFromState(false);
        return true;
    }

    UnbindFromAttributes();

    BoundAbilitySystemComponent = ASC;
    HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
        .AddUObject(this, &ThisClass::HandleHealthChanged);
    MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMaxHealthAttribute())
        .AddUObject(this, &ThisClass::HandleMaxHealthChanged);
    ShieldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute())
        .AddUObject(this, &ThisClass::HandleShieldChanged);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus bind success - Widget: %s, PlayerCharacter: %s, ASC: %s, HealthHandle: %s, MaxHealthHandle: %s"),
        *GetNameSafe(this),
        *GetNameSafe(PlayerCharacter),
        *GetNameSafe(ASC),
        HealthChangedHandle.IsValid() ? TEXT("valid") : TEXT("invalid"),
        MaxHealthChangedHandle.IsValid() ? TEXT("valid") : TEXT("invalid"));

    CancelAttributeBindRetry();
    if (StatusEffectPanel)
    {
        StatusEffectPanel->BindToAbilitySystemComponent(ASC);
    }
    BindToLevelData(PlayerCharacter->GetPlayerState<AMyPlayerState>());
    RefreshHPFromAttribute(false);
    RefreshLevelDataFromState(false);
    return true;
}

void UMyPlayerStatusWidget::ScheduleAttributeBindRetry()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus bind retry skipped - World is null. Widget: %s"), *GetNameSafe(this));
        return;
    }

    if (AttributeBindRetryTimerHandle.IsValid())
    {
        return;
    }

    if (AttributeBindRetryCount >= MaxAttributeBindRetryCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus bind retry exhausted - Widget: %s, RetryCount: %d"),
            *GetNameSafe(this),
            AttributeBindRetryCount);
        return;
    }

    World->GetTimerManager().SetTimer(
        AttributeBindRetryTimerHandle,
        this,
        &ThisClass::HandleAttributeBindRetry,
        AttributeBindRetryInterval,
        false);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus bind retry scheduled - Widget: %s, NextRetry: %.2fs, RetryCount: %d/%d"),
        *GetNameSafe(this),
        AttributeBindRetryInterval,
        AttributeBindRetryCount + 1,
        MaxAttributeBindRetryCount);
}

void UMyPlayerStatusWidget::CancelAttributeBindRetry()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AttributeBindRetryTimerHandle);
    }

    AttributeBindRetryTimerHandle.Invalidate();
    AttributeBindRetryCount = 0;
}

void UMyPlayerStatusWidget::HandleAttributeBindRetry()
{
    AttributeBindRetryTimerHandle.Invalidate();
    ++AttributeBindRetryCount;

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus bind retry - Widget: %s, RetryCount: %d/%d, OwningPawn: %s"),
        *GetNameSafe(this),
        AttributeBindRetryCount,
        MaxAttributeBindRetryCount,
        *GetNameSafe(GetOwningPlayerPawn()));

    if (!BindToOwningPlayerAttributes() && AttributeBindRetryCount < MaxAttributeBindRetryCount)
    {
        ScheduleAttributeBindRetry();
    }
}

void UMyPlayerStatusWidget::UnbindFromAttributes()
{
    UnbindFromLevelData();

    if (StatusEffectPanel)
    {
        StatusEffectPanel->UnbindFromAbilitySystemComponent();
    }

    if (!BoundAbilitySystemComponent)
    {
        UE_LOG(LogTemp, Verbose, TEXT("PlayerStatus unbind skipped - ASC is null. Widget: %s"), *GetNameSafe(this));
        HealthChangedHandle.Reset();
        MaxHealthChangedHandle.Reset();
        ShieldChangedHandle.Reset();
        return;
    }

    if (HealthChangedHandle.IsValid())
    {
        BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedHandle);
        HealthChangedHandle.Reset();
    }

    if (MaxHealthChangedHandle.IsValid())
    {
        BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMaxHealthAttribute())
            .Remove(MaxHealthChangedHandle);
        MaxHealthChangedHandle.Reset();
    }

    if (ShieldChangedHandle.IsValid())
    {
        BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute())
            .Remove(ShieldChangedHandle);
        ShieldChangedHandle.Reset();
    }

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus unbind complete - Widget: %s"), *GetNameSafe(this));
    BoundAbilitySystemComponent = nullptr;
}

void UMyPlayerStatusWidget::RefreshHPFromAttribute(bool bAnimate)
{
    if (!BoundAbilitySystemComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus refresh skipped - ASC is null. Widget: %s"), *GetNameSafe(this));
        return;
    }

    if (!HPBar)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatus refresh skipped - HPBar is null. Widget: %s"), *GetNameSafe(this));
        return;
    }

    const float CurrentHP = BoundAbilitySystemComponent->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
    const float MaxHP = BoundAbilitySystemComponent->GetNumericAttribute(UMyAttributeSet::GetMaxHealthAttribute());
    const float Shield = BoundAbilitySystemComponent->GetNumericAttribute(UMyAttributeSet::GetShieldAttribute());

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus refresh HP - Widget: %s, CurrentHP: %.2f, MaxHP: %.2f, Shield: %.2f, Animate: %s"),
        *GetNameSafe(this),
        CurrentHP,
        MaxHP,
        Shield,
        bAnimate ? TEXT("true") : TEXT("false"));

    HPBar->SetHP(CurrentHP, MaxHP, bAnimate);
    HPBar->SetShield(Shield, bAnimate);
}

void UMyPlayerStatusWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerStatus Health changed - Widget: %s, OldValue: %.2f, NewValue: %.2f"),
        *GetNameSafe(this),
        Data.OldValue,
        Data.NewValue);

    RefreshHPFromAttribute(true);
}

void UMyPlayerStatusWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerStatus MaxHealth changed - Widget: %s, OldValue: %.2f, NewValue: %.2f"),
        *GetNameSafe(this),
        Data.OldValue,
        Data.NewValue);

    RefreshHPFromAttribute(true);
}

////////////////////////////
//! \author 준혁
//! \brief 보호막 어트리뷰트 변경(서버 적용 또는 클라이언트 복제 수신)을 받아 체력 바의 보호막 구간을 갱신한다.
//! \param Data 어트리뷰트 변경 정보
void UMyPlayerStatusWidget::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
    if (HPBar)
    {
        HPBar->SetShield(Data.NewValue);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 소유 폰의 PlayerState에 레벨/경험치 변경 알림(OnLevelDataChanged)을 구독한다. 이미 같은 PlayerState에 구독 중이면 재구독하지 않는다.
//! \param MyPlayerState 구독할 PlayerState
//! \return 없음
void UMyPlayerStatusWidget::BindToLevelData(AMyPlayerState* MyPlayerState)
{
    if (!MyPlayerState || BoundPlayerState == MyPlayerState)
    {
        return;
    }

    UnbindFromLevelData();

    BoundPlayerState = MyPlayerState;
    LevelDataChangedHandle = MyPlayerState->OnLevelDataChanged.AddUObject(this, &ThisClass::HandleLevelDataChanged);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatus level data bind success - Widget: %s, PlayerState: %s"),
        *GetNameSafe(this),
        *GetNameSafe(MyPlayerState));
}

////////////////////////////
//! \author 준혁
//! \brief 구독 중인 PlayerState의 레벨/경험치 변경 알림을 해제한다.
//! \param 없음
//! \return 없음
void UMyPlayerStatusWidget::UnbindFromLevelData()
{
    if (BoundPlayerState && LevelDataChangedHandle.IsValid())
    {
        BoundPlayerState->OnLevelDataChanged.Remove(LevelDataChangedHandle);
    }

    LevelDataChangedHandle.Reset();
    BoundPlayerState = nullptr;
}

////////////////////////////
//! \author 준혁
//! \brief PlayerState의 레벨/경험치와 캐릭터의 필요 경험치를 읽어 경험치 바와 레벨 텍스트를 갱신한다.
//! \param bAnimate 경험치 바 채움 애니메이션 재생 여부
//! \return 없음
void UMyPlayerStatusWidget::RefreshLevelDataFromState(bool bAnimate)
{
    if (!BoundPlayerState)
    {
        return;
    }

    const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(GetOwningPlayerPawn());
    if (!PlayerCharacter)
    {
        return;
    }

    const int32 Level = BoundPlayerState->GetCharacterLevel();
    const int32 CurrentExp = BoundPlayerState->GetCharacterExp();
    const int32 RequiredExp = PlayerCharacter->GetExpRequiredForNextLevel();

    if (ExpBar)
    {
        if (RequiredExp > 0)
        {
            ExpBar->SetExp(static_cast<float>(CurrentExp), static_cast<float>(RequiredExp), bAnimate);
        }
        else
        {
            // 최대 레벨(또는 Exp 커브 없음)에서는 요구량이 0이므로 게이지를 가득 찬 상태로 표시한다.
            ExpBar->SetExpInstant(1.0f, 1.0f);
        }
    }

    if (PlayerProfile)
    {
        PlayerProfile->SetLevel(Level);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 레벨/경험치 변경 알림(서버 갱신 또는 클라이언트 복제 수신)을 받아 UI를 갱신한다.
//! \param 없음
//! \return 없음
void UMyPlayerStatusWidget::HandleLevelDataChanged()
{
    RefreshLevelDataFromState(true);
}
