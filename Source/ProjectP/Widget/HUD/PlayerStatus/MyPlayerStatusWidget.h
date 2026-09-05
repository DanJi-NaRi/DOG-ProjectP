#pragma once

#include "CommonUserWidget.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "MyPlayerStatusWidget.generated.h"

class AMyPlayerState;
class UAbilitySystemComponent;
class UMyExpBarWidget;
class UMyHPBarWidget;
class UMyItemSlotPanelWidget;
class UMyPlayerProfileWidget;
class UMySkillSlotPanelWidget;
class UMyStatusEffectPanelWidget;
class UTexture2D;

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyPlayerStatusWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetPlayerStatus(
        float CurrentHP,
        float MaxHP,
        float CurrentExp,
        float RequiredExp,
        int32 Level,
        UTexture2D* CharacterIcon,
        bool bAnimate = true);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    bool BindToOwningPlayerAttributes();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativePreConstruct() override;

private:
    void ApplyPreviewStatus();
    void ScheduleAttributeBindRetry();
    void CancelAttributeBindRetry();
    void HandleAttributeBindRetry();
    void UnbindFromAttributes();
    void RefreshHPFromAttribute(bool bAnimate);
    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
    void HandleShieldChanged(const FOnAttributeChangeData& Data);
    void BindToLevelData(AMyPlayerState* MyPlayerState);
    void UnbindFromLevelData();
    void RefreshLevelDataFromState(bool bAnimate);
    void HandleLevelDataChanged();

private:
    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentHP = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewMaxHP = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentExp = 120.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewRequiredExp = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1"))
    int32 PreviewLevel = 1;

    UPROPERTY(EditAnywhere, Category = "Preview")
    TObjectPtr<UTexture2D> PreviewCharacterIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyHPBarWidget> HPBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyExpBarWidget> ExpBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyPlayerProfileWidget> PlayerProfile;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMySkillSlotPanelWidget> SkillSlotPanel;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyItemSlotPanelWidget> ItemSlotPanel;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UMyStatusEffectPanelWidget> StatusEffectPanel;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

    //! 레벨/경험치 변경 알림을 구독 중인 PlayerState. 소유 폰의 PlayerState를 캐시한다.
    UPROPERTY(Transient)
    TObjectPtr<AMyPlayerState> BoundPlayerState;

    FDelegateHandle HealthChangedHandle;
    FDelegateHandle MaxHealthChangedHandle;
    FDelegateHandle ShieldChangedHandle;
    FDelegateHandle LevelDataChangedHandle;

    FTimerHandle AttributeBindRetryTimerHandle;
    int32 AttributeBindRetryCount = 0;
};
