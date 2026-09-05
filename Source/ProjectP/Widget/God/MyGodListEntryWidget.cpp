////////////////////////////
//! \page MyGodListEntryWidget.cpp
//! \brief 신 목록 항목의 표시와 선택 이벤트를 처리하는 구현 파일이다.
#include "MyGodListEntryWidget.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
	UTexture2D* LoadEntryPortrait(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage)
	{
		const TSoftObjectPtr<UTexture2D>* Portrait = nullptr;
		switch (PortraitStage)
		{
		case EMyGodPortraitStage::Silhouette: Portrait = &Presentation.SilhouettePortrait; break;
		case EMyGodPortraitStage::Emote: Portrait = &Presentation.EmotePortrait; break;
		case EMyGodPortraitStage::Full: Portrait = &Presentation.FullPortrait; break;
		default: break;
		}
		if (Portrait && !Portrait->IsNull()) return Portrait->LoadSynchronous();
		return Presentation.Icon.IsNull() ? nullptr : Presentation.Icon.LoadSynchronous();
	}
}

void UMyGodListEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BTN_Select) BTN_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
	ApplyPresentation();
}

void UMyGodListEntryWidget::NativeDestruct()
{
	if (BTN_Select) BTN_Select->OnClicked.RemoveDynamic(this, &ThisClass::HandleSelectClicked);
	Super::NativeDestruct();
}

//! \author 장효제
//! \brief DataTable 행을 목록 항목에 보관하고 이름과 초상화를 갱신한다.
void UMyGodListEntryWidget::InitializeEntry(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage)
{
	PresentationData = Presentation;
	GodTag = Presentation.GodTag;
	CurrentPortraitStage = PortraitStage;
	ApplyPresentation();
}

void UMyGodListEntryWidget::SetSelected(bool bSelected)
{
	if (IMG_SelectedFrame) IMG_SelectedFrame->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (TXT_GodName) TXT_GodName->SetColorAndOpacity(FSlateColor(bSelected ? PresentationData.GetGodLinearColor() : UnselectedTextColor));
}

void UMyGodListEntryWidget::HandleSelectClicked()
{
	if (GodTag.IsValid()) OnGodEntrySelected.Broadcast(GodTag);
}

void UMyGodListEntryWidget::ApplyPresentation()
{
	if (TXT_GodName) TXT_GodName->SetText(PresentationData.DisplayName);
	if (IMG_GodPortrait) IMG_GodPortrait->SetBrushFromTexture(LoadEntryPortrait(PresentationData, CurrentPortraitStage), true);
}
