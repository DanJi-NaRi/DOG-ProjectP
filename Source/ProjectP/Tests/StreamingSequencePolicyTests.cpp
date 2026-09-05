////////////////////////////
//! \file StreamingSequencePolicyTests.cpp
//! \brief DOG 신 채팅 Sequence 재생 정책의 순수 판정 자동화 테스트다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "MyGameplayTags.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"
#include "Streaming/MyStreamingPayloads.h"
#include "Streaming/MyStreamingManagerComponent.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerInputReportPolicyTest,
	"ProjectP.Streaming.PlayerInput.ReportThrottle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 조작 보고 눌림 판정을 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 판정이 계약대로면 true다.
bool FPlayerInputReportPolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingPlayerInput;

	// 첫 조작은 무조건 보고한다. 아직 보고한 적이 없다는 뜻으로 음수를 쓴다.
	TestTrue(TEXT("첫 조작은 보고한다"), ShouldReport(0.0, -1.0));

	// 이동과 마우스는 프레임마다 들어온다. 그대로 보내면 안 된다.
	TestFalse(TEXT("직후 조작은 누른다"), ShouldReport(10.0, 10.0));
	TestFalse(
		TEXT("간격이 덜 지났으면 누른다"),
		ShouldReport(10.0 + ReportIntervalSeconds - 0.1, 10.0));

	// 간격을 채우면 다시 보고한다. 경계는 포함한다.
	TestTrue(
		TEXT("간격을 채우면 보고한다"),
		ShouldReport(10.0 + ReportIntervalSeconds, 10.0));
	TestTrue(TEXT("한참 뒤 조작은 보고한다"), ShouldReport(1000.0, 10.0));

	// 잠수 판정이 수십 초 단위라 이 간격이 판정을 흔들면 안 된다.
	TestTrue(TEXT("보고 간격은 잠수 판정보다 훨씬 짧다"), ReportIntervalSeconds <= 10.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSequenceBusyPolicyTest,
	"ProjectP.Streaming.Sequence.BusyPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStreamingSequenceBusyPolicyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	TestEqual(
		TEXT("Idle + Drop은 즉시 시작"),
		ResolveSubmitAction(false, EMyStreamingSequenceBusyPolicy::Drop, EMyStreamingSequenceBusyPolicy::Drop),
		EMyStreamingSequenceSubmitAction::StartImmediately);
	TestEqual(
		TEXT("Idle + Queue는 즉시 시작"),
		ResolveSubmitAction(false, EMyStreamingSequenceBusyPolicy::Drop, EMyStreamingSequenceBusyPolicy::Queue),
		EMyStreamingSequenceSubmitAction::StartImmediately);
	TestEqual(
		TEXT("Drop + Drop은 새 요청 폐기"),
		ResolveSubmitAction(true, EMyStreamingSequenceBusyPolicy::Drop, EMyStreamingSequenceBusyPolicy::Drop),
		EMyStreamingSequenceSubmitAction::DropIncoming);
	TestEqual(
		TEXT("Drop + Queue는 현재 Drop 중단"),
		ResolveSubmitAction(true, EMyStreamingSequenceBusyPolicy::Drop, EMyStreamingSequenceBusyPolicy::Queue),
		EMyStreamingSequenceSubmitAction::InterruptActive);
	TestEqual(
		TEXT("Queue + Drop은 새 요청 폐기"),
		ResolveSubmitAction(true, EMyStreamingSequenceBusyPolicy::Queue, EMyStreamingSequenceBusyPolicy::Drop),
		EMyStreamingSequenceSubmitAction::DropIncoming);
	TestEqual(
		TEXT("Queue + Queue는 FIFO 대기"),
		ResolveSubmitAction(true, EMyStreamingSequenceBusyPolicy::Queue, EMyStreamingSequenceBusyPolicy::Queue),
		EMyStreamingSequenceSubmitAction::QueueIncoming);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSequenceNoConsecutiveLineTest,
	"ProjectP.Streaming.Sequence.NoConsecutiveLine",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStreamingSequenceNoConsecutiveLineTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	const FName LineA(TEXT("Line_A"));
	const FName LineB(TEXT("Line_B"));
	const FName LineC(TEXT("Line_C"));

	const TArray<int32> TwoCandidates =
		BuildEligibleCandidateIndexes({LineA, LineB}, LineA);
	TestEqual(TEXT("후보가 둘이면 한 후보만 남음"), TwoCandidates.Num(), 1);
	TestEqual(TEXT("후보가 둘이면 직전 Line 제외"), TwoCandidates[0], 1);

	const TArray<int32> ThreeCandidates =
		BuildEligibleCandidateIndexes({LineA, LineB, LineC}, LineB);
	TestEqual(TEXT("후보가 셋이면 둘이 남음"), ThreeCandidates.Num(), 2);
	TestEqual(TEXT("후보가 셋이면 첫 후보 유지"), ThreeCandidates[0], 0);
	TestEqual(TEXT("후보가 셋이면 셋째 후보 유지"), ThreeCandidates[1], 2);

	const TArray<int32> OneCandidate =
		BuildEligibleCandidateIndexes({LineA}, LineA);
	TestEqual(TEXT("후보가 하나면 반복 허용"), OneCandidate.Num(), 1);
	TestEqual(TEXT("단일 후보 인덱스"), OneCandidate[0], 0);

	const TArray<int32> NoPrevious =
		BuildEligibleCandidateIndexes({LineA, LineB}, NAME_None);
	TestEqual(TEXT("표시 이력이 없으면 모든 후보 허용"), NoPrevious.Num(), 2);

	const TArray<int32> Weights = {1, 2, 5};
	const TArray<int32> Eligible = {1, 2};
	TestEqual(TEXT("남은 첫 후보 Weight 시작 경계"), SelectWeightedCandidateIndex(Weights, Eligible, 1), 1);
	TestEqual(TEXT("남은 첫 후보 Weight 끝 경계"), SelectWeightedCandidateIndex(Weights, Eligible, 2), 1);
	TestEqual(TEXT("남은 둘째 후보 Weight 시작 경계"), SelectWeightedCandidateIndex(Weights, Eligible, 3), 2);
	TestEqual(TEXT("남은 둘째 후보 Weight 끝 경계"), SelectWeightedCandidateIndex(Weights, Eligible, 7), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSmallTalkIntervalRangeTest,
	"ProjectP.Streaming.SmallTalk.IntervalRange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief SmallTalk 고정 세 구간의 검증과 Weight 경계 선택을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 구간 계약 검사가 통과하면 true다.
bool FStreamingSmallTalkIntervalRangeTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	const FMySmallTalkIntervalRange ShortRange{10.0f / 3.0f, 20.0f / 3.0f, 2};
	const FMySmallTalkIntervalRange NormalRange{10.0f, 20.0f, 6};
	const FMySmallTalkIntervalRange LongRange{30.0f, 50.0f, 2};
	TestTrue(
		TEXT("첫 플레이 테스트 구간은 유효"),
		AreSmallTalkIntervalRangesValid(ShortRange, NormalRange, LongRange));

	FMySmallTalkIntervalRange InvalidRange = ShortRange;
	InvalidRange.MinSeconds = 21.0f;
	InvalidRange.MaxSeconds = 20.0f;
	TestFalse(
		TEXT("Min > Max 구간은 거부"),
		AreSmallTalkIntervalRangesValid(InvalidRange, NormalRange, LongRange));

	TestEqual(TEXT("Roll 1은 Short"), SelectSmallTalkIntervalRange(2, 6, 2, 1), EMySmallTalkIntervalRangeName::Short);
	TestEqual(TEXT("Roll 2는 Short"), SelectSmallTalkIntervalRange(2, 6, 2, 2), EMySmallTalkIntervalRangeName::Short);
	TestEqual(TEXT("Roll 3은 Normal"), SelectSmallTalkIntervalRange(2, 6, 2, 3), EMySmallTalkIntervalRangeName::Normal);
	TestEqual(TEXT("Roll 8은 Normal"), SelectSmallTalkIntervalRange(2, 6, 2, 8), EMySmallTalkIntervalRangeName::Normal);
	TestEqual(TEXT("Roll 9는 Long"), SelectSmallTalkIntervalRange(2, 6, 2, 9), EMySmallTalkIntervalRangeName::Long);
	TestEqual(TEXT("범위 밖 Roll은 None"), SelectSmallTalkIntervalRange(2, 6, 2, 11), EMySmallTalkIntervalRangeName::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSequenceServerRequestTest,
	"ProjectP.Streaming.Sequence.ServerRequest",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 직접 Sequence 요청의 접수 분류와 서버 권한 거부 계약을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 요청 계약 검사가 통과하면 true다.
bool FStreamingSequenceServerRequestTest::RunTest(const FString& Parameters)
{
	const FGuid ExecutionId(1, 2, 3, 4);
	const FMyStreamingSequenceRequestResult StartedResult{
		ExecutionId,
		EMyStreamingSequenceRequestStatus::AcceptedStarted,
		EMyStreamingSequenceExecutionState::Running};
	const FMyStreamingSequenceRequestResult QueuedResult{
		ExecutionId,
		EMyStreamingSequenceRequestStatus::AcceptedQueued,
		EMyStreamingSequenceExecutionState::Pending};
	const FMyStreamingSequenceRequestResult InterruptedResult{
		ExecutionId,
		EMyStreamingSequenceRequestStatus::AcceptedAfterInterrupt,
		EMyStreamingSequenceExecutionState::Running};
	const FMyStreamingSequenceRequestResult RejectedResult{
		ExecutionId,
		EMyStreamingSequenceRequestStatus::RejectedBuildFailed,
		EMyStreamingSequenceExecutionState::RejectedBeforeExecution};
	FMyStreamingSequenceRequestResult CompletedResult = QueuedResult;
	CompletedResult.ExecutionState = EMyStreamingSequenceExecutionState::Succeeded;

	TestTrue(TEXT("즉시 시작은 접수 결과다"), StartedResult.IsAccepted());
	TestTrue(TEXT("대기열 등록은 접수 결과다"), QueuedResult.IsAccepted());
	TestTrue(TEXT("중단 후 시작은 접수 결과다"), InterruptedResult.IsAccepted());
	TestFalse(TEXT("조립 실패는 거부 결과다"), RejectedResult.IsAccepted());
	TestFalse(TEXT("대기 중 실행은 최종 결과가 아니다"), QueuedResult.IsTerminal());
	TestTrue(TEXT("완료된 실행은 최종 결과다"), CompletedResult.IsTerminal());

	UMyStreamingManagerComponent* OwnerlessManager =
		NewObject<UMyStreamingManagerComponent>();
	TestNotNull(TEXT("Owner 없는 Manager 생성"), OwnerlessManager);
	if (!OwnerlessManager)
	{
		return false;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = ExecutionId;
	Request.SequenceId = TEXT("Seq_Test_ServerRequest");
	AddExpectedError(
		TEXT("원인=서버 권한 없음"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const FMyStreamingSequenceRequestResult AuthorityResult =
		OwnerlessManager->RequestSequence(Request);
	TestEqual(
		TEXT("서버 권한을 증명할 Owner가 없으면 거부"),
		AuthorityResult.Status,
		EMyStreamingSequenceRequestStatus::RejectedNoAuthority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingSequenceExecutionIdentityTest,
	"ProjectP.Streaming.Sequence.ExecutionIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief SequenceId와 실행 ID 조합의 새 실행, 재요청, 충돌 판정을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 실행 ID 판정이 통과하면 true다.
bool FStreamingSequenceExecutionIdentityTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	FMyStreamingSequenceRequest ExistingRequest;
	ExistingRequest.SequenceExecutionId = FGuid(10, 20, 30, 40);
	ExistingRequest.SequenceId = TEXT("Seq_Mission_Reward");
	ExistingRequest.BusyPolicy = EMyStreamingSequenceBusyPolicy::Queue;
	ExistingRequest.SourceEventTag =
		MyGameplayTags::Streaming_Event_Combat_Kill.GetTag();

	FMyStreamingSequenceRequest DifferentExecutionRequest = ExistingRequest;
	DifferentExecutionRequest.SequenceExecutionId = FGuid(11, 20, 30, 40);
	TestEqual(
		TEXT("같은 SequenceId와 다른 ExecutionId는 새 실행"),
		ResolveRequestIdentity(ExistingRequest, DifferentExecutionRequest),
		EMyStreamingSequenceRequestIdentity::DifferentExecution);

	TestEqual(
		TEXT("같은 ExecutionId와 같은 요청 내용은 재요청"),
		ResolveRequestIdentity(ExistingRequest, ExistingRequest),
		EMyStreamingSequenceRequestIdentity::Retry);

	FMyStreamingSequenceRequest DifferentSequenceRequest = ExistingRequest;
	DifferentSequenceRequest.SequenceId = TEXT("Seq_Mission_OtherReward");
	TestEqual(
		TEXT("같은 ExecutionId와 다른 SequenceId는 충돌"),
		ResolveRequestIdentity(ExistingRequest, DifferentSequenceRequest),
		EMyStreamingSequenceRequestIdentity::Conflict);

	FMyStreamingSequenceRequest DifferentPolicyRequest = ExistingRequest;
	DifferentPolicyRequest.BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
	TestEqual(
		TEXT("같은 ExecutionId와 다른 BusyPolicy는 충돌"),
		ResolveRequestIdentity(ExistingRequest, DifferentPolicyRequest),
		EMyStreamingSequenceRequestIdentity::Conflict);

	FMyStreamingSequenceRequest DifferentSourceRequest = ExistingRequest;
	DifferentSourceRequest.SourceEventTag =
		MyGameplayTags::Streaming_Event_Combat_Hit.GetTag();
	TestEqual(
		TEXT("같은 ExecutionId와 다른 SourceEventTag는 충돌"),
		ResolveRequestIdentity(ExistingRequest, DifferentSourceRequest),
		EMyStreamingSequenceRequestIdentity::Conflict);

	FMyStreamingSequenceRequest DifferentRecipientsRequest = ExistingRequest;
	DifferentRecipientsRequest.RecipientUserIndexes = {101, 102, 103};
	TestEqual(
		TEXT("같은 ExecutionId라도 파티 수령자가 다르면 충돌"),
		ResolveRequestIdentity(ExistingRequest, DifferentRecipientsRequest),
		EMyStreamingSequenceRequestIdentity::Conflict);

	FMyStreamingSequenceRequest DifferentPayloadRequest = ExistingRequest;
	DifferentPayloadRequest.PayloadMesoAmount = 200;
	TestEqual(
		TEXT("같은 ExecutionId라도 Mission Meso Payload가 다르면 충돌"),
		ResolveRequestIdentity(ExistingRequest, DifferentPayloadRequest),
		EMyStreamingSequenceRequestIdentity::Conflict);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingZoneDonationIdempotencyTest,
	"ProjectP.Streaming.ZoneDonation.Idempotency",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief [D-6] 같은 OrderedZones 인덱스는 Manager 수명 동안 한 번만 소비하는 계약을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 음수 거부, 최초 허용, 중복 거부가 모두 성립하면 true다.
bool FStreamingZoneDonationIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	TSet<int32> ProcessedIndexes;
	TestFalse(TEXT("음수 ZoneIndex는 거부"), TryMarkZoneDonationProcessed(ProcessedIndexes, -1));
	TestTrue(TEXT("ZoneIndex 0 최초 처리는 허용"), TryMarkZoneDonationProcessed(ProcessedIndexes, 0));
	TestFalse(TEXT("ZoneIndex 0 중복 처리는 거부"), TryMarkZoneDonationProcessed(ProcessedIndexes, 0));
	TestTrue(TEXT("다른 ZoneIndex는 독립 허용"), TryMarkZoneDonationProcessed(ProcessedIndexes, 1));
	TestEqual(TEXT("성공한 서로 다른 두 Zone만 기록"), ProcessedIndexes.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingDonationMesoResolveTest,
	"ProjectP.Streaming.Sequence.DonationMesoResolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief [D-2] RollFromLine 금액 확정의 범위/경계/None 계약과 결정성을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 금액 확정 계약 검사가 통과하면 true다.
bool FStreamingDonationMesoResolveTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	int32 Amount = -1;

	// 1) RollFromLine 결과는 Min~Max 범위 안이며, 범위를 벗어난 raw 값도 클램프된다.
	for (int32 Roll = 90; Roll <= 320; ++Roll)
	{
		TestTrue(TEXT("RollFromLine 유효 계약"),
			TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 100, 300, Roll, Amount));
		TestTrue(TEXT("RollFromLine 결과가 Min 이상"), Amount >= 100);
		TestTrue(TEXT("RollFromLine 결과가 Max 이하"), Amount <= 300);
	}
	TestTrue(TEXT("범위 안 raw는 그대로"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 100, 300, 200, Amount));
	TestEqual(TEXT("범위 안 raw는 그대로 200"), Amount, 200);

	// 2) Min=Max이면 항상 그 값이다.
	TestTrue(TEXT("Min=Max 유효"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 50, 50, 999, Amount));
	TestEqual(TEXT("Min=Max는 항상 그 값"), Amount, 50);

	// 3) Chat/None은 항상 0이다.
	TestTrue(TEXT("None 유효"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::None, 0, 0, 12345, Amount));
	TestEqual(TEXT("None은 0"), Amount, 0);

	// Payload는 Mission Instance가 한 번 확정한 signed Meso를 그대로 사용한다.
	TestTrue(TEXT("Payload 유효"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::Payload, 0, 0, 200, Amount));
	TestEqual(TEXT("Payload는 확정값 유지"), Amount, 200);
	TestTrue(TEXT("음수 Payload 유효"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::Payload, 0, 0, -250, Amount));
	TestEqual(TEXT("음수 Payload는 확정값 유지"), Amount, -250);
	TestFalse(TEXT("Payload 0은 실패"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::Payload, 0, 0, 0, Amount));

	// 4) 같은 입력은 같은 결과다(결정성). 실행/표시 중 값이 바뀌지 않음을 뒷받침한다.
	int32 First = -1;
	int32 Second = -2;
	TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 100, 300, 250, First);
	TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 100, 300, 250, Second);
	TestEqual(TEXT("같은 입력은 같은 결과"), First, Second);

	// 방어: 잘못된 범위는 실패로 처리한다.
	TestFalse(TEXT("Min<=0은 실패"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 0, 300, 100, Amount));
	TestFalse(TEXT("Min>Max는 실패"),
		TryResolveStepRewardAmount(EMyStreamingRewardSource::RollFromLine, 300, 100, 200, Amount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingDonationGrantContractTest,
	"ProjectP.Streaming.Sequence.DonationGrantContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief [D-3] Donation 지급 전 검사와 후조건 계약을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 지급 계약 검사가 통과하면 true다.
bool FStreamingDonationGrantContractTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;
	using EReason = EMyStreamingStepActionFailureReason;

	// 정상: 모든 조건 충족 → 시도 가능(None)
	TestEqual(TEXT("정상 조건은 None"),
		CheckDonationGrantPreconditions(true, true, true, 200, 500), EReason::None);

	// 2) 금액 0 이하 → NonPositiveAmount, 지급 거부
	TestEqual(TEXT("금액 0은 거부"),
		CheckDonationGrantPreconditions(true, true, true, 0, 500), EReason::NonPositiveAmount);
	TestEqual(TEXT("음수 금액은 거부"),
		CheckDonationGrantPreconditions(true, true, true, -10, 500), EReason::NonPositiveAmount);

	// 3) 대상 PlayerState 없음 → InvalidRecipient
	TestEqual(TEXT("대상 없음은 거부"),
		CheckDonationGrantPreconditions(true, false, true, 200, 500), EReason::InvalidRecipient);

	// 4) InventoryComponent 없음 → InvalidInventory
	TestEqual(TEXT("인벤토리 없음은 거부"),
		CheckDonationGrantPreconditions(true, true, false, 200, 500), EReason::InvalidInventory);

	// 서버 권한 없음 → NoAuthority
	TestEqual(TEXT("권한 없음은 거부"),
		CheckDonationGrantPreconditions(false, true, true, 200, 500), EReason::NoAuthority);

	// 5) 오버플로 가능 금액 → Overflow, 지급 거부
	TestEqual(TEXT("오버플로는 거부"),
		CheckDonationGrantPreconditions(true, true, true, 100, TNumericLimits<int32>::Max() - 50), EReason::Overflow);
	TestEqual(TEXT("int32 최대 경계까지는 허용"),
		CheckDonationGrantPreconditions(true, true, true, 50, TNumericLimits<int32>::Max() - 50), EReason::None);

	// 1)/6) 후조건: After == Before + Amount만 성공
	TestTrue(TEXT("정상 후조건 성립"), IsDonationGrantPostconditionMet(500, 200, 700));
	TestFalse(TEXT("후조건 불일치는 성공 아님"), IsDonationGrantPostconditionMet(500, 200, 650));
	TestFalse(TEXT("증가 없음은 성공 아님"), IsDonationGrantPostconditionMet(500, 200, 500));

	TestEqual(TEXT("양수 완료 Meso는 그대로 지급"), ResolveAppliedMesoDelta(200, 50), 200);
	TestEqual(TEXT("음수 완료 Meso는 잔액까지만 차감"), ResolveAppliedMesoDelta(-500, 285), -285);
	TestEqual(TEXT("잔액이 충분하면 요청량 전부 차감"), ResolveAppliedMesoDelta(-200, 500), -200);
	TestEqual(TEXT("잔액 0이면 실제 변화 없음"), ResolveAppliedMesoDelta(-200, 0), 0);

	// UserIndex 유효 범위 계약: > 0만 인증된 지급 대상이다(0/음수 거부).
	TestFalse(TEXT("UserIndex 0은 무효"), IsAuthenticatedUserIndex(0));
	TestFalse(TEXT("UserIndex 음수는 무효"), IsAuthenticatedUserIndex(-1));
	TestTrue(TEXT("UserIndex 1은 유효"), IsAuthenticatedUserIndex(1));
	TestTrue(TEXT("UserIndex 큰 값은 유효"), IsAuthenticatedUserIndex(12345));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingDonationBubbleContractTest,
	"ProjectP.Streaming.Sequence.DonationBubbleContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief [D-4/D-7] Donation 성공 버블·Notice 공통 전송 조건과 표시 문구 조립을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 모든 Donation 표시 계약 검사가 통과하면 true다.
bool FStreamingDonationBubbleContractTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;
	using EPresent = EMyStreamingPresentationType;
	using EAction = EMyStreamingActionType;

	// 1) 지급 성공 Donation → 같은 판정으로 버블과 Notice 모두 전송 대상
	TestTrue(TEXT("성공 Donation은 버블과 Notice 전송 대상"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::GrantMeso, true, true, 300));

	// 2) bSucceeded=false → 전송 금지
	TestFalse(TEXT("실패는 전송 금지"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::GrantMeso, true, false, 300));

	// 3) bAttempted=false → 전송 금지
	TestFalse(TEXT("미시도는 전송 금지"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::GrantMeso, false, false, 0));

	// 4) 일반 GrantMeso의 실제 적용값이 0 이하면 전송 금지
	TestFalse(TEXT("0원은 전송 금지"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::GrantMeso, true, true, 0));
	TestFalse(TEXT("음수 금액은 전송 금지"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::GrantMeso, true, true, -5));
	TestTrue(TEXT("Mission 음수 Delta는 역후원 버블과 Notice 대상"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::ApplyMesoDelta, true, true, -5));
	TestFalse(TEXT("실제 차감 0은 역후원 버블과 Notice 생략"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::ApplyMesoDelta, true, true, 0));

	// 6/7) 일반 Chat 또는 비-GrantMeso는 전송 금지(일반 Chat 경로와 분리)
	TestFalse(TEXT("Chat은 Donation 버블 전송 아님"),
		ShouldSendRewardBubble(EPresent::Chat, EAction::None, true, true, 300));
	TestFalse(TEXT("GrantMeso 아니면 전송 금지"),
		ShouldSendRewardBubble(EPresent::Donation, EAction::None, true, true, 300));

	// 8) Exp/Item 보상은 지급 성공 자체가 표시 조건이다. Meso 변화량은 0이다.
	TestTrue(TEXT("성공 Exp 보상은 버블 전송 대상"),
		ShouldSendRewardBubble(EPresent::ExpReward, EAction::GrantExp, true, true, 0));
	TestTrue(TEXT("성공 Item 보상은 버블 전송 대상"),
		ShouldSendRewardBubble(EPresent::ItemReward, EAction::GrantItem, true, true, 0));
	TestFalse(TEXT("실패한 Item 보상은 전송 금지"),
		ShouldSendRewardBubble(EPresent::ItemReward, EAction::GrantItem, true, false, 0));
	TestFalse(TEXT("Exp 보상 연출에 다른 Action이면 전송 금지"),
		ShouldSendRewardBubble(EPresent::ExpReward, EAction::GrantMeso, true, true, 0));

	// 9) 보상 종류 판정
	EMyStreamingRewardKind Kind = EMyStreamingRewardKind::Meso;
	TestTrue(TEXT("Donation은 Meso 보상"),
		TryResolveRewardKind(EPresent::Donation, Kind) && Kind == EMyStreamingRewardKind::Meso);
	TestTrue(TEXT("ExpReward는 Exp 보상"),
		TryResolveRewardKind(EPresent::ExpReward, Kind) && Kind == EMyStreamingRewardKind::Exp);
	TestTrue(TEXT("ItemReward는 Item 보상"),
		TryResolveRewardKind(EPresent::ItemReward, Kind) && Kind == EMyStreamingRewardKind::Item);
	TestFalse(TEXT("Chat은 보상 연출이 아님"),
		TryResolveRewardKind(EPresent::Chat, Kind));

	const FText God = NSLOCTEXT("ProjectPStreamingTest", "TestGod", "호루스");
	const FText Character = NSLOCTEXT("ProjectPStreamingTest", "TestCharacter", "네페르");

	// 보상 종류별 문구가 각각 다른 시스템 줄을 만든다.
	{
		const FString ExpText = FormatRewardMessage(
			FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character,
			EMyStreamingRewardKind::Exp, 120, FText::GetEmpty(), FText::GetEmpty()).ToString();
		TestTrue(TEXT("Exp 문구에 획득량이 들어간다"), ExpText.Contains(TEXT("120")));
		TestTrue(TEXT("Exp 문구에 경험치 표기가 들어간다"), ExpText.Contains(TEXT("경험치")));
		TestFalse(TEXT("Exp 문구에는 Meso 이미지가 없다"), ExpText.Contains(TEXT("id=\"Meso\"")));

		const FText ItemName = NSLOCTEXT("ProjectPStreamingTest", "TestItem", "소형 회복 물약");
		const FString ItemText = FormatRewardMessage(
			FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character,
			EMyStreamingRewardKind::Item, 2, ItemName, FText::GetEmpty()).ToString();
		TestTrue(TEXT("Item 문구에 아이템 이름이 들어간다"),
			ItemText.Contains(TEXT("소형 회복 물약")));
		TestTrue(TEXT("Item 문구에 개수가 들어간다"), ItemText.Contains(TEXT("2")));
		TestFalse(TEXT("Item 문구에는 Meso 이미지가 없다"), ItemText.Contains(TEXT("id=\"Meso\"")));
	}
	const auto CountNewlines = [](const FString& In)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < In.Len(); ++Index)
		{
			if (In[Index] == TEXT('\n')) { ++Count; }
		}
		return Count;
	};

	// D-5B-1) 작성 대사 있음 → 두 줄, 사이 줄바꿈 정확히 1개. 표시 금액은 실제 적용값 사용.
	{
		const FString S = FormatDonationMessage(FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character, 132, FText::FromString(TEXT("받아라, 내 성의다."))).ToString();
		TestTrue(TEXT("신 이름 포함"), S.Contains(TEXT("호루스")));
		TestTrue(TEXT("수령 캐릭터 포함"), S.Contains(TEXT("네페르님")));
		TestTrue(TEXT("지급 금액 포함"), S.Contains(TEXT("132")));
		TestTrue(TEXT("금액 뒤 Meso 이미지"), S.Contains(TEXT("132<img id=\"Meso\"/>를")));
		TestTrue(TEXT("작성 대사 포함"), S.Contains(TEXT("받아라, 내 성의다.")));
		TestEqual(TEXT("줄바꿈 정확히 1개"), CountNewlines(S), 1);
	}

	// Mission 음수 Delta는 실제 차감량의 절댓값과 역후원 문구를 사용한다.
	{
		const FString S = FormatDonationMessage(
			FGameplayTag::RequestGameplayTag(TEXT("God.Set")),
			NSLOCTEXT("ProjectPStreamingTest", "TestSet", "세트"),
			Character,
			-285,
			FText::FromString(TEXT("걸렸군."))).ToString();
		TestTrue(TEXT("역후원 실제 차감액 포함"), S.Contains(TEXT("285<img id=\"Meso\"/>")));
		TestTrue(TEXT("역후원 문구 포함"), S.Contains(TEXT("네페르님에게서")) && S.Contains(TEXT("가져갔습니다")));
		TestFalse(TEXT("역후원에 음수 부호 미표시"), S.Contains(TEXT("-285")));
	}

	// D-5B-2) 작성 대사 없음 → 시스템 문구만, 끝에 줄바꿈 없음
	{
		const FString S = FormatDonationMessage(FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character, 90, FText::GetEmpty()).ToString();
		TestTrue(TEXT("시스템 문구 존재"), S.Contains(TEXT("90")));
		TestEqual(TEXT("빈 대사는 줄바꿈 0개"), CountNewlines(S), 0);
	}

	// D-5B-3) 공백/빈 줄만 있는 대사 → 시스템 문구만
	{
		const FString S = FormatDonationMessage(FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character, 50, FText::FromString(TEXT("  \n   \n "))).ToString();
		TestEqual(TEXT("공백 대사는 줄바꿈 0개"), CountNewlines(S), 0);
	}

	// D-5B-4) 여러 줄 대사 → 내부 줄바꿈 보존(시스템1 + 내부1 = 2)
	{
		const FString S = FormatDonationMessage(FGameplayTag::RequestGameplayTag(TEXT("God.Horus")), God, Character, 70, FText::FromString(TEXT("받아라.\n이것은 나의 성의다."))).ToString();
		TestTrue(TEXT("첫 줄 보존"), S.Contains(TEXT("받아라.")));
		TestTrue(TEXT("둘째 줄 보존"), S.Contains(TEXT("이것은 나의 성의다.")));
		TestEqual(TEXT("여러 줄 대사는 줄바꿈 2개"), CountNewlines(S), 2);
	}

	// D-5D) Donation 전용 CharacterId 한글 매핑과 안전한 fallback.
	TestEqual(TEXT("100은 네페르"), GetDonationRecipientCharacterDisplayName(100).ToString(), FString(TEXT("네페르")));
	TestEqual(TEXT("200은 인푸"), GetDonationRecipientCharacterDisplayName(200).ToString(), FString(TEXT("인푸")));
	TestEqual(TEXT("300은 헤루"), GetDonationRecipientCharacterDisplayName(300).ToString(), FString(TEXT("헤루")));
	TestEqual(TEXT("알 수 없는 캐릭터는 플레이어"), GetDonationRecipientCharacterDisplayName(999).ToString(), FString(TEXT("플레이어")));

	// Donation 결과는 버블과 Notice가 함께 사용할 god/img 마크업을 한 번만 만든다.
	{
		const FString RichText = FormatDonationMessage(
			FGameplayTag::RequestGameplayTag(TEXT("God.Horus")),
			God,
			Character,
			132,
			FText::FromString(TEXT("<god id=\"God.Horus\">강조 대사</>"))).ToString();
		TestTrue(TEXT("신 이름 동적 색상 태그"), RichText.Contains(TEXT("<god id=\"God.Horus\">호루스</>")));
		TestTrue(TEXT("Meso 인라인 이미지 태그"), RichText.Contains(TEXT("<img id=\"Meso\"/>")));
		TestTrue(TEXT("작성 대사 마크업 보존"), RichText.Contains(TEXT("<god id=\"God.Horus\">강조 대사</>")));
	}

	// D-5C-6) 일반 Chat 메시지 데이터의 기본 PresentationType은 Chat이다.
	{
		FMyStreamingChatMessageData Data;
		TestTrue(TEXT("기본 PresentationType은 Chat"), Data.PresentationType == EMyStreamingPresentationType::Chat);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingStandaloneDonationGateTest,
	"ProjectP.Streaming.Sequence.StandaloneDonationGate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief [D-4A] Standalone 개발 검증 진입점이 NM_Standalone에서만 허용됨을 검사한다.
//! \param Parameters 자동화 테스트 실행 인자다.
//! \return 게이트 계약 검사가 통과하면 true다.
bool FStreamingStandaloneDonationGateTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingSequencePolicy;

	TestTrue(TEXT("Standalone은 허용"), IsStandaloneDonationTestAllowed(NM_Standalone));
	TestFalse(TEXT("DedicatedServer는 거부"), IsStandaloneDonationTestAllowed(NM_DedicatedServer));
	TestFalse(TEXT("ListenServer는 거부"), IsStandaloneDonationTestAllowed(NM_ListenServer));
	TestFalse(TEXT("Client는 거부"), IsStandaloneDonationTestAllowed(NM_Client));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
