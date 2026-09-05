////////////////////////////
//! \page MyStreamingMission.cpp
//! \brief StreamingManager가 소유하는 파티 Mission 서버 원본 구현 파일이다.
#include "Streaming/MyStreamingManagerComponent.h"

#include "Dungeon/DungeonGS.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GAS/MyPlayerState.h"
#include "MyGameplayTags.h"
#include "Streaming/MyLevelContentOverride.h"
#include "Streaming/MyStreamingPayloads.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace MyMissionPolicy
{
	bool IsAntiAFKMissionTag(const FGameplayTag MissionTag)
	{
		return MissionTag.IsValid()
			&& MissionTag.MatchesTag(MyGameplayTags::Streaming_Mission_AntiAFK);
	}

	////////////////////////////
	//! \author 장효제
	//! \brief StartMission Step 요청이 Definition 한 행에 정확히 대응하는지 판정한다.
	bool DoesDefinitionMatchStartRequest(
		const FGameplayTag DefinitionMissionTag,
		const FName DefinitionStartSequenceId,
		const FGameplayTag RequestedMissionTag,
		const FName SourceSequenceId)
	{
		return RequestedMissionTag.IsValid()
			&& !SourceSequenceId.IsNone()
			&& DefinitionMissionTag.MatchesTagExact(RequestedMissionTag)
			&& DefinitionStartSequenceId == SourceSequenceId;
	}

	////////////////////////////
	//! \author 장효제
	//! \brief Mission 서버 원본을 만들기 전 거부 사유를 계산한다.
	//! \note 재사용 금지는 일반 Mission에만 적용하고, 같은 태그 Active 중복은 종류와 무관하게 막는다.
	EMyMissionActivationRejection CheckMissionActivation(
		const FGameplayTag MissionTag,
		const FName SourceSequenceId,
		const bool bIsUsedMissionTag,
		const bool bHasActiveSameTag,
		const bool bDefinitionValid,
		const bool bAssigneesResolved)
	{
		if (!MissionTag.IsValid() || SourceSequenceId.IsNone())
		{
			return EMyMissionActivationRejection::InvalidRequest;
		}

		if (!IsAntiAFKMissionTag(MissionTag) && bIsUsedMissionTag)
		{
			return EMyMissionActivationRejection::AlreadyUsed;
		}

		if (bHasActiveSameTag)
		{
			return EMyMissionActivationRejection::AlreadyActive;
		}

		if (!bDefinitionValid)
		{
			return EMyMissionActivationRejection::DefinitionNotFound;
		}

		if (!bAssigneesResolved)
		{
			return EMyMissionActivationRejection::AssigneeNotSatisfied;
		}

		return EMyMissionActivationRejection::None;
	}

	////////////////////////////
	//! \author 장효제
	//! \brief 잠수 감지가 시작 Sequence를 요청할 AntiAFK Definition 후보인지 판정한다.
	bool IsAntiAFKStartCandidate(
		const FGameplayTag MissionTag,
		const FName StartSequenceId,
		const bool bDefinitionValid,
		const bool bHasActiveSameTag)
	{
		return IsAntiAFKMissionTag(MissionTag)
			&& !StartSequenceId.IsNone()
			&& bDefinitionValid
			&& !bHasActiveSameTag;
	}

	////////////////////////////
	//! \author 장효제
	//! \brief 전투 사실이 이 Mission의 스킬 사용 목표에 기여하는지 판정한다.
	//! \param EventTag 사실 태그다.
	//! \param UsedSkillTag 쓴 스킬 태그다.
	//! \param InstigatorTag 사실을 낸 대상 태그다.
	//! \param RequiredSkillTag 목표가 요구하는 스킬이다. 비면 모든 스킬을 센다.
	//! \return 기여하면 true다.
	bool IsSkillUseContribution(
		FGameplayTag EventTag,
		FGameplayTag UsedSkillTag,
		FGameplayTag InstigatorTag,
		FGameplayTag RequiredSkillTag)
	{
		return EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_SkillUsed)
			&& UsedSkillTag.IsValid()
			&& InstigatorTag.MatchesTag(MyGameplayTags::Character_Player)
			&& (!RequiredSkillTag.IsValid() || UsedSkillTag.MatchesTag(RequiredSkillTag));
	}

	bool IsKillContribution(
		FGameplayTag EventTag,
		bool bIsKill,
		FGameplayTag InstigatorTag,
		FGameplayTag TargetTag,
		FGameplayTag RequiredTargetTag)
	{
		return bIsKill
			&& EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_Kill)
			&& InstigatorTag.MatchesTag(MyGameplayTags::Character_Player)
			&& (!RequiredTargetTag.IsValid() || TargetTag.MatchesTag(RequiredTargetTag));
	}

	////////////////////////////
	//! \author 장효제
	//! \brief 전투 사건의 구체 플레이어 태그가 Mission 대상에 포함되는지 판정한다.
	bool IsAssigneeContribution(
		EMyMissionAssigneeSelector AssigneeSelector,
		const TArray<FGameplayTag>& AssigneeCharacterTags,
		FGameplayTag InstigatorTag)
	{
		if (!InstigatorTag.MatchesTag(MyGameplayTags::Character_Player))
		{
			return false;
		}

		return AssigneeSelector == EMyMissionAssigneeSelector::AllParty
			|| (AssigneeSelector == EMyMissionAssigneeSelector::FixedCharacter
				&& AssigneeCharacterTags.Contains(InstigatorTag));
	}

	EMyMissionState ResolveKillMissionState(
		int32 ProgressCount,
		int32 RequiredCount,
		float ServerTime,
		float EndsAtServerTime)
	{
		if (RequiredCount > 0 && ProgressCount >= RequiredCount)
		{
			return EMyMissionState::Completed;
		}

		return ServerTime >= EndsAtServerTime
			? EMyMissionState::Expired
			: EMyMissionState::Active;
	}
}

void UMyStreamingManagerComponent::StopMissionLoop()
{
	if (UWorld* World = GetWorld())
	{
		for (FMyMissionServerState& Mission : MissionStates)
		{
			World->GetTimerManager().ClearTimer(Mission.MissionTimeoutTimerHandle);
		}
	}
	MissionStates.Reset();
	DeferredMissionSequences.Reset();
	DungeonPartyRoster.Reset();
	UsedMissionTags.Reset();
	bMissionLoopStarted = false;
#if !UE_BUILD_SHIPPING
	StandaloneMissionRecipient.Reset();
#endif
}

////////////////////////////
//! \author 장효제
//! \brief 인증된 준비 명단과 Mission 데이터를 고정하고 Zone 기반 공급을 준비한다.
//! \param ReadyUserIndexes 최초 필수 인원 준비 때의 인증 UserIndex 목록이다.
void UMyStreamingManagerComponent::StartMissionLoop(const TArray<int32>& ReadyUserIndexes)
{
	const AActor* OwnerActor = GetOwner();
	if (bMissionLoopStarted || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}
	if (const AMyLevelContentTableOverride* LevelOverride = AMyLevelContentTableOverride::FindForWorld(GetWorld()))
	{
		if (!MissionDefinitionTable && LevelOverride->MissionDefinitionTable)
		{
			MissionDefinitionTable = LevelOverride->MissionDefinitionTable;
		}
		if (!MissionObjectiveTable && LevelOverride->MissionObjectiveTable)
		{
			MissionObjectiveTable = LevelOverride->MissionObjectiveTable;
		}
	}
	if (!MissionDefinitionTable)
	{
		MissionDefinitionTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/LeDuat/Systems/Streaming/DT_MissionDefinitions.DT_MissionDefinitions"),
			nullptr,
			LOAD_NoWarn);
	}
	if (!MissionObjectiveTable)
	{
		MissionObjectiveTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/LeDuat/Systems/Streaming/DT_MissionCombatObjectives.DT_MissionCombatObjectives"),
			nullptr,
			LOAD_NoWarn);
	}

	DungeonPartyRoster.Reset();
	for (const int32 UserIndex : ReadyUserIndexes)
	{
		if (MyStreamingSequencePolicy::IsAuthenticatedUserIndex(UserIndex))
		{
			DungeonPartyRoster.AddUnique(UserIndex);
		}
	}
	DungeonPartyRoster.Sort();

	bool bHasStandaloneRecipient = false;
#if !UE_BUILD_SHIPPING
	bHasStandaloneRecipient = StandaloneMissionRecipient.IsValid()
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode());
#endif
	if ((!bHasStandaloneRecipient && DungeonPartyRoster.IsEmpty())
		|| !MissionDefinitionTable
		|| !MissionObjectiveTable)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Mission Loop 시작 실패] RosterCount=%d DefinitionTable=%s ObjectiveTable=%s"),
			DungeonPartyRoster.Num(),
			*GetNameSafe(MissionDefinitionTable),
			*GetNameSafe(MissionObjectiveTable));
		return;
	}

	bMissionLoopStarted = true;
	TArray<FMyResolvedStreamingSequence> DeferredSequences =
		MoveTemp(DeferredMissionSequences);
	for (FMyResolvedStreamingSequence& Sequence : DeferredSequences)
	{
		SubmitSequence(MoveTemp(Sequence));
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Mission 공급 준비] PartyCount=%d DeferredSequences=%d"),
		DungeonPartyRoster.Num() + (bHasStandaloneRecipient ? 1 : 0),
		DeferredSequences.Num());
}

#if !UE_BUILD_SHIPPING
////////////////////////////
//! \author 장효제
//! \brief 비Shipping Standalone에서 초기화된 Demo Player를 명시적 Mission 수령자로 전달한다.
//! \param DemoPlayerState TryInitializeDemoPlayer가 초기화에 성공한 실제 로컬 PlayerState다.
void UMyStreamingManagerComponent::StartStandaloneMissionLoop(AMyPlayerState* DemoPlayerState)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode())
		|| !DemoPlayerState
		|| DemoPlayerState->IsAuthVerified())
	{
		return;
	}

	StandaloneMissionRecipient = DemoPlayerState;
	StartMissionLoop(TArray<int32>{});
}
#endif

////////////////////////////
//! \author 장효제
//! \brief 잠수 감지에서 재사용 가능한 AntiAFK Definition의 시작 Sequence를 요청한다.
//! \return 시작 Sequence 요청이 접수되었으면 true다.
bool UMyStreamingManagerComponent::TryStartAntiAFKMission()
{
	const AActor* OwnerActor = GetOwner();
	if (!bMissionLoopStarted || !OwnerActor || !OwnerActor->HasAuthority()
		|| !GetWorld() || !MissionDefinitionTable)
	{
		return false;
	}

	for (const FName RowName : MissionDefinitionTable->GetRowNames())
	{
		const FMyMissionDefinitionRow* Definition =
			MissionDefinitionTable->FindRow<FMyMissionDefinitionRow>(
				RowName,
				TEXT("UMyStreamingManagerComponent::TryStartAntiAFKMission"),
				false);
		if (!Definition)
		{
			continue;
		}

		const FMyMissionCombatObjectiveRow* Objective = nullptr;
		if (!MyMissionPolicy::IsAntiAFKStartCandidate(
				Definition->MissionTag,
				Definition->StartSequenceId,
				IsDefinitionValid(*Definition, Objective),
				HasActiveMissionWithTag(Definition->MissionTag)))
		{
			continue;
		}

		// 어떤 Sequence가 재생되는지는 조건 표만 정한다. 표가 정하지 못하면 재생하지
		// 않는다. 어떤 Definition을 쓸지는 위 IsAntiAFKStartCandidate가 Definition의
		// StartSequenceId로 고르므로, 둘이 어긋나면 validator가 잡는다.
		FMyStreamingSequenceRequest Request;
		Request.SequenceExecutionId = FGuid::NewGuid();
		Request.SequenceId = ResolveAntiAFKSequenceId(MyStreamingAntiAFKRuleNames::Enter);
		if (Request.SequenceId.IsNone())
		{
			return false;
		}
		Request.BusyPolicy = EMyStreamingSequenceBusyPolicy::Queue;
		Request.SourceEventTag = MyGameplayTags::Streaming_Event_Mission_Start;
		return RequestSequence(Request).IsAccepted();
	}

	return false;
}

////////////////////////////
//! \author 장효제
//! \brief 같은 종류 태그의 Mission이 이미 진행 중인지 검사한다.
//! \param MissionTag 검사할 Mission 종류 태그다.
//! \return State가 Active인 같은 태그의 서버 원본이 있으면 true다.
bool UMyStreamingManagerComponent::HasActiveMissionWithTag(const FGameplayTag MissionTag) const
{
	return MissionStates.ContainsByPredicate([MissionTag](const FMyMissionServerState& Mission)
	{
		return Mission.State == EMyMissionState::Active
			&& Mission.Definition.MissionTag.MatchesTagExact(MissionTag);
	});
}

////////////////////////////
//! \author 장효제
//! \brief StartMission Step의 태그와 시작 Sequence에 정확히 일치하는 Mission 서버 원본을 만든다.
//! \param MissionTag 시작할 Mission 종류 태그다.
//! \param SourceSequenceId StartMission Step을 재생 중인 SequenceId다.
//! \return Mission을 실제로 시작했으면 true다.
bool UMyStreamingManagerComponent::TryActivateMission(
	const FGameplayTag MissionTag,
	const FName SourceSequenceId)
{
	if (!bMissionLoopStarted || !GetWorld() || !MissionDefinitionTable
		|| !MissionObjectiveTable)
	{
		return false;
	}

	FName DefinitionRowName;
	const FMyMissionDefinitionRow* Definition = nullptr;
	for (const FName RowName : MissionDefinitionTable->GetRowNames())
	{
		const FMyMissionDefinitionRow* Candidate =
			MissionDefinitionTable->FindRow<FMyMissionDefinitionRow>(
				RowName,
				TEXT("UMyStreamingManagerComponent::TryActivateMission"),
				false);
		if (Candidate
			&& MyMissionPolicy::DoesDefinitionMatchStartRequest(
				Candidate->MissionTag,
				Candidate->StartSequenceId,
				MissionTag,
				SourceSequenceId))
		{
			DefinitionRowName = RowName;
			Definition = Candidate;
			break;
		}
	}

	const FMyMissionCombatObjectiveRow* Objective = nullptr;
	TArray<int32> AssigneeUserIndexes;
	TArray<FGameplayTag> AssigneeCharacterTags;
	const bool bDefinitionValid = Definition && IsDefinitionValid(*Definition, Objective);
	const EMyMissionActivationRejection Rejection = MyMissionPolicy::CheckMissionActivation(
		MissionTag,
		SourceSequenceId,
		UsedMissionTags.Contains(MissionTag),
		HasActiveMissionWithTag(MissionTag),
		bDefinitionValid,
		bDefinitionValid
			&& ResolveAssignees(*Definition, AssigneeUserIndexes, AssigneeCharacterTags));
	if (Rejection != EMyMissionActivationRejection::None)
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[Mission 시작 거부] SequenceId=%s MissionTag=%s 원인=%s"),
			*SourceSequenceId.ToString(),
			*MissionTag.ToString(),
			*UEnum::GetValueAsString(Rejection));
		return false;
	}

	const bool bAntiAFK = MyMissionPolicy::IsAntiAFKMissionTag(MissionTag);
	const float ServerTime = GetMissionServerTime();
	FMyMissionServerState& Mission = MissionStates.AddDefaulted_GetRef();
	Mission.MissionInstanceId = FGuid::NewGuid();
	Mission.DefinitionRowName = DefinitionRowName;
	Mission.Definition = *Definition;
	Mission.Objective = *Objective;
	Mission.AssigneeUserIndexes = MoveTemp(AssigneeUserIndexes);
	Mission.AssigneeCharacterTags = MoveTemp(AssigneeCharacterTags);
	Mission.State = EMyMissionState::Active;
	Mission.ResolvedMesoDelta = FMath::RandRange(
		Definition->CompletionMesoMin,
		Definition->CompletionMesoMax);
	Mission.ActivatedAtServerTime = ServerTime;
	Mission.MissionEndsAtServerTime =
		ServerTime + Definition->MissionTimeLimitSeconds;

	// GameStart 목표는 게임 시작부터의 파티 누계를 그대로 쓴다. 활성화 시점의 누계를
	// 심어 두면 이후 증가는 기존 경로가 그대로 처리해 항상 전역 누계와 같아진다.
	// MissionStart 목표(제한시간 안에 몇 마리)는 0에서 시작해야 하므로 심지 않는다.
	if (Mission.Objective.CountOrigin == EMyMissionCountOrigin::GameStart)
	{
		switch (Mission.Objective.ConditionType)
		{
		case EMyMissionConditionType::SkillUseCount:
			Mission.ProgressCount = CountPartySkillUses(Mission.Objective.SkillTag);
			break;
		case EMyMissionConditionType::EventCount:
			Mission.ProgressCount = CountPartyEvents(
				Mission.Objective.EventTag, FGameplayTag(), 0.0f, 0);
			break;
		case EMyMissionConditionType::ItemCount:
			Mission.ProgressCount = CountPartyItems(
				Mission.Objective.EventTag, Mission.Objective.ItemId, 0.0f, 0);
			break;
		default:
			Mission.ProgressCount =
				CountPartyKills(Mission.Objective.TargetTag, 0.0f, false);
			break;
		}
		UE_LOG(LogStreamingManager, Log,
			TEXT("[Mission 누계 승계] InstanceId=%s Target=%s Progress=%d/%d"),
			*Mission.MissionInstanceId.ToString(),
			*Mission.Objective.TargetTag.ToString(),
			Mission.ProgressCount,
			Mission.Objective.RequiredCount);
	}
	if (!bAntiAFK)
	{
		UsedMissionTags.Add(Definition->MissionTag);
	}

	FTimerDelegate TimeoutDelegate;
	TimeoutDelegate.BindUObject(this, &ThisClass::HandleMissionTimeout, Mission.MissionInstanceId);
	GetWorld()->GetTimerManager().SetTimer(
		Mission.MissionTimeoutTimerHandle,
		TimeoutDelegate,
		Definition->MissionTimeLimitSeconds,
		false);

	int32 RecipientCount = Mission.AssigneeUserIndexes.Num();
#if !UE_BUILD_SHIPPING
	RecipientCount += StandaloneMissionRecipient.IsValid() ? 1 : 0;
#endif

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Mission 시작] Trigger=Sequence SequenceId=%s InstanceId=%s Row=%s MissionTag=%s GodTag=%s PartyCount=%d RequiredKills=%d Meso=%d EndsAt=%.3f"),
		*SourceSequenceId.ToString(),
		*Mission.MissionInstanceId.ToString(),
		*Mission.DefinitionRowName.ToString(),
		*Mission.Definition.MissionTag.ToString(),
		*Mission.Definition.ProposerGodTag.ToString(),
		RecipientCount,
		Mission.Objective.RequiredCount,
		Mission.ResolvedMesoDelta,
		Mission.MissionEndsAtServerTime);

	// Notice 대상만 채운다. 지급 수령자(RecipientUserIndex/RecipientUserIndexes)는 건드리지 않는다.
	// 같은 Sequence의 뒤 Step이 Donation이면 요청이 확정한 수령자를 그대로 써야 한다.
	ActiveSequence.MissionNoticeUserIndexes = DungeonPartyRoster;
#if !UE_BUILD_SHIPPING
	ActiveSequence.MissionNoticeStandaloneRecipient = StandaloneMissionRecipient;
#endif
	PublishMissionViews();

	// 누계를 승계해 시작하자마자 목표를 이미 넘긴 경우다. 다음 처치를 기다리면
	// 처치가 더 없을 때 완료 대신 만료가 되므로 여기서 끝낸다.
	// Mission 참조는 배열이 바뀌면 무효가 되므로 값만 먼저 꺼내 둔다.
	const FGuid ActivatedInstanceId = Mission.MissionInstanceId;
	const bool bAlreadySatisfied = Mission.Objective.RequiredCount > 0
		&& Mission.ProgressCount >= Mission.Objective.RequiredCount;
	if (bAlreadySatisfied)
	{
		UE_LOG(LogStreamingManager, Log,
			TEXT("[Mission 즉시 완료] InstanceId=%s 원인=누계 승계로 목표 도달"),
			*ActivatedInstanceId.ToString());
		FinishActiveMission(ActivatedInstanceId, EMyMissionState::Completed);
	}
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief 첫 버전의 FixedCharacter/AllParty·SharedTotal·KillCount·완료 Meso 계약을 런타임에서도 방어한다.
//! \param Definition 검사할 Mission Definition이다.
//! \param OutObjective Objective 행을 반환한다.
//! \return 첫 플레이 버전에서 시작 가능한 행이면 true다.
bool UMyStreamingManagerComponent::IsDefinitionValid(
	const FMyMissionDefinitionRow& Definition,
	const FMyMissionCombatObjectiveRow*& OutObjective) const
{
	OutObjective = nullptr;
	if (!MissionObjectiveTable
		|| !Definition.MissionTag.IsValid()
		|| !Definition.ProposerGodTag.IsValid()
		|| Definition.StartSequenceId.IsNone()
		|| Definition.ObjectiveRow.IsNone()
		|| Definition.ProgressPolicy != EMyMissionProgressPolicy::SharedTotal
		|| Definition.CompletionMesoMin > Definition.CompletionMesoMax
		|| (Definition.ProposerGodTag.MatchesTagExact(MyGameplayTags::God_Set)
			? Definition.CompletionMesoMax >= 0
			: Definition.CompletionMesoMin <= 0)
		|| !FMath::IsFinite(Definition.MissionTimeLimitSeconds)
		|| Definition.MissionTimeLimitSeconds <= 0.0f
		|| Definition.CompletionSequenceId.IsNone())
	{
		return false;
	}

	const bool bAllParty = Definition.AssigneeSelector == EMyMissionAssigneeSelector::AllParty
		&& Definition.FixedAssigneeCharacters.IsEmpty();
	const bool bFixedCharacter = Definition.AssigneeSelector == EMyMissionAssigneeSelector::FixedCharacter
		&& Definition.FixedAssigneeCharacters.Num() == 1
		&& Definition.FixedAssigneeCharacters[0].IsValid()
		&& Definition.FixedAssigneeCharacters[0].MatchesTag(MyGameplayTags::Character_Player)
		&& !Definition.FixedAssigneeCharacters[0].MatchesTagExact(MyGameplayTags::Character_Player);
	if (!bAllParty && !bFixedCharacter)
	{
		return false;
	}

	OutObjective = MissionObjectiveTable->FindRow<FMyMissionCombatObjectiveRow>(
		Definition.ObjectiveRow,
		TEXT("UMyStreamingManagerComponent::IsDefinitionValid"),
		false);
	return OutObjective
		&& (OutObjective->ConditionType == EMyMissionConditionType::KillCount
			|| OutObjective->ConditionType == EMyMissionConditionType::SkillUseCount
			|| OutObjective->ConditionType == EMyMissionConditionType::EventCount
			|| OutObjective->ConditionType == EMyMissionConditionType::ItemCount)
		&& !OutObjective->SkillTag.IsValid()
		&& OutObjective->RequiredCount > 0
		&& FMath::IsNearlyZero(OutObjective->RequiredAmount);
}

////////////////////////////
//! \author 장효제
//! \brief Definition의 선택 규칙을 실제 파티 PlayerState와 CharacterTag로 해석한다.
//! \param Definition 대상 선택 규칙을 가진 Mission Definition이다.
//! \param OutUserIndexes 운영 Mission 수행자 UserIndex 목록이다.
//! \param OutCharacterTags UI와 기여 판정에 사용할 확정 CharacterTag 목록이다.
//! \return AllParty 전원 또는 FixedCharacter 한 명을 온전히 찾았으면 true다.
bool UMyStreamingManagerComponent::ResolveAssignees(
	const FMyMissionDefinitionRow& Definition,
	TArray<int32>& OutUserIndexes,
	TArray<FGameplayTag>& OutCharacterTags) const
{
	OutUserIndexes.Reset();
	OutCharacterTags.Reset();

#if !UE_BUILD_SHIPPING
	const AActor* OwnerActor = GetOwner();
	if (StandaloneMissionRecipient.IsValid()
		&& OwnerActor
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode()))
	{
		const FGameplayTag LocalCharacterTag =
			MyGameplayTags::GetPlayerCharacterTag(StandaloneMissionRecipient->GetSelectedCharacterId());
		if (Definition.AssigneeSelector == EMyMissionAssigneeSelector::AllParty)
		{
			if (LocalCharacterTag.IsValid())
			{
				OutCharacterTags.Add(LocalCharacterTag);
			}
			return true;
		}
		if (Definition.AssigneeSelector == EMyMissionAssigneeSelector::FixedCharacter
			&& LocalCharacterTag.MatchesTagExact(Definition.FixedAssigneeCharacters[0]))
		{
			OutCharacterTags.Add(LocalCharacterTag);
			return true;
		}
		return false;
	}
#endif

	for (const int32 UserIndex : DungeonPartyRoster)
	{
		AMyPlayerState* PlayerState = FindAuthenticatedPlayerState(UserIndex);
		if (!PlayerState)
		{
			return false;
		}

		const FGameplayTag CharacterTag =
			MyGameplayTags::GetPlayerCharacterTag(PlayerState->GetSelectedCharacterId());
		if (Definition.AssigneeSelector == EMyMissionAssigneeSelector::AllParty)
		{
			OutUserIndexes.Add(UserIndex);
			if (CharacterTag.IsValid())
			{
				OutCharacterTags.AddUnique(CharacterTag);
			}
		}
		else if (CharacterTag.MatchesTagExact(Definition.FixedAssigneeCharacters[0]))
		{
			OutUserIndexes.Add(UserIndex);
			OutCharacterTags.Add(CharacterTag);
			return true;
		}
	}

	return Definition.AssigneeSelector == EMyMissionAssigneeSelector::AllParty
		&& !OutUserIndexes.IsEmpty()
		&& OutUserIndexes.Num() == DungeonPartyRoster.Num();
}

////////////////////////////
//! \author 장효제
//! \brief 운영 파티 UserIndex와 정확히 일치하는 인증 PlayerState를 찾는다.
//! \param UserIndex 인증된 양수 사용자 식별자다.
//! \return 인증·소유 Controller·UserIndex가 모두 유효한 PlayerState다.
AMyPlayerState* UMyStreamingManagerComponent::FindAuthenticatedPlayerState(int32 UserIndex) const
{
	if (!MyStreamingSequencePolicy::IsAuthenticatedUserIndex(UserIndex))
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (MyPlayerState
			&& MyPlayerState->IsAuthVerified()
			&& MyPlayerState->GetUserIndex() == UserIndex
			&& MyPlayerState->GetOwningController())
		{
			return MyPlayerState;
		}
	}
	return nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief 서버 Combat 사실을 조건에 맞는 모든 Active Mission이 독립적으로 소비한다.
//! \param Channel 수신한 GameplayMessage 채널이다.
//! \param Payload 서버 전투 사실 Payload다.
void UMyStreamingManagerComponent::HandleMissionCombatPayload(
	FGameplayTag Channel,
	const FMyStreamingCombatPayload& Payload)
{
	const float ServerTime = GetMissionServerTime();
	TArray<TPair<FGuid, EMyMissionState>> FinishedMissions;
	bool bProgressChanged = false;
	for (FMyMissionServerState& Mission : MissionStates)
	{
		if (Mission.State != EMyMissionState::Active)
		{
			continue;
		}

		const bool bIsSkillObjective =
			Mission.Objective.ConditionType == EMyMissionConditionType::SkillUseCount;
		const bool bContributes = bIsSkillObjective
			? MyMissionPolicy::IsSkillUseContribution(
				Payload.EventTag,
				Payload.SkillTag,
				Payload.InstigatorTag,
				Mission.Objective.SkillTag)
			: MyMissionPolicy::IsKillContribution(
				Payload.EventTag,
				Payload.bIsKill,
				Payload.InstigatorTag,
				Payload.TargetTag,
				Mission.Objective.TargetTag);

		if (ServerTime <= Mission.MissionEndsAtServerTime
			&& MyMissionPolicy::IsAssigneeContribution(
				Mission.Definition.AssigneeSelector,
				Mission.AssigneeCharacterTags,
				Payload.InstigatorTag)
			&& bContributes)
		{
			++Mission.ProgressCount;
			bProgressChanged = true;
			UE_LOG(LogStreamingManager, Log,
				TEXT("[Mission 진행] InstanceId=%s Progress=%d/%d Target=%s"),
				*Mission.MissionInstanceId.ToString(),
				Mission.ProgressCount,
				Mission.Objective.RequiredCount,
				*Payload.TargetTag.ToString());
		}

		const EMyMissionState Result = MyMissionPolicy::ResolveKillMissionState(
			Mission.ProgressCount,
			Mission.Objective.RequiredCount,
			ServerTime,
			Mission.MissionEndsAtServerTime);
		if (Result == EMyMissionState::Completed || Result == EMyMissionState::Expired)
		{
			FinishedMissions.Emplace(Mission.MissionInstanceId, Result);
		}
	}

	if (bProgressChanged)
	{
		PublishMissionViews();
	}
	for (const TPair<FGuid, EMyMissionState>& FinishedMission : FinishedMissions)
	{
		FinishActiveMission(FinishedMission.Key, FinishedMission.Value);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 절대 종료 시각에 완료 진행도를 먼저 확인하고 아니면 만료로 끝낸다.
//! \param MissionInstanceId 만료 시각에 도달한 Mission Instance ID다.
void UMyStreamingManagerComponent::HandleMissionTimeout(FGuid MissionInstanceId)
{
	FMyMissionServerState* Mission = MissionStates.FindByPredicate([MissionInstanceId](const FMyMissionServerState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	if (!Mission || Mission->State != EMyMissionState::Active)
	{
		return;
	}

	const EMyMissionState Result = MyMissionPolicy::ResolveKillMissionState(
		Mission->ProgressCount,
		Mission->Objective.RequiredCount,
		GetMissionServerTime(),
		Mission->MissionEndsAtServerTime);
	FinishActiveMission(MissionInstanceId, Result);
}

////////////////////////////
//! \author 장효제
//! \brief Mission 완료 또는 만료를 한 번 확정하고 결과 연출과 View 제거를 처리한다.
//! \param MissionInstanceId 종료할 Mission Instance ID다.
//! \param Result Completed 또는 Expired 결과다.
void UMyStreamingManagerComponent::FinishActiveMission(
	FGuid MissionInstanceId,
	EMyMissionState Result)
{
	FMyMissionServerState* Mission = MissionStates.FindByPredicate([MissionInstanceId](const FMyMissionServerState& Candidate)
	{
		return Candidate.MissionInstanceId == MissionInstanceId;
	});
	if (!Mission
		|| Mission->State != EMyMissionState::Active
		|| (Result != EMyMissionState::Completed && Result != EMyMissionState::Expired))
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(Mission->MissionTimeoutTimerHandle);
	Mission->State = Result;

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Mission 결과] InstanceId=%s Result=%s Progress=%d/%d MesoDelta=%d"),
		*Mission->MissionInstanceId.ToString(),
		Result == EMyMissionState::Completed ? TEXT("Completed") : TEXT("Expired"),
		Mission->ProgressCount,
		Mission->Objective.RequiredCount,
		Mission->ResolvedMesoDelta);

	if (Result == EMyMissionState::Completed)
	{
		PublishMissionViews();
		RequestMissionSequence(
			*Mission,
			Mission->Definition.CompletionSequenceId,
			MyGameplayTags::Streaming_Event_Mission_Completed,
			Mission->ResolvedMesoDelta,
			true);

		FTimerDelegate RemovalDelegate;
		RemovalDelegate.BindUObject(this, &ThisClass::RemoveMissionView, MissionInstanceId);
		GetWorld()->GetTimerManager().SetTimer(
			Mission->MissionTimeoutTimerHandle,
			RemovalDelegate,
			2.0f,
			false);
	}
	else
	{
		RemoveMissionView(MissionInstanceId);
	}

}

////////////////////////////
//! \author 장효제
//! \brief 완료 연출 시간이 끝났거나 Mission이 만료되면 공개 View와 서버 원본을 제거한다.
//! \param MissionInstanceId 제거할 Mission Instance ID다.
void UMyStreamingManagerComponent::RemoveMissionView(FGuid MissionInstanceId)
{
	const int32 RemovedCount = MissionStates.RemoveAll([MissionInstanceId](const FMyMissionServerState& Mission)
	{
		return Mission.MissionInstanceId == MissionInstanceId
			&& Mission.State != EMyMissionState::Active;
	});
	if (RemovedCount > 0)
	{
		PublishMissionViews();
	}
}

////////////////////////////
//! \author 장효제
//! \brief Mission에서 확정한 God·Meso·파티를 기존 Streaming Sequence 요청에 전달한다.
//! \param Mission 요청 원본 Mission이다.
//! \param SequenceId 요청할 시작 또는 완료 Sequence다.
//! \param SourceEventTag 요청 출처 Mission 이벤트 태그다.
//! \param PayloadMesoAmount 완료 시 적용할 signed Meso 변화량이며 시작 Sequence는 0이다.
//! \param bAssigneesOnly true면 수행자만, false면 공개 파티 전원을 수령자로 지정한다.
void UMyStreamingManagerComponent::RequestMissionSequence(
	const FMyMissionServerState& Mission,
	FName SequenceId,
	FGameplayTag SourceEventTag,
	int32 PayloadMesoAmount,
	bool bAssigneesOnly)
{
	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = SequenceId;
	Request.BusyPolicy = EMyStreamingSequenceBusyPolicy::Queue;
	Request.SourceEventTag = SourceEventTag;
	Request.PayloadGodTag = Mission.Definition.ProposerGodTag;
	Request.PayloadMesoAmount = PayloadMesoAmount;
	Request.RecipientUserIndexes = bAssigneesOnly
		? Mission.AssigneeUserIndexes
		: DungeonPartyRoster;
#if !UE_BUILD_SHIPPING
	Request.StandaloneTestRecipient = StandaloneMissionRecipient;
#endif

	const FMyStreamingSequenceRequestResult RequestResult = RequestSequence(Request);
	UE_LOG(LogStreamingManager, Log,
		TEXT("[Mission Sequence 요청] SequenceId=%s ExecutionId=%s Accepted=%s Meso=%d"),
		*SequenceId.ToString(),
		*Request.SequenceExecutionId.ToString(),
		RequestResult.IsAccepted() ? TEXT("true") : TEXT("false"),
		PayloadMesoAmount);
}

////////////////////////////
//! \author 장효제
//! \brief 서버 Mission 원본을 종료 시각 순 공개 View로 변환해 DungeonGS에 반영한다.
void UMyStreamingManagerComponent::PublishMissionViews() const
{
	const UWorld* World = GetWorld();
	ADungeonGS* DungeonGS = World ? World->GetGameState<ADungeonGS>() : nullptr;
	if (!DungeonGS)
	{
		return;
	}

	TArray<FMyMissionPublicView> MissionViews;
	MissionViews.Reserve(MissionStates.Num());
	for (const FMyMissionServerState& Mission : MissionStates)
	{
		FMyMissionPublicView& View = MissionViews.AddDefaulted_GetRef();
		View.MissionInstanceId = Mission.MissionInstanceId;
		View.DefinitionRowName = Mission.DefinitionRowName;
		View.MissionTag = Mission.Definition.MissionTag;
		View.ProposerGodTag = Mission.Definition.ProposerGodTag;
		View.AssigneeCharacterTags = Mission.AssigneeCharacterTags;
		View.State = Mission.State;
		View.ResolvedMesoDelta = Mission.ResolvedMesoDelta;
		View.ActivatedAtServerTime = Mission.ActivatedAtServerTime;
		View.MissionEndsAtServerTime = Mission.MissionEndsAtServerTime;

		FMyMissionObjectiveView& ObjectiveView = View.Objectives.AddDefaulted_GetRef();
		ObjectiveView.ObjectiveRowName = Mission.Definition.ObjectiveRow;
		ObjectiveView.ProgressCount = Mission.ProgressCount;
		ObjectiveView.RequiredCount = Mission.Objective.RequiredCount;
		ObjectiveView.bCompleted = Mission.State == EMyMissionState::Completed;
	}

	MissionViews.StableSort([](const FMyMissionPublicView& Left, const FMyMissionPublicView& Right)
	{
		if (!FMath::IsNearlyEqual(Left.MissionEndsAtServerTime, Right.MissionEndsAtServerTime))
		{
			return Left.MissionEndsAtServerTime < Right.MissionEndsAtServerTime;
		}
		return Left.ActivatedAtServerTime < Right.ActivatedAtServerTime;
	});
	DungeonGS->SetMissionViews(MissionViews);
}

////////////////////////////
//! \author 장효제
//! \brief Mission 판정에 사용할 권위 서버 월드 시각을 반환한다.
//! \return GameState 서버 시각이며 없으면 World 시간을 사용한다.
float UMyStreamingManagerComponent::GetMissionServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
}
