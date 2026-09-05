////////////////////////////
//! \page MyGodDetailPanelWidget.cpp
//! \brief 선택된 신의 정적 정보와 호감도 표시를 갱신하는 패널 구현 파일이다.
#include "MyGodDetailPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace
{
	UTexture2D* LoadPortrait(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage)
	{
		const TSoftObjectPtr<UTexture2D>* Portrait = nullptr;
		switch (PortraitStage)
		{
		case EMyGodPortraitStage::Silhouette: Portrait = &Presentation.SilhouettePortrait; break;
		case EMyGodPortraitStage::Emote: Portrait = &Presentation.EmotePortrait; break;
		case EMyGodPortraitStage::Full: Portrait = &Presentation.FullPortrait; break;
		default: break;
		}

		if (Portrait && !Portrait->IsNull())
		{
			return Portrait->LoadSynchronous();
		}
		return Presentation.Icon.IsNull() ? nullptr : Presentation.Icon.LoadSynchronous();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 선택된 신의 이름, 소개와 단계별 초상화를 갱신한다.
void UMyGodDetailPanelWidget::SetGodPresentation(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage)
{
	if (TXT_GodName)
	{
		TXT_GodName->SetText(Presentation.DisplayName);
		TXT_GodName->SetColorAndOpacity(FSlateColor(Presentation.GetGodLinearColor()));
	}
	if (TXT_Introduction) TXT_Introduction->SetText(Presentation.Introduction);
	if (IMG_GodPortrait) IMG_GodPortrait->SetBrushFromTexture(LoadPortrait(Presentation, PortraitStage), true);
}

//! \author 장효제
//! \brief 외부 호감도 소유자가 전달한 레벨과 진행 수치를 텍스트와 ProgressBar에 적용한다.
void UMyGodDetailPanelWidget::SetAffinityDisplay(int32 Level, int32 CurrentValue, int32 RequiredValue)
{
	const int32 SafeCurrentValue = FMath::Max(CurrentValue, 0);
	const int32 SafeRequiredValue = FMath::Max(RequiredValue, 0);
	if (PB_Affinity)
	{
		const float Percent = SafeRequiredValue > 0 ? static_cast<float>(SafeCurrentValue) / static_cast<float>(SafeRequiredValue) : 0.0f;
		PB_Affinity->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
	if (TXT_AffinityLevel)
	{
		TXT_AffinityLevel->SetText(FText::Format(NSLOCTEXT("ProjectPGodPage", "AffinityLevelFormat", "Lv. {0}"), FText::AsNumber(FMath::Max(Level, 0))));
	}
	if (TXT_AffinityValue)
	{
		TXT_AffinityValue->SetText(FText::Format(NSLOCTEXT("ProjectPGodPage", "AffinityValueFormat", "{0}/{1}"), FText::AsNumber(SafeCurrentValue), FText::AsNumber(SafeRequiredValue)));
	}
}

void UMyGodDetailPanelWidget::RebuildBlessingIds(const TArray<FName>& BlessingIds)
{
	if (!HB_BlessingSlots || !WidgetTree) return;
	HB_BlessingSlots->ClearChildren();
	for (int32 Index = 0; Index < BlessingIds.Num(); ++Index)
	{
		UCommonTextBlock* BlessingText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), *FString::Printf(TEXT("TXT_Blessing_%d"), Index));
		BlessingText->SetText(FText::FromName(BlessingIds[Index]));
		if (UHorizontalBoxSlot* BlessingSlot = HB_BlessingSlots->AddChildToHorizontalBox(BlessingText))
		{
			BlessingSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}
	}
}

void UMyGodDetailPanelWidget::RebuildFavoriteActions(const TArray<FText>& FavoriteActions)
{
	if (!VB_FavoriteActions || !WidgetTree) return;
	VB_FavoriteActions->ClearChildren();
	for (int32 Index = 0; Index < FavoriteActions.Num(); ++Index)
	{
		UCommonTextBlock* ActionText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), *FString::Printf(TEXT("TXT_FavoriteAction_%d"), Index));
		ActionText->SetText(FavoriteActions[Index]);
		ActionText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* ActionSlot = VB_FavoriteActions->AddChildToVerticalBox(ActionText))
		{
			ActionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}
}
