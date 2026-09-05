////////////////////////////
//! \page MyGodPageWidget.cpp
//! \brief 신 목록 선택과 상세 패널 갱신을 조율하는 God Page 구현 파일이다.
#include "MyGodPageWidget.h"

#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"
#include "MyGodDetailPanelWidget.h"
#include "MyGodListEntryWidget.h"
#include "MyPlayerController.h"
#include "UObject/ConstructorHelpers.h"

//! \author 장효제
//! \brief Menu 입력 모드와 공용 신 Presentation DataTable 기본값을 설정한다.
UMyGodPageWidget::UMyGodPageWidget()
{
	InputMode = EMyWidgetInputMode::GameAndMenu;
	SetIsFocusable(true);

	static ConstructorHelpers::FObjectFinder<UDataTable> GodPresentationTableAsset(
		MyGodPresentation::DefaultTablePath);
	if (GodPresentationTableAsset.Succeeded())
	{
		GodPresentationTable = GodPresentationTableAsset.Object;
	}
}

void UMyGodPageWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (BTN_Close)
	{
		BTN_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
}

void UMyGodPageWidget::NativeDestruct()
{
	if (BTN_Close)
	{
		BTN_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	for (UMyGodListEntryWidget* EntryWidget : GodEntryWidgets)
	{
		if (EntryWidget)
		{
			EntryWidget->OnGodEntrySelected.RemoveDynamic(this, &ThisClass::HandleGodEntrySelected);
		}
	}

	GodEntryWidgets.Reset();
	GodPresentations.Reset();
	Super::NativeDestruct();
}

void UMyGodPageWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (GodEntryWidgets.IsEmpty())
	{
		BuildGodList();
	}
	else
	{
		RefreshSelection();
	}

	SetFocus();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ESC 입력을 받을 포커스 대상으로 신 호감도 창 자신을 반환하는 함수
// Return Value : 포커스를 받을 신 호감도 위젯
UWidget* UMyGodPageWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMyGodPageWidget*>(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 신 호감도 창이 열린 상태에서 ESC 입력 시 해당 창을 닫는 함수
// InGeometry : 신 호감도 창의 현재 지오메트리
// InKeyEvent : 입력된 키 이벤트
// Return Value : ESC 입력을 처리했으면 Handled
FReply UMyGodPageWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		DeactivateWidget();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//! \author 장효제
//! \brief 외부 이벤트나 목록 클릭으로 지정된 GodTag를 현재 선택 상태로 적용한다.
void UMyGodPageWidget::SelectGod(FGameplayTag GodTag)
{
	if (!GodTag.IsValid())
	{
		return;
	}

	if (!GodPresentations.IsEmpty() && !FindPresentation(GodTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] SelectGod ignored - presentation is missing. GodTag=%s"),
			*GodTag.ToString());
		return;
	}

	SelectedGodTag = GodTag;
	StoreLastViewedGod();
	if (!GodEntryWidgets.IsEmpty())
	{
		RefreshSelection();
	}
}

void UMyGodPageWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

void UMyGodPageWidget::HandleGodEntrySelected(FGameplayTag GodTag)
{
	SelectGod(GodTag);
}

//! \author 장효제
//! \brief Presentation DataTable을 표시 순서와 RowName으로 정렬해 왼쪽 목록을 한 번 생성한다.
void UMyGodPageWidget::BuildGodList()
{
	GodEntryWidgets.Reset();
	GodPresentations.Reset();
	if (VB_GodList)
	{
		VB_GodList->ClearChildren();
	}

	if (!GodPresentationTable || !GodListEntryWidgetClass || !VB_GodList)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] BuildGodList failed. Table=%s EntryClass=%s List=%s"),
			*GetNameSafe(GodPresentationTable),
			*GetNameSafe(GodListEntryWidgetClass),
			*GetNameSafe(VB_GodList));
		return;
	}

	struct FSortedGodPresentation
	{
		FName RowName;
		FMyGodPresentationRow Presentation;
	};

	TArray<FSortedGodPresentation> SortedPresentations;
	for (const FName RowName : GodPresentationTable->GetRowNames())
	{
		const FMyGodPresentationRow* Row = GodPresentationTable->FindRow<FMyGodPresentationRow>(
			RowName,
			TEXT("UMyGodPageWidget::BuildGodList"));
		if (!Row || !Row->GodTag.IsValid())
		{
			continue;
		}

		SortedPresentations.Add({ RowName, *Row });
	}

	SortedPresentations.Sort([](const FSortedGodPresentation& Left, const FSortedGodPresentation& Right)
	{
		if (Left.Presentation.DisplayOrder != Right.Presentation.DisplayOrder)
		{
			return Left.Presentation.DisplayOrder < Right.Presentation.DisplayOrder;
		}

		return Left.RowName.ToString() < Right.RowName.ToString();
	});

	for (int32 Index = 1; Index < SortedPresentations.Num(); ++Index)
	{
		const FSortedGodPresentation& Previous = SortedPresentations[Index - 1];
		const FSortedGodPresentation& Current = SortedPresentations[Index];
		if (Previous.Presentation.DisplayOrder == Current.Presentation.DisplayOrder)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GodPage] Duplicate DisplayOrder. Order=%d Rows=%s,%s"),
				Current.Presentation.DisplayOrder,
				*Previous.RowName.ToString(),
				*Current.RowName.ToString());
		}
	}

	for (const FSortedGodPresentation& SortedPresentation : SortedPresentations)
	{
		const FMyGodPresentationRow& Presentation = SortedPresentation.Presentation;
		GodPresentations.Add(Presentation);
		UMyGodListEntryWidget* EntryWidget = CreateWidget<UMyGodListEntryWidget>(this, GodListEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->InitializeEntry(Presentation, DefaultPortraitStage);
		EntryWidget->OnGodEntrySelected.AddUniqueDynamic(this, &ThisClass::HandleGodEntrySelected);
		GodEntryWidgets.Add(EntryWidget);

		if (UVerticalBoxSlot* EntrySlot = VB_GodList->AddChildToVerticalBox(EntryWidget))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}

	RestoreLastViewedGod();
	RefreshSelection();
}

//! \author 장효제
//! \brief 목록 구조를 변경하지 않고 선택 강조와 상세 패널만 갱신한다.
void UMyGodPageWidget::RefreshSelection()
{
	const FMyGodPresentationRow* SelectedPresentation = FindPresentation(SelectedGodTag);
	if (!SelectedPresentation && !GodPresentations.IsEmpty())
	{
		SelectedGodTag = GodPresentations[0].GodTag;
		SelectedPresentation = &GodPresentations[0];
		StoreLastViewedGod();
	}

	if (SelectedPresentation && WBP_GodDetailPanel)
	{
		WBP_GodDetailPanel->SetGodPresentation(*SelectedPresentation, DefaultPortraitStage);
	}

	for (UMyGodListEntryWidget* EntryWidget : GodEntryWidgets)
	{
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetSelected(EntryWidget->GetGodTag().MatchesTagExact(SelectedGodTag));
	}
}

////////////////////////////
//! \author 장효제
//! \brief 현재 PlayerController에 저장된 마지막 조회 신을 복원하고, 없으면 첫 번째 신을 선택한다.
void UMyGodPageWidget::RestoreLastViewedGod()
{
	if (FindPresentation(SelectedGodTag))
	{
		StoreLastViewedGod();
		return;
	}

	const AMyPlayerController* MyPlayerController = GetOwningPlayer<AMyPlayerController>();
	const FGameplayTag LastViewedGodTag = MyPlayerController
		? MyPlayerController->GetLastViewedGodTag()
		: FGameplayTag();

	if (FindPresentation(LastViewedGodTag))
	{
		SelectedGodTag = LastViewedGodTag;
	}
	else if (!GodPresentations.IsEmpty())
	{
		SelectedGodTag = GodPresentations[0].GodTag;
	}

	StoreLastViewedGod();
}

////////////////////////////
//! \author 장효제
//! \brief 현재 선택된 신을 로컬 PlayerController에 저장한다.
void UMyGodPageWidget::StoreLastViewedGod() const
{
	if (!SelectedGodTag.IsValid())
	{
		return;
	}

	if (AMyPlayerController* MyPlayerController = GetOwningPlayer<AMyPlayerController>())
	{
		MyPlayerController->SetLastViewedGodTag(SelectedGodTag);
	}
}

const FMyGodPresentationRow* UMyGodPageWidget::FindPresentation(FGameplayTag GodTag) const
{
	if (!GodTag.IsValid())
	{
		return nullptr;
	}

	return GodPresentations.FindByPredicate([GodTag](const FMyGodPresentationRow& Presentation)
	{
		return Presentation.GodTag.MatchesTagExact(GodTag);
	});
}
