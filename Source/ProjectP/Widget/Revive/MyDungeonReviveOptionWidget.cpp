////////////////////////////
//! \page MyDungeonReviveOptionWidget.cpp
//! \brief 부활 옵션 카드 위젯 구현 파일이다.
#include "Widget/Revive/MyDungeonReviveOptionWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "God/MyGodPresentationTypes.h"

////////////////////////////
//! \author 장효제
//! \brief 카드 버튼 클릭 델리게이트를 바인딩한다.
//! \param 없음
//! \return 없음
void UMyDungeonReviveOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Select)
	{
		BTN_Select->OnClicked.AddUniqueDynamic(this, &UMyDungeonReviveOptionWidget::HandleSelectButtonClicked);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 카드 버튼 클릭 델리게이트를 해제한다.
//! \param 없음
//! \return 없음
void UMyDungeonReviveOptionWidget::NativeDestruct()
{
	if (BTN_Select)
	{
		BTN_Select->OnClicked.RemoveDynamic(this, &UMyDungeonReviveOptionWidget::HandleSelectButtonClicked);
	}

	Super::NativeDestruct();
}

////////////////////////////
//! \author 장효제
//! \brief 부활 옵션의 초상화, 이름, 설명, 비용, 부활 체력, 대기시간을 카드에 반영한다.
//! \param InOption 표시할 부활 옵션 데이터
//! \return 없음
void UMyDungeonReviveOptionWidget::SetReviveOption(const FDungeonReviveOption& InOption)
{
	OptionId = InOption.OptionId;

	// 제안 신이 지정돼 있으면 이름, 초상화, 대표색을 DT_GodPresentation에서 가져온다.
	// (대화 위젯과 같은 규칙: 테이블 값이 옵션에 직접 넣은 값보다 우선한다)
	FText DisplayName = InOption.DisplayName;
	TSoftObjectPtr<UTexture2D> Portrait = InOption.Portrait;
	FLinearColor GodColor = FLinearColor::White;
	if (InOption.GodTag.IsValid())
	{
		if (!GodPresentationTable)
		{
			GodPresentationTable = MyGodPresentation::LoadDefaultTable();
		}

		if (const FMyGodPresentationRow* Presentation =
			MyGodPresentation::FindByTag(GodPresentationTable, InOption.GodTag))
		{
			DisplayName = Presentation->DisplayName;
			Portrait = Presentation->FullPortrait.IsNull() ? Presentation->Icon : Presentation->FullPortrait;
			GodColor = Presentation->GetGodLinearColor();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DT_GodPresentation에 부활 옵션의 GodTag 행이 없습니다: %s (Option: %s)"),
				*InOption.GodTag.ToString(),
				*InOption.OptionId.ToString());
		}
	}

	if (TXT_DisplayName)
	{
		// 이름이 비어 있으면 데이터 확인이 가능하도록 OptionId를 대신 보여준다.
		TXT_DisplayName->SetText(DisplayName.IsEmpty()
			? FText::FromName(InOption.OptionId)
			: DisplayName);
		TXT_DisplayName->SetColorAndOpacity(FSlateColor(GodColor));
	}

	if (TXT_MesoCost)
	{
		TXT_MesoCost->SetText(InOption.MesoCost > 0
			? FText::Format(MesoCostFormat, FText::AsNumber(InOption.MesoCost))
			: FreeCostText);
	}

	if (TXT_StyleLabel)
	{
		TXT_StyleLabel->SetText(InOption.StyleId.IsNone()
			? FText::GetEmpty()
			: FText::FromName(InOption.StyleId));
	}

	if (IMG_Portrait)
	{
		// 카드 몇 장 분량의 초상화만 다루므로 동기 로드로 단순화한다 (상점 슬롯과 동일)
		if (UTexture2D* PortraitTexture = Portrait.LoadSynchronous())
		{
			IMG_Portrait->SetBrushFromTexture(PortraitTexture);
			IMG_Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IMG_Portrait->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (TXT_Description)
	{
		TXT_Description->SetText(InOption.Description);
	}

	if (TXT_HealthPercent)
	{
		TXT_HealthPercent->SetText(FText::Format(
			HealthPercentFormat,
			FText::AsNumber(FMath::RoundToInt(InOption.ReviveHealthPercent * 100.0f))));
	}

	if (TXT_ReviveDelay)
	{
		TXT_ReviveDelay->SetText(FText::Format(
			ReviveDelayFormat,
			FText::AsNumber(FMath::RoundToInt(InOption.ReviveDelaySeconds))));
	}

	BP_OnStyleApplied(InOption.StyleId);
}

////////////////////////////
//! \author 장효제
//! \brief 메소 보유량에 따라 카드 버튼을 잠그고 비용 텍스트 색상을 바꾼다.
//! \param bInAffordable 메소가 충분하면 true
//! \return 없음
void UMyDungeonReviveOptionWidget::SetAffordable(bool bInAffordable)
{
	if (BTN_Select)
	{
		BTN_Select->SetIsEnabled(bInAffordable);
	}

	if (TXT_MesoCost)
	{
		if (!bHasCachedDefaultCostColor)
		{
			DefaultCostColor = TXT_MesoCost->GetColorAndOpacity();
			bHasCachedDefaultCostColor = true;
		}

		TXT_MesoCost->SetColorAndOpacity(bInAffordable
			? DefaultCostColor
			: FSlateColor(NotAffordableCostColor));
	}
}

////////////////////////////
//! \author 장효제
//! \brief 카드 클릭 시 부활 패널에 OptionId를 알린다.
//! \param 없음
//! \return 없음
void UMyDungeonReviveOptionWidget::HandleSelectButtonClicked()
{
	if (!OptionId.IsNone())
	{
		OnOptionSelected.Broadcast(OptionId);
	}
}
