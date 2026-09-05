#include "MyPlayerProfileWidget.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UMyPlayerProfileWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (IsDesignTime())
    {
        SetProfile(PreviewLevel, PreviewCharacterIcon);
    }
    else
    {
        RefreshLevelText();
    }
}

void UMyPlayerProfileWidget::SetProfile(int32 Level, UTexture2D* CharacterIcon)
{
    SetLevel(Level);
    SetCharacterIcon(CharacterIcon);
}

void UMyPlayerProfileWidget::SetLevel(int32 Level)
{
    CachedLevel = FMath::Max(Level, 1);
    RefreshLevelText();
}

void UMyPlayerProfileWidget::SetCharacterIcon(UTexture2D* CharacterIcon)
{
    if (Image_CharacterIcon && CharacterIcon)
    {
        Image_CharacterIcon->SetBrushFromTexture(CharacterIcon, true);
    }
}

void UMyPlayerProfileWidget::RefreshLevelText()
{
    if (Text_Level)
    {
        Text_Level->SetText(FText::Format(NSLOCTEXT("ProjectP", "LevelFormat", "Lv. {0}"), FText::AsNumber(CachedLevel)));
    }
}
