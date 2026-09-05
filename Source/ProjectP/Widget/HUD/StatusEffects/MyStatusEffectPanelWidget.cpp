#include "MyStatusEffectPanelWidget.h"

#include "AbilitySystemComponent.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/DataTable.h"
#include "MyStatusEffectSlot.h"

void UMyStatusEffectPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RefreshStatusEffects();
}

void UMyStatusEffectPanelWidget::NativeDestruct()
{
    UnbindFromAbilitySystemComponent();

    Super::NativeDestruct();
}

void UMyStatusEffectPanelWidget::BindToAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
    if (BoundAbilitySystemComponent == InAbilitySystemComponent)
    {
        RefreshStatusEffects();
        return;
    }

    UnbindFromAbilitySystemComponent();
    BoundAbilitySystemComponent = InAbilitySystemComponent;

    if (BoundAbilitySystemComponent)
    {
        GenericTagChangedHandle = BoundAbilitySystemComponent
            ->RegisterGenericGameplayTagEvent()
            .AddUObject(this, &ThisClass::HandleOwnedTagChanged);
    }

    RefreshStatusEffects();
}

void UMyStatusEffectPanelWidget::UnbindFromAbilitySystemComponent()
{
    if (BoundAbilitySystemComponent && GenericTagChangedHandle.IsValid())
    {
        BoundAbilitySystemComponent
            ->RegisterGenericGameplayTagEvent()
            .Remove(GenericTagChangedHandle);
    }

    GenericTagChangedHandle.Reset();
    ClearStatusEffectSlots();
    BoundAbilitySystemComponent = nullptr;
}

void UMyStatusEffectPanelWidget::RefreshStatusEffects()
{
    if (!BoundAbilitySystemComponent)
    {
        ClearStatusEffectSlots();
        return;
    }

    FGameplayTagContainer OwnedTags;
    BoundAbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);

    TSet<FGameplayTag> ActiveStatusTags;
    for (const FGameplayTag& OwnedTag : OwnedTags)
    {
        if (IsStatusEffectTag(OwnedTag) && BoundAbilitySystemComponent->GetTagCount(OwnedTag) > 0)
        {
            ActiveStatusTags.Add(OwnedTag);
            AddOrRefreshStatusEffectSlot(OwnedTag);
        }
    }

    TArray<FGameplayTag> TagsToRemove;
    for (const TPair<FGameplayTag, TObjectPtr<UMyStatusEffectSlot>>& Pair : ActiveStatusEffectSlots)
    {
        if (!ActiveStatusTags.Contains(Pair.Key))
        {
            TagsToRemove.Add(Pair.Key);
        }
    }

    for (const FGameplayTag& TagToRemove : TagsToRemove)
    {
        RemoveStatusEffectSlot(TagToRemove);
    }

    RefreshPanelLayout();
}

void UMyStatusEffectPanelWidget::HandleOwnedTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
    if (!IsStatusEffectTag(ChangedTag))
    {
        return;
    }

    if (NewCount > 0)
    {
        AddOrRefreshStatusEffectSlot(ChangedTag);
    }
    else
    {
        RemoveStatusEffectSlot(ChangedTag);
    }

    RefreshPanelLayout();
}

void UMyStatusEffectPanelWidget::AddOrRefreshStatusEffectSlot(FGameplayTag StatusTag)
{
    if (!BoundAbilitySystemComponent || !HB_StatusEffects || !StatusTag.IsValid())
    {
        return;
    }

    if (TObjectPtr<UMyStatusEffectSlot>* ExistingSlot = ActiveStatusEffectSlots.Find(StatusTag))
    {
        if (ExistingSlot->Get())
        {
            ExistingSlot->Get()->RefreshStatusEffect();
            return;
        }

        ActiveStatusEffectSlots.Remove(StatusTag);
    }

    if (!StatusEffectSlotClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("StatusEffectPanel cannot create slot - StatusEffectSlotClass is null. Widget: %s, StatusTag: %s"),
            *GetNameSafe(this),
            *StatusTag.ToString());
        return;
    }

    UMyStatusEffectSlot* NewSlot = CreateWidget<UMyStatusEffectSlot>(GetOwningPlayer(), StatusEffectSlotClass);
    if (!NewSlot)
    {
        NewSlot = CreateWidget<UMyStatusEffectSlot>(this, StatusEffectSlotClass);
    }

    if (!NewSlot)
    {
        return;
    }

    FMyStatusEffectDefinition Definition;
    ResolveStatusEffectDefinition(StatusTag, Definition);

    NewSlot->SetStatusEffectDefinition(StatusTag, Definition.DisplayName, Definition.Icon);
    NewSlot->BindToAbilitySystemComponent(BoundAbilitySystemComponent);

    ActiveStatusEffectSlots.Add(StatusTag, NewSlot);
    StatusApplicationOrder.AddUnique(StatusTag);
}

void UMyStatusEffectPanelWidget::RemoveStatusEffectSlot(FGameplayTag StatusTag)
{
    TObjectPtr<UMyStatusEffectSlot>* FoundSlot = ActiveStatusEffectSlots.Find(StatusTag);
    if (!FoundSlot)
    {
        return;
    }

    if (UMyStatusEffectSlot* StatusSlot = FoundSlot->Get())
    {
        StatusSlot->UnbindFromAbilitySystemComponent();
        StatusSlot->RemoveFromParent();
    }

    ActiveStatusEffectSlots.Remove(StatusTag);
    StatusApplicationOrder.Remove(StatusTag);
}

void UMyStatusEffectPanelWidget::ClearStatusEffectSlots()
{
    for (const TPair<FGameplayTag, TObjectPtr<UMyStatusEffectSlot>>& Pair : ActiveStatusEffectSlots)
    {
        if (UMyStatusEffectSlot* StatusSlot = Pair.Value.Get())
        {
            StatusSlot->UnbindFromAbilitySystemComponent();
            StatusSlot->RemoveFromParent();
        }
    }

    ActiveStatusEffectSlots.Reset();
    StatusApplicationOrder.Reset();

    if (HB_StatusEffects)
    {
        HB_StatusEffects->ClearChildren();
    }
}

////////////////////////////
//! \author 장효제
//! \brief 활성 상태효과 슬롯을 제거하지 않고 CC, Debuff, Buff 순서로 배치하며 초과 개수를 표시한다.
void UMyStatusEffectPanelWidget::RefreshPanelLayout()
{
    if (!HB_StatusEffects)
    {
        return;
    }

    TArray<FGameplayTag> OrderedTags;
    ActiveStatusEffectSlots.GetKeys(OrderedTags);
    OrderedTags.Sort([this](const FGameplayTag& Left, const FGameplayTag& Right)
    {
        const int32 LeftCategory = GetStatusEffectCategoryPriority(Left);
        const int32 RightCategory = GetStatusEffectCategoryPriority(Right);
        if (LeftCategory != RightCategory)
        {
            return LeftCategory < RightCategory;
        }

        const int32 LeftOrder = StatusApplicationOrder.IndexOfByKey(Left);
        const int32 RightOrder = StatusApplicationOrder.IndexOfByKey(Right);
        return LeftOrder < RightOrder;
    });

    const int32 SafeMaxVisible = FMath::Max(2, MaxVisibleStatusEffects);
    const bool bHasOverflow = OrderedTags.Num() > SafeMaxVisible;
    const bool bCanShowOverflow = bHasOverflow && TXT_Overflow;
    const int32 VisibleStatusCount = bCanShowOverflow
        ? SafeMaxVisible - 1
        : FMath::Min(SafeMaxVisible, OrderedTags.Num());

    for (int32 Index = 0; Index < OrderedTags.Num(); ++Index)
    {
        UMyStatusEffectSlot* StatusSlot = ActiveStatusEffectSlots.FindRef(OrderedTags[Index]);
        if (!StatusSlot)
        {
            continue;
        }

        if (StatusSlot->GetParent() != HB_StatusEffects)
        {
            HB_StatusEffects->AddChildToHorizontalBox(StatusSlot);
        }

        StatusSlot->RefreshStatusEffect();
        StatusSlot->SetVisibility(Index < VisibleStatusCount
            ? ESlateVisibility::SelfHitTestInvisible
            : ESlateVisibility::Collapsed);

        HB_StatusEffects->ShiftChild(Index, StatusSlot);
        if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(StatusSlot->Slot))
        {
            BoxSlot->SetPadding(StatusSlotPadding);
            BoxSlot->SetHorizontalAlignment(HAlign_Center);
            BoxSlot->SetVerticalAlignment(VAlign_Center);
        }
    }

    if (TXT_Overflow)
    {
        if (bCanShowOverflow)
        {
            TXT_Overflow->SetText(FText::Format(
                NSLOCTEXT("StatusEffect", "OverflowCount", "+{0}"),
                FText::AsNumber(OrderedTags.Num() - VisibleStatusCount)));
            TXT_Overflow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

            if (TXT_Overflow->GetParent() != HB_StatusEffects)
            {
                HB_StatusEffects->AddChildToHorizontalBox(TXT_Overflow);
            }

            HB_StatusEffects->ShiftChild(VisibleStatusCount, TXT_Overflow);
            if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(TXT_Overflow->Slot))
            {
                BoxSlot->SetPadding(StatusSlotPadding);
                BoxSlot->SetHorizontalAlignment(HAlign_Center);
                BoxSlot->SetVerticalAlignment(VAlign_Center);
            }
        }
        else
        {
            TXT_Overflow->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

bool UMyStatusEffectPanelWidget::ResolveStatusEffectDefinition(FGameplayTag StatusTag, FMyStatusEffectDefinition& OutDefinition) const
{
    OutDefinition = FMyStatusEffectDefinition();
    OutDefinition.StatusTag = StatusTag;
    OutDefinition.DisplayName = BuildFallbackDisplayName(StatusTag);

    for (const FMyStatusEffectDefinition& Definition : StatusEffectDefinitions)
    {
        if (Definition.StatusTag == StatusTag)
        {
            OutDefinition = Definition;
            if (OutDefinition.DisplayName.IsEmpty())
            {
                OutDefinition.DisplayName = BuildFallbackDisplayName(StatusTag);
            }
            return true;
        }
    }

    if (StatusEffectDefinitionTable)
    {
        static const FString ContextString(TEXT("StatusEffectDefinition"));
        TArray<FMyStatusEffectDefinition*> Rows;
        StatusEffectDefinitionTable->GetAllRows(ContextString, Rows);

        for (const FMyStatusEffectDefinition* Row : Rows)
        {
            if (Row && Row->StatusTag == StatusTag)
            {
                OutDefinition = *Row;
                if (OutDefinition.DisplayName.IsEmpty())
                {
                    OutDefinition.DisplayName = BuildFallbackDisplayName(StatusTag);
                }
                return true;
            }
        }
    }

    return false;
}

bool UMyStatusEffectPanelWidget::IsStatusEffectTag(FGameplayTag Tag) const
{
    if (!Tag.IsValid())
    {
        return false;
    }

    const FGameplayTag BuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff"), false);
    const FGameplayTag CrowdControlRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.CC"), false);
    const FGameplayTag DebuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff"), false);

    return (BuffRoot.IsValid() && Tag != BuffRoot && Tag.MatchesTag(BuffRoot))
        || (CrowdControlRoot.IsValid() && Tag != CrowdControlRoot && Tag.MatchesTag(CrowdControlRoot))
        || (DebuffRoot.IsValid() && Tag != DebuffRoot && Tag.MatchesTag(DebuffRoot));
}

////////////////////////////
//! \author 장효제
//! \brief 상태효과 카테고리의 HUD 정렬 우선순위를 반환한다.
//! \param Tag 분류할 상태효과 태그
//! \return CC는 0, Debuff는 1, Buff는 2, 그 외는 3
int32 UMyStatusEffectPanelWidget::GetStatusEffectCategoryPriority(FGameplayTag Tag) const
{
    const FGameplayTag CrowdControlRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.CC"), false);
    const FGameplayTag DebuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff"), false);
    const FGameplayTag BuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff"), false);

    if (CrowdControlRoot.IsValid() && Tag.MatchesTag(CrowdControlRoot))
    {
        return 0;
    }
    if (DebuffRoot.IsValid() && Tag.MatchesTag(DebuffRoot))
    {
        return 1;
    }
    if (BuffRoot.IsValid() && Tag.MatchesTag(BuffRoot))
    {
        return 2;
    }
    return 3;
}

FText UMyStatusEffectPanelWidget::BuildFallbackDisplayName(FGameplayTag StatusTag) const
{
    FString TagString = StatusTag.ToString();
    FString LeafName;
    if (TagString.Split(TEXT("."), nullptr, &LeafName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        return FText::FromString(LeafName);
    }

    return FText::FromString(TagString);
}
