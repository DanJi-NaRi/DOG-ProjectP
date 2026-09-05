                                          ////////////////////////////
//! \page MyMissionWidget.cpp
//! \brief 기존 WBP_Mission의 Mission View 구독과 디자인 행 WBP 배치를 구현한다.
#include "Widget/HUD/Mission/MyMissionWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dungeon/DungeonPC.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Widget/HUD/Mission/MyMissionRowWidget.h"

////////////////////////////
//! \author 장효제
//! \brief 에디터 디자인 뷰에서 중요 Mission 행 미리보기를 반영한다.
void UMyMissionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 DungeonPC와 Mission Definition을 확보하고 최초 HUD 행을 만든다.
void UMyMissionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!MissionDefinitionTable)
	{
		MissionDefinitionTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/LeDuat/Systems/Streaming/DT_MissionDefinitions.DT_MissionDefinitions"),
			nullptr,
			LOAD_NoWarn);
	}

	// Mission 헤더·행 클릭으로 설정 팝업을 열려면 패널이 마우스 입력을 받아야 한다.
	SetVisibility(ESlateVisibility::Visible);

	TryBindMissionPlayerController();
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief Mission View 델리게이트 연결을 해제한다.
void UMyMissionWidget::NativeDestruct()
{
	if (MissionPlayerController.IsValid())
	{
		MissionPlayerController->OnMissionHudSelectionChanged.RemoveDynamic(
			this,
			&ThisClass::HandleMissionHudSelectionChanged);
	}
	MissionPlayerController.Reset();
	TerminalStateFirstSeenTimes.Reset();
	TerminalMissionRows.Reset();

	Super::NativeDestruct();
}

////////////////////////////
//! \author 장효제
//! \brief 늦게 준비된 PlayerController를 연결하고 종료 결과 행을 2초 뒤 숨긴다.
//! \param MyGeometry 현재 위젯 Geometry다.
//! \param InDeltaTime 이전 Tick 이후 경과 시간이다.
void UMyMissionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TryBindMissionPlayerController();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	for (const TPair<FGuid, float>& TerminalState : TerminalStateFirstSeenTimes)
	{
		if (Now - TerminalState.Value < ResultDisplayDuration)
		{
			continue;
		}

		if (const TWeakObjectPtr<UMyMissionRowWidget>* Row = TerminalMissionRows.Find(TerminalState.Key);
			Row && Row->IsValid())
		{
			Row->Get()->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 현재 DungeonPC 공개 View로 VB_MissionRows의 최대 세 Mission 디자인 행을 다시 만든다.
void UMyMissionWidget::RefreshMissionRows()
{
	if (!VB_MissionRows)
	{
		if (!bLoggedMissingContainer)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("MyMissionWidget requires a VerticalBox named VB_MissionRows in WBP_Mission."));
			bLoggedMissingContainer = true;
		}
		return;
	}

	VB_MissionRows->ClearChildren();
	TerminalMissionRows.Reset();

	// 전용 영역이 없으면 같은 VerticalBox 최상단에 넣으므로 일반 행보다 먼저 만든다.
	RefreshImportantMissionRow();

	if (!EnsureMissionRowWidgetClass())
	{
		if (!bLoggedMissingRowClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("MyMissionWidget requires MissionRowWidgetClass to be configured."));
			bLoggedMissingRowClass = true;
		}
		return;
	}

	ADungeonPC* DungeonPC = MissionPlayerController.Get();
	const UWorld* World = GetWorld();
	if (!DungeonPC || !World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const TArray<FMyMissionPublicView> MissionViews = DungeonPC->GetMissionHudViews();
	TSet<FGuid> CurrentMissionIds;
	for (const FMyMissionPublicView& MissionView : MissionViews)
	{
		CurrentMissionIds.Add(MissionView.MissionInstanceId);
		const bool bIsTerminal = MissionView.State == EMyMissionState::Completed;
		if (bIsTerminal)
		{
			float& FirstSeenTime = TerminalStateFirstSeenTimes.FindOrAdd(MissionView.MissionInstanceId, Now);
			if (Now - FirstSeenTime >= ResultDisplayDuration)
			{
				continue;
			}
		}
		else
		{
			TerminalStateFirstSeenTimes.Remove(MissionView.MissionInstanceId);
		}

		UMyMissionRowWidget* MissionRow = CreateWidget<UMyMissionRowWidget>(
			GetOwningPlayer(),
			MissionRowWidgetClass);
		if (!MissionRow)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create Mission row widget."));
			continue;
		}

		// HUD 행은 신 정보를 표시하지 않으므로 God 테이블을 넘기지 않는다.
		MissionRow->SetMissionDisplayData(
			MyMissionDisplay::MakeDisplayDataFromView(
				MissionView,
				MissionDefinitionTable,
				nullptr));

		UVerticalBoxSlot* RowSlot = VB_MissionRows->AddChildToVerticalBox(MissionRow);
		if (RowSlot)
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetVerticalAlignment(VAlign_Top);
			RowSlot->SetPadding(FMargin(0.0f, 2.0f));
		}
		if (bIsTerminal)
		{
			TerminalMissionRows.Add(MissionView.MissionInstanceId, MissionRow);
		}
	}

	for (auto It = TerminalStateFirstSeenTimes.CreateIterator(); It; ++It)
	{
		if (!CurrentMissionIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 복제 Mission View 또는 로컬 HUD 선택 변경을 즉시 다시 표시한다.
void UMyMissionWidget::HandleMissionHudSelectionChanged()
{
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief Owning Player가 준비되면 DungeonPC Mission View 델리게이트를 한 번 연결한다.
void UMyMissionWidget::TryBindMissionPlayerController()
{
	if (MissionPlayerController.IsValid())
	{
		return;
	}

	ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer());
	if (!DungeonPC)
	{
		return;
	}

	MissionPlayerController = DungeonPC;
	DungeonPC->OnMissionHudSelectionChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleMissionHudSelectionChanged);
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 실제 특수 이벤트 시스템 없이 중요 Mission 행 표현을 확인할 로컬 표시 데이터를 넣는다.
//! \param InPreviewData 서버와 무관한 로컬 UI 표시 데이터다.
void UMyMissionWidget::SetImportantMissionPreview(const FMyMissionDisplayData& InPreviewData)
{
	ImportantPreviewData = InPreviewData;
	ImportantPreviewData.bIsImportant = true;
	ImportantPreviewData.bIsSelectable = false;
	bHasImportantPreview = true;
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 중요 Mission 표시 데이터를 지우고 전용 영역을 다시 숨긴다.
void UMyMissionWidget::ClearImportantMissionPreview()
{
	ImportantPreviewData = FMyMissionDisplayData();
	bHasImportantPreview = false;
	RefreshMissionRows();
}

////////////////////////////
//! \author 장효제
//! \brief 중요 Mission 표시 데이터가 있을 때만 전용 영역에 비선택 행 하나를 만든다.
void UMyMissionWidget::RefreshImportantMissionRow()
{
	const bool bDesignTime = IsDesignTime();
	const bool bHasData = bDesignTime ? bUseDesignTimeImportantPreview : bHasImportantPreview;
	const FMyMissionDisplayData& DisplayData =
		bDesignTime ? DesignTimeImportantPreview : ImportantPreviewData;

	// 전용 영역을 배치했으면 그쪽을 쓰고, 없으면 일반 행 컨테이너 최상단에 넣는다.
	if (PNL_ImportantMission)
	{
		PNL_ImportantMission->ClearChildren();
		PNL_ImportantMission->SetVisibility(
			bHasData ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bHasData || !EnsureMissionRowWidgetClass())
	{
		return;
	}

	UPanelWidget* ImportantContainer = PNL_ImportantMission ? PNL_ImportantMission.Get() : VB_MissionRows.Get();
	if (!ImportantContainer)
	{
		return;
	}

	const TSubclassOf<UMyMissionRowWidget> RowWidgetClass =
		ImportantMissionRowWidgetClass ? ImportantMissionRowWidgetClass : MissionRowWidgetClass;

	// 에디터 디자인 뷰에는 OwningPlayer가 없으므로 소유 위젯 기준으로 생성한다.
	UMyMissionRowWidget* ImportantRow = CreateWidget<UMyMissionRowWidget>(this, RowWidgetClass);
	if (!ImportantRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create important Mission row widget."));
		return;
	}

	FMyMissionDisplayData ImportantDisplayData = DisplayData;
	ImportantDisplayData.bIsImportant = true;
	ImportantDisplayData.bIsSelectable = false;
	ImportantRow->SetMissionDisplayData(ImportantDisplayData);

	ImportantContainer->AddChild(ImportantRow);
}

////////////////////////////
//! \author 장효제
//! \brief 행 WBP가 지정되지 않았으면 기본 경로에서 한 번 지연 로드한다.
//! \return 사용할 수 있는 행 위젯 클래스가 있으면 true다.
bool UMyMissionWidget::EnsureMissionRowWidgetClass()
{
	if (!MissionRowWidgetClass)
	{
		MissionRowWidgetClass = LoadClass<UMyMissionRowWidget>(
			nullptr,
			TEXT("/Game/LeDuat/Widget/Dungeon/WBP_MissionRow.WBP_MissionRow_C"),
			nullptr,
			LOAD_NoWarn);
	}
	return MissionRowWidgetClass != nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief HUD Mission 패널 클릭을 Mission 설정 팝업 열기로 처리한다.
//! \param InGeometry 현재 위젯 Geometry다.
//! \param InMouseEvent 발생한 마우스 이벤트다.
//! \return 팝업을 열었으면 Handled다.
FReply UMyMissionWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (ADungeonPC* DungeonPC = MissionPlayerController.Get())
	{
		DungeonPC->OpenMissionSettingPopup();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
