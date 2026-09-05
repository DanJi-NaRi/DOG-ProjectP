#pragma once

#include "CommonUserWidget.h"
#include "MyPlayerProfileWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyPlayerProfileWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetProfile(int32 Level, UTexture2D* CharacterIcon);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetLevel(int32 Level);

    UFUNCTION(BlueprintCallable, Category = "UI|PlayerStatus")
    void SetCharacterIcon(UTexture2D* CharacterIcon);

protected:
    virtual void NativePreConstruct() override;

private:
    void RefreshLevelText();

private:
    UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1"))
    int32 PreviewLevel = 1;

    UPROPERTY(EditAnywhere, Category = "Preview")
    TObjectPtr<UTexture2D> PreviewCharacterIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> Image_CharacterIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> Image_Frame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UCommonTextBlock> Text_Level;

    int32 CachedLevel = 1;
};
