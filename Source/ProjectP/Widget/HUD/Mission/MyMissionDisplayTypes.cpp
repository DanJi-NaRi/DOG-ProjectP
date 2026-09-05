////////////////////////////
//! \page MyMissionDisplayTypes.cpp
//! \brief 서버 Mission View를 UI 전용 표시 데이터로 변환하는 구현 파일이다.
#include "Widget/HUD/Mission/MyMissionDisplayTypes.h"

#include "Components/Widget.h"
#include "Engine/DataTable.h"
#include "God/MyGodPresentationTypes.h"

namespace MyMissionDisplay
{
	namespace
	{
		////////////////////////////
		//! \author 장효제
		//! \brief GodTag에 대응하는 표시 이름과 아이콘을 표시 데이터에 채운다.
		//! \param GodPresentationTable 신 표시 정보를 담은 공용 DataTable이다.
		//! \param GodTag 조회할 제안 신 태그다.
		//! \param OutDisplayData 신 표시 항목을 채울 대상 표시 데이터다.
		void ApplyGodPresentation(
			const UDataTable* GodPresentationTable,
			FGameplayTag GodTag,
			FMyMissionDisplayData& OutDisplayData)
		{
			if (!GodPresentationTable || !GodTag.IsValid())
			{
				return;
			}

			TArray<FMyGodPresentationRow*> AllPresentations;
			GodPresentationTable->GetAllRows(
				TEXT("MyMissionDisplay::ApplyGodPresentation"),
				AllPresentations);
			for (const FMyGodPresentationRow* Presentation : AllPresentations)
			{
				if (!Presentation || !Presentation->GodTag.MatchesTagExact(GodTag))
				{
					continue;
				}

				OutDisplayData.GodName = Presentation->DisplayName;
				OutDisplayData.GodIcon = Presentation->Icon.LoadSynchronous();
				OutDisplayData.bShowGod =
					!OutDisplayData.GodName.IsEmpty() || OutDisplayData.GodIcon != nullptr;
				return;
			}
		}
	}

	////////////////////////////
	//! \author 장효제
	//! \brief 복제된 Mission View와 DataTable을 HUD·팝업 공용 표시 데이터로 변환한다.
	//! \param MissionView 변환할 서버 공개 Mission View다.
	//! \param MissionDefinitionTable 제목과 조건 문구를 조회할 Mission Definition 테이블이다.
	//! \param GodPresentationTable 신 이름과 초상화를 조회할 테이블이며 nullptr이면 신 표시를 생략한다.
	//! \return 일반 Mission 한 건을 표시할 UI 전용 데이터다.
	FMyMissionDisplayData MakeDisplayDataFromView(
		const FMyMissionPublicView& MissionView,
		const UDataTable* MissionDefinitionTable,
		const UDataTable* GodPresentationTable)
	{
		FMyMissionDisplayData DisplayData;
		DisplayData.MissionInstanceId = MissionView.MissionInstanceId;
		DisplayData.MissionState = MissionView.State;
		DisplayData.EndsAtServerTime = MissionView.MissionEndsAtServerTime;
		DisplayData.bShowRemainingTime = MissionView.MissionEndsAtServerTime > 0.0f;
		DisplayData.bIsImportant = false;
		DisplayData.bIsSelectable = true;

		if (MissionDefinitionTable)
		{
			if (const FMyMissionDefinitionRow* Definition =
				MissionDefinitionTable->FindRow<FMyMissionDefinitionRow>(
					MissionView.DefinitionRowName,
					TEXT("MyMissionDisplay::MakeDisplayDataFromView"),
					false))
			{
				DisplayData.DisplayName = Definition->DisplayName;
				DisplayData.Description = Definition->Description;
				DisplayData.bShowPartyBadge =
					Definition->AssigneeSelector == EMyMissionAssigneeSelector::AllParty;
			}
		}

		// 기존 HUD의 Description → DisplayName → RowName 폴백 순서를 그대로 유지한다.
		if (DisplayData.Description.IsEmpty() && DisplayData.DisplayName.IsEmpty())
		{
			DisplayData.Description = FText::FromName(MissionView.DefinitionRowName);
		}

		if (MissionView.State == EMyMissionState::Completed)
		{
			DisplayData.ProgressText = NSLOCTEXT("MissionUI", "MissionCompleted", "달성");
			DisplayData.bShowProgress = true;
		}
		else if (!MissionView.Objectives.IsEmpty())
		{
			const FMyMissionObjectiveView& Objective = MissionView.Objectives[0];
			DisplayData.ProgressText = FText::Format(
				NSLOCTEXT("MissionUI", "MissionProgress", "{0} / {1}"),
				FText::AsNumber(Objective.ProgressCount),
				FText::AsNumber(Objective.RequiredCount));
			DisplayData.bShowProgress = true;
		}

		// 세트 Mission의 음수 Delta를 대비해 절댓값이 아니라 부호를 그대로 표시한다.
		if (MissionView.ResolvedMesoDelta != 0)
		{
			DisplayData.MesoDeltaText = FText::Format(
				NSLOCTEXT("MissionUI", "MissionMesoDelta", "달성 시 {0} Meso"),
				FText::FromString(FString::Printf(TEXT("%+d"), MissionView.ResolvedMesoDelta)));
			DisplayData.bShowMesoDelta = true;
		}

		ApplyGodPresentation(GodPresentationTable, MissionView.ProposerGodTag, DisplayData);
		return DisplayData;
	}

	////////////////////////////
	//! \author 장효제
	//! \brief WBP에 배치되지 않았을 수 있는 선택 표시 위젯의 가시성만 조절한다.
	//! \param Widget 선택 배치 위젯이며 nullptr이면 아무것도 하지 않는다.
	//! \param bShouldShow 이번 표시 데이터가 해당 요소를 보여줄지 여부다.
	void ApplyOptionalVisibility(UWidget* Widget, bool bShouldShow)
	{
		if (Widget)
		{
			Widget->SetVisibility(
				bShouldShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
