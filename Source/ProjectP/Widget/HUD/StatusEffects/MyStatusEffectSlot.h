#pragma once

#include "CommonUserWidget.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "MyStatusEffectSlot.generated.h"

class UAbilitySystemComponent;
class UCommonTextBlock;
class UImage;
class UTexture2D;

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyStatusEffectSlot : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void SetStatusEffectDefinition(FGameplayTag InStatusTag, FText InDisplayName, UTexture2D* InIcon);

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void BindToAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent);

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void UnbindFromAbilitySystemComponent();

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void RefreshStatusEffect();

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void SetStatusEffectActive(float RemainingTime, float Duration, int32 StackCount);

    UFUNCTION(BlueprintCallable, Category = "UI|StatusEffect")
    void ClearStatusEffect();

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void HandleStatusTagChanged(const FGameplayTag ChangedTag, int32 NewCount);
    void HandleStackCountChanged(FActiveGameplayEffectHandle EffectHandle, int32 NewStackCount, int32 PreviousStackCount);
    void HandleDurationTick();
    void HandleCategoryPulseTick();
    bool QueryStatusEffectState(
        float& OutRemainingTime,
        float& OutDuration,
        int32& OutStackCount,
        TArray<FActiveGameplayEffectHandle>* OutEffectHandles = nullptr) const;
    void RefreshStackChangeBindings(const TArray<FActiveGameplayEffectHandle>& EffectHandles);
    void ClearStackChangeBindings();
    void ApplyDefinitionDisplay();
    void ApplyCategoryDisplay();
    void ApplyActiveDisplay(float RemainingTime, float Duration, int32 StackCount);
    void ApplyInactiveDisplay();
    void StartDurationTimerIfNeeded(float RemainingTime, float Duration);
    void StopDurationTimer();
    void StartCategoryPulseIfNeeded();
    void StopCategoryPulse();

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    FGameplayTag StatusTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect", meta = (AllowPrivateAccess = "true"))
    bool bCollapseWhenInactive = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StatusEffect", meta = (ClampMin = "0.01", AllowPrivateAccess = "true"))
    float DurationTickInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor BuffCategoryColor = FLinearColor(0.10f, 0.80f, 0.75f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor DebuffCategoryColor = FLinearColor(0.62f, 0.28f, 0.85f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor CrowdControlCategoryColor = FLinearColor(1.0f, 0.25f, 0.08f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect|Style", meta = (AllowPrivateAccess = "true"))
    FLinearColor DurationOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect|Style", meta = (ClampMin = "0.2", AllowPrivateAccess = "true"))
    float CrowdControlPulsePeriod = 1.2f;

    UPROPERTY(EditAnywhere, Category = "Preview")
    bool bPreviewActive = true;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewRemainingTime = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewDuration = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1"))
    int32 PreviewStackCount = 1;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Icon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_CategoryFrame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_CategoryBadge;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_DurationOverlay;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> TXT_CategoryBadge;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> TXT_Duration;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> TXT_Stack;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

    FDelegateHandle StatusTagChangedHandle;
    TMap<FActiveGameplayEffectHandle, FDelegateHandle> StackChangeDelegateHandles;
    FTimerHandle DurationTickTimerHandle;
    FTimerHandle CategoryPulseTimerHandle;
    bool bIsCrowdControl = false;
};
