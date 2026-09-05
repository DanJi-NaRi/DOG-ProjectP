////////////////////////////
//! \page MyMissionRowWidget.cpp
//! \brief 개별 Mission HUD 행의 데이터 적용을 구현한다.
#include "Widget/HUD/Mission/MyMissionRowWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

using MyMissionDisplay::ApplyOptionalVisibility;

////////////////////////////
//! \author 장효제
//! \brief 에디터 디자인 뷰에서도 Class Defaults의 행 배경색을 반영한다.
void UMyMissionRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyRowVisualState();
}

////////////////////////////
//! \author 장효제
//! \brief 중요 여부에 따라 배경색을 적용해 중요 행 전용 WBP 없이도 구분되게 한다.
void UMyMissionRowWidget::ApplyRowVisualState()
{
	if (BRD_RowBackground)
	{
		BRD_RowBackground->SetBrushColor(bIsImportantRow ? ImportantRowColor : NormalRowColor);
	}
}

////////////////////////////
//! \author 장효제
//! \brief Mission 식별자, 목표 문구, 진행 문구와 상태를 현재 HUD 행에 적용한다.
//! \param InMissionInstanceId 표시할 Mission 인스턴스 식별자다.
//! \param InObjectiveText 표시할 Mission 목표 문구다.
//! \param InProgressText 표시할 Mission 진행 또는 결과 문구다.
//! \param InMissionState 표시할 Mission 상태다.
void UMyMissionRowWidget::SetMissionRowData(
	const FGuid& InMissionInstanceId,
	const FText& InObjectiveText,
	const FText& InProgressText,
	EMyMissionState InMissionState)
{
	MissionInstanceId = InMissionInstanceId;
	MissionState = InMissionState;

	if (TXT_Objective)
	{
		TXT_Objective->SetText(InObjectiveText);
	}
	if (TXT_Progress)
	{
		TXT_Progress->SetText(InProgressText);
	}
	BP_OnMissionStateApplied(InMissionState);
}

////////////////////////////
//! \author 장효제
//! \brief 일반 Mission과 중요 Mission을 같은 UI 전용 표시 데이터로 HUD 행에 적용한다.
//! \param InDisplayData 표시할 UI 전용 Mission 데이터다.
void UMyMissionRowWidget::SetMissionDisplayData(const FMyMissionDisplayData& InDisplayData)
{
	MissionInstanceId = InDisplayData.MissionInstanceId;
	MissionState = InDisplayData.MissionState;
	bIsImportantRow = InDisplayData.bIsImportant;

	// 기존 HUD 계약대로 조건 문구를 우선하고 없으면 연출 제목으로 대체한다.
	if (TXT_Objective)
	{
		TXT_Objective->SetText(
			InDisplayData.Description.IsEmpty() ? InDisplayData.DisplayName : InDisplayData.Description);
	}
	if (TXT_Progress)
	{
		TXT_Progress->SetText(InDisplayData.ProgressText);
		TXT_Progress->SetVisibility(
			InDisplayData.bShowProgress ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TXT_DisplayName)
	{
		TXT_DisplayName->SetText(InDisplayData.DisplayName);
	}
	if (TXT_MesoDelta)
	{
		TXT_MesoDelta->SetText(InDisplayData.MesoDeltaText);
	}
	if (TXT_GodName)
	{
		TXT_GodName->SetText(InDisplayData.GodName);
	}
	if (IMG_GodIcon && InDisplayData.GodIcon)
	{
		IMG_GodIcon->SetBrushFromTexture(InDisplayData.GodIcon);
	}

	ApplyOptionalVisibility(TXT_DisplayName, !InDisplayData.DisplayName.IsEmpty());
	ApplyOptionalVisibility(TXT_MesoDelta, InDisplayData.bShowMesoDelta);
	ApplyOptionalVisibility(TXT_GodName, InDisplayData.bShowGod);
	ApplyOptionalVisibility(IMG_GodIcon, InDisplayData.bShowGod && InDisplayData.GodIcon != nullptr);

	ApplyRowVisualState();
	BP_OnMissionStateApplied(InDisplayData.MissionState);
}

////////////////////////////
//! \author 장효제
//! \brief 현재 HUD 행이 표시하는 Mission 인스턴스 식별자를 반환한다.
//! \return 현재 Mission 인스턴스 식별자다.
FGuid UMyMissionRowWidget::GetMissionInstanceId() const
{
	return MissionInstanceId;
}
