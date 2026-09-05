////////////////////////////
//! \file StreamingKillCountPolicyTests.cpp
//! \brief 처치 누계 Rule의 문턱 통과·대상 매칭·시간 창 판정의 순수 정책 자동화 테스트다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Streaming/MyStreamingAntiAFKRuleTypes.h"
#include "Streaming/MyStreamingCountRuleTypes.h"
#include "Streaming/MyStreamingStateRuleTypes.h"
#include "Streaming/MyMissionTypes.h"
#include "Streaming/MyStreamingKillCountRuleTypes.h"
#include "Streaming/MyStreamingManagerComponent.h"
#include "MyGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingKillCountThresholdTest,
	"ProjectP.Streaming.KillCount.Threshold",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 처치 누계가 문턱을 통과하는 순간만 잡는지, 대상과 시간 창 판정이 계약대로인지 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 판정이 계약대로면 true다.
bool FStreamingKillCountThresholdTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingKillCountRulePolicy;

	// 문턱을 넘는 순간에만 발동한다. 넘은 뒤에도 매번 발동하면 같은 대사가 쏟아진다.
	TestTrue(TEXT("2에서 3으로 갈 때 3마리 문턱 통과"), DidCrossThreshold(2, 3, 3));
	TestFalse(TEXT("3에서 4로 가면 이미 넘긴 뒤라 발동하지 않음"), DidCrossThreshold(3, 4, 3));
	TestFalse(TEXT("문턱에 못 미치면 발동하지 않음"), DidCrossThreshold(1, 2, 3));
	TestFalse(TEXT("마릿수가 0인 Rule은 발동하지 않음"), DidCrossThreshold(0, 1, 0));
	// 시간 창 Rule은 기록이 빠지면서 셈이 줄었다가 다시 늘 수 있다. 그때도 통과 순간만 잡는다.
	TestTrue(TEXT("셈이 줄었다가 다시 문턱에 닿으면 통과"), DidCrossThreshold(9, 10, 10));

	// 빈 Rule 태그는 모든 대상을 센다.
	const FGameplayTag AnyEnemy = MyGameplayTags::Character_Enemy.GetTag();
	const FGameplayTag Boss = MyGameplayTags::Character_Enemy_Boss.GetTag();
	const FGameplayTag Player = MyGameplayTags::Character_Player.GetTag();

	TestTrue(TEXT("빈 Rule 태그는 일반 적을 센다"), DoesKillMatchTarget(AnyEnemy, FGameplayTag()));
	TestTrue(TEXT("빈 Rule 태그는 보스도 센다"), DoesKillMatchTarget(Boss, FGameplayTag()));
	// 계층 매칭이므로 Character.Enemy Rule은 보스 처치도 센다.
	TestTrue(TEXT("모든 적 Rule은 보스 처치도 센다"), DoesKillMatchTarget(Boss, AnyEnemy));
	// 반대로 보스 Rule은 일반 적 처치를 세지 않는다.
	TestFalse(TEXT("보스 Rule은 일반 적 처치를 세지 않는다"), DoesKillMatchTarget(AnyEnemy, Boss));
	TestFalse(TEXT("보스 Rule은 플레이어 처치를 세지 않는다"), DoesKillMatchTarget(Player, Boss));

	// 시간 창 유무 판정
	TestFalse(TEXT("0초는 누계 Rule"), IsWindowRule(0.0f));
	TestTrue(TEXT("20초는 시간 창 Rule"), IsWindowRule(20.0f));

	// 창 경계는 포함한다. 정확히 창 길이만큼 지난 기록은 아직 센다.
	TestTrue(TEXT("창 안의 기록은 센다"), IsWithinWindow(100.0, 110.0, 20.0f));
	TestTrue(TEXT("창 경계의 기록은 센다"), IsWithinWindow(100.0, 120.0, 20.0f));
	TestFalse(TEXT("창을 벗어난 기록은 세지 않는다"), IsWithinWindow(100.0, 121.0, 20.0f));

	// 처치 누계 소스도 파티 전원이 보상 수령자다. Kill 사실에 UserIndex가 없기 때문이다.
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::KillCount);
	TestEqual(
		TEXT("처치 누계 보상 수령자는 파티 전원"),
		Contract.RecipientMode,
		EMyStreamingRecipientMode::Party);
	TestTrue(
		TEXT("처치 누계는 채팅 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::Chat));
	TestTrue(
		TEXT("처치 누계는 보상 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::Reward));
	TestTrue(
		TEXT("처치 누계는 미션 시작 허용"),
		Contract.IsReactionAllowed(EMyStreamingReaction::MissionStart));
	TestFalse(TEXT("처치 누계는 단일 Step 제약이 없다"), Contract.bRequireSingleStep);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMissionCountOriginTest,
	"ProjectP.Mission.CountOrigin",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 진행도를 어느 시점부터 세는지가 목표 행 계약대로인지 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 기본값과 대상 매칭이 계약대로면 true다.
bool FMissionCountOriginTest::RunTest(const FString& Parameters)
{
	// 기본은 Mission이 걸린 순간부터다. 열을 비운 기존 목표가 그대로 동작해야 한다.
	const FMyMissionCombatObjectiveRow Objective;
	TestEqual(
		TEXT("목표의 기본 집계 시작점은 미션 시작"),
		Objective.CountOrigin,
		EMyMissionCountOrigin::MissionStart);

	// 누계 승계는 목표의 TargetTag로 전역 누계를 고른다. 그 매칭은 처치 누계 Rule과
	// 같은 계층 규칙을 쓰므로, 여기서 두 쪽이 어긋나지 않는지 함께 확인한다.
	const FGameplayTag AnyEnemy = MyGameplayTags::Character_Enemy.GetTag();
	const FGameplayTag Boss = MyGameplayTags::Character_Enemy_Boss.GetTag();

	TestTrue(
		TEXT("모든 적 목표는 보스 처치 누계도 승계한다"),
		MyStreamingKillCountRulePolicy::DoesKillMatchTarget(Boss, AnyEnemy));
	TestFalse(
		TEXT("보스 목표는 일반 적 처치 누계를 승계하지 않는다"),
		MyStreamingKillCountRulePolicy::DoesKillMatchTarget(AnyEnemy, Boss));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAntiAFKRouteContractTest,
	"ProjectP.Streaming.AntiAFK.RouteContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 잠수 판정 시간이 조건 표에서만 나오는 계약을 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 계약대로면 true다.
bool FAntiAFKRouteContractTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingAntiAFKRulePolicy;

	// 표를 읽기 전 기본값은 0이다. 0이면 감시를 시작하지 않아야 한다.
	// 예전에는 60초가 C++에 박혀 있어 표가 없어도 조용히 돌았다.
	TestFalse(TEXT("판정 시간 0은 감시를 시작하지 않는다"), IsIdleSecondsValid(0.0f));
	TestFalse(TEXT("음수 판정 시간은 거부"), IsIdleSecondsValid(-1.0f));
	TestTrue(TEXT("양수 판정 시간만 감시를 시작한다"), IsIdleSecondsValid(60.0f));

	// 조건 행 이름은 표와 런타임이 같은 값을 써야 한다.
	TestEqual(
		TEXT("진입 행 이름"),
		MyStreamingAntiAFKRuleNames::Enter,
		FName(TEXT("Rule_AntiAFK_Enter")));
	TestEqual(
		TEXT("해제 행 이름"),
		MyStreamingAntiAFKRuleNames::Resume,
		FName(TEXT("Rule_AntiAFK_Resume")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingCountRulePolicyTest,
	"ProjectP.Streaming.Count.Policy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 사건 누계·아이템 누계 판정이 계약대로인지 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 판정이 계약대로면 true다.
bool FStreamingCountRulePolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingCountRulePolicy;

	// 처치 누계·스킬 사용과 같은 문턱 판정을 쓴다.
	TestTrue(TEXT("19에서 20으로 갈 때 통과"), DidCrossThreshold(19, 20, 20));
	TestFalse(TEXT("이미 넘긴 뒤에는 발동하지 않음"), DidCrossThreshold(20, 21, 20));
	TestFalse(TEXT("횟수가 0인 Rule은 발동하지 않음"), DidCrossThreshold(0, 1, 0));

	TestFalse(TEXT("0초는 누계 Rule"), IsWindowRule(0.0f));
	TestTrue(TEXT("15초는 시간 창 Rule"), IsWindowRule(15.0f));

	// 아이템은 RowName이라 계층이 없다. 완전일치로만 비교한다.
	TestTrue(TEXT("빈 Rule ItemId는 모든 아이템을 센다"), DoesItemMatch(FName(TEXT("Potion_Small")), NAME_None));
	TestTrue(TEXT("같은 아이템은 센다"), DoesItemMatch(FName(TEXT("Potion_Small")), FName(TEXT("Potion_Small"))));
	TestFalse(TEXT("다른 아이템은 세지 않는다"), DoesItemMatch(FName(TEXT("Potion_Large")), FName(TEXT("Potion_Small"))));

	// 사건 누계는 계층 매칭이다. 도네 SourceTag로 신을 좁힌다.
	const FGameplayTag Granted = MyGameplayTags::Streaming_Event_Donation_Granted.GetTag();
	const FGameplayTag Ra = MyGameplayTags::God_Ra.GetTag();
	TestTrue(TEXT("빈 Rule 태그는 모든 사건을 센다"), DoesTagMatch(Granted, FGameplayTag()));
	TestTrue(TEXT("같은 사건은 센다"), DoesTagMatch(Granted, Granted));
	TestFalse(TEXT("신을 좁힌 Rule은 신이 없는 사건을 세지 않는다"), DoesTagMatch(FGameplayTag(), Ra));

	// 누계 Rule은 시간 필터를 쓰지 않는다. 예전에는 SourceTag로 좁히는 누계 Rule이
	// 시간 창 기록을 봐서 최근 몇 초만 세는 버그가 있었다.
	TestFalse(TEXT("0초는 시간 창이 아니다"), IsWindowRule(0.0f));
	// 30초 간격으로 세 번 온 사건은 어떤 시간 창으로도 3이 되지 않는다.
	// 누계 Rule이 시간 필터를 쓰면 영원히 1에 머문다.
	TestFalse(
		TEXT("25초 창은 30초 전 기록을 세지 않는다"),
		IsWithinWindow(0.0, 30.0, 25.0f));
	TestFalse(
		TEXT("25초 창은 60초 전 기록도 세지 않는다"),
		IsWithinWindow(0.0, 60.0, 25.0f));

	// 계약: 사건 누계는 파티, 아이템은 개인이다.
	TestEqual(
		TEXT("사건 누계 수령자는 파티"),
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::CountEvent).RecipientMode,
		EMyStreamingRecipientMode::Party);
	TestEqual(
		TEXT("아이템 수령자는 사건을 일으킨 개인"),
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::ItemEvent).RecipientMode,
		EMyStreamingRecipientMode::Instigator);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingStateRulePolicyTest,
	"ProjectP.Streaming.State.Policy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 상태 유지 조건의 판정이 계약대로인지 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 판정이 계약대로면 true다.
bool FStreamingStateRulePolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingStateRulePolicy;

	const FGameplayTag Combat = MyGameplayTags::Streaming_State_Combat.GetTag();
	const FGameplayTag Plate = MyGameplayTags::Streaming_State_Plate_Pressed.GetTag();

	// 빈 Rule 태그는 아무 상태와도 짝짓지 않는다. 사실 조건과 다른 점이다.
	// "모든 상태"라는 뜻이 성립하지 않아, 비우면 죽은 행이 된다.
	TestFalse(TEXT("빈 Rule 상태는 어떤 상태와도 안 맞는다"), DoesStateMatch(Combat, FGameplayTag()));
	TestTrue(TEXT("같은 상태는 맞는다"), DoesStateMatch(Combat, Combat));
	TestFalse(TEXT("다른 상태는 안 맞는다"), DoesStateMatch(Plate, Combat));

	// 유지 시간은 0보다 커야 한다. 0이면 켜지자마자 발동한다.
	TestFalse(TEXT("0초 유지는 쓸 수 없다"), IsHoldSecondsValid(0.0f));
	TestFalse(TEXT("음수 유지는 쓸 수 없다"), IsHoldSecondsValid(-1.0f));
	TestTrue(TEXT("양수 유지만 쓴다"), IsHoldSecondsValid(60.0f));

	// AndStateTag가 없으면 StateTag만 본다.
	TestTrue(TEXT("상태 하나만 요구하면 그것만 켜지면 된다"), ShouldHold(true, false, false));
	TestFalse(TEXT("그 상태가 꺼지면 재지 않는다"), ShouldHold(false, false, true));

	// 두 상태를 요구하면 둘 다 켜져야 한다. 하나만 꺼져도 재던 시간을 버린다.
	TestTrue(TEXT("둘 다 켜지면 잰다"), ShouldHold(true, true, true));
	TestFalse(TEXT("두 번째가 꺼지면 안 잰다"), ShouldHold(true, true, false));
	TestFalse(TEXT("첫 번째가 꺼지면 안 잰다"), ShouldHold(false, true, true));

	// 사실 하나가 여러 뜻을 겸할 때가 있다. 점프도 스킬 사용 사실로 온다.
	// "이동만 했을 때"에서는 점프를 스킬을 쓴 것으로 치지 않아야 한다.
	const FGameplayTag SkillUsed =
		MyGameplayTags::Streaming_Event_Combat_SkillUsed.GetTag();
	const FGameplayTag JumpParent = MyGameplayTags::Skill_Common_Jump.GetTag();
	const FGameplayTag JumpMoving = MyGameplayTags::Skill_Common_Jump_Moving.GetTag();
	const FGameplayTag OtherSkill = MyGameplayTags::Skill_Inpu_ScaleSmash.GetTag();

	TestTrue(
		TEXT("제외가 없으면 리셋 사실은 그대로 리셋한다"),
		ShouldResetOnEvent(SkillUsed, OtherSkill, SkillUsed, FGameplayTag()));
	TestTrue(
		TEXT("제외에 걸리지 않는 스킬은 리셋한다"),
		ShouldResetOnEvent(SkillUsed, OtherSkill, SkillUsed, JumpParent));
	TestFalse(
		TEXT("제외에 걸린 점프는 리셋하지 않는다"),
		ShouldResetOnEvent(SkillUsed, JumpMoving, SkillUsed, JumpParent));
	TestFalse(
		TEXT("리셋 사실이 아니면 애초에 리셋하지 않는다"),
		ShouldResetOnEvent(Combat, OtherSkill, SkillUsed, FGameplayTag()));
	TestFalse(
		TEXT("리셋 사실을 지정하지 않으면 아무것도 리셋하지 않는다"),
		ShouldResetOnEvent(SkillUsed, OtherSkill, FGameplayTag(), FGameplayTag()));

	// 이동에는 전이 지점이 없어 속도를 주기로 본다. 문턱이 하나면 그 언저리에서
	// 상태가 쉴 새 없이 뒤집히므로, 켜는 문턱을 끄는 문턱보다 높게 둔다.
	using namespace MyStreamingMoveWatch;
	TestTrue(TEXT("켜는 문턱보다 빠르면 이동이다"), ResolveMoving(EnterMovingSpeed, false));
	TestFalse(TEXT("끄는 문턱보다 느리면 정지다"), ResolveMoving(ExitMovingSpeed, true));
	TestTrue(TEXT("두 문턱 사이에서는 이동을 유지한다"), ResolveMoving(50.0f, true));
	TestFalse(TEXT("두 문턱 사이에서는 정지도 유지한다"), ResolveMoving(50.0f, false));
	TestTrue(
		TEXT("끄는 문턱이 켜는 문턱보다 낮아야 깜빡이지 않는다"),
		ExitMovingSpeed < EnterMovingSpeed);

	// 같은 조건을 재는 Rule들은 한 묶음이다. 묶어야 발동 순간에 Weight로
	// 하나만 고를 수 있다. 묶지 않으면 후보가 여럿일 때 전부 따로 발동한다.
	FMyStreamingStateRuleRow Left;
	Left.StateTag = Combat;
	Left.HoldSeconds = 60.0f;

	FMyStreamingStateRuleRow Right = Left;
	Right.SequenceId = TEXT("Seq_Other");
	Right.Weight = 5;
	Right.CooldownSeconds = 30.0f;
	TestTrue(
		TEXT("시퀀스와 Weight만 달라도 같은 조건이다"),
		HaveSameCondition(Left, Right));

	Right.HoldSeconds = 20.0f;
	TestFalse(TEXT("유지 시간이 다르면 다른 조건이다"), HaveSameCondition(Left, Right));

	Right = Left;
	Right.StateTag = Plate;
	TestFalse(TEXT("상태가 다르면 다른 조건이다"), HaveSameCondition(Left, Right));

	Right = Left;
	Right.AndStateTag = Plate;
	TestFalse(TEXT("함께 요구하는 상태가 다르면 다른 조건이다"), HaveSameCondition(Left, Right));

	Right = Left;
	Right.ResetOnEventTag = MyGameplayTags::Streaming_Event_Combat_SkillUsed.GetTag();
	TestFalse(TEXT("리셋 조건이 다르면 다른 조건이다"), HaveSameCondition(Left, Right));

	// 시간을 재는 단위는 조건 묶음 하나와 출처 하나의 짝이다.
	// 출처를 구분하지 않으면 같은 상태를 켜는 객체가 여럿일 때
	// 하나가 꺼지는 것만으로 나머지의 시간까지 함께 사라진다.
	UObject* const FirstSource = NewObject<UDataTable>();
	UObject* const SecondSource = NewObject<UDataTable>();
	const FMyStreamingStateHoldKey KeyA{TEXT("Rule_A"), FObjectKey(FirstSource)};
	const FMyStreamingStateHoldKey KeyASame{TEXT("Rule_A"), FObjectKey(FirstSource)};
	const FMyStreamingStateHoldKey KeyB{TEXT("Rule_A"), FObjectKey(SecondSource)};
	const FMyStreamingStateHoldKey KeyC{TEXT("Rule_B"), FObjectKey(FirstSource)};

	TestTrue(TEXT("같은 묶음과 같은 출처는 같은 짝이다"), KeyA == KeyASame);
	TestFalse(TEXT("출처가 다르면 다른 짝이다"), KeyA == KeyB);
	TestFalse(TEXT("묶음이 다르면 다른 짝이다"), KeyA == KeyC);
	TestEqual(
		TEXT("같은 짝은 같은 해시를 갖는다"),
		GetTypeHash(KeyA),
		GetTypeHash(KeyASame));

	TSet<FMyStreamingStateHoldKey> HoldKeys;
	HoldKeys.Add(KeyA);
	HoldKeys.Add(KeyB);
	HoldKeys.Remove(KeyB);
	TestTrue(TEXT("한 출처를 지워도 다른 출처는 남는다"), HoldKeys.Contains(KeyA));

	// 상태 유지도 파티 전원이 수령자다. 상태에는 UserIndex가 없다.
	TestEqual(
		TEXT("상태 유지 수령자는 파티"),
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::StateHold).RecipientMode,
		EMyStreamingRecipientMode::Party);

	return true;
}

#endif
