////////////////////////////
//! \page MyMissionSettingPopupWidget.cpp
//! \brief Mission 설정 팝업의 DraftSelection과 최대 3개 규칙을 구현한다.
#include "Widget/HUD/Mission/MyMissionSettingPopupWidget.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dungeon/DungeonPC.h"
#include "Engine/DataTable.h"
#include "God/MyGodPresentationTypes.h"
#include "InputCoreTypes.h"
#include "Widget/HUD/Mission/MyMissionSettingRowWidget.h"

////////////////////////////
//! \author 장효제
//! \brief 키보드 이동·스킬 입력을 막지 않으면서 팝업 내부 마우스 입력을 받게 설정한다.
UMyMissionSettingPopupWidget::UMyMissionSettingPopupWidget()
{
	InputMode = EMyWidgetInputMode::GameAndMenu;
	SetIsFocusable(true);
}

////////////////////////////
//! \author 장효제
//! \brief 에디터 디자인 뷰에서 중요 Mission 행 미리보기를 반영한다.
void UMyMissionSettingPopupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 현재 HUD 선택을 DraftSelection에 복사하고 실시간 목록 구독을 시작한다.
void UMyMissionSettingPopupWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (!MissionDefinitionTable)
	{
		MissionDefinitionTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/LeDuat/Systems/Streaming/DT_MissionDefinitions.DT_MissionDefinitions"),
			nullptr,
			LOAD_NoWarn);
	}
	if (!GodPresentationTable)
	{
		GodPresentationTable = MyGodPresentation::LoadDefaultTable();
	}

	if (!MissionSettingRowWidgetClass)
	{
		// WBP를 아직 만들지 않았어도 조용히 넘어가도록 경고 없이 지연 로드한다.
		MissionSettingRowWidgetClass = LoadClass<UMyMissionSettingRowWidget>(
			nullptr,
			TEXT("/Game/LeDuat/Widget/Dungeon/WBP_MissionSettingRow.WBP_MissionSettingRow_C"),
			nullptr,
			LOAD_NoWarn);
	}

	MissionPlayerController = Cast<ADungeonPC>(GetOwningPlayer());
	DraftSelection.Reset();
	if (ADungeonPC* DungeonPC = MissionPlayerController.Get())
	{
		for (const FMyMissionPublicView& MissionView : DungeonPC->GetMissionHudViews())
		{
			DraftSelection.AddUnique(MissionView.MissionInstanceId);
		}
		DungeonPC->OnMissionHudSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleMissionViewsChanged);
	}

	if (BTN_Apply)
	{
		BTN_Apply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyClicked);
	}
	if (BTN_Cancel)
	{
		BTN_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Close)
	{
		BTN_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Background)
	{
		BTN_Background->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Reset)
	{
		BTN_Reset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetClicked);
	}

	RebuildMissionRows();
	SetFocus();
}

////////////////////////////
//! \author 장효제
//! \brief 실시간 목록 구독과 버튼 델리게이트를 해제하고 DraftSelection을 버린다.
void UMyMissionSettingPopupWidget::NativeOnDeactivated()
{
	if (ADungeonPC* DungeonPC = MissionPlayerController.Get())
	{
		DungeonPC->OnMissionHudSelectionChanged.RemoveDynamic(this, &ThisClass::HandleMissionViewsChanged);
	}
	MissionPlayerController.Reset();

	if (BTN_Apply)
	{
		BTN_Apply->OnClicked.RemoveDynamic(this, &ThisClass::HandleApplyClicked);
	}
	if (BTN_Cancel)
	{
		BTN_Cancel->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Close)
	{
		BTN_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Background)
	{
		BTN_Background->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}
	if (BTN_Reset)
	{
		BTN_Reset->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetClicked);
	}

	DraftSelection.Reset();

	Super::NativeOnDeactivated();
}

////////////////////////////
//! \author 장효제
//! \brief ESC 입력을 받을 포커스 대상으로 팝업 자신을 반환한다.
//! \return 포커스를 받을 팝업 위젯이다.
UWidget* UMyMissionSettingPopupWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMyMissionSettingPopupWidget*>(this);
}

////////////////////////////
//! \author 장효제
//! \brief ESC 입력을 취소와 동일하게 처리해 DraftSelection을 버리고 닫는다.
//! \param InGeometry 현재 팝업 Geometry다.
//! \param InKeyEvent 입력된 키 이벤트다.
//! \return ESC를 처리했으면 Handled다.
FReply UMyMissionSettingPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleCancelClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

////////////////////////////
//! \author 장효제
//! \brief 실제 특수 이벤트 시스템 없이 중요 Mission 행 표현을 확인할 로컬 표시 데이터를 넣는다.
//! \param InPreviewData 서버와 무관한 로컬 UI 표시 데이터다.
void UMyMissionSettingPopupWidget::SetImportantMissionPreview(const FMyMissionDisplayData& InPreviewData)
{
	ImportantPreviewData = InPreviewData;
	ImportantPreviewData.bIsImportant = true;
	ImportantPreviewData.bIsSelectable = false;
	bHasImportantPreview = true;
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 중요 Mission 표시 데이터를 지우고 최상단 행을 제거한다.
void UMyMissionSettingPopupWidget::ClearImportantMissionPreview()
{
	ImportantPreviewData = FMyMissionDisplayData();
	bHasImportantPreview = false;
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 복제 Mission View가 바뀌면 사라진 선택만 정리하고 목록을 다시 만든다.
void UMyMissionSettingPopupWidget::HandleMissionViewsChanged()
{
	// 종료된 Mission은 선택 수만 줄이고 다른 Mission을 자동으로 보충하지 않는다.
	const TArray<FMyMissionPublicView> SelectableViews = GetSelectableMissionViews();
	PruneDraftSelection(SelectableViews);
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 행이 전달한 MissionInstanceId를 최대 3개 규칙으로 토글한다.
//! \param MissionInstanceId 토글을 요청한 일반 Mission 식별자다.
void UMyMissionSettingPopupWidget::HandleMissionRowClicked(FGuid MissionInstanceId)
{
	ToggleDraftSelection(MissionInstanceId);
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 유효한 DraftSelection을 로컬 HUD 선택에 반영하고 팝업을 닫는다.
void UMyMissionSettingPopupWidget::HandleApplyClicked()
{
	if (ADungeonPC* DungeonPC = MissionPlayerController.Get())
	{
		DungeonPC->ApplyMissionHudSelection(DraftSelection);
	}
	DeactivateWidget();
}

////////////////////////////
//! \author 장효제
//! \brief DraftSelection을 폐기하고 실제 HUD 선택은 그대로 둔 채 팝업을 닫는다.
void UMyMissionSettingPopupWidget::HandleCancelClicked()
{
	DraftSelection.Reset();
	DeactivateWidget();
}

////////////////////////////
//! \author 장효제
//! \brief 확인창 없이 DraftSelection만 남은 시간이 짧은 일반 Mission 최대 3개로 바꾼다.
void UMyMissionSettingPopupWidget::HandleResetClicked()
{
	DraftSelection.Reset();
	for (const FMyMissionPublicView& MissionView : GetSelectableMissionViews())
	{
		if (DraftSelection.Num() >= MaxHudMissionCount)
		{
			break;
		}
		DraftSelection.AddUnique(MissionView.MissionInstanceId);
	}
	RebuildMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 중요 Mission 행을 최상단에 두고 일반 Mission 행을 남은 시간 순으로 다시 만든다.
void UMyMissionSettingPopupWidget::RebuildMissionRows()
{
	if (!SB_MissionRows)
	{
		return;
	}

	SB_MissionRows->ClearChildren();

	const bool bDesignTime = IsDesignTime();
	const bool bShowImportantRow = bDesignTime ? bUseDesignTimeImportantPreview : bHasImportantPreview;
	if (bShowImportantRow)
	{
		FMyMissionDisplayData ImportantDisplayData =
			bDesignTime ? DesignTimeImportantPreview : ImportantPreviewData;
		ImportantDisplayData.bIsImportant = true;
		ImportantDisplayData.bIsSelectable = false;
		if (UMyMissionSettingRowWidget* ImportantRow = CreateMissionRow(ImportantDisplayData))
		{
			ImportantRow->SetSelectionState(false, false);
		}
	}

	const TArray<FMyMissionPublicView> SelectableViews = GetSelectableMissionViews();
	const bool bDraftFull = DraftSelection.Num() >= MaxHudMissionCount;
	for (const FMyMissionPublicView& MissionView : SelectableViews)
	{
		FMyMissionDisplayData RowDisplayData = MyMissionDisplay::MakeDisplayDataFromView(
			MissionView,
			MissionDefinitionTable,
			GodPresentationTable);

		// 3개를 채우면 미선택 행만 비활성화하고 기존 선택은 밀어내지 않는다.
		const bool bSelected = DraftSelection.Contains(MissionView.MissionInstanceId);
		RowDisplayData.bIsSelectable = bSelected || !bDraftFull;

		if (UMyMissionSettingRowWidget* MissionRow = CreateMissionRow(RowDisplayData))
		{
			MissionRow->SetSelectionState(bSelected, RowDisplayData.bIsSelectable);
			MissionRow->OnMissionSettingRowClicked.AddUniqueDynamic(this, &ThisClass::HandleMissionRowClicked);
		}
	}

	if (TXT_EmptyMission)
	{
		TXT_EmptyMission->SetVisibility(
			SelectableViews.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BTN_Apply)
	{
		BTN_Apply->SetIsEnabled(!SelectableViews.IsEmpty());
	}
	if (BTN_Reset)
	{
		BTN_Reset->SetIsEnabled(!SelectableViews.IsEmpty());
	}

	UpdateSelectionGuide();
}

////////////////////////////
//! \author 장효제
//! \brief 선택 수만 바꾸는 안내 문구를 갱신한다. 중요 Mission은 이 수에 포함하지 않는다.
void UMyMissionSettingPopupWidget::UpdateSelectionGuide()
{
	if (!TXT_SelectionGuide)
	{
		return;
	}

	TXT_SelectionGuide->SetText(FText::Format(
		NSLOCTEXT("MissionUI", "MissionSelectionGuide", "HUD에 표시할 일반 미션을 최대 3개까지 선택하세요. ({0}/{1})"),
		FText::AsNumber(DraftSelection.Num()),
		FText::AsNumber(MaxHudMissionCount)));
}

////////////////////////////
//! \author 장효제
//! \brief 이미 선택했으면 해제하고 아니면 상한 안에서만 추가한다.
//! \param MissionInstanceId 토글 대상 일반 Mission 식별자다.
void UMyMissionSettingPopupWidget::ToggleDraftSelection(FGuid MissionInstanceId)
{
	if (DraftSelection.Remove(MissionInstanceId) > 0)
	{
		return;
	}
	if (DraftSelection.Num() >= MaxHudMissionCount)
	{
		return;
	}

	DraftSelection.AddUnique(MissionInstanceId);
}

////////////////////////////
//! \author 장효제
//! \brief 목록에서 사라진 Mission만 DraftSelection에서 제거한다.
//! \param SelectableViews 현재 선택 가능한 일반 Mission View 목록이다.
void UMyMissionSettingPopupWidget::PruneDraftSelection(const TArray<FMyMissionPublicView>& SelectableViews)
{
	DraftSelection.RemoveAll([&SelectableViews](const FGuid& MissionInstanceId)
	{
		return !SelectableViews.ContainsByPredicate([&MissionInstanceId](const FMyMissionPublicView& MissionView)
		{
			return MissionView.MissionInstanceId == MissionInstanceId;
		});
	});
}

////////////////////////////
//! \author 장효제
//! \brief 팝업이 선택 대상으로 표시할 활성 Mission만 남은 시간 순으로 반환한다.
//! \return 종료된 결과 View를 제외한 Active Mission View 목록이다.
TArray<FMyMissionPublicView> UMyMissionSettingPopupWidget::GetSelectableMissionViews() const
{
	TArray<FMyMissionPublicView> SelectableViews;
	const ADungeonPC* DungeonPC = MissionPlayerController.Get();
	if (!DungeonPC)
	{
		return SelectableViews;
	}

	// GetMissionPopupViews가 이미 남은 시간 오름차순, 동률은 시작 순서로 정렬한다.
	for (const FMyMissionPublicView& MissionView : DungeonPC->GetMissionPopupViews())
	{
		if (MissionView.State == EMyMissionState::Active)
		{
			SelectableViews.Add(MissionView);
		}
	}
	return SelectableViews;
}

////////////////////////////
//! \author 장효제
//! \brief 표시 데이터에 맞는 행 WBP를 만들어 ScrollBox 끝에 추가한다.
//! \param RowDisplayData 행에 적용할 UI 전용 표시 데이터다.
//! \return 생성한 행 위젯이며 실패하면 nullptr이다.
UMyMissionSettingRowWidget* UMyMissionSettingPopupWidget::CreateMissionRow(
	const FMyMissionDisplayData& RowDisplayData)
{
	const TSubclassOf<UMyMissionSettingRowWidget> RowWidgetClass =
		RowDisplayData.bIsImportant && ImportantMissionSettingRowWidgetClass
			? ImportantMissionSettingRowWidgetClass
			: MissionSettingRowWidgetClass;
	if (!RowWidgetClass || !SB_MissionRows)
	{
		return nullptr;
	}

	// 에디터 디자인 뷰에는 OwningPlayer가 없으므로 소유 위젯 기준으로 생성한다.
	UMyMissionSettingRowWidget* MissionRow = CreateWidget<UMyMissionSettingRowWidget>(this, RowWidgetClass);
	if (!MissionRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create Mission setting row widget."));
		return nullptr;
	}

	MissionRow->SetMissionDisplayData(RowDisplayData);
	SB_MissionRows->AddChild(MissionRow);
	return MissionRow;
}
