#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "MySkillSlotPanelWidget.generated.h"

class UAbilitySystemComponent;
class UMySkillControlComponent;
class UMySkillDefinitionDataAsset;
class UMySkillSlotWidget;
struct FMySkillSlotSpec;

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMySkillSlotPanelWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlotPanel")
    void RefreshSkillSlots();

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlotPanel")
    bool BindToOwningPlayerSkillCooldowns();

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlotPanel")
    void StartCooldownByInputTag(FGameplayTag InputTag);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlotPanel")
    void StartCooldownByCooldownTag(FGameplayTag CooldownTag);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativePreConstruct() override;

private:
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

    bool RefreshSkillSlotsFromOwningPlayer();
    UMySkillControlComponent* GetOwningSkillControlComponent() const;
    void InitializeSlotFromSkillDefinition(UMySkillSlotWidget* SlotWidget, const FMySkillSlotSpec& SkillSlotSpec, FText InputKeyText);
    void InitializeEmptySlot(UMySkillSlotWidget* SlotWidget, FText InputKeyText);
    void BindCooldownTag(UMySkillSlotWidget* SlotWidget);
    void UnbindFromSkillCooldowns();
    void ScheduleCooldownBindRetry();
    void CancelCooldownBindRetry();
    void HandleCooldownBindRetry();
    void HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount);

private:
    //UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    //TObjectPtr<UMySkillSlotWidget> Slot_Basic;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMySkillSlotWidget> Slot_Q;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMySkillSlotWidget> Slot_E;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMySkillSlotWidget> Slot_R;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMySkillSlotWidget> Slot_C;

    //UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    //TObjectPtr<UMySkillSlotWidget> Slot_Move;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

    TMap<FGameplayTag, TObjectPtr<UMySkillSlotWidget>> SlotsByInputTag;
    TMap<FGameplayTag, TObjectPtr<UMySkillSlotWidget>> SlotsByCooldownTag;
    TMap<FGameplayTag, FDelegateHandle> CooldownTagDelegateHandles;

    FTimerHandle CooldownBindRetryTimerHandle;
    int32 CooldownBindRetryCount = 0;
};
