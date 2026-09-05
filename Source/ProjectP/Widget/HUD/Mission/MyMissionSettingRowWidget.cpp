////////////////////////////
//! \page MyMissionSettingRowWidget.cpp
//! \brief Mission 설정 팝업 행의 표시 적용과 토글 전달을 구현한다.
#include "Widget/HUD/Mission/MyMissionSettingRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

using MyMissionDisplay::ApplyOptionalVisibility;

////////////////////////////
//! \author 장효제
//! \brief 에디터 디자인 뷰에서도 Class Defaults의 행 상태 색을 즉시 반영한다.
void UMyMissionSettingRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyRowVisualState();
}

////////////////////////////
//! \author 장효제
//! \brief 행 전체 클릭과 CheckBox 클릭을 같은 토글 요청으로 묶는다.
void UMyMissionSettingRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Row)
	{
		BTN_Row->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRowButtonClicked);
	}
	if (CB_Select)
	{
		CB_Select->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectCheckBoxChanged);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 행 입력 델리게이트 연결을 해제한다.
void UMyMissionSettingRowWidget::NativeDestruct()
{
	if (BTN_Row)
	{
		BTN_Row->OnClicked.RemoveDynamic(this, &ThisClass::HandleRowButtonClicked);
	}
	if (CB_Select)
	{
		CB_Select->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleSelectCheckBoxChanged);
	}
	OnMissionSettingRowClicked.Clear();

	Super::NativeDestruct();
}

////////////////////////////
//! \author 장효제
//! \brief 목록 전체를 다시 만들지 않고 남은 시간 문구만 실시간으로 갱신한다.
//! \param MyGeometry 현재 위젯 Geometry다.
//! \param InDeltaTime 이전 Tick 이후 경과 시간이다.
void UMyMissionSettingRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateRemainingTimeText();
}

////////////////////////////
//! \author 장효제
//! \brief 일반 Mission과 중요 Mission을 같은 UI 전용 표시 데이터로 행에 적용한다.
//! \param InDisplayData 표시할 UI 전용 Mission 데이터다.
void UMyMissionSettingRowWidget::SetMissionDisplayData(const FMyMissionDisplayData& InDisplayData)
{
	DisplayData = InDisplayData;

	if (TXT_GodName)
	{
		TXT_GodName->SetText(DisplayData.GodName);
	}
	if (IMG_GodIcon && DisplayData.GodIcon)
	{
		IMG_GodIcon->SetBrushFromTexture(DisplayData.GodIcon);
	}
	if (TXT_DisplayName)
	{
		TXT_DisplayName->SetText(DisplayData.DisplayName);
	}
	if (TXT_Description)
	{
		TXT_Description->SetText(DisplayData.Description);
	}
	if (TXT_Progress)
	{
		TXT_Progress->SetText(DisplayData.ProgressText);
	}
	if (TXT_MesoDelta)
	{
		TXT_MesoDelta->SetText(DisplayData.MesoDeltaText);
	}

	// 1회성 특수 이벤트는 진행도·남은 시간·Meso·신 정보가 없을 수 있으므로 개별로 숨긴다.
	ApplyOptionalVisibility(TXT_GodName, DisplayData.bShowGod);
	ApplyOptionalVisibility(IMG_GodIcon, DisplayData.bShowGod && DisplayData.GodIcon != nullptr);
	ApplyOptionalVisibility(TXT_DisplayName, !DisplayData.DisplayName.IsEmpty());
	ApplyOptionalVisibility(TXT_Description, !DisplayData.Description.IsEmpty());
	ApplyOptionalVisibility(TXT_Progress, DisplayData.bShowProgress);
	ApplyOptionalVisibility(TXT_RemainingTime, DisplayData.bShowRemainingTime);
	ApplyOptionalVisibility(TXT_MesoDelta, DisplayData.bShowMesoDelta);
	ApplyOptionalVisibility(PNL_PartyBadge, DisplayData.bShowPartyBadge);

	// 중요 Mission 행은 CheckBox가 없고 클릭으로도 토글하지 않는다.
	if (CB_Select)
	{
		CB_Select->SetVisibility(
			DisplayData.bIsImportant ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (BTN_Row)
	{
		BTN_Row->SetVisibility(
			DisplayData.bIsImportant ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	UpdateRemainingTimeText();
	ApplyRowVisualState();
	SetSelectionState(DisplayData.bIsSelectable && bIsSelected, DisplayData.bIsSelectable);
}

////////////////////////////
//! \author 장효제
//! \brief 부모 팝업이 확정한 선택 여부와 선택 가능 여부를 행 표현에 반영한다.
//! \param bInSelected 현재 DraftSelection에 포함되어 있는지 여부다.
//! \param bInSelectable 지금 이 행을 토글할 수 있는지 여부다.
void UMyMissionSettingRowWidget::SetSelectionState(bool bInSelected, bool bInSelectable)
{
	bIsSelected = bInSelected;
	bIsSelectable = bInSelectable && !DisplayData.bIsImportant;

	TGuardValue<bool> ApplyingGuard(bApplyingSelectionState, true);
	if (CB_Select)
	{
		CB_Select->SetIsChecked(bIsSelected);
		CB_Select->SetIsEnabled(bIsSelectable);
	}
	if (BTN_Row)
	{
		BTN_Row->SetIsEnabled(bIsSelectable);
	}

	ApplyRowVisualState();
	BP_OnMissionRowStateApplied(DisplayData.bIsImportant, bIsSelected, bIsSelectable);
}

////////////////////////////
//! \author 장효제
//! \brief 현재 행이 표시하는 Mission 인스턴스 식별자를 반환한다.
//! \return 표시 중인 Mission 인스턴스 식별자다.
FGuid UMyMissionSettingRowWidget::GetMissionInstanceId() const
{
	return DisplayData.MissionInstanceId;
}

////////////////////////////
//! \author 장효제
//! \brief 행 전체 클릭을 토글 요청으로 바꾼다.
void UMyMissionSettingRowWidget::HandleRowButtonClicked()
{
	RequestToggle();
}

////////////////////////////
//! \author 장효제
//! \brief CheckBox 클릭을 토글 요청으로 바꾼다.
//! \param bChecked CheckBox가 새로 가지게 된 체크 상태이며 판정은 부모 팝업이 한다.
void UMyMissionSettingRowWidget::HandleSelectCheckBoxChanged(bool bChecked)
{
	if (bApplyingSelectionState)
	{
		return;
	}

	RequestToggle();
}

////////////////////////////
//! \author 장효제
//! \brief 선택 정책을 스스로 판단하지 않고 MissionInstanceId만 부모 팝업에 전달한다.
void UMyMissionSettingRowWidget::RequestToggle()
{
	if (DisplayData.bIsImportant || !bIsSelectable)
	{
		// 부모가 확정한 선택 불가 상태를 행이 되돌리지 않도록 표시만 원복한다.
		SetSelectionState(bIsSelected, bIsSelectable);
		return;
	}

	OnMissionSettingRowClicked.Broadcast(DisplayData.MissionInstanceId);
}

////////////////////////////
//! \author 장효제
//! \brief 중요·선택·선택 가능 상태의 우선순위로 행 배경색과 비활성 투명도를 결정해 적용한다.
//!
//! 색 판정을 C++이 소유하므로 일반 행 WBP와 중요 행 WBP가 같은 상태 그래프를
//! 각각 들고 있을 필요가 없다. Blueprint는 색 값만 Class Defaults로 조절한다.
void UMyMissionSettingRowWidget::ApplyRowVisualState()
{
	// 디자인 뷰에는 표시 데이터가 없으므로 선택 가능한 기본 상태로 색을 보여준다.
	const bool bSelectableForVisual = IsDesignTime() ? true : bIsSelectable;

	FLinearColor RowColor = NormalRowColor;
	float RowOpacity = 1.0f;
	if (DisplayData.bIsImportant)
	{
		RowColor = ImportantRowColor;
	}
	else if (bIsSelected)
	{
		RowColor = SelectedRowColor;
	}
	else if (!bSelectableForVisual)
	{
		RowColor = DisabledRowColor;
		RowOpacity = DisabledRowOpacity;
	}

	if (BRD_RowBackground)
	{
		BRD_RowBackground->SetBrushColor(RowColor);
	}
	SetRenderOpacity(RowOpacity);
}

////////////////////////////
//! \author 장효제
//! \brief 권위 서버 종료 시각과 현재 서버 시각의 차이를 남은 시간 문구로 만든다.
void UMyMissionSettingRowWidget::UpdateRemainingTimeText()
{
	if (!TXT_RemainingTime || !DisplayData.bShowRemainingTime)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	const int32 RemainingSeconds = FMath::Max(
		0,
		FMath::CeilToInt(DisplayData.EndsAtServerTime - GameState->GetServerWorldTimeSeconds()));
	TXT_RemainingTime->SetText(FText::FromString(
		FString::Printf(TEXT("%d:%02d"), RemainingSeconds / 60, RemainingSeconds % 60)));
}
