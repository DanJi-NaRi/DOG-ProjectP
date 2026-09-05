#pragma once

#include "CommonUserWidget.h"
#include "GameplayEffectTypes.h"
#include "Player/Types/PlayerLifeTypes.h"
#include "MyTeamMemberStatusWidget.generated.h"

class AMyPlayerState;
class APlayerState;
class APawn;
class UAbilitySystemComponent;
class UCommonTextBlock;
class UImage;
class UMyExpBarWidget;
class UMyHPBarWidget;
class UTexture2D;

USTRUCT(BlueprintType)
struct PROJECTP_API FTeamMemberProfileTextureSet
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|TeamMember")
    TObjectPtr<UTexture2D> AliveTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|TeamMember")
    TObjectPtr<UTexture2D> DownTexture;
};

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyTeamMemberStatusWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void InitializeCharacterSlot(int32 InCharacterId);

    UFUNCTION(BlueprintCallable, Category = "UI|TeamMember")
    bool BindToPlayerState(AMyPlayerState* InPlayerState);

    UFUNCTION(BlueprintCallable, Category = "UI|TeamMember")
    void MarkDisconnected();

    UFUNCTION(BlueprintPure, Category = "UI|TeamMember")
    int32 GetTeamMemberUserIndex() const;

    UFUNCTION(BlueprintPure, Category = "UI|TeamMember")
    int32 GetTeamMemberCharacterId() const;

    UFUNCTION(BlueprintPure, Category = "UI|TeamMember")
    bool IsBoundToPlayerState(const AMyPlayerState* InPlayerState) const;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void ApplyPreviewStatus();
    void ApplyBarStyle();
    void CacheIdentityFromPlayerState();
    void BindToAbilitySystemComponent();
    void UnbindFromPlayerState();
    void UnbindFromAbilitySystemComponent();
    void RefreshAll(bool bAnimate);
    void RefreshPlayerInfo();
    void RefreshAttributes(bool bAnimate);
    void RefreshLevelAndExp(bool bAnimate);
    void RefreshLifePresentation();
    UTexture2D* ResolveProfileTexture(bool bDown) const;
    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
    void HandleShieldChanged(const FOnAttributeChangeData& Data);
    void HandleLevelDataChanged();
    void HandleLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState);

    UFUNCTION()
    void HandlePawnSet(APlayerState* PlayerState, APawn* NewPawn, APawn* OldPawn);

private:
    UPROPERTY(EditAnywhere, Category = "Preview")
    FString PreviewUsername = TEXT("Username");

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1"))
    int32 PreviewLevel = 1;

    UPROPERTY(EditAnywhere, Category = "Preview")
    int32 PreviewCharacterId = 100;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentHP = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewMaxHP = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewShield = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0"))
    float PreviewCurrentExp = 40.0f;

    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
    float PreviewRequiredExp = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Preview")
    bool bPreviewDown = false;

    UPROPERTY(EditDefaultsOnly, Category = "Style")
    TMap<int32, FTeamMemberProfileTextureSet> ProfileTexturesByCharacterId;

    UPROPERTY(EditDefaultsOnly, Category = "Style|HP")
    TObjectPtr<UTexture2D> TeamNormalHPTexture;

    UPROPERTY(EditDefaultsOnly, Category = "Style|HP")
    TObjectPtr<UTexture2D> TeamDangerHPTexture;

    UPROPERTY(EditDefaultsOnly, Category = "Style|HP")
    TObjectPtr<UTexture2D> TeamHPBackgroundTexture;

    UPROPERTY(EditDefaultsOnly, Category = "Style|HP")
    TObjectPtr<UTexture2D> TeamShieldTexture;

    UPROPERTY(EditDefaultsOnly, Category = "Style|Exp")
    TObjectPtr<UTexture2D> TeamExpFillTexture;

    UPROPERTY(EditDefaultsOnly, Category = "Style|Exp")
    TObjectPtr<UTexture2D> TeamExpBackgroundTexture;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Profile;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> Text_PlayerInfo;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyHPBarWidget> HPBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyExpBarWidget> ExpBar;

    UPROPERTY(Transient)
    TObjectPtr<AMyPlayerState> BoundPlayerState;

    UPROPERTY(Transient)
    TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

    int32 CachedUserIndex = -1;
    int32 CachedCharacterId = -1;
    int32 CachedLevel = 1;
    FString CachedUsername;
    bool bDisconnected = true;
    bool bShowingRuntimePreview = false;

    FDelegateHandle HealthChangedHandle;
    FDelegateHandle MaxHealthChangedHandle;
    FDelegateHandle ShieldChangedHandle;
    FDelegateHandle LevelDataChangedHandle;
    FDelegateHandle LifeStateChangedHandle;
};
