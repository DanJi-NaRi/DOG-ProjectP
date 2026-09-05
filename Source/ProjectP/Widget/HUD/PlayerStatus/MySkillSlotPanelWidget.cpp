#include "MySkillSlotPanelWidget.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "MySkillSlotWidget.h"
#include "Player/Components/MySkillControlComponent.h"
#include "Player/PlayerCharacterBase.h"

namespace
{
    constexpr float CooldownBindRetryInterval = 0.2f;
    constexpr int32 MaxCooldownBindRetryCount = 25;

    //! \brief 쿨다운 GE의 실제 남은 시간으로 슬롯 쿨다운 표시를 시작한다. 아직 GE가 복제되지 않았으면 전체 시간으로 대체한다.
    void StartSlotCooldownFromRemaining(UMySkillSlotWidget* SlotWidget, const UAbilitySystemComponent* ASC)
    {
        if (!SlotWidget)
        {
            return;
        }

        const float Remaining = UMyAbilitySystemLibrary::GetCooldownRemainingByTag(ASC, SlotWidget->GetCooldownTag());
        SlotWidget->StartCooldown(Remaining > 0.0f ? Remaining : SlotWidget->GetCooldownDuration());
    }
}

//! \editor 준혁 - 인증 후 폰 교체 시 스킬 아이콘이 갱신되도록 OnPossessedPawnChanged 구독 추가
void UMySkillSlotPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* OwningPC = GetOwningPlayer())
    {
        OwningPC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UMySkillSlotPanelWidget::HandlePossessedPawnChanged);
    }

    RefreshSkillSlots();
    BindToOwningPlayerSkillCooldowns();
}

void UMySkillSlotPanelWidget::NativeDestruct()
{
    if (APlayerController* OwningPC = GetOwningPlayer())
    {
        OwningPC->OnPossessedPawnChanged.RemoveDynamic(this, &UMySkillSlotPanelWidget::HandlePossessedPawnChanged);
    }

    CancelCooldownBindRetry();
    UnbindFromSkillCooldowns();

    Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 빙의 폰 변경 시 스킬 슬롯 아이콘과 쿨다운 바인딩을 새 폰 기준으로 갱신하는 함수.
//!        던전 입장 인증 후 선택 캐릭터로 폰이 교체될 때 HUD가 이전 폰의 스킬을 표시하는 문제를 막는다.
//! \param OldPawn 이전에 빙의했던 폰
//! \param NewPawn 새로 빙의한 폰
void UMySkillSlotPanelWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    RefreshSkillSlots();

    // 폰이 바뀌면 ASC도 바뀔 수 있으므로 쿨다운 바인딩을 다시 잡는다. (실패 시 내부 재시도)
    CancelCooldownBindRetry();
    CooldownBindRetryCount = 0;
    BindToOwningPlayerSkillCooldowns();
}

void UMySkillSlotPanelWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    RefreshSkillSlots();
}

void UMySkillSlotPanelWidget::RefreshSkillSlots()
{
    SlotsByInputTag.Reset();

    if (RefreshSkillSlotsFromOwningPlayer())
    {
        return;
    }

    //InitializeEmptySlot(Slot_Basic, FText::FromString(TEXT("LMB")));
    InitializeEmptySlot(Slot_Q, FText::FromString(TEXT("Q")));
    InitializeEmptySlot(Slot_E, FText::FromString(TEXT("E")));
    InitializeEmptySlot(Slot_R, FText::FromString(TEXT("R")));
    InitializeEmptySlot(Slot_C, FText::FromString(TEXT("C")));
    //InitializeEmptySlot(Slot_Move, FText::FromString(TEXT("Space")));
}

bool UMySkillSlotPanelWidget::RefreshSkillSlotsFromOwningPlayer()
{
    UMySkillControlComponent* SkillControlComponent = GetOwningSkillControlComponent();
    if (!SkillControlComponent)
    {
        return false;
    }

    //InitializeSlotFromSkillDefinition(Slot_Basic, SkillControlComponent->BasicAttackSlot, FText::FromString(TEXT("LMB")));
    InitializeSlotFromSkillDefinition(Slot_Q, SkillControlComponent->QSkillSlot, FText::FromString(TEXT("Q")));
    InitializeSlotFromSkillDefinition(Slot_E, SkillControlComponent->ESkillSlot, FText::FromString(TEXT("E")));
    InitializeSlotFromSkillDefinition(Slot_R, SkillControlComponent->RSkillSlot, FText::FromString(TEXT("R")));
    InitializeSlotFromSkillDefinition(Slot_C, SkillControlComponent->CSkillSlot, FText::FromString(TEXT("C")));
    //InitializeSlotFromSkillDefinition(Slot_Move, SkillControlComponent->MoveSkillSlot, FText::FromString(TEXT("Space")));

    return true;
}

UMySkillControlComponent* UMySkillSlotPanelWidget::GetOwningSkillControlComponent() const
{
    const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(GetOwningPlayerPawn());
    return PlayerCharacter ? PlayerCharacter->MySkillControlComponent : nullptr;
}

void UMySkillSlotPanelWidget::InitializeSlotFromSkillDefinition(UMySkillSlotWidget* SlotWidget, const FMySkillSlotSpec& SkillSlotSpec, FText InputKeyText)
{
    if (!SlotWidget)
    {
        return;
    }

    SlotWidget->SetInputKeyText(InputKeyText);

    const UMySkillDefinitionDataAsset* SkillDefinition = SkillSlotSpec.GetSkillDefinition();
    if (!SkillDefinition)
    {
        SlotWidget->ClearSkillData();
        SlotWidget->SetInputKeyText(InputKeyText);
        SlotWidget->SetLocked(true);
        return;
    }

    FMySkillDataEntry SkillData;
    if (!SkillDefinition->BuildSkillDataEntry(SkillData))
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotPanel initialize failed - invalid SkillDefinition. Panel: %s, Slot: %s, Definition: %s"),
            *GetNameSafe(this),
            *GetNameSafe(SlotWidget),
            *GetNameSafe(SkillDefinition));

        SlotWidget->ClearSkillData();
        SlotWidget->SetInputKeyText(InputKeyText);
        SlotWidget->SetLocked(true);
        return;
    }

    SlotWidget->SetSkillData(SkillData);
    SlotWidget->SetInputKeyText(InputKeyText);
    SlotWidget->SetLocked(false);

    if (SkillData.InputTag.IsValid())
    {
        SlotsByInputTag.Add(SkillData.InputTag, SlotWidget);
    }
}

bool UMySkillSlotPanelWidget::BindToOwningPlayerSkillCooldowns()
{
    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(GetOwningPlayerPawn());
    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotPanel cooldown bind failed - OwningPawn is not APlayerCharacterBase. Widget: %s, OwningPawn: %s"),
            *GetNameSafe(this),
            *GetNameSafe(GetOwningPlayerPawn()));
        ScheduleCooldownBindRetry();
        return false;
    }

    UAbilitySystemComponent* ASC = PlayerCharacter->GetAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotPanel cooldown bind failed - ASC is null. Widget: %s, PlayerCharacter: %s"),
            *GetNameSafe(this),
            *GetNameSafe(PlayerCharacter));
        ScheduleCooldownBindRetry();
        return false;
    }

    if (BoundAbilitySystemComponent == ASC)
    {
        return true;
    }

    UnbindFromSkillCooldowns();
    BoundAbilitySystemComponent = ASC;

    //BindCooldownTag(Slot_Basic);
    BindCooldownTag(Slot_Q);
    BindCooldownTag(Slot_E);
    BindCooldownTag(Slot_R);
    BindCooldownTag(Slot_C);

    CancelCooldownBindRetry();

    UE_LOG(LogTemp, Log, TEXT("SkillSlotPanel cooldown bind success - Widget: %s, ASC: %s, BoundTagCount: %d"),
        *GetNameSafe(this),
        *GetNameSafe(ASC),
        CooldownTagDelegateHandles.Num());

    return true;
}

void UMySkillSlotPanelWidget::StartCooldownByInputTag(FGameplayTag InputTag)
{
    TObjectPtr<UMySkillSlotWidget>* FoundSlot = SlotsByInputTag.Find(InputTag);
    if (!FoundSlot || !FoundSlot->Get())
    {
        return;
    }

    UMySkillSlotWidget* SlotWidget = FoundSlot->Get();
    StartSlotCooldownFromRemaining(SlotWidget, BoundAbilitySystemComponent);
}

void UMySkillSlotPanelWidget::StartCooldownByCooldownTag(FGameplayTag CooldownTag)
{
    TObjectPtr<UMySkillSlotWidget>* FoundSlot = SlotsByCooldownTag.Find(CooldownTag);
    if (!FoundSlot || !FoundSlot->Get())
    {
        return;
    }

    UMySkillSlotWidget* SlotWidget = FoundSlot->Get();
    StartSlotCooldownFromRemaining(SlotWidget, BoundAbilitySystemComponent);
}

void UMySkillSlotPanelWidget::InitializeEmptySlot(UMySkillSlotWidget* SlotWidget, FText InputKeyText)
{
    if (!SlotWidget)
    {
        return;
    }

    SlotWidget->ClearSkillData();
    SlotWidget->SetInputKeyText(InputKeyText);
    SlotWidget->SetLocked(true);
}

void UMySkillSlotPanelWidget::BindCooldownTag(UMySkillSlotWidget* SlotWidget)
{
    if (!BoundAbilitySystemComponent || !SlotWidget)
    {
        return;
    }

    const FGameplayTag CooldownTag = SlotWidget->GetCooldownTag();
    if (!CooldownTag.IsValid())
    {
        return;
    }

    SlotsByCooldownTag.Add(CooldownTag, SlotWidget);

    FDelegateHandle DelegateHandle = BoundAbilitySystemComponent
        ->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &ThisClass::HandleCooldownTagChanged);

    CooldownTagDelegateHandles.Add(CooldownTag, DelegateHandle);

    if (BoundAbilitySystemComponent->HasMatchingGameplayTag(CooldownTag))
    {
        StartSlotCooldownFromRemaining(SlotWidget, BoundAbilitySystemComponent);
    }
}

void UMySkillSlotPanelWidget::UnbindFromSkillCooldowns()
{
    if (BoundAbilitySystemComponent)
    {
        for (const TPair<FGameplayTag, FDelegateHandle>& Pair : CooldownTagDelegateHandles)
        {
            if (Pair.Key.IsValid() && Pair.Value.IsValid())
            {
                BoundAbilitySystemComponent
                    ->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved)
                    .Remove(Pair.Value);
            }
        }
    }

    SlotsByCooldownTag.Reset();
    SlotsByInputTag.Reset();
    CooldownTagDelegateHandles.Reset();
    BoundAbilitySystemComponent = nullptr;
}

void UMySkillSlotPanelWidget::ScheduleCooldownBindRetry()
{
    UWorld* World = GetWorld();
    if (!World || CooldownBindRetryTimerHandle.IsValid() || CooldownBindRetryCount >= MaxCooldownBindRetryCount)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        CooldownBindRetryTimerHandle,
        this,
        &ThisClass::HandleCooldownBindRetry,
        CooldownBindRetryInterval,
        false);
}

void UMySkillSlotPanelWidget::CancelCooldownBindRetry()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownBindRetryTimerHandle);
    }

    CooldownBindRetryTimerHandle.Invalidate();
    CooldownBindRetryCount = 0;
}

void UMySkillSlotPanelWidget::HandleCooldownBindRetry()
{
    CooldownBindRetryTimerHandle.Invalidate();
    ++CooldownBindRetryCount;

    if (!BindToOwningPlayerSkillCooldowns() && CooldownBindRetryCount < MaxCooldownBindRetryCount)
    {
        ScheduleCooldownBindRetry();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 쿨타임 태그 개수 변경에 맞춰 해당 스킬 슬롯의 쿨타임 표시를 시작하거나 종료하는 함수
// CooldownTag : 변경된 쿨타임 태그
// NewCount : 변경 후 쿨타임 태그 개수
void UMySkillSlotPanelWidget::HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount)
{
    if (NewCount <= 0)
    {
        TObjectPtr<UMySkillSlotWidget>* FoundSlot = SlotsByCooldownTag.Find(CooldownTag);
        if (FoundSlot && FoundSlot->Get())
        {
            FoundSlot->Get()->ClearCooldown();
            FoundSlot->Get()->PlayCooldownResetEffect();
        }

        return;
    }

    StartCooldownByCooldownTag(CooldownTag);
}
