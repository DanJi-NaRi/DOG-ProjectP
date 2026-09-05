////////////////////////////
//! \page MyStreamingManagerComponent.h
//! \brief 전투 Payload를 스트리밍 채팅 메시지로 번역하는 Manager Component 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"
#include "MyStreamingChatTypes.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingAntiAFKRuleTypes.h"
#include "MyStreamingCountRuleTypes.h"
#include "MyStreamingStateRuleTypes.h"
#include "MyStreamingKillCountRuleTypes.h"
#include "MyStreamingSkillUseRuleTypes.h"
#include "MyStreamingTuningTypes.h"
#include "MyStreamingMesoRuleTypes.h"
#include "MyStreamingPayloads.h"
#include "MyStreamingGimmickTypes.h"
#include "MyMissionTypes.h"
#include "MyStreamingZoneDonationTypes.h"
#include "MyStreamingZoneTypes.h"
#include "MyStreamingManagerComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStreamingManager, Log, All);


class UDataTable;
class AMyPlayerState;
struct FMyStreamingPlayerInputPayload;
struct FMyStreamingCountEventPayload;
struct FMyStreamingItemEventPayload;
struct FMyStreamingStatePayload;

//! \brief StreamingManager가 소유하는 Mission 서버 원본 한 건이다.
struct FMyMissionServerState
{
	FGuid MissionInstanceId;
	FName DefinitionRowName;
	FMyMissionDefinitionRow Definition;
	FMyMissionCombatObjectiveRow Objective;
	TArray<int32> AssigneeUserIndexes;
	TArray<FGameplayTag> AssigneeCharacterTags;
	EMyMissionState State = EMyMissionState::None;
	int32 ProgressCount = 0;
	int32 ResolvedMesoDelta = 0;
	float ActivatedAtServerTime = 0.0f;
	float MissionEndsAtServerTime = 0.0f;
	FTimerHandle MissionTimeoutTimerHandle;
};

////////////////////////////
//! \enum EMySmallTalkIntervalRangeName
//! \author 장효제
//! \brief SmallTalk 예약 간격 추첨에 사용한 고정 튜닝 구간 이름이다.
UENUM(BlueprintType)
enum class EMySmallTalkIntervalRangeName : uint8
{
	None,
	Short,
	Normal,
	Long,
};

////////////////////////////
//! \struct FMySmallTalkIntervalRange
//! \author 장효제
//! \brief SmallTalk 구간의 초 범위와 선택 가중치다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMySmallTalkIntervalRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SmallTalk", meta = (ClampMin = "0.0"))
	float MinSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SmallTalk", meta = (ClampMin = "0.0"))
	float MaxSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SmallTalk", meta = (ClampMin = "0"))
	int32 Weight = 0;
};

UENUM()
enum class EMySmallTalkTimelineEventType : uint8
{
	Scheduled,
	Attempted,
	Played,
	Completed,
	Dropped,
	Interrupted,
	CancelledByDialogue,
	Exhausted,
};

#if !UE_BUILD_SHIPPING
//! \brief 후속 Debug Graph의 서버 원본이 되는 SmallTalk 수명 이벤트 한 건이다.
struct FMySmallTalkTimelineEvent
{
	EMySmallTalkTimelineEventType EventType = EMySmallTalkTimelineEventType::Scheduled;
	float ServerTime = 0.0f;
	FName SequenceId;
	FName BlockingSequenceId;
	EMySmallTalkIntervalRangeName SelectedRange = EMySmallTalkIntervalRangeName::None;
	float Interval = 0.0f;
	float ScheduledTime = 0.0f;
};
#endif


//! \brief 재생 중 새 Sequence 요청을 어떻게 처리할지 나타내는 순수 정책 결과다.
enum class EMyStreamingSequenceSubmitAction : uint8
{
	StartImmediately,
	DropIncoming,
	QueueIncoming,
	InterruptActive,
};

//! \brief 서버의 Sequence 요청이 기존 실행 경로에 접수되었는지 나타낸다.
enum class EMyStreamingSequenceRequestStatus : uint8
{
	AcceptedStarted,
	AcceptedQueued,
	AcceptedAfterInterrupt,
	RejectedNoAuthority,
	RejectedInvalidRequest,
	RejectedBuildFailed,
	RejectedByBusyPolicy,
	RejectedExecutionUnavailable,
	RejectedExecutionIdConflict,
	RejectedExecutionRecordCapacity,
};

//! \brief 접수된 한 번의 Sequence 실행이 현재 어느 단계에 있는지 나타낸다.
enum class EMyStreamingSequenceExecutionState : uint8
{
	RejectedBeforeExecution,
	Pending,
	Running,
	Succeeded,
	FailedInterrupted,
	FailedTerminal,
};

//! \brief 서버 코드가 직접 Sequence를 요청할 때 전달하는 최소 정보다.
struct FMyStreamingSequenceRequest
{
	FGuid SequenceExecutionId;
	FName SequenceId;
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
	FGameplayTag SourceEventTag;
	// Mission 결과 Sequence가 시작 때 확정한 God과 Meso를 다시 사용한다.
	FGameplayTag PayloadGodTag;
	int32 PayloadMesoAmount = 0;
	// D-3: 지급 대상 플레이어의 서버 identity(UserIndex). -1이면 지급 대상 없음(일반 Chat 등).
	int32 RecipientUserIndex = -1;
	// D-6: 한 번 확정한 Donation을 독립 지급할 파티원 identity 목록. 비어 있으면 위 단일 값을 사용한다.
	TArray<int32> RecipientUserIndexes;
#if !UE_BUILD_SHIPPING
	// D-4A: 비Shipping·Standalone 개발 검증 전용 명시적 테스트 수령자. 운영 요청에서는 항상 비어 있다.
	TWeakObjectPtr<AMyPlayerState> StandaloneTestRecipient;
#endif
};

//! \brief Sequence 요청의 최초 접수 결과와 현재 실행 상태를 함께 돌려준다.
struct FMyStreamingSequenceRequestResult
{
	FGuid SequenceExecutionId;
	EMyStreamingSequenceRequestStatus Status =
		EMyStreamingSequenceRequestStatus::RejectedInvalidRequest;
	EMyStreamingSequenceExecutionState ExecutionState =
		EMyStreamingSequenceExecutionState::RejectedBeforeExecution;

	bool IsAccepted() const
	{
		return Status == EMyStreamingSequenceRequestStatus::AcceptedStarted
			|| Status == EMyStreamingSequenceRequestStatus::AcceptedQueued
			|| Status == EMyStreamingSequenceRequestStatus::AcceptedAfterInterrupt;
	}

	bool IsTerminal() const
	{
		return ExecutionState == EMyStreamingSequenceExecutionState::RejectedBeforeExecution
			|| ExecutionState == EMyStreamingSequenceExecutionState::Succeeded
			|| ExecutionState == EMyStreamingSequenceExecutionState::FailedInterrupted
			|| ExecutionState == EMyStreamingSequenceExecutionState::FailedTerminal;
	}
};

//! \brief 기존 실행 기록과 새 요청의 실행 ID 관계를 나타낸다.
enum class EMyStreamingSequenceRequestIdentity : uint8
{
	DifferentExecution,
	Retry,
	Conflict,
};

//! \brief [D-3] Donation 지급 시도의 실패 사유다. None은 실패 없음(성공 또는 미시도)을 뜻한다.
enum class EMyStreamingStepActionFailureReason : uint8
{
	None,
	NoAuthority,
	InvalidRecipient,
	InvalidInventory,
	InvalidContract,
	NonPositiveAmount,
	Overflow,
	PostconditionMismatch,
};

//! \brief 서버 사실을 Rule로 변환하는 진입 경로의 종류다. 핸들러 하나와 1:1로 대응한다.
enum class EMyStreamingRuleSource : uint8
{
	Combat,
	KillCount,
	SkillUse,
	CountEvent,
	ItemEvent,
	StateHold,
	Meso,
	Gimmick,
	Zone,
	ZoneDonation,
	SmallTalk,
};

//! \brief 실제로 지급되는 보상의 종류다. 표시 문구를 고르는 데 쓴다.
enum class EMyStreamingRewardKind : uint8
{
	Meso,
	Exp,
	Item,
};

//! \brief 하나의 Rule 조건이 낼 수 있는 반응의 종류다. 값은 비트 위치로 쓴다.
enum class EMyStreamingReaction : uint8
{
	Chat = 0,
	Reward = 1,
	MissionStart = 2,
};

//! \brief 이 소스가 Donation 수령자를 어떻게 확정하는지다.
enum class EMyStreamingRecipientMode : uint8
{
	None,		// 수령자를 만들 수 없다. Donation 반응을 허용하지 않는다.
	Instigator,	// 사실을 일으킨 개인 한 명이다.
	Party,		// 연결·인증된 파티 전원이 각자 독립 지급받는다.
};

//! \brief Rule 소스 하나의 반응 허용 집합과 수령자 해석 전략이다.
//! \note 허용 반응과 수령자를 한 값에 묶어 "Donation 허용인데 수령자 없음"을 한 곳에서만 정의한다.
struct FMyStreamingRuleSourceContract
{
	uint8 AllowedReactions = 0;
	EMyStreamingRecipientMode RecipientMode = EMyStreamingRecipientMode::None;
	// ZoneDonation 고유 제약이다. 후보가 여럿이어도 StepOrder는 하나여야 한다.
	bool bRequireSingleStep = false;

	bool IsReactionAllowed(const EMyStreamingReaction Reaction) const
	{
		return (AllowedReactions & (1u << static_cast<uint8>(Reaction))) != 0;
	}
};

namespace MyStreamingSequencePolicy
{
	//! \brief Rule 소스의 확정 계약을 반환한다. 소스별 허용 반응과 수령자 전략의 단일 출처다.
	//! \param Source 사실을 Rule로 변환한 진입 경로다.
	//! \return 그 소스가 낼 수 있는 반응 집합과 수령자 해석 전략이다.
	//! \note Tools/dog_chat_validator/validate.py의 RULE_SOURCE_CONTRACTS와 반드시 함께 수정한다.
	PROJECTP_API FMyStreamingRuleSourceContract GetRuleSourceContract(EMyStreamingRuleSource Source);

	//! \brief 이 Action이 서버 상태를 바꾸는지 판정한다.
	//! \param ActionType 검사할 Step의 Action 종류다.
	//! \return None이 아니면 true다. validator의 STATE_CHANGING_ACTIONS와 같은 집합이다.
	PROJECTP_API bool IsStatefulActionType(EMyStreamingActionType ActionType);

	//! \brief 상태 변경 Step을 포함한 Sequence의 BusyPolicy 허용 여부다.
	//! \param bContainsStatefulStep 상태를 바꾸는 Step이 하나라도 있는지 여부다.
	//! \param BusyPolicy 요청된 Sequence의 BusyPolicy다.
	//! \return 상태 변경이 없거나 Queue면 true다. Drop은 상태 변경 요청을 잃을 수 있어 거부한다.
	PROJECTP_API bool IsStatefulSequenceBusyPolicyAllowed(
		bool bContainsStatefulStep,
		EMyStreamingSequenceBusyPolicy BusyPolicy);


	//! \brief 현재/새 BusyPolicy 조합을 런타임 상태 변경 없이 판정한다.
	PROJECTP_API EMyStreamingSequenceSubmitAction ResolveSubmitAction(
		bool bIsSequencePlaying,
		EMyStreamingSequenceBusyPolicy ActivePolicy,
		EMyStreamingSequenceBusyPolicy IncomingPolicy);

	//! \brief 두 요청의 실행 ID와 내용으로 새 실행, 재요청, 충돌을 구분한다.
	PROJECTP_API EMyStreamingSequenceRequestIdentity ResolveRequestIdentity(
		const FMyStreamingSequenceRequest& ExistingRequest,
		const FMyStreamingSequenceRequest& IncomingRequest);

	//! \brief 후보가 둘 이상이면 직전에 표시한 Line을 한 번의 추첨에서 제외한다.
	PROJECTP_API TArray<int32> BuildEligibleCandidateIndexes(
		const TArray<FName>& CandidateRowNames,
		FName LastDisplayedLineRowName);

	//! \brief 허용 후보들의 정수 Weight 누적 구간에서 주어진 추첨값의 후보를 찾는다.
	PROJECTP_API int32 SelectWeightedCandidateIndex(
		const TArray<int32>& CandidateWeights,
		const TArray<int32>& EligibleIndexes,
		int32 RandomWeight);

	//! \brief SmallTalk 고정 세 구간의 최소 런타임 계약을 검사한다.
	PROJECTP_API bool AreSmallTalkIntervalRangesValid(
		const FMySmallTalkIntervalRange& ShortRange,
		const FMySmallTalkIntervalRange& NormalRange,
		const FMySmallTalkIntervalRange& LongRange);

	//! \brief 세 구간의 누적 Weight에서 주어진 1-based Roll이 속한 구간을 반환한다.
	PROJECTP_API EMySmallTalkIntervalRangeName SelectSmallTalkIntervalRange(
		int32 ShortWeight,
		int32 NormalWeight,
		int32 LongWeight,
		int32 RandomWeight);

	//! \brief [D-2] Donation Step의 확정 금액을 계산한다. RNG를 포함하지 않아 순수하게 테스트 가능하다.
	//! \param RewardSource 금액 출처. RollFromLine일 때만 Line 범위로 확정한다.
	//! \param RewardMin Line 원본 최솟값이다.
	//! \param RewardMax Line 원본 최댓값이다.
	//! \param ResolvedInputValue RollFromLine의 raw 추첨값 또는 Mission Payload의 확정 Meso다.
	//! \param OutAmount 확정 금액이다. None은 0이다.
	//! \return 금액 출처와 입력 계약이 유효하면 true다.
	PROJECTP_API bool TryResolveStepRewardAmount(
		EMyStreamingRewardSource RewardSource,
		int32 RewardMin,
		int32 RewardMax,
		int32 ResolvedInputValue,
		int32& OutAmount);

	//! \brief [D-3] Donation 지급 전 검사 결과를 계산한다. RNG/UObject 없이 순수하게 테스트 가능하다.
	//! \param bHasAuthority 서버 권한 보유 여부다.
	//! \param bRecipientValid 지급 대상 PlayerState 유효성이다.
	//! \param bInventoryValid 대상 InventoryComponent 유효성이다.
	//! \param Amount 지급하려는 확정 금액이다.
	//! \param BeforeMeso 지급 전 현재 Meso다(오버플로 검사용).
	//! \return None이면 지급 시도 가능, 그 외는 거부 사유다.
	PROJECTP_API EMyStreamingStepActionFailureReason CheckDonationGrantPreconditions(
		bool bHasAuthority,
		bool bRecipientValid,
		bool bInventoryValid,
		int32 Amount,
		int32 BeforeMeso);

	//! \brief [D-3] 지급 후조건(After == Before + Amount)이 정확히 성립하는지 검사한다.
	PROJECTP_API bool IsDonationGrantPostconditionMet(
		int32 BeforeMeso,
		int32 GrantedAmount,
		int32 AfterMeso);

	//! \brief 요청된 signed 완료 Meso를 현재 잔액 범위의 실제 변화량으로 해석한다.
	PROJECTP_API int32 ResolveAppliedMesoDelta(int32 RequestedDelta, int32 CurrentMeso);

	//! \brief [D-3] 인증된 서버 identity인지 검사한다. 프로젝트 계약상 UserIndex > 0만 유효하다.
	PROJECTP_API bool IsAuthenticatedUserIndex(int32 UserIndex);

	//! \brief [D-6] 같은 던전 수명에서 Zone 인덱스를 처음 처리하는지 원자적으로 기록한다.
	//! \param ProcessedIndexes 이미 처리된 OrderedZones 인덱스 집합이다.
	//! \param ZoneIndex 새로 처리할 0-based OrderedZones 인덱스다.
	//! \return 유효한 인덱스가 처음 추가되었으면 true, 잘못됐거나 이미 있으면 false다.
	PROJECTP_API bool TryMarkZoneDonationProcessed(
		TSet<int32>& ProcessedIndexes,
		int32 ZoneIndex);

	//! \brief [D-4A] Standalone 개발 검증 진입점 허용 여부. NM_Standalone에서만 true(운영 NetMode는 전부 false).
	PROJECTP_API bool IsStandaloneDonationTestAllowed(ENetMode NetMode);

	//! \brief Step 필드 조합에서 반응 종류를 판정한다.
	//! \param PresentationType Step의 표시 종류다.
	//! \param ActionType Step의 상태 변경 종류다.
	//! \param bHasMissionTag Step에 MissionTag가 지정되어 있는지 여부다.
	//! \param RewardSource Step의 Meso 출처다.
	//! \param RewardMin Line 원본 최솟값이다.
	//! \param RewardMax Line 원본 최댓값이다.
	//! \param OutReaction 판정된 반응 종류다. 규격 밖 조합에서는 값을 쓰지 않는다.
	//! \return 세 반응 중 하나의 규격을 만족하면 true다. ItemReward/GrantItem은 미구현이라 false다.
	PROJECTP_API bool TryResolveStepReaction(
		EMyStreamingPresentationType PresentationType,
		EMyStreamingActionType ActionType,
		bool bHasMissionTag,
		EMyStreamingRewardSource RewardSource,
		int32 RewardMin,
		int32 RewardMax,
		FName RewardItemId,
		EMyStreamingReaction& OutReaction);

	//! \brief Rule이 선택한 Sequence의 Step 하나가 그 소스 계약에서 허용되는 조합인지 판정한다.
	//! \param PresentationType Step의 표시 종류다.
	//! \param ActionType Step의 상태 변경 종류다.
	//! \param bHasMissionTag Step에 MissionTag가 지정되어 있는지 여부다.
	//! \param RewardSource Step의 Meso 출처다.
	//! \param RewardMin Line 원본 최솟값이다.
	//! \param RewardMax Line 원본 최댓값이다.
	//! \param Contract 이 Step을 참조한 Rule 소스의 계약이다.
	//! \return 소스 계약이 허용하는 반응이면 true다.
	PROJECTP_API bool IsRuleSequenceStepContractValid(
		EMyStreamingPresentationType PresentationType,
		EMyStreamingActionType ActionType,
		bool bHasMissionTag,
		EMyStreamingRewardSource RewardSource,
		int32 RewardMin,
		int32 RewardMax,
		FName RewardItemId,
		const FMyStreamingRuleSourceContract& Contract);

	//! \brief 이 Step을 일반 Chat으로 방송하고 MissionStart Notice까지 보낼지 판정한다.
	//! \param PresentationType Step의 표시 종류다.
	//! \param bIsStartMissionAction Step의 ActionType이 StartMission인지 여부다.
	//! \param bStartMissionSucceeded StartMission Action이 실제로 Mission을 만들었는지 여부다.
	//! \return Mission 생성에 실패한 StartMission Step은 false다. 그 외 Chat/MissionStart는 true다.
	PROJECTP_API bool ShouldBroadcastStepPresentation(
		EMyStreamingPresentationType PresentationType,
		bool bIsStartMissionAction,
		bool bStartMissionSucceeded);

	//! \brief 이 Step의 지급 결과가 수령자 버블 전송 대상인지 판정한다.
	//! \param PresentationType Step의 표시 종류다.
	//! \param ActionType Step의 상태 변경 종류다.
	//! \param bAttempted 지급을 시도했는지 여부다.
	//! \param bSucceeded 지급이 성공했는지 여부다.
	//! \param AppliedMesoDelta 실제 적용된 signed Meso 변화량이다. Meso 보상에만 쓴다.
	//! \return 성공한 Meso/Exp/Item 지급이면 true다.
	PROJECTP_API bool ShouldSendRewardBubble(
		EMyStreamingPresentationType PresentationType,
		EMyStreamingActionType ActionType,
		bool bAttempted,
		bool bSucceeded,
		int32 AppliedMesoDelta);

	//! \brief Step의 표시 종류에서 보상 종류를 판정한다.
	//! \param PresentationType Step의 표시 종류다.
	//! \param OutKind 판정된 보상 종류다.
	//! \return 보상 연출이면 true다.
	PROJECTP_API bool TryResolveRewardKind(
		EMyStreamingPresentationType PresentationType,
		EMyStreamingRewardKind& OutKind);

	//! \brief [D-5D] Donation 수령자의 CharacterId를 한글 표시명으로 변환한다.
	//! \param SelectedCharacterId 수령자 PlayerState에 확정된 캐릭터 ID다.
	//! \return 100=네페르, 200=인푸, 300=헤루이며 알 수 없는 값은 플레이어다.
	PROJECTP_API FText GetDonationRecipientCharacterDisplayName(int32 SelectedCharacterId);

	//! \brief 실제 지급 금액, 수령 캐릭터명과 작성 대사를 하나의 Donation RichText 문구로 만든다.
	//! \param GodTag god RichText 태그의 id에 사용할 신 GameplayTag다.
	//! \param GodDisplayName 화면 표시용 신 이름이다.
	//! \param CharacterDisplayName 지급받은 캐릭터의 화면 표시명이다.
	//! \param AppliedMesoDelta 실제 적용된 signed Meso 변화량이다.
	//! \param AuthoredLineMessage 선택된 Line의 원본 MessageText다. 앞뒤 공백/빈 줄은 제거하고 내부 줄바꿈은 보존한다.
	//! \note 작성 대사가 비었거나 공백뿐이면 시스템 문구만 반환하고 끝에 줄바꿈을 남기지 않는다.
	PROJECTP_API FText FormatDonationMessage(
		FGameplayTag GodTag,
		const FText& GodDisplayName,
		const FText& CharacterDisplayName,
		int32 AppliedMesoDelta,
		const FText& AuthoredLineMessage);

	//! \brief Meso/Exp/Item 지급 결과를 하나의 보상 RichText 문구로 만든다.
	//! \param GodTag god RichText 태그의 id에 사용할 신 GameplayTag다.
	//! \param GodDisplayName 화면 표시용 신 이름이다.
	//! \param CharacterDisplayName 지급받은 캐릭터의 화면 표시명이다.
	//! \param Kind 지급된 보상 종류다.
	//! \param Amount Meso는 signed 변화량, Exp는 획득량, Item은 개수다.
	//! \param ItemDisplayName Item 보상의 화면 표시명이다. 다른 보상은 비운다.
	//! \param AuthoredLineMessage 선택된 Line의 원본 MessageText다.
	//! \note 작성 대사가 비었거나 공백뿐이면 시스템 문구만 반환한다.
	PROJECTP_API FText FormatRewardMessage(
		FGameplayTag GodTag,
		const FText& GodDisplayName,
		const FText& CharacterDisplayName,
		EMyStreamingRewardKind Kind,
		int32 Amount,
		const FText& ItemDisplayName,
		const FText& AuthoredLineMessage);
}


//! \brief Sequence 조립에 필요한 공통 입력이다. Combat 전용 타입을 포함하지 않는다.
struct FMyStreamingSequenceBuildInput
{
	FGuid SequenceExecutionId;
	FName SequenceId;
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
	FGameplayTag SourceEventTag;
	FGameplayTag PayloadGodTag;
	int32 PayloadMesoAmount = 0;
	// D-3: 요청에서 확정 Sequence로 넘겨 실행부까지 전달할 지급 대상 UserIndex다.
	int32 RecipientUserIndex = -1;
	// D-6: 요청에서 확정 Sequence로 전달할 인증 파티원 UserIndex 목록이다.
	TArray<int32> RecipientUserIndexes;
#if !UE_BUILD_SHIPPING
	// D-4A: 요청→확정 Sequence로 전달되는 Standalone 개발 검증 전용 테스트 수령자다.
	TWeakObjectPtr<AMyPlayerState> StandaloneTestRecipient;
#endif
};

//! \brief [D-3] Step Action 실행 후 결과다. 확정 입력값(ResolvedRewardAmount)과 의미를 섞지 않는다.
//! \brief D-4는 이 결과의 bSucceeded로 성공 버블 출력 여부를 판단한다.
struct FMyResolvedStepActionResult
{
	bool bAttempted = false;			// 이 Step에서 상태 변경 Action을 시도했는가.
	bool bSucceeded = false;			// 지급 후조건까지 만족했는가.
	int32 AppliedMesoDelta = 0;	// 실제로 적용된 signed 변화량이다.
	EMyStreamingStepActionFailureReason FailureReason = EMyStreamingStepActionFailureReason::None;
};

//! \brief 같은 SequenceId와 StepOrder의 후보 중 한 번 선택되어 확정된 실제 재생 Step이다.
//! \brief DM-3A: 선택된 같은 Line의 확장 필드 전체를 하나의 원자적 선택 결과로 함께 보존한다.
struct FMyResolvedStreamingStep
{
	int32 StepOrder = 0;	// 0 = 아직 CSV 행으로부터 초기화되지 않았다.
	float DelayFromPreviousStepSeconds = 0.0f;
	FName LineRowName;
	FMyStreamingChatMessageData ChatMessage;

	// 선택된 Line에서 함께 복사되는 확장 데이터. 이후 단계에서 다시 조회하거나 다시 뽑지 않는다.
	EMyStreamingGodSource GodSource = EMyStreamingGodSource::Line;
	EMyStreamingPresentationType PresentationType = EMyStreamingPresentationType::Chat;
	EMyStreamingActionType ActionType = EMyStreamingActionType::None;
	FGameplayTag MissionTag;
	EMyStreamingRewardSource RewardSource = EMyStreamingRewardSource::None;
	int32 RewardMin = 0;
	int32 RewardMax = 0;
	FName RewardItemId;
	EMyStreamingPresentationTier PresentationTier = EMyStreamingPresentationTier::None;

	// D-2: 서버가 이 Step에서 한 번 확정한 후원 금액이다. Min/Max 원본은 위에 그대로 두고 결과만 여기 보존한다.
	// RollFromLine이 아니면 0이며, 실행부·지급부·UI에서 다시 뽑지 않는다.
	int32 ResolvedRewardAmount = 0;

	// D-3: 실행부에서 이 Step의 Action을 한 번 실행한 후 결과다(확정 입력값과 별도).
	FMyResolvedStepActionResult ActionResult;
};

//! \brief 모든 Step의 Line 선택이 끝난 뒤 실행기에 전달되는 확정 Sequence다.
//! \brief 공통 조립/확정 구조체는 서버에서만 사용하므로 USTRUCT()로 선언하지 않는다.
struct FMyResolvedStreamingSequence
{
	FGuid SequenceExecutionId;
	FName SequenceId;
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
	TArray<FMyResolvedStreamingStep> Steps;
	// D-3: 이 확정 Sequence의 지급 대상 UserIndex다. 실행부가 이 값으로만 대상을 찾는다.
	int32 RecipientUserIndex = -1;
	// D-6: 같은 Resolved Step/금액을 각각 독립 실행할 인증 파티원 UserIndex 목록이다.
	TArray<int32> RecipientUserIndexes;
	// Mission 시작 Notice 전용 대상이다. 지급 수령자와 분리해 서로 덮어쓰지 않는다.
	TArray<int32> MissionNoticeUserIndexes;
#if !UE_BUILD_SHIPPING
	// D-4A: Standalone 개발 검증 시에만 채워지는 명시적 테스트 수령자다. 운영 확정 Sequence에서는 비어 있다.
	TWeakObjectPtr<AMyPlayerState> StandaloneTestRecipient;
	// Mission 시작 Notice 전용 Standalone 수령자다.
	TWeakObjectPtr<AMyPlayerState> MissionNoticeStandaloneRecipient;
#endif
};


////////////////////////////
//! \class UMyStreamingManagerComponent
//! \author 장효제
//! \brief Combat 채널을 구독하고 전투 사실 Payload를 UI 채팅 메시지로 변환해 재발행한다.
UCLASS(ClassGroup = (Streaming), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UMyStreamingManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyStreamingManagerComponent();

	////////////////////////////
	//! \author 장효제
	//! \brief 서버 C++ 코드가 SequenceId를 기존 조립 및 BusyPolicy 실행 경로에 요청한다.
	//! \param Request SequenceId, BusyPolicy, 출처 EventTag를 담은 요청이다.
	//! \return 실행 경로 접수 또는 거부 이유를 담은 동기 결과다.
	//! \warning 서버 권한이 없는 Owner의 호출은 항상 거부된다.
	FMyStreamingSequenceRequestResult RequestSequence(
		const FMyStreamingSequenceRequest& Request);

	////////////////////////////

#if !UE_BUILD_SHIPPING
	////////////////////////////
#endif

	////////////////////////////
	//! \author 장효제
	//! \brief [D-4] 수령자 클라이언트에서 Donation 결과 버블을 기존 God Chat 경로로 1회 표시한다.
	//! \param BubbleData 서버가 조립해 수령자 전용 Client RPC로 전달한 표시 데이터다.
	//! \note 수령자 ADungeonPC의 Client RPC에서만 호출된다. 지급은 하지 않는다.
	void ClientDisplayDonationBubble(const FMyStreamingChatMessageData& BubbleData);

	//! \author 장효제
	//! \brief 인증 파티 또는 Standalone Demo Player 준비 후 Dungeon Scope SmallTalk 예약을 시작한다.
	void StartSmallTalkScheduler();

	void StartMissionLoop(const TArray<int32>& ReadyUserIndexes);
	bool TryStartAntiAFKMission();

#if !UE_BUILD_SHIPPING
	void StartStandaloneMissionLoop(AMyPlayerState* DemoPlayerState);
#endif

	////////////////////////////
	//! \author 장효제
	//! \brief 파티 잠수 감시를 시작한다.
	void StartAntiAFK();

	////////////////////////////
	//! \author 장효제
	//! \brief Dungeon 종료 경계에서 잠수 Timer와 Activity 구독을 정리한다.
	void StopAntiAFK();

	//! \author 장효제
	//! \brief 서버 Story Dialogue 세션 시작을 등록하고 SmallTalk 예약 또는 남은 Step을 취소한다.
	void NotifyStoryDialogueStarted(AActor* SourceObelisk, int32 UserId);

	//! \author 장효제
	//! \brief 서버 Story Dialogue 세션 종료를 제거하고 마지막 세션이면 새 SmallTalk Interval을 예약한다.
	void NotifyStoryDialogueEnded(AActor* SourceObelisk, int32 UserId);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 전투
	void RegisterCombatMessageListener();
	void UnregisterCombatMessageListener();
	void HandleCombatPayload(FGameplayTag Channel, const FMyStreamingCombatPayload& Payload);
	void HandleMissionCombatPayload(FGameplayTag Channel, const FMyStreamingCombatPayload& Payload);
	// 메소
	void RegisterMesoMessageListener();
	void UnregisterMesoMessageListener();
	void HandleMesoPayload(
		FGameplayTag Channel,
		const FMyStreamingMesoPayload& Payload);
	const FMyStreamingMesoRuleRow* SelectMesoRule(
		const FMyStreamingMesoPayload& Payload,
		int64 PreviousTotal,
		int64 NewTotal) const;
	void HandleKillCountPayload(const FMyStreamingCombatPayload& Payload);
	void HandleSkillUsePayload(const FMyStreamingCombatPayload& Payload);
	const FMyStreamingSkillUseRuleRow* SelectSkillUseRule(FGameplayTag UsedSkillTag) const;
	int32 CountPartySkillUses(FGameplayTag RuleSkillTag) const;
	void ApplyTuningTable();
	void ApplyAntiAFKRuleTable();
	FName ResolveAntiAFKSequenceId(FName RuleRowName) const;
	void RegisterCountEventListener();
	void UnregisterCountEventListener();
	void HandleCountEventPayload(FGameplayTag Channel, const FMyStreamingCountEventPayload& Payload);
	void AdvanceMissionsForCountEvent(FGameplayTag EventTag, FName ItemId, int32 Amount);
	void RegisterStateListener();
	void UnregisterStateListener();
	void HandleStatePayload(FGameplayTag Channel, const FMyStreamingStatePayload& Payload);
	void RefreshStateHoldTimers();
	void ResetStateHoldTimersForEvent(FGameplayTag EventTag, FGameplayTag SourceTag);
	void HandleStateHoldElapsed(FMyStreamingStateHoldKey HoldKey);
	void ClearAllStateHoldTimers();
	bool IsAnyStateActive(FGameplayTag StateTag) const;
	void CollectStateSources(FGameplayTag StateTag, TSet<FObjectKey>& OutSources) const;
	FName ResolveStateRuleGroupId(const FMyStreamingStateRuleRow& Rule) const;
	const FMyStreamingStateRuleRow* PickStateRuleFromGroup(
		const FMyStreamingStateRuleRow& Condition) const;
	void RegisterItemEventListener();
	void UnregisterItemEventListener();
	void HandleItemEventPayload(FGameplayTag Channel, const FMyStreamingItemEventPayload& Payload);
	int32 CountPartyEvents(FGameplayTag RuleEventTag, FGameplayTag RuleSourceTag, float WindowSeconds, int32 ExcludeLatest) const;
	int32 CountPartyItems(FGameplayTag RuleEventTag, FName RuleItemId, float WindowSeconds, int32 ExcludeLatest) const;
	void ArmSkillIdleTimers();
	void RearmSkillIdleTimer(FName RuleRowName, float IdleSeconds);
	void HandleSkillIdleTimeout(FName RuleRowName);
	bool RequestRuleSequence(FName SequenceId, EMyStreamingSequenceBusyPolicy BusyPolicy, EMyStreamingRuleSource Source, FGameplayTag SourceEventTag);
	const FMyStreamingKillCountRuleRow* SelectKillCountRule(FGameplayTag KillTargetTag) const;
	int32 CountPartyKills(FGameplayTag RuleTargetTag, float WindowSeconds, bool bExcludeLatest) const;
	float GetLongestKillWindowSeconds() const;
	// 
	void RegisterPlayerInputListener();
	void UnregisterPlayerInputListener();
	void HandlePlayerInput(FGameplayTag Channel, const FMyStreamingPlayerInputPayload& Payload);
	void ArmAntiAFKTimer();
	void HandleAntiAFKTimeout();
	void EnterPartyAFK();
	void ResumeFromAFK(FGameplayTag SourceEventTag, bool bDeferPresentation);
	void RequestAntiAFKResumeSequence();
	void RegisterGimmickMessageListener();
	void UnregisterGimmickMessageListener();
	void HandleGimmickResetPayload(FGameplayTag Channel, const FMyStreamingGimmickResetPayload& Payload);
	void RegisterZoneDonationMessageListener();
	void UnregisterZoneDonationMessageListener();
	void HandleZoneClearedPayload(FGameplayTag Channel, const FMyStreamingZoneClearedPayload& Payload);
	void RegisterZoneRuleMessageListener();
	void UnregisterZoneRuleMessageListener();
	void HandleZoneEventPayload(FGameplayTag Channel, const FMyStreamingZoneEventPayload& Payload);
	void StopMissionLoop();
	bool TryActivateMission(FGameplayTag MissionTag, FName SourceSequenceId);
	bool HasActiveMissionWithTag(FGameplayTag MissionTag) const;
	bool IsDefinitionValid(const FMyMissionDefinitionRow& Definition, const FMyMissionCombatObjectiveRow*& OutObjective) const;
	bool ResolveAssignees(const FMyMissionDefinitionRow& Definition, TArray<int32>& OutUserIndexes, TArray<FGameplayTag>& OutCharacterTags) const;
	AMyPlayerState* FindAuthenticatedPlayerState(int32 UserIndex) const;
	void HandleMissionTimeout(FGuid MissionInstanceId);
	void FinishActiveMission(FGuid MissionInstanceId, EMyMissionState Result);
	void RemoveMissionView(FGuid MissionInstanceId);
	void RequestMissionSequence(const FMyMissionServerState& Mission, FName SequenceId, FGameplayTag SourceEventTag, int32 PayloadMesoAmount = 0, bool bAssigneesOnly = false);
	void PublishMissionViews() const;
	float GetMissionServerTime() const;
	void BroadcastChatMessage(const FMyStreamingChatMessageData& ChatMessage) const;
	void ApplyGodPresentation(FMyStreamingChatMessageData& ChatMessage) const;
	//--
	bool BuildResolvedSequence(
		const FMyStreamingSequenceBuildInput& BuildInput,
		FMyResolvedStreamingSequence& OutSequence
	) const;
	bool StartSequence(FMyResolvedStreamingSequence&& Sequence);
	FMyStreamingSequenceRequestResult SubmitSequence(FMyResolvedStreamingSequence&& Sequence);
	void UpdateExecutionState(
		const FGuid& SequenceExecutionId,
		EMyStreamingSequenceExecutionState ExecutionState);
	void FailActiveSequenceAndStartNextPendingSequence();
	void StartNextPendingSequence();
	void ScheduleNextStep();
	void HandleSequenceTimerElapsed();
	void FinishSequence();
	void ResetSequencePlayback();
	bool IsSmallTalkSequenceId(FName SequenceId) const;
	TArray<FName> CollectUnusedSmallTalkSequenceIds() const;
	void ScheduleNextSmallTalk();
	void HandleSmallTalkTimerElapsed();
	void CancelSmallTalkForDialogue();
	void PruneInvalidStoryDialogueSessions();
	bool IsStoryDialogueBlockingSmallTalk() const;
	void RecordSmallTalkTimeline(
		EMySmallTalkTimelineEventType EventType,
		FName SequenceId = NAME_None,
		FName BlockingSequenceId = NAME_None,
		EMySmallTalkIntervalRangeName SelectedRange = EMySmallTalkIntervalRangeName::None,
		float Interval = 0.0f,
		float ScheduledTime = 0.0f);
	void DispatchChatMessage( const FMyStreamingChatMessageData& ChatMessage);
	//-- D-3: 확정 Step의 서버 Action(현재는 Donation Meso 지급)을 정확히 한 번 실행한다.
	void ExecuteResolvedStepAction(FMyResolvedStreamingStep& Step, int32 RecipientUserIndex);
	//-- 경험치/아이템 보상을 수령자 한 명에게 정확히 한 번 지급한다.
	void ExecuteRewardGrant(FMyResolvedStreamingStep& Step, int32 RecipientUserIndex);
	//-- 지급 대상 PlayerState를 확정한다(Standalone 테스트 수령자 우선).
	class AMyPlayerState* ResolveStepRecipient(int32 RecipientUserIndex) const;
	//-- D-3: 지급 대상 UserIndex에 해당하는 PlayerState를 소유 GameState에서 찾는다(임의 검색 아님).
	class AMyPlayerState* FindRecipientPlayerState(int32 RecipientUserIndex) const;
	//-- 실제 Meso 변화가 발생한 Donation Step을 수령자에게 Bubble과 Notice로 보낸다(서버).
	void SendDonationBubbleToRecipient(const FMyResolvedStreamingStep& Step, int32 RecipientUserIndex);
	//-- Mission 시작을 고정 파티의 기존 Notice 경로로 전송한다.
	void SendMissionNoticeToParty(const FMyResolvedStreamingStep& Step);
	//-- D-6: 전용 룰에서 ZoneIndex와 정확히 일치하는 하나의 SequenceId를 찾는다.
	bool TryFindZoneDonationSequenceId(int32 ZoneIndex, FName& OutSequenceId) const;
	//-- D-6: Rule이 참조한 Sequence가 단일 Donation Step 후보만 갖는지 런타임에서도 방어한다.
	bool IsRuleSequenceContractValid(
		FName SequenceId,
		const FMyStreamingRuleSourceContract& Contract,
		bool& bOutNeedsRecipients) const;
	bool IsChatOnlySequenceContractValid(FName SequenceId) const;
	//-- D-6: 현재 Dungeon GameState의 연결·인증된 파티원 UserIndex를 결정적 순서로 수집한다.
	TArray<int32> CollectZoneDonationRecipientUserIndexes() const;
	bool TryFillRuleRecipients(
		FMyStreamingSequenceRequest& Request,
		const FMyStreamingRuleSourceContract& Contract,
		bool bNeedsRecipients,
		int32 InstigatorUserIndex) const;
	//--
	FMyStreamingCombatRuleMatchResult MatchCombatRule(const FMyStreamingCombatPayload& Payload) const;
	bool DoesCombatRuleMatch(const FMyStreamingCombatPayload& Payload, const FMyStreamingCombatRuleRow& Rule) const;
	bool DoesTagMatch(FGameplayTag PayloadTag, FGameplayTag RuleTag) const;
	bool DoesFloatRangeMatch(float PayloadValue, float RuleMinValue, float RuleMaxValue) const;
	bool DoesBoolMatch(bool bPayloadValue, EMyStreamingRuleBoolMatch RuleMatch) const;
	FText BuildDebugMessageText(const FMyStreamingCombatPayload& Payload, const FMyStreamingCombatRuleMatchResult& MatchResult) const;
	FText GetGodDisplayName(FGameplayTag GodTag) const;
	FString GetTagLeafName(FGameplayTag Tag) const;


	UFUNCTION(NetMulticast, Reliable)
	void MulticastBroadcastChatMessage(const FMyStreamingChatMessageData& ChatMessage);
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat", meta = (AllowPrivateAccess = "true"))
	FText DefaultGodName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat", meta = (AllowPrivateAccess = "true", Categories = "God"))
	FGameplayTag DefaultGodTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Rule", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingCombatRuleRow"))
	TObjectPtr<UDataTable> CombatRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Meso", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingMesoRuleRow"))
	TObjectPtr<UDataTable> MesoRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|KillCount", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingKillCountRuleRow"))
	TObjectPtr<UDataTable> KillCountRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|SkillUse", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingSkillUseRuleRow"))
	TObjectPtr<UDataTable> SkillUseRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Line", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingChatLineRow"))
	TObjectPtr<UDataTable> ChatLineTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Gimmick", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingGimmickRuleRow"))
	TObjectPtr<UDataTable> GimmickRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ZoneDonation", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingZoneDonationRuleRow"))
	TObjectPtr<UDataTable> ZoneDonationRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Zone", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingZoneRuleRow"))
	TObjectPtr<UDataTable> ZoneRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Presentation", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyGodPresentationRow"))
	TObjectPtr<UDataTable> GodPresentationTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Mission", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionDefinitionRow"))
	TObjectPtr<UDataTable> MissionDefinitionTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Mission", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionCombatObjectiveRow"))
	TObjectPtr<UDataTable> MissionObjectiveTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|SmallTalk", meta = (AllowPrivateAccess = "true"))
	FMySmallTalkIntervalRange ShortSmallTalkInterval;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|SmallTalk", meta = (AllowPrivateAccess = "true"))
	FMySmallTalkIntervalRange NormalSmallTalkInterval;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|SmallTalk", meta = (AllowPrivateAccess = "true"))
	FMySmallTalkIntervalRange LongSmallTalkInterval;

	//! 잡담이 뭉칠 때 쓰는 간격이다. Weight는 뭉칠 확률(백분율)이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|SmallTalk", meta = (AllowPrivateAccess = "true"))
	FMySmallTalkIntervalRange ClusterSmallTalkInterval;

	//! 잠수로 볼 시간이다. DT_StreamingAntiAFKRules의 진입 행이 유일한 출처다.
	//! 표를 읽기 전에는 0이며, 0이면 잠수 감시를 시작하지 않는다.
	float AntiAFKTimeoutSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Tuning", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingTuningRow"))
	TObjectPtr<UDataTable> TuningTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|AntiAFK", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingAntiAFKRuleRow"))
	TObjectPtr<UDataTable> AntiAFKRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Count", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingCountRuleRow"))
	TObjectPtr<UDataTable> CountRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Count", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingItemRuleRow"))
	TObjectPtr<UDataTable> ItemRuleTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|State", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingStateRuleRow"))
	TObjectPtr<UDataTable> StateRuleTable = nullptr;

	UPROPERTY(Transient)
	TMap<FName, float> LastSequenceTriggerTimeMap;

	//! \brief Sequence와 Step별로 마지막에 실제 UI까지 전달한 Line을 컴포넌트 수명 동안 기억한다.
	TMap<FName, TMap<int32, FName>> LastDisplayedLineRowNamesBySequence;

	FGameplayMessageListenerHandle CombatMessageListenerHandle;
	FGameplayMessageListenerHandle PlayerInputListenerHandle;
	FGameplayMessageListenerHandle CountEventListenerHandle;
	FGameplayMessageListenerHandle ItemEventListenerHandle;
	FGameplayMessageListenerHandle StateListenerHandle;
	FGameplayMessageListenerHandle GimmickMessageListenerHandle;
	FGameplayMessageListenerHandle ZoneDonationMessageListenerHandle;
	FGameplayMessageListenerHandle ZoneRuleMessageListenerHandle;
	FGameplayMessageListenerHandle MesoMessageListenerHandle;
	TMap<int32, int64> EarnedMesoTotalsByUserIndex;
	TMap<int32, int64> SpentMesoTotalsByUserIndex;
	//! 던전 한 판의 파티 처치 누계다. Payload가 준 TargetTag를 그대로 열쇠로 쓴다.
	TMap<FGameplayTag, int32> PartyKillTotalsByTargetTag;
	//! 시간 창 Rule이 셀 최근 처치 기록이다. 가장 긴 창보다 오래된 기록은 버린다.
	TArray<FMyStreamingKillRecord> RecentPartyKills;
	//! 던전 한 판의 파티 스킬 사용 누계다. Payload가 준 SkillTag를 그대로 열쇠로 쓴다.
	TMap<FGameplayTag, int32> PartySkillUseTotalsBySkillTag;
	//! 미사용 Rule마다 하나씩 도는 타이머다. 같은 스킬을 쓰면 다시 감는다.
	TMap<FName, FTimerHandle> SkillIdleTimerHandles;
	//! 지금 켜져 있는 상태와 그 상태를 켠 출처들이다.
	//! 출처를 구분해야 같은 상태를 켜는 객체가 여럿일 때 하나가 꺼졌다고
	//! 나머지까지 꺼진 것으로 보지 않는다.
	TMap<FGameplayTag, TSet<FObjectKey>> ActiveStateSources;
	//! 조건 묶음과 출처의 짝마다 하나씩 도는 타이머다. 상태가 꺼지면 취소한다.
	TMap<FMyStreamingStateHoldKey, FTimerHandle> StateHoldTimerHandles;
	//! 이미 발동한 짝이다. 상태가 꺼지거나 리셋 사실이 와야 다시 잰다.
	TSet<FMyStreamingStateHoldKey> FiredStateHolds;
	//! 던전 한 판의 사건 기록 전부다. 누계 Rule과 시간 창 Rule이 함께 쓴다.
	//! 던전 동안 자르지 않는다. 자르면 누계 Rule이 최근 몇 초만 세게 된다.
	//! 던전이 끝나면 EndPlay가 비운다.
	TArray<FMyStreamingCountRecord> PartyCountRecords;
	TMap<FName, float> LastGimmickRuleTriggerTimeMap;
	TMap<FName, float> LastZoneRuleTriggerTimeMap;

	bool bSmallTalkSchedulerStarted = false;
	//! 방금 잡담을 재생했는지다. 뭉침은 재생 직후에만 일어난다.
	bool bSmallTalkJustPlayed = false;
	bool bSmallTalkSchedulerEnabled = false;
	bool bSmallTalkExhausted = false;
	bool bActiveSequenceIsSmallTalk = false;
	FTimerHandle SmallTalkTimerHandle;
	TSet<FName> UsedSmallTalkSequenceIds;
	TMap<TWeakObjectPtr<AActor>, TSet<int32>> ActiveStoryDialogueSessions;
	FTimerHandle AntiAFKTimerHandle;
	bool bAntiAFKStarted = false;
	bool bIsPartyAFK = false;
	bool bResumePresentationAfterDialogue = false;
	TArray<int32> DungeonPartyRoster;
	TSet<FGameplayTag> UsedMissionTags;
	TArray<FMyMissionServerState> MissionStates;
	TArray<FMyResolvedStreamingSequence> DeferredMissionSequences;
	bool bMissionLoopStarted = false;
#if !UE_BUILD_SHIPPING
	TWeakObjectPtr<AMyPlayerState> StandaloneMissionRecipient;
#endif
#if !UE_BUILD_SHIPPING
	TArray<FMySmallTalkTimelineEvent> SmallTalkTimelineEvents;
#endif

	//! \brief D-6: Manager(던전 GameState) 수명 동안 이미 소비한 OrderedZones 인덱스다.
	TSet<int32> ProcessedZoneDonationIndexes;

	struct FSequenceExecutionRecord
	{
		FMyStreamingSequenceRequest Request;
		FMyStreamingSequenceRequestResult Result;
	};

	static constexpr int32 MaxSequenceExecutionRecordCount = 4096;

	//! \brief Manager 수명 동안 실행 ID별 최초 요청과 현재/최종 결과를 보관한다.
	TMap<FGuid, FSequenceExecutionRecord> SequenceExecutionRecords;

	bool bIsSequencePlaying = false;

	FMyResolvedStreamingSequence ActiveSequence;

	int32 NextStepIndex = 0;

	FTimerHandle SequenceTimerHandle;

	// 큐처럼 이용함
	TArray<FMyResolvedStreamingSequence> PendingSequences;
};
