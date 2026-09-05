////////////////////////////
//! \page MyShopSlotWidget.cpp
//! \brief 상점 판매 아이템 슬롯 위젯 구현 파일이다.
#include "Widget/Shop/MyShopSlotWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼 클릭 델리게이트를 바인딩한다.
void UMyShopSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Slot)
	{
		if (!bHasCachedDefaultBrushes)
		{
			DefaultNormalBrush = BTN_Slot->GetStyle().Normal;
			DefaultHoveredBrush = BTN_Slot->GetStyle().Hovered;
			bHasCachedDefaultBrushes = true;
		}

		ApplySelectionStyle();
		BTN_Slot->OnClicked.AddUniqueDynamic(this, &UMyShopSlotWidget::HandleSlotButtonClicked);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼 클릭 델리게이트를 해제한다.
void UMyShopSlotWidget::NativeDestruct()
{
	if (BTN_Slot)
	{
		BTN_Slot->OnClicked.RemoveDynamic(this, &UMyShopSlotWidget::HandleSlotButtonClicked);
	}

	Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯에 표시할 판매 아이템의 아이콘/이름/가격을 설정한다.
//! \param InItemId 아이템 ID
//! \param InItemData 아이템 정적 데이터
void UMyShopSlotWidget::SetItem(FName InItemId, const FMyItemData& InItemData)
{
	ItemId = InItemId;

	if (IMG_Icon)
	{
		// 소량의 아이콘 텍스처만 다루므로 동기 로드로 단순화한다 (인벤토리 슬롯과 동일)
		if (UTexture2D* IconTexture = InItemData.Icon.LoadSynchronous())
		{
			IMG_Icon->SetBrushFromTexture(IconTexture);
			IMG_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IMG_Icon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (TXT_Name)
	{
		TXT_Name->SetText(InItemData.DisplayName);
		TXT_Name->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (TXT_Price)
	{
		TXT_Price->SetText(FText::AsNumber(InItemData.BuyPrice));
		TXT_Price->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (IMG_Price)
	{
		IMG_Price->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 빈 슬롯 상태로 만든다. 고정 그리드의 빈 칸 표시에 사용한다.
void UMyShopSlotWidget::SetEmpty()
{
	ItemId = NAME_None;

	if (IMG_Icon)
	{
		IMG_Icon->SetVisibility(ESlateVisibility::Hidden);
	}

	if (TXT_Name)
	{
		TXT_Name->SetVisibility(ESlateVisibility::Hidden);
	}

	if (TXT_Price)
	{
		TXT_Price->SetVisibility(ESlateVisibility::Hidden);
	}

	if (IMG_Price)
	{
		IMG_Price->SetVisibility(ESlateVisibility::Hidden);
	}

	SetSelected(false);
}

////////////////////////////
//! \author 준혁
//! \brief 선택 상태를 갱신하고, 바뀐 경우에만 BP 연출 이벤트를 호출한다.
//! \param bInSelected 선택 여부
void UMyShopSlotWidget::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	ApplySelectionStyle();
	BP_OnSelectionChanged(bSelected);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 선택된 상점 슬롯의 BTN_Slot Normal/Hovered 이미지를 Pressed(Clicked) 이미지로 유지하고 선택 해제 시 원래 이미지로 복원하는 함수
void UMyShopSlotWidget::ApplySelectionStyle()
{
	if (!BTN_Slot || !bHasCachedDefaultBrushes)
	{
		return;
	}

	FButtonStyle UpdatedStyle = BTN_Slot->GetStyle();
	UpdatedStyle.Normal = bSelected ? UpdatedStyle.Pressed : DefaultNormalBrush;
	UpdatedStyle.Hovered = bSelected ? UpdatedStyle.Pressed : DefaultHoveredBrush;
	BTN_Slot->SetStyle(UpdatedStyle);
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼 클릭 시 아이템 ID를 브로드캐스트한다.
void UMyShopSlotWidget::HandleSlotButtonClicked()
{
	if (!ItemId.IsNone())
	{
		OnSlotClicked.Broadcast(ItemId);
	}
}
