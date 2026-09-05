#pragma once

#include "CommonUserWidget.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStatusEffectPanelWidget.generated.h"

class UAbilitySystemComponent;
class UCommonTextBlock;
class UDataTable;
class UHorizontalBox;
class UMyStatusEffectSlot;
class UTexture2D;

USTRUCT(BlueprintType)
struct FMyStatusEffectDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect")
    FGameplayTag StatusTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect")
    TObjectPtr<UTexture2D> Icon;
};

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyStatusEffectPanelWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void BindToAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent);

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void UnbindFromAbilitySystemComponent();

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void RefreshStatusEffects();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void HandleOwnedTagChanged(const FGameplayTag ChangedTag, int32 NewCount);
    void AddOrRefreshStatusEffectSlot(FGameplayTag StatusTag);
    void RemoveStatusEffectSlot(FGameplayTag StatusTag);
    void ClearStatusEffectSlots();
    void RefreshPanelLayout();
    bool ResolveStatusEffectDefinition(FGameplayTag StatusTag, FMyStatusEffectDefinition& OutDefinition) const;
    bool IsStatusEffectTag(FGameplayTag Tag) const;
    int32 GetStatusEffectCategoryPriority(FGameplayTag Tag) const;
    FText BuildFallbackDisplayName(FGameplayTag StatusTag) const;

private:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UHorizontalBox> HB_StatusEffects;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> TXT_Overflow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UMyStatusEffectSlot> StatusEffectSlotClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    TArray<FMyStatusEffectDefinition> StatusEffectDefinitions;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDataTable> StatusEffectDefinitionTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    FMargin StatusSlotPadding = FMargin(2.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ClampMin = "2", AllowPrivateAccess = "true"))
    int32 MaxVisibleStatusEffects = 6;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

    UPROPERTY(Transient)
    TMap<FGameplayTag, TObjectPtr<UMyStatusEffectSlot>> ActiveStatusEffectSlots;

    TArray<FGameplayTag> StatusApplicationOrder;
    FDelegateHandle GenericTagChangedHandle;
};
