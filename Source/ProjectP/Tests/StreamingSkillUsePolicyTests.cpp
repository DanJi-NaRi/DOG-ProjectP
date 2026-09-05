////////////////////////////
//! \file StreamingSkillUsePolicyTests.cpp
//! \brief 스킬 사용 Rule의 문턱 통과·스킬 매칭·미사용 판정의 순수 정책 자동화 테스트다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Streaming/MyMissionTypes.h"
#include "Streaming/MyStreamingManagerComponent.h"
#include "Streaming/MyStreamingSkillUseRuleTypes.h"
#include "MyGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSkillUsePolicyTest,
	"ProjectP.Streaming.SkillUse.Policy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 사용 누계 문턱과 스킬 계층 매칭, 미사용 Rule 구분이 계약대로인지 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 판정이 계약대로면 true다.
bool FStreamingSkillUsePolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSkillUseRulePolicy;

	// 문턱을 넘는 순간에만 발동한다. 처치 누계와 같은 판정이다.
	TestTrue(TEXT("19에서 20으로 갈 때 20번 문턱 통과"), DidCrossThreshold(19, 20, 20));
	TestFalse(TEXT("20에서 21로 가면 이미 넘긴 뒤라 발동하지 않음"), DidCrossThreshold(20, 21, 20));
	TestFalse(TEXT("문턱에 못 미치면 발동하지 않음"), DidCrossThreshold(1, 2, 20));
	TestFalse(TEXT("횟수가 0인 Rule은 발동하지 않음"), DidCrossThreshold(0, 1, 0));

	// 미사용 Rule과 누적 Rule의 구분
	TestFalse(TEXT("0초는 누적 사용 Rule"), IsIdleRule(0.0f));
	TestTrue(TEXT("45초는 미사용 Rule"), IsIdleRule(45.0f));

	const FGameplayTag AnyJump = MyGameplayTags::Skill_Common_Jump.GetTag();
	const FGameplayTag JumpInPlace = MyGameplayTags::Skill_Common_Jump_InPlace.GetTag();
	const FGameplayTag JumpMoving = MyGameplayTags::Skill_Common_Jump_Moving.GetTag();

	// 빈 Rule 태그는 모든 스킬을 센다.
	TestTrue(TEXT("빈 Rule 태그는 어떤 스킬이든 센다"), DoesUseMatchSkill(JumpInPlace, FGameplayTag()));
	// 계층 매칭이므로 점프 상위 태그 Rule은 두 종류를 모두 센다.
	TestTrue(TEXT("점프 Rule은 제자리 점프를 센다"), DoesUseMatchSkill(JumpInPlace, AnyJump));
	TestTrue(TEXT("점프 Rule은 이동 중 점프도 센다"), DoesUseMatchSkill(JumpMoving, AnyJump));
	// 제자리 점프 Rule은 이동 중 점프를 세지 않는다. "움직이지 않고"의 근거다.
	TestFalse(
		TEXT("제자리 점프 Rule은 이동 중 점프를 세지 않는다"),
		DoesUseMatchSkill(JumpMoving, JumpInPlace));
	TestFalse(
		TEXT("제자리 점프 Rule은 다른 스킬을 세지 않는다"),
		DoesUseMatchSkill(MyGameplayTags::Skill_Heru_BasicAttack.GetTag(), JumpInPlace));

	// 스킬 사용 소스도 파티 전원이 보상 수령자다. 사실에 UserIndex가 없기 때문이다.
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::SkillUse);
	TestEqual(
		TEXT("스킬 사용 보상 수령자는 파티 전원"),
		Contract.RecipientMode,
		EMyStreamingRecipientMode::Party);
	TestTrue(
		TEXT("스킬 사용은 채팅 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::Chat));
	TestTrue(
		TEXT("스킬 사용은 보상 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::Reward));
	TestTrue(
		TEXT("스킬 사용은 미션 시작 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::MissionStart));

	// Mission 목표도 같은 스킬 매칭을 쓴다. 두 쪽이 어긋나면 진행도가 달라진다.
	TestTrue(
		TEXT("스킬 목표는 계층 하위 사용을 센다"),
		MyMissionPolicy::IsSkillUseContribution(
			MyGameplayTags::Streaming_Event_Combat_SkillUsed,
			JumpInPlace,
			MyGameplayTags::Character_Player.GetTag(),
			AnyJump));
	TestFalse(
		TEXT("처치 사실은 스킬 목표에 기여하지 않는다"),
		MyMissionPolicy::IsSkillUseContribution(
			MyGameplayTags::Streaming_Event_Combat_Kill,
			JumpInPlace,
			MyGameplayTags::Character_Player.GetTag(),
			AnyJump));
	TestFalse(
		TEXT("적이 쓴 스킬은 목표에 기여하지 않는다"),
		MyMissionPolicy::IsSkillUseContribution(
			MyGameplayTags::Streaming_Event_Combat_SkillUsed,
			JumpInPlace,
			MyGameplayTags::Character_Enemy.GetTag(),
			AnyJump));

	return true;
}

#endif
