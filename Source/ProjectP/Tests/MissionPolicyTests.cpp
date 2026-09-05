////////////////////////////
//! \file MissionPolicyTests.cpp
//! \brief Mission 시작 판정, 처치·종료와 MissionStart Sequence 계약의 순수 정책 자동화 테스트다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Streaming/MyMissionTypes.h"
#include "Streaming/MyStreamingManagerComponent.h"
#include "MyGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMissionFirstPlayablePolicyTest,
	"ProjectP.Mission.FirstPlayablePolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 파티 Kill 판정, 성공 우선 종료와 Mission 태그 분류 계약을 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 모든 첫 버전 계약이 통과하면 true다.
bool FMissionFirstPlayablePolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyMissionPolicy;

	TestTrue(
		TEXT("플레이어의 적 처치는 공동 진행"),
		IsKillContribution(
			MyGameplayTags::Streaming_Event_Combat_Kill,
			true,
			MyGameplayTags::Character_Player,
			MyGameplayTags::Character_Enemy,
			MyGameplayTags::Character_Enemy));
	TestFalse(
		TEXT("Kill이 아니면 진행하지 않음"),
		IsKillContribution(
			MyGameplayTags::Streaming_Event_Combat_Hit,
			false,
			MyGameplayTags::Character_Player,
			MyGameplayTags::Character_Enemy,
			MyGameplayTags::Character_Enemy));

	const TArray<FGameplayTag> NeferAssignee = {MyGameplayTags::Character_Player_Nefer};
	TestTrue(
		TEXT("AllParty는 모든 구체 플레이어 태그 기여 허용"),
		IsAssigneeContribution(
			EMyMissionAssigneeSelector::AllParty,
			TArray<FGameplayTag>{},
			MyGameplayTags::Character_Player_Inpu));
	TestTrue(
		TEXT("FixedCharacter는 지정 캐릭터 기여 허용"),
		IsAssigneeContribution(
			EMyMissionAssigneeSelector::FixedCharacter,
			NeferAssignee,
			MyGameplayTags::Character_Player_Nefer));
	TestFalse(
		TEXT("FixedCharacter는 다른 캐릭터 기여 거부"),
		IsAssigneeContribution(
			EMyMissionAssigneeSelector::FixedCharacter,
			NeferAssignee,
			MyGameplayTags::Character_Player_Heru));
	TestTrue(
		TEXT("캐릭터 ID 200은 인푸 태그"),
		MyGameplayTags::GetPlayerCharacterTag(200).MatchesTagExact(MyGameplayTags::Character_Player_Inpu));

	TestEqual(
		TEXT("종료 시각에 목표가 차면 달성 우선"),
		ResolveKillMissionState(3, 3, 10.0f, 10.0f),
		EMyMissionState::Completed);
	TestEqual(
		TEXT("종료 시각에 목표 미달이면 만료"),
		ResolveKillMissionState(2, 3, 10.0f, 10.0f),
		EMyMissionState::Expired);
	TestEqual(
		TEXT("종료 전 목표 미달이면 Active"),
		ResolveKillMissionState(2, 3, 9.0f, 10.0f),
		EMyMissionState::Active);

	TestTrue(
		TEXT("Streaming.Mission.AntiAFK 하위 태그는 잠수 방지 Mission"),
		IsAntiAFKMissionTag(
			FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.AntiAFK.Set.Kill1"))));
	TestFalse(
		TEXT("일반 Mission 태그는 잠수 방지 Mission이 아님"),
		IsAntiAFKMissionTag(
			FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.Combat.KillEnemies.Set"))));

	// Kill 누적 회귀: 목표 직전까지는 Active를 유지하고 목표를 채운 순간에만 Completed로 확정한다.
	const int32 RequiredKillCount = 3;
	for (int32 ProgressCount = 0; ProgressCount < RequiredKillCount; ++ProgressCount)
	{
		TestEqual(
			*FString::Printf(TEXT("Kill %d/%d은 진행 중"), ProgressCount, RequiredKillCount),
			ResolveKillMissionState(ProgressCount, RequiredKillCount, 5.0f, 10.0f),
			EMyMissionState::Active);
	}
	TestEqual(
		TEXT("Kill 목표를 채우면 종료 전에도 달성"),
		ResolveKillMissionState(RequiredKillCount, RequiredKillCount, 5.0f, 10.0f),
		EMyMissionState::Completed);
	TestEqual(
		TEXT("Kill 초과 달성도 달성"),
		ResolveKillMissionState(RequiredKillCount + 1, RequiredKillCount, 5.0f, 10.0f),
		EMyMissionState::Completed);

	// 제한시간 만료 회귀: 종료 시각을 넘긴 미달 진행은 만료로 확정한다.
	TestEqual(
		TEXT("종료 시각을 지난 미달 진행은 만료"),
		ResolveKillMissionState(1, RequiredKillCount, 10.1f, 10.0f),
		EMyMissionState::Expired);
	TestEqual(
		TEXT("종료 시각을 지나도 목표를 채웠으면 달성 우선"),
		ResolveKillMissionState(RequiredKillCount, RequiredKillCount, 10.1f, 10.0f),
		EMyMissionState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMissionStartActivationPolicyTest,
	"ProjectP.Mission.StartActivationPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief StartMission Step이 Definition 한 행만 골라 서버 미션을 정확히 한 번 만드는 계약을 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 일치 판정과 네 가지 거부 사유가 모두 계약대로면 true다.
bool FMissionStartActivationPolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyMissionPolicy;

	const FGameplayTag ZoneMissionTag =
		FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.Combat.KillEnemies.Zone00"));
	const FGameplayTag OtherZoneMissionTag =
		FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.Combat.KillEnemies.Zone01"));
	const FGameplayTag AntiAFKMissionTag =
		FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.AntiAFK.Set.Kill1"));
	const FName StartSequenceId(TEXT("Seq_Mission_Zone00_Start"));
	const FName OtherSequenceId(TEXT("Seq_Mission_Zone01_Start"));

	// Definition 선택: 태그 완전일치와 StartSequenceId 일치가 동시에 성립할 때만 후보가 된다.
	TestTrue(
		TEXT("태그와 시작 Sequence가 모두 같으면 해당 Definition"),
		DoesDefinitionMatchStartRequest(
			ZoneMissionTag, StartSequenceId, ZoneMissionTag, StartSequenceId));
	TestFalse(
		TEXT("태그가 다르면 Definition이 아님"),
		DoesDefinitionMatchStartRequest(
			OtherZoneMissionTag, StartSequenceId, ZoneMissionTag, StartSequenceId));
	TestFalse(
		TEXT("시작 Sequence가 다르면 Definition이 아님"),
		DoesDefinitionMatchStartRequest(
			ZoneMissionTag, OtherSequenceId, ZoneMissionTag, StartSequenceId));
	TestFalse(
		TEXT("상위 태그는 완전일치가 아니므로 Definition이 아님"),
		DoesDefinitionMatchStartRequest(
			ZoneMissionTag,
			StartSequenceId,
			FGameplayTag::RequestGameplayTag(TEXT("Streaming.Mission.Combat.KillEnemies")),
			StartSequenceId));
	TestFalse(
		TEXT("MissionTag가 비면 Definition을 고르지 않음"),
		DoesDefinitionMatchStartRequest(
			ZoneMissionTag, StartSequenceId, FGameplayTag(), StartSequenceId));

	// 정상 경로: 서버 미션을 실제로 만든다.
	TestEqual(
		TEXT("계약을 모두 만족하면 Mission 생성 진행"),
		CheckMissionActivation(ZoneMissionTag, StartSequenceId, false, false, true, true),
		EMyMissionActivationRejection::None);

	// 미등록 태그: DataTable에 없는 태그와 유효하지 않은 태그를 각각 거부한다.
	TestEqual(
		TEXT("등록되지 않은 태그는 유효하지 않아 거부"),
		CheckMissionActivation(
			FGameplayTag::RequestGameplayTag(
				TEXT("Streaming.Mission.Combat.KillEnemies.NotRegistered"), false),
			StartSequenceId,
			false,
			false,
			true,
			true),
		EMyMissionActivationRejection::InvalidRequest);
	TestEqual(
		TEXT("재생 SequenceId가 없으면 거부"),
		CheckMissionActivation(ZoneMissionTag, NAME_None, false, false, true, true),
		EMyMissionActivationRejection::InvalidRequest);
	TestEqual(
		TEXT("대응 Definition을 찾지 못하면 거부"),
		CheckMissionActivation(ZoneMissionTag, StartSequenceId, false, false, false, true),
		EMyMissionActivationRejection::DefinitionNotFound);

	// 중복: 소비한 일반 Mission 태그와 이미 Active인 같은 태그를 각각 거부한다.
	TestEqual(
		TEXT("이미 소비한 일반 Mission 태그는 재시작 거부"),
		CheckMissionActivation(ZoneMissionTag, StartSequenceId, true, false, true, true),
		EMyMissionActivationRejection::AlreadyUsed);
	TestEqual(
		TEXT("같은 태그가 Active면 중복 시작 거부"),
		CheckMissionActivation(ZoneMissionTag, StartSequenceId, false, true, true, true),
		EMyMissionActivationRejection::AlreadyActive);
	TestEqual(
		TEXT("AntiAFK Mission은 소비 이력과 무관하게 재사용 허용"),
		CheckMissionActivation(AntiAFKMissionTag, StartSequenceId, true, false, true, true),
		EMyMissionActivationRejection::None);
	TestEqual(
		TEXT("AntiAFK Mission도 같은 태그 Active 중복은 거부"),
		CheckMissionActivation(AntiAFKMissionTag, StartSequenceId, true, true, true, true),
		EMyMissionActivationRejection::AlreadyActive);

	// 수행자 부재: Definition은 유효해도 수행자가 없으면 서버가 거부한다.
	TestEqual(
		TEXT("수행자를 해석하지 못하면 거부"),
		CheckMissionActivation(ZoneMissionTag, StartSequenceId, false, false, true, false),
		EMyMissionActivationRejection::AssigneeNotSatisfied);

	// AntiAFK 후보: 잠수 감지는 재사용 가능한 AntiAFK Definition의 시작 Sequence만 요청한다.
	TestTrue(
		TEXT("Active가 없는 유효한 AntiAFK Definition은 후보"),
		IsAntiAFKStartCandidate(AntiAFKMissionTag, StartSequenceId, true, false));
	TestFalse(
		TEXT("일반 Mission Definition은 AntiAFK 후보가 아님"),
		IsAntiAFKStartCandidate(ZoneMissionTag, StartSequenceId, true, false));
	TestFalse(
		TEXT("StartSequenceId가 없으면 AntiAFK 후보가 아님"),
		IsAntiAFKStartCandidate(AntiAFKMissionTag, NAME_None, true, false));
	TestFalse(
		TEXT("런타임 계약을 통과하지 못한 Definition은 후보가 아님"),
		IsAntiAFKStartCandidate(AntiAFKMissionTag, StartSequenceId, false, false));
	TestFalse(
		TEXT("같은 AntiAFK Mission이 Active면 후보가 아님"),
		IsAntiAFKStartCandidate(AntiAFKMissionTag, StartSequenceId, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMissionStartSequenceContractTest,
	"ProjectP.Streaming.Mission.StartSequenceContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief MissionStart Sequence의 Queue 전용 계약과 생성 실패 시 연출 억제를 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return BusyPolicy 허용과 방송 게이트가 계약대로면 true다.
bool FMissionStartSequenceContractTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	// 소스 계약 자체의 자기 정합성: Donation을 허용하면 반드시 수령자 전략이 있어야 한다.
	const TArray<EMyStreamingRuleSource> AllSources = {
		EMyStreamingRuleSource::Combat,
		EMyStreamingRuleSource::Meso,
		EMyStreamingRuleSource::Gimmick,
		EMyStreamingRuleSource::Zone,
		EMyStreamingRuleSource::ZoneDonation,
		EMyStreamingRuleSource::SmallTalk};
	for (const EMyStreamingRuleSource Source : AllSources)
	{
		const FMyStreamingRuleSourceContract Contract = GetRuleSourceContract(Source);
		if (Contract.IsReactionAllowed(EMyStreamingReaction::Reward))
		{
			TestNotEqual(
				TEXT("Donation 허용 소스는 수령자 전략이 있어야 함"),
				Contract.RecipientMode,
				EMyStreamingRecipientMode::None);
		}
	}

	// Step 조합에서 허용 소스가 갈리는지 확인한다.
	const auto IsAllowed = [](const EMyStreamingRuleSource Source,
		const EMyStreamingPresentationType Presentation,
		const EMyStreamingActionType Action,
		const bool bHasMissionTag,
		const EMyStreamingRewardSource RewardSource,
		const int32 RewardMin,
		const int32 RewardMax,
		const FName RewardItemId = NAME_None)
	{
		return IsRuleSequenceStepContractValid(
			Presentation, Action, bHasMissionTag, RewardSource, RewardMin, RewardMax,
			RewardItemId,
			GetRuleSourceContract(Source));
	};

	TestTrue(
		TEXT("Zone Rule은 Chat/None Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::Chat, EMyStreamingActionType::None,
			false, EMyStreamingRewardSource::None, 0, 0));
	TestFalse(
		TEXT("ZoneDonation은 Chat Step을 받지 않음"),
		IsAllowed(EMyStreamingRuleSource::ZoneDonation,
			EMyStreamingPresentationType::Chat, EMyStreamingActionType::None,
			false, EMyStreamingRewardSource::None, 0, 0));

	TestTrue(
		TEXT("Zone Rule은 MissionStart Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::MissionStart, EMyStreamingActionType::StartMission,
			true, EMyStreamingRewardSource::None, 0, 0));
	TestTrue(
		TEXT("Combat Rule도 MissionStart Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::MissionStart, EMyStreamingActionType::StartMission,
			true, EMyStreamingRewardSource::None, 0, 0));
	TestFalse(
		TEXT("SmallTalk은 MissionStart Step 거부"),
		IsAllowed(EMyStreamingRuleSource::SmallTalk,
			EMyStreamingPresentationType::MissionStart, EMyStreamingActionType::StartMission,
			true, EMyStreamingRewardSource::None, 0, 0));
	TestFalse(
		TEXT("MissionTag가 없는 MissionStart Step은 거부"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::MissionStart, EMyStreamingActionType::StartMission,
			false, EMyStreamingRewardSource::None, 0, 0));
	TestFalse(
		TEXT("MissionStart Step은 Meso를 직접 지급할 수 없음"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::MissionStart, EMyStreamingActionType::StartMission,
			true, EMyStreamingRewardSource::RollFromLine, 10, 20));

	TestTrue(
		TEXT("Combat Rule은 Donation Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::Donation, EMyStreamingActionType::GrantMeso,
			false, EMyStreamingRewardSource::RollFromLine, 10, 20));
	TestFalse(
		TEXT("수령자를 만들 수 없는 SmallTalk은 Donation Step 거부"),
		IsAllowed(EMyStreamingRuleSource::SmallTalk,
			EMyStreamingPresentationType::Donation, EMyStreamingActionType::GrantMeso,
			false, EMyStreamingRewardSource::RollFromLine, 10, 20));
	TestFalse(
		TEXT("Donation 금액 범위가 잘못되면 거부"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::Donation, EMyStreamingActionType::GrantMeso,
			false, EMyStreamingRewardSource::RollFromLine, 30, 20));
	TestFalse(
		TEXT("Rule 경로의 Donation은 Payload 금액을 쓸 수 없음"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::Donation, EMyStreamingActionType::GrantMeso,
			false, EMyStreamingRewardSource::Payload, 0, 0));
	TestFalse(
		TEXT("ApplyMesoDelta는 Mission 완료 전용이라 Rule 경로에서 거부"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::Donation, EMyStreamingActionType::ApplyMesoDelta,
			false, EMyStreamingRewardSource::Payload, 0, 0));

	TestFalse(
		TEXT("MissionTag가 붙은 Chat Step은 어느 소스에서도 거부"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::Chat, EMyStreamingActionType::None,
			true, EMyStreamingRewardSource::None, 0, 0));
	// Exp 보상은 Meso와 같은 수량 계약을 쓴다.
	TestTrue(
		TEXT("Zone Rule은 Exp 보상 Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::ExpReward, EMyStreamingActionType::GrantExp,
			false, EMyStreamingRewardSource::RollFromLine, 50, 80));
	TestFalse(
		TEXT("Exp 보상도 수량 범위가 잘못되면 거부"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::ExpReward, EMyStreamingActionType::GrantExp,
			false, EMyStreamingRewardSource::RollFromLine, 80, 50));
	TestFalse(
		TEXT("Exp 보상에 ItemId가 붙으면 거부"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::ExpReward, EMyStreamingActionType::GrantExp,
			false, EMyStreamingRewardSource::RollFromLine, 50, 80,
			TEXT("Potion_Small")));
	TestFalse(
		TEXT("수령자를 만들 수 없는 SmallTalk은 Exp 보상 거부"),
		IsAllowed(EMyStreamingRuleSource::SmallTalk,
			EMyStreamingPresentationType::ExpReward, EMyStreamingActionType::GrantExp,
			false, EMyStreamingRewardSource::RollFromLine, 50, 80));

	// Item 보상도 Meso·Exp와 같은 수량 계약을 쓴다. 개수를 RewardMin~RewardMax에서 뽑는다.
	TestTrue(
		TEXT("Combat Rule은 Item 보상 Step 허용"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::ItemReward, EMyStreamingActionType::GrantItem,
			false, EMyStreamingRewardSource::RollFromLine, 1, 3,
			TEXT("Potion_Small")));
	TestTrue(
		TEXT("개수가 고정이면 최소와 최대를 같게 적는다"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::ItemReward, EMyStreamingActionType::GrantItem,
			false, EMyStreamingRewardSource::RollFromLine, 1, 1,
			TEXT("Potion_Small")));
	TestFalse(
		TEXT("ItemId가 없는 Item 보상은 거부"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::ItemReward, EMyStreamingActionType::GrantItem,
			false, EMyStreamingRewardSource::RollFromLine, 1, 3,
			NAME_None));
	TestFalse(
		TEXT("개수 범위가 없는 Item 보상은 거부"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::ItemReward, EMyStreamingActionType::GrantItem,
			false, EMyStreamingRewardSource::None, 0, 0,
			TEXT("Potion_Small")));
	TestFalse(
		TEXT("개수 범위가 뒤집힌 Item 보상은 거부"),
		IsAllowed(EMyStreamingRuleSource::Combat,
			EMyStreamingPresentationType::ItemReward, EMyStreamingActionType::GrantItem,
			false, EMyStreamingRewardSource::RollFromLine, 3, 1,
			TEXT("Potion_Small")));
	TestFalse(
		TEXT("Chat Step에 ItemId가 붙으면 거부"),
		IsAllowed(EMyStreamingRuleSource::Zone,
			EMyStreamingPresentationType::Chat, EMyStreamingActionType::None,
			false, EMyStreamingRewardSource::None, 0, 0,
			TEXT("Potion_Small")));

	// 상태 변경 Step은 종류와 무관하게 Queue 전용이다.
	TestFalse(TEXT("None은 상태 변경이 아님"),
		IsStatefulActionType(EMyStreamingActionType::None));
	TestTrue(TEXT("StartMission은 상태 변경"),
		IsStatefulActionType(EMyStreamingActionType::StartMission));
	TestTrue(TEXT("GrantMeso는 상태 변경"),
		IsStatefulActionType(EMyStreamingActionType::GrantMeso));
	TestTrue(TEXT("ApplyMesoDelta는 상태 변경"),
		IsStatefulActionType(EMyStreamingActionType::ApplyMesoDelta));

	TestTrue(
		TEXT("상태 변경이 없으면 Drop도 허용"),
		IsStatefulSequenceBusyPolicyAllowed(false, EMyStreamingSequenceBusyPolicy::Drop));
	TestFalse(
		TEXT("상태 변경 Sequence는 Drop 거부"),
		IsStatefulSequenceBusyPolicyAllowed(true, EMyStreamingSequenceBusyPolicy::Drop));
	TestTrue(
		TEXT("상태 변경 Sequence는 Queue 허용"),
		IsStatefulSequenceBusyPolicyAllowed(true, EMyStreamingSequenceBusyPolicy::Queue));

	TestTrue(
		TEXT("Mission을 만든 MissionStart Step은 채팅과 Notice 표시"),
		ShouldBroadcastStepPresentation(
			EMyStreamingPresentationType::MissionStart, true, true));
	TestFalse(
		TEXT("Mission 생성에 실패한 MissionStart Step은 채팅과 Notice 억제"),
		ShouldBroadcastStepPresentation(
			EMyStreamingPresentationType::MissionStart, true, false));
	TestTrue(
		TEXT("StartMission이 아닌 Chat Step은 기존대로 방송"),
		ShouldBroadcastStepPresentation(
			EMyStreamingPresentationType::Chat, false, false));
	TestFalse(
		TEXT("Donation Step은 일반 Chat 방송 대상이 아님"),
		ShouldBroadcastStepPresentation(
			EMyStreamingPresentationType::Donation, false, false));
	return true;
}

#endif
