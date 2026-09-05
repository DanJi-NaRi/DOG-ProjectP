////////////////////////////
//! \page MyItemTooltipWidget.cpp
//! \brief 아이템 상세 정보 툴팁 위젯 구현 파일이다.
#include "Widget/Inventory/MyItemTooltipWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

////////////////////////////
//! \author 준혁
//! \brief 초기 상태를 빈 상태(아이템 미선택)로 둔다. 툴팁 박스는 항상 표시된다.
void UMyItemTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ShowEmpty();
}

////////////////////////////
//! \author 준혁
//! \brief 툴팁에 아이템 이름/설명/타입/개수/아이콘을 채우고 표시한다.
//! \param InItemId 아이템 ID
//! \param InItemData 아이템 정적 데이터
//! \param InCount 보유 개수
void UMyItemTooltipWidget::ShowItem(FName InItemId, const FMyItemData& InItemData, int32 InCount)
{
	ShownItemId = InItemId;

	if (TXT_Name)
	{
		TXT_Name->SetText(InItemData.DisplayName);
	}

	if (TXT_Description)
	{
		TXT_Description->SetText(InItemData.Description);
	}

	if (TXT_Type)
	{
		TXT_Type->SetText(GetItemTypeDisplayText(InItemData.ItemType));
	}

	if (TXT_Count)
	{
		TXT_Count->SetText(FText::Format(
			NSLOCTEXT("MyItem", "Tooltip_OwnedCount", "보유 수량 {0}"), FText::AsNumber(InCount)));
	}

	if (IMG_Icon)
	{
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
}

////////////////////////////
//! \author 준혁
//! \brief 선택 아이템을 초기화하고 빈 상태 문구만 남긴다. 박스 자체는 계속 표시된다.
void UMyItemTooltipWidget::ShowEmpty()
{
	ShownItemId = NAME_None;

	if (TXT_Name)
	{
		TXT_Name->SetText(EmptyStateText);
	}

	if (TXT_Description)
	{
		TXT_Description->SetText(FText::GetEmpty());
	}

	if (TXT_Type)
	{
		TXT_Type->SetText(FText::GetEmpty());
	}

	if (TXT_Count)
	{
		TXT_Count->SetText(FText::GetEmpty());
	}

	if (IMG_Icon)
	{
		IMG_Icon->SetVisibility(ESlateVisibility::Hidden);
	}

	if (TXT_Price)
	{
		TXT_Price->SetText(FText::GetEmpty());
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 상점에서 선택한 아이템의 가격과 구매 수량을 곱한 총 가격을 툴팁에 표시하는 함수
// InTotalPrice : 상점 위젯에서 계산한 총 구매 가격
void UMyItemTooltipWidget::SetTotalPrice(int64 InTotalPrice)
{
	if (TXT_Price)
	{
		TXT_Price->SetText(FText::AsNumber(InTotalPrice));
	}
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 타입 enum을 표시용 텍스트로 변환한다.
//! \param ItemType 아이템 타입
//! \return 표시용 텍스트
FText UMyItemTooltipWidget::GetItemTypeDisplayText(EMyItemType ItemType)
{
	switch (ItemType)
	{
	case EMyItemType::Consumable:
		return NSLOCTEXT("MyItem", "ItemType_Consumable", "소모품");
	case EMyItemType::Material:
		return NSLOCTEXT("MyItem", "ItemType_Material", "재료");
	case EMyItemType::Quest:
		return NSLOCTEXT("MyItem", "ItemType_Quest", "퀘스트");
	default:
		return NSLOCTEXT("MyItem", "ItemType_Etc", "기타");
	}
}
