////////////////////////////
//! \page MyStreamingManagerComponent.cpp
//! \brief 전투 Payload를 스트리밍 채팅 메시지로 번역하는 Manager Component 구현 파일이다.
#include "MyStreamingManagerComponent.h"

#include "God/MyGodPresentationTypes.h"
#include "MyGameplayTags.h"
#include "GAS/MyPlayerState.h"
#include "Item/MyInventoryComponent.h"
#include "Player/PlayerCharacterBase.h"
#include "Dungeon/DungeonPC.h"
#include "GameFramework/GameStateBase.h"
#include "Streaming/MyLevelContentOverride.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogStreamingManager);

namespace
{
}

namespace MyStreamingSequencePolicy
{
	EMyStreamingSequenceSubmitAction ResolveSubmitAction(
		const bool bIsSequencePlaying,
		const EMyStreamingSequenceBusyPolicy ActivePolicy,
		const EMyStreamingSequenceBusyPolicy IncomingPolicy)
	{
		if (!bIsSequencePlaying)
		{
			return EMyStreamingSequenceSubmitAction::StartImmediately;
		}

		if (ActivePolicy == EMyStreamingSequenceBusyPolicy::Drop
			&& IncomingPolicy == EMyStreamingSequenceBusyPolicy::Queue)
		{
			return EMyStreamingSequenceSubmitAction::InterruptActive;
		}

		if (IncomingPolicy == EMyStreamingSequenceBusyPolicy::Drop)
		{
			return EMyStreamingSequenceSubmitAction::DropIncoming;
		}

		return EMyStreamingSequenceSubmitAction::QueueIncoming;
	}

	EMyStreamingSequenceRequestIdentity ResolveRequestIdentity(
		const FMyStreamingSequenceRequest& ExistingRequest,
		const FMyStreamingSequenceRequest& IncomingRequest)
	{
		if (ExistingRequest.SequenceExecutionId != IncomingRequest.SequenceExecutionId)
		{
			return EMyStreamingSequenceRequestIdentity::DifferentExecution;
		}

		if (ExistingRequest.SequenceId == IncomingRequest.SequenceId
			&& ExistingRequest.BusyPolicy == IncomingRequest.BusyPolicy
			&& ExistingRequest.SourceEventTag == IncomingRequest.SourceEventTag
			&& ExistingRequest.PayloadGodTag == IncomingRequest.PayloadGodTag
			&& ExistingRequest.PayloadMesoAmount == IncomingRequest.PayloadMesoAmount
			&& ExistingRequest.RecipientUserIndex == IncomingRequest.RecipientUserIndex
			&& ExistingRequest.RecipientUserIndexes == IncomingRequest.RecipientUserIndexes)
		{
			return EMyStreamingSequenceRequestIdentity::Retry;
		}

		return EMyStreamingSequenceRequestIdentity::Conflict;
	}

	TArray<int32> BuildEligibleCandidateIndexes(
		const TArray<FName>& CandidateRowNames,
		const FName LastDisplayedLineRowName)
	{
		TArray<int32> EligibleIndexes;
		EligibleIndexes.Reserve(CandidateRowNames.Num());

		const bool bCanExcludeLast = CandidateRowNames.Num() >= 2
			&& !LastDisplayedLineRowName.IsNone();
		for (int32 Index = 0; Index < CandidateRowNames.Num(); ++Index)
		{
			if (!bCanExcludeLast || CandidateRowNames[Index] != LastDisplayedLineRowName)
			{
				EligibleIndexes.Add(Index);
			}
		}

		// 잘못된 중복 RowName 데이터가 들어와도 재생 자체를 멈추지는 않는다.
		if (EligibleIndexes.IsEmpty())
		{
			for (int32 Index = 0; Index < CandidateRowNames.Num(); ++Index)
			{
				EligibleIndexes.Add(Index);
			}
		}

		return EligibleIndexes;
	}

	int32 SelectWeightedCandidateIndex(
		const TArray<int32>& CandidateWeights,
		const TArray<int32>& EligibleIndexes,
		int32 RandomWeight)
	{
		for (const int32 CandidateIndex : EligibleIndexes)
		{
			if (!CandidateWeights.IsValidIndex(CandidateIndex)
				|| CandidateWeights[CandidateIndex] <= 0)
			{
				return INDEX_NONE;
			}

			RandomWeight -= CandidateWeights[CandidateIndex];
			if (RandomWeight <= 0)
			{
				return CandidateIndex;
			}
		}

		return INDEX_NONE;
	}

	bool AreSmallTalkIntervalRangesValid(
		const FMySmallTalkIntervalRange& ShortRange,
		const FMySmallTalkIntervalRange& NormalRange,
		const FMySmallTalkIntervalRange& LongRange)
	{
		const auto IsRangeValid = [](const FMySmallTalkIntervalRange& Range)
		{
			return FMath::IsFinite(Range.MinSeconds)
				&& FMath::IsFinite(Range.MaxSeconds)
				&& Range.MinSeconds >= 0.0f
				&& Range.MaxSeconds >= Range.MinSeconds
				&& Range.Weight >= 0;
		};

		return IsRangeValid(ShortRange)
			&& IsRangeValid(NormalRange)
			&& IsRangeValid(LongRange)
			&& ShortRange.Weight + NormalRange.Weight + LongRange.Weight > 0;
	}

	EMySmallTalkIntervalRangeName SelectSmallTalkIntervalRange(
		const int32 ShortWeight,
		const int32 NormalWeight,
		const int32 LongWeight,
		int32 RandomWeight)
	{
		if (ShortWeight < 0 || NormalWeight < 0 || LongWeight < 0
			|| RandomWeight < 1
			|| RandomWeight > ShortWeight + NormalWeight + LongWeight)
		{
			return EMySmallTalkIntervalRangeName::None;
		}

		RandomWeight -= ShortWeight;
		if (RandomWeight <= 0)
		{
			return EMySmallTalkIntervalRangeName::Short;
		}
		RandomWeight -= NormalWeight;
		if (RandomWeight <= 0)
		{
			return EMySmallTalkIntervalRangeName::Normal;
		}
		return LongWeight > 0
			? EMySmallTalkIntervalRangeName::Long
			: EMySmallTalkIntervalRangeName::None;
	}

	bool TryResolveStepRewardAmount(
		EMyStreamingRewardSource RewardSource,
		int32 RewardMin,
		int32 RewardMax,
		int32 ResolvedInputValue,
		int32& OutAmount)
	{
		OutAmount = 0;
		switch (RewardSource)
		{
		case EMyStreamingRewardSource::None:
			// 돈 없음. Chat/None Step은 항상 0이다.
			return true;
		case EMyStreamingRewardSource::Payload:
			if (ResolvedInputValue == 0)
			{
				return false;
			}
			OutAmount = ResolvedInputValue;
			return true;
		case EMyStreamingRewardSource::RollFromLine:
			// 방어: validator가 막지만 런타임에서도 잘못된 범위를 실패로 처리한다.
			if (!(RewardMin > 0 && RewardMin <= RewardMax))
			{
				return false;
			}
			// 호출자가 뽑은 raw 값을 범위로 클램프해 항상 [Min,Max] 안을 보장한다.
			OutAmount = FMath::Clamp(ResolvedInputValue, RewardMin, RewardMax);
			return true;
		default:
			return false;
		}
	}

	EMyStreamingStepActionFailureReason CheckDonationGrantPreconditions(
		bool bHasAuthority,
		bool bRecipientValid,
		bool bInventoryValid,
		int32 Amount,
		int32 BeforeMeso)
	{
		if (!bHasAuthority)
		{
			return EMyStreamingStepActionFailureReason::NoAuthority;
		}
		if (!bRecipientValid)
		{
			return EMyStreamingStepActionFailureReason::InvalidRecipient;
		}
		if (!bInventoryValid)
		{
			return EMyStreamingStepActionFailureReason::InvalidInventory;
		}
		if (Amount <= 0)
		{
			return EMyStreamingStepActionFailureReason::NonPositiveAmount;
		}
		// int32 오버플로 방지: Before + Amount가 int32 최대치를 넘으면 지급하지 않는다.
		if (BeforeMeso > TNumericLimits<int32>::Max() - Amount)
		{
			return EMyStreamingStepActionFailureReason::Overflow;
		}
		return EMyStreamingStepActionFailureReason::None;
	}

	bool IsDonationGrantPostconditionMet(
		int32 BeforeMeso,
		int32 GrantedAmount,
		int32 AfterMeso)
	{
		return AfterMeso == BeforeMeso + GrantedAmount;
	}

	int32 ResolveAppliedMesoDelta(int32 RequestedDelta, int32 CurrentMeso)
	{
		if (RequestedDelta >= 0)
		{
			return RequestedDelta;
		}

		const int64 RequestedLoss = -static_cast<int64>(RequestedDelta);
		return -static_cast<int32>(FMath::Min<int64>(
			FMath::Max(0, CurrentMeso),
			RequestedLoss));
	}

	bool IsAuthenticatedUserIndex(int32 UserIndex)
	{
		// 프로젝트 인증 계약: UserIndex 0과 음수는 유효하지 않다.
		return UserIndex > 0;
	}

	bool TryMarkZoneDonationProcessed(
		TSet<int32>& ProcessedIndexes,
		const int32 ZoneIndex)
	{
		if (ZoneIndex < 0 || ProcessedIndexes.Contains(ZoneIndex))
		{
			return false;
		}

		ProcessedIndexes.Add(ZoneIndex);
		return true;
	}

	bool IsStandaloneDonationTestAllowed(ENetMode NetMode)
	{
		// Standalone에서만 개발 검증 진입점을 허용한다. ListenServer/Client/DedicatedServer는 거부.
		return NetMode == NM_Standalone;
	}

	FMyStreamingRuleSourceContract GetRuleSourceContract(const EMyStreamingRuleSource Source)
	{
		const uint8 ChatBit = 1u << static_cast<uint8>(EMyStreamingReaction::Chat);
		const uint8 DonationBit = 1u << static_cast<uint8>(EMyStreamingReaction::Reward);
		const uint8 MissionStartBit = 1u << static_cast<uint8>(EMyStreamingReaction::MissionStart);

		FMyStreamingRuleSourceContract Contract;
		switch (Source)
		{
		case EMyStreamingRuleSource::Combat:
		case EMyStreamingRuleSource::KillCount:
		case EMyStreamingRuleSource::SkillUse:
		case EMyStreamingRuleSource::CountEvent:
		case EMyStreamingRuleSource::StateHold:
		case EMyStreamingRuleSource::Gimmick:
		case EMyStreamingRuleSource::Zone:
			// 이 세 Payload에는 UserIndex가 없어 개인 지목이 불가능하다. 파티 전원이 유일하게 안전한 해석이다.
			Contract.AllowedReactions = ChatBit | DonationBit | MissionStartBit;
			Contract.RecipientMode = EMyStreamingRecipientMode::Party;
			break;

		case EMyStreamingRuleSource::ItemEvent:
			// 인벤토리는 PlayerState 소유라 UserIndex를 안다. 쓴 사람에게 지급한다.
		case EMyStreamingRuleSource::Meso:
			// Meso Payload는 UserIndex를 실어 보내므로 사실을 일으킨 개인에게 지급한다.
			Contract.AllowedReactions = ChatBit | DonationBit | MissionStartBit;
			Contract.RecipientMode = EMyStreamingRecipientMode::Instigator;
			break;

		case EMyStreamingRuleSource::ZoneDonation:
			// Zone 클리어 보상은 한 Step의 여러 대사 후보 중 하나만 뽑는 기존 계약을 유지한다.
			Contract.AllowedReactions = DonationBit;
			Contract.RecipientMode = EMyStreamingRecipientMode::Party;
			Contract.bRequireSingleStep = true;
			break;

		case EMyStreamingRuleSource::SmallTalk:
			// 예약 잡담은 서버 상태를 바꾸지 않는다.
			Contract.AllowedReactions = ChatBit;
			Contract.RecipientMode = EMyStreamingRecipientMode::None;
			break;
		}

		return Contract;
	}

	bool IsStatefulActionType(const EMyStreamingActionType ActionType)
	{
		return ActionType != EMyStreamingActionType::None;
	}

	bool IsStatefulSequenceBusyPolicyAllowed(
		const bool bContainsStatefulStep,
		const EMyStreamingSequenceBusyPolicy BusyPolicy)
	{
		// 상태 변경 요청은 잃어버리면 복구할 수 없으므로 Queue만 허용한다.
		return !bContainsStatefulStep
			|| BusyPolicy == EMyStreamingSequenceBusyPolicy::Queue;
	}

	bool TryResolveStepReaction(
		const EMyStreamingPresentationType PresentationType,
		const EMyStreamingActionType ActionType,
		const bool bHasMissionTag,
		const EMyStreamingRewardSource RewardSource,
		const int32 RewardMin,
		const int32 RewardMax,
		const FName RewardItemId,
		EMyStreamingReaction& OutReaction)
	{
		const bool bHasNoAmount =
			RewardSource == EMyStreamingRewardSource::None && RewardMin == 0 && RewardMax == 0;
		const bool bHasNoItem = RewardItemId.IsNone();

		// Meso·Exp·Item은 모두 같은 수량 계약을 쓴다. Item도 개수를 RewardMin~RewardMax에서 뽑는다.
		const bool bHasRolledAmount =
			RewardSource == EMyStreamingRewardSource::RollFromLine
			&& RewardMin > 0
			&& RewardMin <= RewardMax;

		if (PresentationType == EMyStreamingPresentationType::Chat
			&& ActionType == EMyStreamingActionType::None
			&& !bHasMissionTag
			&& bHasNoAmount
			&& bHasNoItem)
		{
			OutReaction = EMyStreamingReaction::Chat;
			return true;
		}

		if (PresentationType == EMyStreamingPresentationType::MissionStart
			&& ActionType == EMyStreamingActionType::StartMission
			&& bHasMissionTag
			&& bHasNoAmount
			&& bHasNoItem)
		{
			OutReaction = EMyStreamingReaction::MissionStart;
			return true;
		}

		// Rule 경로는 PayloadMesoAmount가 0이라 RollFromLine만 수량을 확정할 수 있고,
		// ApplyMesoDelta는 Mission 완료 Payload 전용이다.
		const bool bIsRolledAmountReward =
			(PresentationType == EMyStreamingPresentationType::Donation
				&& ActionType == EMyStreamingActionType::GrantMeso)
			|| (PresentationType == EMyStreamingPresentationType::ExpReward
				&& ActionType == EMyStreamingActionType::GrantExp);
		if (bIsRolledAmountReward && bHasRolledAmount && !bHasMissionTag && bHasNoItem)
		{
			OutReaction = EMyStreamingReaction::Reward;
			return true;
		}

		// Item 보상도 개수를 RewardMin~RewardMax에서 뽑는다. 지급할 ItemId만 더 필요하다.
		if (PresentationType == EMyStreamingPresentationType::ItemReward
			&& ActionType == EMyStreamingActionType::GrantItem
			&& bHasRolledAmount
			&& !bHasMissionTag
			&& !RewardItemId.IsNone())
		{
			OutReaction = EMyStreamingReaction::Reward;
			return true;
		}

		return false;
	}

	bool IsRuleSequenceStepContractValid(
		const EMyStreamingPresentationType PresentationType,
		const EMyStreamingActionType ActionType,
		const bool bHasMissionTag,
		const EMyStreamingRewardSource RewardSource,
		const int32 RewardMin,
		const int32 RewardMax,
		const FName RewardItemId,
		const FMyStreamingRuleSourceContract& Contract)
	{
		EMyStreamingReaction Reaction = EMyStreamingReaction::Chat;
		if (!TryResolveStepReaction(
				PresentationType, ActionType, bHasMissionTag, RewardSource, RewardMin, RewardMax,
				RewardItemId, Reaction))
		{
			return false;
		}

		// 수령자를 만들 수 없는 소스는 Donation을 낼 수 없다. 지급 없는 무음 실패를 막는다.
		if (Reaction == EMyStreamingReaction::Reward
			&& Contract.RecipientMode == EMyStreamingRecipientMode::None)
		{
			return false;
		}

		return Contract.IsReactionAllowed(Reaction);
	}

	bool ShouldBroadcastStepPresentation(
		const EMyStreamingPresentationType PresentationType,
		const bool bIsStartMissionAction,
		const bool bStartMissionSucceeded)
	{
		// Mission 생성에 실패한 StartMission Step은 채팅도 Notice도 남기지 않는다.
		if (bIsStartMissionAction && !bStartMissionSucceeded)
		{
			return false;
		}

		return PresentationType == EMyStreamingPresentationType::Chat
			|| PresentationType == EMyStreamingPresentationType::MissionStart;
	}

	bool TryResolveRewardKind(
		const EMyStreamingPresentationType PresentationType,
		EMyStreamingRewardKind& OutKind)
	{
		switch (PresentationType)
		{
		case EMyStreamingPresentationType::Donation:
			OutKind = EMyStreamingRewardKind::Meso;
			return true;
		case EMyStreamingPresentationType::ExpReward:
			OutKind = EMyStreamingRewardKind::Exp;
			return true;
		case EMyStreamingPresentationType::ItemReward:
			OutKind = EMyStreamingRewardKind::Item;
			return true;
		default:
			return false;
		}
	}

	bool ShouldSendRewardBubble(
		EMyStreamingPresentationType PresentationType,
		EMyStreamingActionType ActionType,
		bool bAttempted,
		bool bSucceeded,
		int32 AppliedMesoDelta)
	{
		if (!bAttempted || !bSucceeded)
		{
			return false;
		}

		// Meso는 실제 잔액 변화가 있어야 표시한다. Exp/Item은 지급 성공 자체가 표시 조건이다.
		switch (PresentationType)
		{
		case EMyStreamingPresentationType::Donation:
			return (ActionType == EMyStreamingActionType::GrantMeso && AppliedMesoDelta > 0)
				|| (ActionType == EMyStreamingActionType::ApplyMesoDelta && AppliedMesoDelta != 0);
		case EMyStreamingPresentationType::ExpReward:
			return ActionType == EMyStreamingActionType::GrantExp;
		case EMyStreamingPresentationType::ItemReward:
			return ActionType == EMyStreamingActionType::GrantItem;
		default:
			return false;
		}
	}

	FText GetDonationRecipientCharacterDisplayName(int32 SelectedCharacterId)
	{
		// Donation 문구에만 사용하는 기획 확정 한글명이다. 기존 Messenger 표시명 계약은 변경하지 않는다.
		switch (SelectedCharacterId)
		{
		case 100:
			return NSLOCTEXT("ProjectPStreaming", "DonationCharacterNefer", "네페르");
		case 200:
			return NSLOCTEXT("ProjectPStreaming", "DonationCharacterInpu", "인푸");
		case 300:
			return NSLOCTEXT("ProjectPStreaming", "DonationCharacterHeru", "헤루");
		default:
			return NSLOCTEXT("ProjectPStreaming", "DonationCharacterFallback", "플레이어");
		}
	}

	FText FormatDonationMessage(
		FGameplayTag GodTag,
		const FText& GodDisplayName,
		const FText& CharacterDisplayName,
		int32 AppliedMesoDelta,
		const FText& AuthoredLineMessage)
	{
		return FormatRewardMessage(
			GodTag,
			GodDisplayName,
			CharacterDisplayName,
			EMyStreamingRewardKind::Meso,
			AppliedMesoDelta,
			FText::GetEmpty(),
			AuthoredLineMessage);
	}

	FText FormatRewardMessage(
		FGameplayTag GodTag,
		const FText& GodDisplayName,
		const FText& CharacterDisplayName,
		const EMyStreamingRewardKind Kind,
		const int32 Amount,
		const FText& ItemDisplayName,
		const FText& AuthoredLineMessage)
	{
		auto EscapeRichTextContent = [](const FText& SourceText)
		{
			FString Escaped = SourceText.ToString();
			Escaped.ReplaceInline(TEXT("&"), TEXT("&amp;"), ESearchCase::CaseSensitive);
			Escaped.ReplaceInline(TEXT("<"), TEXT("&lt;"), ESearchCase::CaseSensitive);
			Escaped.ReplaceInline(TEXT(">"), TEXT("&gt;"), ESearchCase::CaseSensitive);
			return FText::FromString(Escaped);
		};

		FFormatNamedArguments Args;
		Args.Add(TEXT("GodTag"), FText::FromString(GodTag.ToString()));
		Args.Add(TEXT("God"), EscapeRichTextContent(GodDisplayName));
		Args.Add(TEXT("Character"), EscapeRichTextContent(CharacterDisplayName));
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::Abs(static_cast<int64>(Amount))));
		Args.Add(TEXT("Item"), EscapeRichTextContent(ItemDisplayName));

		FText SystemLine;
		switch (Kind)
		{
		case EMyStreamingRewardKind::Exp:
			SystemLine = FText::Format(
				NSLOCTEXT(
					"ProjectPStreaming",
					"ExpRewardRichTextFormatWithCharacter",
					"<god id=\"{GodTag}\">{God}</>의 후원으로 {Character}님이 경험치 {Amount}을(를) 획득했습니다."),
				Args);
			break;

		case EMyStreamingRewardKind::Item:
			SystemLine = FText::Format(
				NSLOCTEXT(
					"ProjectPStreaming",
					"ItemRewardRichTextFormatWithCharacter",
					"<god id=\"{GodTag}\">{God}</>의 후원으로 {Character}님이 {Item} {Amount}개를 획득했습니다."),
				Args);
			break;

		default:
			SystemLine = Amount < 0
				? FText::Format(
					NSLOCTEXT(
						"ProjectPStreaming",
						"ReverseDonationRichTextFormatWithCharacter",
						"<god id=\"{GodTag}\">{God}</>가 {Character}님에게서 {Amount}<img id=\"Meso\"/>를 가져갔습니다."),
					Args)
				: FText::Format(
					NSLOCTEXT(
						"ProjectPStreaming",
						"DonationRichTextFormatWithCharacter",
						"<god id=\"{GodTag}\">{God}</>의 후원으로 {Character}님이 {Amount}<img id=\"Meso\"/>를 획득했습니다."),
					Args);
			break;
		}

		// D-5B: 작성 대사의 앞뒤 공백/빈 줄만 제거하고 내부 줄바꿈은 보존한다.
		FString AuthoredTrimmed = AuthoredLineMessage.ToString();
		AuthoredTrimmed.TrimStartAndEndInline();
		if (AuthoredTrimmed.IsEmpty())
		{
			// 작성 대사가 없거나 공백뿐이면 시스템 문구만, 끝에 줄바꿈을 남기지 않는다.
			return SystemLine;
		}

		// 시스템 문구와 작성 대사 사이에 줄바꿈 한 개만 둔다(빈 줄 두 개 방지).
		return FText::FromString(SystemLine.ToString() + TEXT("\n") + AuthoredTrimmed);
	}

}

//! \author 장효제
//! \brief GameState 소유 컴포넌트를 복제 가능하게 초기화한다.
UMyStreamingManagerComponent::UMyStreamingManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DefaultGodName = NSLOCTEXT("ProjectPStreaming", "DefaultStreamingGodName", "토트");
	ShortSmallTalkInterval = {10.0f / 3.0f, 20.0f / 3.0f, 2};
	NormalSmallTalkInterval = {10.0f, 20.0f, 6};
	LongSmallTalkInterval = {30.0f, 50.0f, 2};
	// 뭉침은 재생 직후에만 쓴다. Weight는 가중치가 아니라 뭉칠 확률(백분율)이다.
	ClusterSmallTalkInterval = {0.8f, 2.5f, 40};

	static ConstructorHelpers::FObjectFinder<UDataTable> CombatRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingCombatRules.DT_StreamingCombatRules"));
	if (CombatRuleTableAsset.Succeeded())
	{
		CombatRuleTable = CombatRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> MesoRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingMesoRules.DT_StreamingMesoRules"));
	if (MesoRuleTableAsset.Succeeded())
	{
		MesoRuleTable = MesoRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> KillCountRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingKillCountRules.DT_StreamingKillCountRules"));
	if (KillCountRuleTableAsset.Succeeded())
	{
		KillCountRuleTable = KillCountRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> SkillUseRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingSkillUseRules.DT_StreamingSkillUseRules"));
	if (SkillUseRuleTableAsset.Succeeded())
	{
		SkillUseRuleTable = SkillUseRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> TuningTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingTuning.DT_StreamingTuning"));
	if (TuningTableAsset.Succeeded())
	{
		TuningTable = TuningTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> AntiAFKRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingAntiAFKRules.DT_StreamingAntiAFKRules"));
	if (AntiAFKRuleTableAsset.Succeeded())
	{
		AntiAFKRuleTable = AntiAFKRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> CountRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingCountRules.DT_StreamingCountRules"));
	if (CountRuleTableAsset.Succeeded())
	{
		CountRuleTable = CountRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ItemRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingItemRules.DT_StreamingItemRules"));
	if (ItemRuleTableAsset.Succeeded())
	{
		ItemRuleTable = ItemRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> StateRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingStateRules.DT_StreamingStateRules"));
	if (StateRuleTableAsset.Succeeded())
	{
		StateRuleTable = StateRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ChatLineTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingChatLines.DT_StreamingChatLines"));
	if (ChatLineTableAsset.Succeeded())
	{
		ChatLineTable = ChatLineTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> GimmickRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingGimmickRules.DT_StreamingGimmickRules"));
	if (GimmickRuleTableAsset.Succeeded())
	{
		GimmickRuleTable = GimmickRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ZoneDonationRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingZoneDonationRules.DT_StreamingZoneDonationRules"));
	if (ZoneDonationRuleTableAsset.Succeeded())
	{
		ZoneDonationRuleTable = ZoneDonationRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> ZoneRuleTableAsset(TEXT("/Game/LeDuat/Systems/Streaming/DT_StreamingZoneRules.DT_StreamingZoneRules"));
	if (ZoneRuleTableAsset.Succeeded())
	{
		ZoneRuleTable = ZoneRuleTableAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> GodPresentationTableAsset(MyGodPresentation::DefaultTablePath);
	if (GodPresentationTableAsset.Succeeded())
	{
		GodPresentationTable = GodPresentationTableAsset.Object;
	}
}

FMyStreamingSequenceRequestResult UMyStreamingManagerComponent::RequestSequence(
	const FMyStreamingSequenceRequest& Request)
{
	const auto MakeResult = [&Request](
		const EMyStreamingSequenceRequestStatus Status,
		const EMyStreamingSequenceExecutionState ExecutionState)
	{
		return FMyStreamingSequenceRequestResult{
			Request.SequenceExecutionId,
			Status,
			ExecutionState
		};
	};

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[요청 거부] 원인=서버 권한 없음 Owner=%s SequenceId=%s ExecutionId=%s"),
			*GetNameSafe(OwnerActor),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString());
		return MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedNoAuthority,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	if (!Request.SequenceExecutionId.IsValid())
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[요청 거부] 원인=유효하지 않은 ExecutionId SequenceId=%s ExecutionId=%s SourceEventTag=%s"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString(),
			*Request.SourceEventTag.ToString());
		return MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedInvalidRequest,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	if (const FSequenceExecutionRecord* ExistingRecord =
		SequenceExecutionRecords.Find(Request.SequenceExecutionId))
	{
		const EMyStreamingSequenceRequestIdentity Identity =
			MyStreamingSequencePolicy::ResolveRequestIdentity(
				ExistingRecord->Request,
				Request);
		if (Identity == EMyStreamingSequenceRequestIdentity::Retry)
		{
			UE_LOG(LogStreamingManager, Log,
				TEXT("[재요청 결과 반환] SequenceId=%s ExecutionId=%s RequestStatus=%d ExecutionState=%d"),
				*Request.SequenceId.ToString(),
				*Request.SequenceExecutionId.ToString(),
				static_cast<int32>(ExistingRecord->Result.Status),
				static_cast<int32>(ExistingRecord->Result.ExecutionState));
			return ExistingRecord->Result;
		}

		UE_LOG(LogStreamingManager, Error,
			TEXT("[요청 거부] 원인=ExecutionId 계약 충돌 ExecutionId=%s ExistingSequenceId=%s IncomingSequenceId=%s ExistingBusyPolicy=%d IncomingBusyPolicy=%d ExistingSourceEventTag=%s IncomingSourceEventTag=%s"),
			*Request.SequenceExecutionId.ToString(),
			*ExistingRecord->Request.SequenceId.ToString(),
			*Request.SequenceId.ToString(),
			static_cast<int32>(ExistingRecord->Request.BusyPolicy),
			static_cast<int32>(Request.BusyPolicy),
			*ExistingRecord->Request.SourceEventTag.ToString(),
			*Request.SourceEventTag.ToString());
		return MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedExecutionIdConflict,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	if (Request.SequenceId.IsNone() || !Request.SourceEventTag.IsValid())
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[요청 거부] 원인=잘못된 직접 요청 SequenceId=%s ExecutionId=%s SourceEventTag=%s"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString(),
			*Request.SourceEventTag.ToString());
		return MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedInvalidRequest,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	if (SequenceExecutionRecords.Num() >= MaxSequenceExecutionRecordCount)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[요청 거부] 원인=Execution 기록 한도 도달 SequenceId=%s ExecutionId=%s RecordCount=%d MaxRecordCount=%d"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString(),
			SequenceExecutionRecords.Num(),
			MaxSequenceExecutionRecordCount);
		return MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedExecutionRecordCapacity,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	FSequenceExecutionRecord& NewRecord =
		SequenceExecutionRecords.Add(Request.SequenceExecutionId);
	NewRecord.Request = Request;
	NewRecord.Result = MakeResult(
		EMyStreamingSequenceRequestStatus::RejectedExecutionUnavailable,
		EMyStreamingSequenceExecutionState::RejectedBeforeExecution);

	FMyStreamingSequenceBuildInput BuildInput;
	BuildInput.SequenceExecutionId = Request.SequenceExecutionId;
	BuildInput.SequenceId = Request.SequenceId;
	BuildInput.BusyPolicy = Request.BusyPolicy;
	BuildInput.SourceEventTag = Request.SourceEventTag;
	BuildInput.PayloadGodTag = Request.PayloadGodTag;
	BuildInput.PayloadMesoAmount = Request.PayloadMesoAmount;
	BuildInput.RecipientUserIndex = Request.RecipientUserIndex;
	BuildInput.RecipientUserIndexes = Request.RecipientUserIndexes;
#if !UE_BUILD_SHIPPING
	BuildInput.StandaloneTestRecipient = Request.StandaloneTestRecipient;
#endif

	FMyResolvedStreamingSequence Sequence;
	if (!BuildResolvedSequence(BuildInput, Sequence))
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[요청 거부] 원인=Sequence 조립 실패 SequenceId=%s ExecutionId=%s SourceEventTag=%s"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString(),
			*Request.SourceEventTag.ToString());
		NewRecord.Result = MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedBuildFailed,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
		return NewRecord.Result;
	}

	const bool bContainsStartMission = Sequence.Steps.ContainsByPredicate(
		[](const FMyResolvedStreamingStep& Step)
		{
			return Step.ActionType == EMyStreamingActionType::StartMission;
		});
	// 상태를 바꾸는 Step은 종류와 무관하게 Queue 전용이다. Drop이면 요청이 조용히 사라진다.
	const bool bContainsStatefulStep = Sequence.Steps.ContainsByPredicate(
		[](const FMyResolvedStreamingStep& Step)
		{
			return MyStreamingSequencePolicy::IsStatefulActionType(Step.ActionType);
		});
	if (!MyStreamingSequencePolicy::IsStatefulSequenceBusyPolicyAllowed(
			bContainsStatefulStep, Sequence.BusyPolicy))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[요청 거부] 상태 변경 Sequence는 Queue 전용 SequenceId=%s ExecutionId=%s"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString());
		NewRecord.Result = MakeResult(
			EMyStreamingSequenceRequestStatus::RejectedInvalidRequest,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
		return NewRecord.Result;
	}
	if (bContainsStartMission && !bMissionLoopStarted)
	{
		DeferredMissionSequences.Add(MoveTemp(Sequence));
		NewRecord.Result = MakeResult(
			EMyStreamingSequenceRequestStatus::AcceptedQueued,
			EMyStreamingSequenceExecutionState::Pending);
		UE_LOG(LogStreamingManager, Log,
			TEXT("[Mission Sequence 준비 대기] SequenceId=%s ExecutionId=%s Pending=%d"),
			*Request.SequenceId.ToString(),
			*Request.SequenceExecutionId.ToString(),
			DeferredMissionSequences.Num());
		return NewRecord.Result;
	}

	return SubmitSequence(MoveTemp(Sequence));
}


#if !UE_BUILD_SHIPPING
#endif

void UMyStreamingManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (const AMyLevelContentTableOverride* LevelOverride = AMyLevelContentTableOverride::FindForWorld(GetWorld()))
		{
			if (LevelOverride->CombatRuleTable)
			{
				CombatRuleTable = LevelOverride->CombatRuleTable;
			}
			if (LevelOverride->MesoRuleTable)
			{
				MesoRuleTable = LevelOverride->MesoRuleTable;
			}
			if (LevelOverride->KillCountRuleTable)
			{
				KillCountRuleTable = LevelOverride->KillCountRuleTable;
			}
			if (LevelOverride->SkillUseRuleTable)
			{
				SkillUseRuleTable = LevelOverride->SkillUseRuleTable;
			}
			if (LevelOverride->TuningTable)
			{
				TuningTable = LevelOverride->TuningTable;
			}
			if (LevelOverride->AntiAFKRuleTable)
			{
				AntiAFKRuleTable = LevelOverride->AntiAFKRuleTable;
			}
			if (LevelOverride->CountRuleTable)
			{
				CountRuleTable = LevelOverride->CountRuleTable;
			}
			if (LevelOverride->ItemRuleTable)
			{
				ItemRuleTable = LevelOverride->ItemRuleTable;
			}
			if (LevelOverride->StateRuleTable)
			{
				StateRuleTable = LevelOverride->StateRuleTable;
			}
			if (LevelOverride->ChatLineTable)
			{
				ChatLineTable = LevelOverride->ChatLineTable;
			}
			if (LevelOverride->GimmickRuleTable)
			{
				GimmickRuleTable = LevelOverride->GimmickRuleTable;
			}
			if (LevelOverride->ZoneDonationRuleTable)
			{
				ZoneDonationRuleTable = LevelOverride->ZoneDonationRuleTable;
			}
			if (LevelOverride->ZoneRuleTable)
			{
				ZoneRuleTable = LevelOverride->ZoneRuleTable;
			}
		}
	}

	// 표에서 읽은 값으로 잠수 판정 시간과 잡담 간격을 덮어쓴다.
	// 잠수 감시와 잡담 예약이 시작되기 전에 끝나야 한다.
	ApplyTuningTable();
	// 잠수 판정 시간은 조건 표가 정한다. 조절값 표보다 뒤에 적용해 조건이 이긴다.
	ApplyAntiAFKRuleTable();

	RegisterCombatMessageListener();
	RegisterCountEventListener();
	RegisterItemEventListener();
	RegisterStateListener();
	RegisterGimmickMessageListener();
	RegisterMesoMessageListener();
	RegisterZoneDonationMessageListener();
	RegisterZoneRuleMessageListener();
}

void UMyStreamingManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAntiAFK();
	StopMissionLoop();
	UnregisterCombatMessageListener();
	UnregisterCountEventListener();
	UnregisterItemEventListener();
	UnregisterStateListener();
	UnregisterGimmickMessageListener();
	UnregisterMesoMessageListener();
	UnregisterZoneDonationMessageListener();
	UnregisterZoneRuleMessageListener();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SmallTalkTimerHandle);
	}
	ResetSequencePlayback();
	PendingSequences.Reset();
	LastDisplayedLineRowNamesBySequence.Reset();
	SequenceExecutionRecords.Reset();
	ProcessedZoneDonationIndexes.Reset();
	LastGimmickRuleTriggerTimeMap.Reset();
	LastZoneRuleTriggerTimeMap.Reset();
	EarnedMesoTotalsByUserIndex.Reset();
	SpentMesoTotalsByUserIndex.Reset();
	PartyKillTotalsByTargetTag.Reset();
	RecentPartyKills.Reset();
	PartySkillUseTotalsBySkillTag.Reset();
	PartyCountRecords.Reset();
	ActiveStateSources.Reset();
	ClearAllStateHoldTimers();
	for (const TPair<FName, FTimerHandle>& Pair : SkillIdleTimerHandles)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(const_cast<FTimerHandle&>(Pair.Value));
		}
	}
	SkillIdleTimerHandles.Reset();
	UsedSmallTalkSequenceIds.Reset();
	bSmallTalkJustPlayed = false;
	ActiveStoryDialogueSessions.Reset();
#if !UE_BUILD_SHIPPING
	SmallTalkTimelineEvents.Reset();
#endif

	Super::EndPlay(EndPlayReason);
}

//! \author 장효제
//! \brief Combat 채널을 구독하여 전투 Payload를 수신한다.
void UMyStreamingManagerComponent::RegisterCombatMessageListener()
{
	UnregisterCombatMessageListener();

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error, TEXT("[시스템 오류] GameplayMessageSubsystem 인스턴스를 찾을 수 없음 작업=Combat listener 등록"));
		return;
	}

	CombatMessageListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingCombatPayload>(
		MyGameplayTags::Streaming_Channel_Combat,
		this,
		&UMyStreamingManagerComponent::HandleCombatPayload);

	UE_LOG(LogStreamingManager, Log, TEXT("[등록] Channel=%s HandleValid=%s Owner=%s"),
		*MyGameplayTags::Streaming_Channel_Combat.GetTag().ToString(),
		CombatMessageListenerHandle.IsValid() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetOwner()));
}

void UMyStreamingManagerComponent::UnregisterCombatMessageListener()
{
	if (CombatMessageListenerHandle.IsValid())
	{
		CombatMessageListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 운영 준비가 끝난 서버에서 SmallTalk 고정 구간을 검증하고 첫 예약을 만든다.
void UMyStreamingManagerComponent::StartSmallTalkScheduler()
{
	const AActor* OwnerActor = GetOwner();
	if (bSmallTalkSchedulerStarted || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	bSmallTalkSchedulerStarted = true;
	if (!MyStreamingSequencePolicy::AreSmallTalkIntervalRangesValid(
		ShortSmallTalkInterval,
		NormalSmallTalkInterval,
		LongSmallTalkInterval))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[SmallTalk 비활성] Interval Range 설정 오류 Short=(%.1f,%.1f,%d) Normal=(%.1f,%.1f,%d) Long=(%.1f,%.1f,%d)"),
			ShortSmallTalkInterval.MinSeconds,
			ShortSmallTalkInterval.MaxSeconds,
			ShortSmallTalkInterval.Weight,
			NormalSmallTalkInterval.MinSeconds,
			NormalSmallTalkInterval.MaxSeconds,
			NormalSmallTalkInterval.Weight,
			LongSmallTalkInterval.MinSeconds,
			LongSmallTalkInterval.MaxSeconds,
			LongSmallTalkInterval.Weight);
		return;
	}

	bSmallTalkSchedulerEnabled = true;
	ScheduleNextSmallTalk();
}

////////////////////////////
//! \author 장효제
//! \brief 오벨리스크 사용자 세션을 멱등 등록하고 첫 세션이면 SmallTalk Presentation을 취소한다.
//! \param SourceObelisk 실제 Dialogue를 시작한 서버 오벨리스크다.
//! \param UserId 기존 오벨리스크 세션이 사용하는 사용자 식별자다.
void UMyStreamingManagerComponent::NotifyStoryDialogueStarted(
	AActor* SourceObelisk,
	const int32 UserId)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValid(SourceObelisk) || UserId < 0)
	{
		return;
	}

	PruneInvalidStoryDialogueSessions();
	const bool bWasBlocked = IsStoryDialogueBlockingSmallTalk();
	ActiveStoryDialogueSessions.FindOrAdd(SourceObelisk).Add(UserId);
	if (!bWasBlocked)
	{
		CancelSmallTalkForDialogue();
	}

	if (bAntiAFKStarted)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AntiAFKTimerHandle);
		}
		AntiAFKTimerHandle.Invalidate();
		if (bIsPartyAFK)
		{
			ResumeFromAFK(FGameplayTag(), true);
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 오벨리스크 사용자 세션을 제거하고 파티의 마지막 세션이면 남은 시간 복원 없이 새로 예약한다.
//! \param SourceObelisk 세션을 종료한 서버 오벨리스크다.
//! \param UserId 제거할 사용자 식별자다.
void UMyStreamingManagerComponent::NotifyStoryDialogueEnded(
	AActor* SourceObelisk,
	const int32 UserId)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !SourceObelisk || UserId < 0)
	{
		return;
	}

	const bool bWasBlocked = IsStoryDialogueBlockingSmallTalk();
	PruneInvalidStoryDialogueSessions();
	if (TSet<int32>* Sessions = ActiveStoryDialogueSessions.Find(SourceObelisk))
	{
		Sessions->Remove(UserId);
		if (Sessions->IsEmpty())
		{
			ActiveStoryDialogueSessions.Remove(SourceObelisk);
		}
	}

	if (bWasBlocked && !IsStoryDialogueBlockingSmallTalk())
	{
		if (bAntiAFKStarted)
		{
			if (bResumePresentationAfterDialogue)
			{
				bResumePresentationAfterDialogue = false;
				RequestAntiAFKResumeSequence();
			}
			ArmAntiAFKTimer();
#if !UE_BUILD_SHIPPING
			UE_LOG(LogStreamingManager, Log,
				TEXT("[잠수 방지] Event=ReArmed Timeout=%.1f"),
				AntiAFKTimeoutSeconds);
#endif
		}
		ScheduleNextSmallTalk();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 서버 Streaming Manager가 Gimmick 사실 채널을 구독한다.
void UMyStreamingManagerComponent::RegisterGimmickMessageListener()
{
	UnregisterGimmickMessageListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[시스템 오류] GameplayMessageSubsystem 인스턴스를 찾을 수 없음 작업=Gimmick listener 등록"));
		return;
	}

	GimmickMessageListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingGimmickResetPayload>(
			MyGameplayTags::Streaming_Channel_Gimmick,
			this,
			&UMyStreamingManagerComponent::HandleGimmickResetPayload);
}

////////////////////////////
//! \author 장효제
//! \brief Gimmick 사실 채널 구독을 해제한다.
void UMyStreamingManagerComponent::UnregisterGimmickMessageListener()
{
	if (GimmickMessageListenerHandle.IsValid())
	{
		GimmickMessageListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief GimmickId와 정확한 누적 초기화 횟수에 일치하는 Chat Rule 하나를 실행한다.
//! \param Channel 수신한 GameplayMessage 채널이다.
//! \param Payload GimmickId와 Dungeon Scope 누적 파티 초기화 횟수다.
void UMyStreamingManagerComponent::HandleGimmickResetPayload(
	FGameplayTag Channel,
	const FMyStreamingGimmickResetPayload& Payload)
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World || !GimmickRuleTable
		|| Payload.GimmickId.IsNone() || Payload.PartyResetCount < 1)
	{
		return;
	}

	struct FCandidate
	{
		FName RowName;
		const FMyStreamingGimmickRuleRow* Rule = nullptr;
	};
	TArray<FCandidate> Candidates;
	int32 TotalWeight = 0;
	const float Now = World->GetTimeSeconds();
	for (const TPair<FName, uint8*>& Pair : GimmickRuleTable->GetRowMap())
	{
		const FMyStreamingGimmickRuleRow* Rule =
			reinterpret_cast<const FMyStreamingGimmickRuleRow*>(Pair.Value);
		if (!Rule || Rule->GimmickId != Payload.GimmickId
			|| Rule->ResetCount != Payload.PartyResetCount
			|| Rule->SequenceId.IsNone() || Rule->Weight <= 0)
		{
			continue;
		}

		if (const float* LastTime = LastGimmickRuleTriggerTimeMap.Find(Pair.Key);
			LastTime && Now < *LastTime + FMath::Max(0.0f, Rule->CooldownSeconds))
		{
			continue;
		}

		Candidates.Add({Pair.Key, Rule});
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	const FCandidate* Selected = nullptr;
	for (const FCandidate& Candidate : Candidates)
	{
		Roll -= Candidate.Rule->Weight;
		if (Roll <= 0)
		{
			Selected = &Candidate;
			break;
		}
	}
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::Gimmick);
	bool bNeedsRecipients = false;
	if (!Selected
		|| !IsRuleSequenceContractValid(Selected->Rule->SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Gimmick Rule 거부] Sequence 계약 오류 GimmickId=%s ResetCount=%d SequenceId=%s"),
			*Payload.GimmickId.ToString(),
			Payload.PartyResetCount,
			Selected ? *Selected->Rule->SequenceId.ToString() : TEXT("None"));
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = Selected->Rule->SequenceId;
	Request.BusyPolicy = Selected->Rule->BusyPolicy;
	Request.SourceEventTag = MyGameplayTags::Streaming_Event_Gimmick_PartyReset.GetTag();
	if (!TryFillRuleRecipients(Request, Contract, bNeedsRecipients, INDEX_NONE))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Gimmick Rule 거부] 원인=Donation 수령자 없음 GimmickId=%s SequenceId=%s"),
			*Payload.GimmickId.ToString(),
			*Selected->Rule->SequenceId.ToString());
		return;
	}
	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	if (Result.IsAccepted())
	{
		LastGimmickRuleTriggerTimeMap.Add(Selected->RowName, Now);
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Gimmick Rule 요청] Channel=%s GimmickId=%s ResetCount=%d Rule=%s SequenceId=%s RequestStatus=%d"),
		*Channel.ToString(),
		*Payload.GimmickId.ToString(),
		Payload.PartyResetCount,
		*Selected->RowName.ToString(),
		*Request.SequenceId.ToString(),
		static_cast<int32>(Result.Status));
}

////////////////////////////
//! \author 장효제
//! \brief 서버 StreamingManager가 Zone Clear Donation 전용 채널을 구독한다.
//! \param
//! \return
void UMyStreamingManagerComponent::RegisterZoneDonationMessageListener()
{
	UnregisterZoneDonationMessageListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[시스템 오류] GameplayMessageSubsystem 인스턴스를 찾을 수 없음 작업=Zone Donation listener 등록"));
		return;
	}

	ZoneDonationMessageListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingZoneClearedPayload>(
			MyGameplayTags::Streaming_Channel_Zone,
			this,
			&UMyStreamingManagerComponent::HandleZoneClearedPayload);
}

////////////////////////////
//! \author 장효제
//! \brief Zone Clear Donation 전용 채널 구독을 해제한다.
//! \param
//! \return
void UMyStreamingManagerComponent::UnregisterZoneDonationMessageListener()
{
	if (ZoneDonationMessageListenerHandle.IsValid())
	{
		ZoneDonationMessageListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 서버 Streaming Manager가 일반 Zone 사실 Payload를 별도 타입으로 구독한다.
void UMyStreamingManagerComponent::RegisterZoneRuleMessageListener()
{
	UnregisterZoneRuleMessageListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[시스템 오류] GameplayMessageSubsystem 인스턴스를 찾을 수 없음 작업=Zone Rule listener 등록"));
		return;
	}

	ZoneRuleMessageListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingZoneEventPayload>(
			MyGameplayTags::Streaming_Channel_Zone_Event,
			this,
			&UMyStreamingManagerComponent::HandleZoneEventPayload);
}

////////////////////////////
//! \author 장효제
//! \brief 일반 Zone 사실 Payload 구독을 해제한다.
void UMyStreamingManagerComponent::UnregisterZoneRuleMessageListener()
{
	if (ZoneRuleMessageListenerHandle.IsValid())
	{
		ZoneRuleMessageListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief EventTag와 ZoneIndex에 정확히 일치하는 일반 Zone Chat Rule 하나를 실행한다.
//! \param Channel 수신한 GameplayMessage 채널이다.
//! \param Payload 서버가 확정한 Zone EventTag, 인덱스와 디버그용 InstigatorUserIndex다.
void UMyStreamingManagerComponent::HandleZoneEventPayload(
	FGameplayTag Channel,
	const FMyStreamingZoneEventPayload& Payload)
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World
		|| !Payload.EventTag.IsValid() || Payload.ZoneIndex < 0)
	{
		return;
	}

	if (!ZoneRuleTable)
	{
		return;
	}

	struct FCandidate
	{
		FName RowName;
		const FMyStreamingZoneRuleRow* Rule = nullptr;
	};
	TArray<FCandidate> Candidates;
	int32 TotalWeight = 0;
	const float Now = World->GetTimeSeconds();
	for (const TPair<FName, uint8*>& Pair : ZoneRuleTable->GetRowMap())
	{
		const FMyStreamingZoneRuleRow* Rule =
			reinterpret_cast<const FMyStreamingZoneRuleRow*>(Pair.Value);
		if (!Rule || Rule->EventTag != Payload.EventTag
			|| Rule->ZoneIndex != Payload.ZoneIndex
			|| Rule->SequenceId.IsNone() || Rule->Weight <= 0)
		{
			continue;
		}

		if (const float* LastTime = LastZoneRuleTriggerTimeMap.Find(Pair.Key);
			LastTime && Now < *LastTime + FMath::Max(0.0f, Rule->CooldownSeconds))
		{
			continue;
		}

		Candidates.Add({Pair.Key, Rule});
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	const FCandidate* Selected = nullptr;
	for (const FCandidate& Candidate : Candidates)
	{
		Roll -= Candidate.Rule->Weight;
		if (Roll <= 0)
		{
			Selected = &Candidate;
			break;
		}
	}
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::Zone);
	bool bNeedsRecipients = false;
	if (!Selected
		|| !IsRuleSequenceContractValid(Selected->Rule->SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Zone Rule 거부] Sequence 계약 오류 EventTag=%s ZoneIndex=%d SequenceId=%s"),
			*Payload.EventTag.ToString(),
			Payload.ZoneIndex,
			Selected ? *Selected->Rule->SequenceId.ToString() : TEXT("None"));
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = Selected->Rule->SequenceId;
	Request.BusyPolicy = Selected->Rule->BusyPolicy;
	Request.SourceEventTag = Payload.EventTag;
	if (!TryFillRuleRecipients(Request, Contract, bNeedsRecipients, Payload.InstigatorUserIndex))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Zone Rule 거부] 원인=Donation 수령자 없음 ZoneIndex=%d SequenceId=%s"),
			Payload.ZoneIndex,
			*Selected->Rule->SequenceId.ToString());
		return;
	}
	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	if (Result.IsAccepted())
	{
		LastZoneRuleTriggerTimeMap.Add(Selected->RowName, Now);
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Zone Rule 요청] Channel=%s EventTag=%s ZoneIndex=%d InstigatorUserIndex=%d Rule=%s SequenceId=%s RequestStatus=%d"),
		*Channel.ToString(),
		*Payload.EventTag.ToString(),
		Payload.ZoneIndex,
		Payload.InstigatorUserIndex,
		*Selected->RowName.ToString(),
		*Request.SequenceId.ToString(),
		static_cast<int32>(Result.Status));
}

//! \author 장효제
//! \brief 서버 권한에서 Payload를 채팅 메시지로 번역하고 클라이언트로 전달한다.
void UMyStreamingManagerComponent::HandleCombatPayload(FGameplayTag Channel, const FMyStreamingCombatPayload& Payload)
{
	const AActor* OwnerActor = GetOwner();
	UE_LOG(LogStreamingManager, Verbose, TEXT("[Payload 수신] Channel=%s Owner=%s NetMode=%d HasAuthority=%s EventTag=%s InstigatorTag=%s TargetTag=%s SkillTag=%s Damage=%.2f HPRatio=%.3f Critical=%s Kill=%s"),
		*Channel.ToString(),
		*GetNameSafe(OwnerActor),
		OwnerActor ? static_cast<int32>(OwnerActor->GetNetMode()) : -1,
		(OwnerActor && OwnerActor->HasAuthority()) ? TEXT("true") : TEXT("false"),
		*Payload.EventTag.ToString(),
		*Payload.InstigatorTag.ToString(),
		*Payload.TargetTag.ToString(),
		*Payload.SkillTag.ToString(),
		Payload.DamageAmount,
		Payload.TargetCurrentHPRatio,
		Payload.bIsCritical ? TEXT("true") : TEXT("false"),
		Payload.bIsKill ? TEXT("true") : TEXT("false"));

	if (OwnerActor && OwnerActor->GetNetMode() != NM_Standalone && !OwnerActor->HasAuthority())
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[처리 종료] 원인=서버 권한 없음 Owner=%s NetMode=%d"),
			*GetNameSafe(OwnerActor),
			static_cast<int32>(OwnerActor->GetNetMode()));
		return;
	}

	HandleMissionCombatPayload(Channel, Payload);
	// 상태를 재는 중에 이 사실이 오면 처음부터 다시 잰다.
	// SkillTag를 함께 넘겨야 "스킬은 리셋하되 점프는 빼고"를 표로 나눌 수 있다.
	ResetStateHoldTimersForEvent(Payload.EventTag, Payload.SkillTag);
	HandleKillCountPayload(Payload);
	HandleSkillUsePayload(Payload);

	const FMyStreamingCombatRuleMatchResult MatchResult = MatchCombatRule(Payload);
	if (!MatchResult.bMatched)
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[처리 종료] 일치하는 전투 규칙 없음 EventTag=%s InstigatorTag=%s TargetTag=%s SkillTag=%s"),
			*Payload.EventTag.ToString(),
			*Payload.InstigatorTag.ToString(),
			*Payload.TargetTag.ToString(),
			*Payload.SkillTag.ToString());
		return;
	}

	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::Combat);
	bool bNeedsRecipients = false;
	if (!IsRuleSequenceContractValid(MatchResult.SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Combat Rule 거부] Sequence 계약 오류 EventTag=%s SequenceId=%s"),
			*Payload.EventTag.ToString(),
			*MatchResult.SequenceId.ToString());
		return;
	}

	FMyStreamingSequenceRequest SequenceRequest;
	SequenceRequest.SequenceExecutionId = FGuid::NewGuid();
	SequenceRequest.SequenceId = MatchResult.SequenceId;
	SequenceRequest.BusyPolicy = MatchResult.MatchedRule.BusyPolicy;
	SequenceRequest.SourceEventTag = Payload.EventTag;
	if (!TryFillRuleRecipients(SequenceRequest, Contract, bNeedsRecipients, INDEX_NONE))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Combat Rule 거부] 원인=Donation 수령자 없음 SequenceId=%s"),
			*MatchResult.SequenceId.ToString());
		return;
	}

	UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 일치] SequenceId=%s ExecutionId=%s EventTag=%s"),
		*MatchResult.SequenceId.ToString(),
		*SequenceRequest.SequenceExecutionId.ToString(),
		*Payload.EventTag.ToString());

	const FMyStreamingSequenceRequestResult RequestResult =
		RequestSequence(SequenceRequest);
	if (!RequestResult.IsAccepted())
	{
		UE_LOG(LogStreamingManager, Verbose,
			TEXT("[처리 종료] Sequence 요청 거부 SequenceId=%s ExecutionId=%s RequestStatus=%d"),
			*MatchResult.SequenceId.ToString(),
			*SequenceRequest.SequenceExecutionId.ToString(),
			static_cast<int32>(RequestResult.Status));
	}
}

////////////////////////////
//! \author 장효제
//! \brief 파티 처치 누계를 갱신하고 문턱을 처음 넘은 Kill Count Rule을 발동한다.
//! \param Payload 전투 사실이다. Kill이 아니거나 플레이어가 낸 것이 아니면 무시한다.
void UMyStreamingManagerComponent::HandleKillCountPayload(const FMyStreamingCombatPayload& Payload)
{
	const bool bIsKill = Payload.bIsKill
		|| Payload.EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_Kill);
	if (!bIsKill
		|| !KillCountRuleTable
		|| !Payload.InstigatorTag.MatchesTag(MyGameplayTags::Character_Player))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// 누계는 Payload가 준 태그를 그대로 열쇠로 쓴다. 종류가 몇 개뿐이라 표가 커지지 않고,
	// Rule 쪽 계층 매칭은 셀 때 합산으로 처리한다.
	int32& Total = PartyKillTotalsByTargetTag.FindOrAdd(Payload.TargetTag);
	++Total;
	RecentPartyKills.Add(FMyStreamingKillRecord{Now, Payload.TargetTag});

	// 가장 긴 시간 창보다 오래된 기록은 어떤 Rule도 쓰지 않는다.
	const float LongestWindow = GetLongestKillWindowSeconds();
	RecentPartyKills.RemoveAll(
		[Now, LongestWindow](const FMyStreamingKillRecord& Record)
		{
			return !MyStreamingKillCountRulePolicy::IsWithinWindow(
				Record.ServerTimeSeconds, Now, LongestWindow);
		});

	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[처치 누계] TargetTag=%s 누계=%d 최근기록=%d"),
		*Payload.TargetTag.ToString(),
		Total,
		RecentPartyKills.Num());

	const FMyStreamingKillCountRuleRow* Rule = SelectKillCountRule(Payload.TargetTag);
	if (!Rule)
	{
		return;
	}

	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::KillCount);
	bool bNeedsRecipients = false;
	if (!IsRuleSequenceContractValid(Rule->SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[KillCount Rule 거부] Sequence 계약 오류 TargetTag=%s SequenceId=%s"),
			*Rule->TargetTag.ToString(),
			*Rule->SequenceId.ToString());
		return;
	}

	FMyStreamingSequenceRequest SequenceRequest;
	SequenceRequest.SequenceExecutionId = FGuid::NewGuid();
	SequenceRequest.SequenceId = Rule->SequenceId;
	SequenceRequest.BusyPolicy = Rule->BusyPolicy;
	SequenceRequest.SourceEventTag = MyGameplayTags::Streaming_Event_Combat_Kill;
	if (!TryFillRuleRecipients(SequenceRequest, Contract, bNeedsRecipients, INDEX_NONE))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[KillCount Rule 거부] 원인=보상 수령자 없음 SequenceId=%s"),
			*Rule->SequenceId.ToString());
		return;
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[KillCount Rule 발동] TargetTag=%s RequiredKills=%d WindowSeconds=%.1f SequenceId=%s"),
		*Rule->TargetTag.ToString(),
		Rule->RequiredKills,
		Rule->WindowSeconds,
		*Rule->SequenceId.ToString());

	RequestSequence(SequenceRequest);
}

////////////////////////////
//! \author 장효제
//! \brief 이번 처치로 문턱을 처음 넘은 Kill Count Rule을 가중치로 하나 뽑는다.
//! \param KillTargetTag 방금 처치당한 대상의 태그다.
//! \return 발동할 Rule이다. 없으면 nullptr다.
const FMyStreamingKillCountRuleRow* UMyStreamingManagerComponent::SelectKillCountRule(
	const FGameplayTag KillTargetTag) const
{
	if (!KillCountRuleTable)
	{
		return nullptr;
	}

	TArray<FMyStreamingKillCountRuleRow*> AllRules;
	KillCountRuleTable->GetAllRows(
		TEXT("UMyStreamingManagerComponent::SelectKillCountRule"), AllRules);
	TArray<const FMyStreamingKillCountRuleRow*> Candidates;
	int32 TotalWeight = 0;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	for (const FMyStreamingKillCountRuleRow* Rule : AllRules)
	{
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| !MyStreamingKillCountRulePolicy::DoesKillMatchTarget(KillTargetTag, Rule->TargetTag))
		{
			continue;
		}

		// 이번 처치를 뺀 값과 넣은 값을 비교해 문턱을 통과하는 순간만 잡는다.
		const int32 NewCount = CountPartyKills(Rule->TargetTag, Rule->WindowSeconds, false);
		const int32 PreviousCount = CountPartyKills(Rule->TargetTag, Rule->WindowSeconds, true);
		if (!MyStreamingKillCountRulePolicy::DidCrossThreshold(
				PreviousCount, NewCount, Rule->RequiredKills))
		{
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
				LastTime && Now < *LastTime + Rule->CooldownSeconds)
			{
				continue;
			}
		}

		Candidates.Add(Rule);
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FMyStreamingKillCountRuleRow* Rule : Candidates)
	{
		Roll -= Rule->Weight;
		if (Roll <= 0)
		{
			return Rule;
		}
	}

	return Candidates.Last();
}

////////////////////////////
//! \author 장효제
//! \brief Rule이 요구하는 대상과 시간 창에 해당하는 파티 처치 수를 센다.
//! \param RuleTargetTag 셀 대상이다. 비어 있으면 모든 대상을 센다.
//! \param WindowSeconds 0이면 던전 누계, 0보다 크면 그 시간 안의 처치만 센다.
//! \param bExcludeLatest 방금 들어온 처치 하나를 빼고 셀지 여부다.
//! \return 센 마릿수다.
int32 UMyStreamingManagerComponent::CountPartyKills(
	const FGameplayTag RuleTargetTag,
	const float WindowSeconds,
	const bool bExcludeLatest) const
{
	const int32 LatestAdjustment = bExcludeLatest ? 1 : 0;

	if (!MyStreamingKillCountRulePolicy::IsWindowRule(WindowSeconds))
	{
		// 누계는 계층 매칭이므로 하위 태그 버킷까지 합산한다.
		int32 Total = 0;
		for (const TPair<FGameplayTag, int32>& Bucket : PartyKillTotalsByTargetTag)
		{
			if (MyStreamingKillCountRulePolicy::DoesKillMatchTarget(Bucket.Key, RuleTargetTag))
			{
				Total += Bucket.Value;
			}
		}
		return FMath::Max(Total - LatestAdjustment, 0);
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	int32 Count = 0;
	for (const FMyStreamingKillRecord& Record : RecentPartyKills)
	{
		if (MyStreamingKillCountRulePolicy::DoesKillMatchTarget(Record.TargetTag, RuleTargetTag)
			&& MyStreamingKillCountRulePolicy::IsWithinWindow(
				Record.ServerTimeSeconds, Now, WindowSeconds))
		{
			++Count;
		}
	}
	return FMath::Max(Count - LatestAdjustment, 0);
}

////////////////////////////
//! \author 장효제
//! \brief 표에 있는 Rule 중 가장 긴 시간 창을 반환한다. 오래된 기록을 버릴 기준이다.
//! \return 초 단위 시간 창이다. 시간 창 Rule이 없으면 0이다.
float UMyStreamingManagerComponent::GetLongestKillWindowSeconds() const
{
	if (!KillCountRuleTable)
	{
		return 0.0f;
	}

	TArray<FMyStreamingKillCountRuleRow*> AllRules;
	KillCountRuleTable->GetAllRows(
		TEXT("UMyStreamingManagerComponent::GetLongestKillWindowSeconds"), AllRules);
	float Longest = 0.0f;
	for (const FMyStreamingKillCountRuleRow* Rule : AllRules)
	{
		if (Rule)
		{
			Longest = FMath::Max(Longest, Rule->WindowSeconds);
		}
	}
	return Longest;
}

////////////////////////////
//! \author 장효제
//! \brief 조건이 고른 Sequence를 계약 검사와 수령자 확보를 거쳐 요청한다.
//! \param SequenceId 재생할 Sequence다.
//! \param BusyPolicy 다른 Sequence가 재생 중일 때의 처리다.
//! \param Source 이 요청을 낸 Rule 소스다. 허용 반응과 수령자 해석을 정한다.
//! \param SourceEventTag 이 요청을 만든 사실 태그다.
//! \return 요청까지 도달했으면 true다.
namespace
{
	//! \brief Rule 소스의 로그용 이름이다. UENUM이 아니라 리플렉션을 쓸 수 없다.
	const TCHAR* GetRuleSourceName(const EMyStreamingRuleSource Source)
	{
		switch (Source)
		{
		case EMyStreamingRuleSource::Combat: return TEXT("Combat");
		case EMyStreamingRuleSource::KillCount: return TEXT("KillCount");
		case EMyStreamingRuleSource::SkillUse: return TEXT("SkillUse");
		case EMyStreamingRuleSource::Meso: return TEXT("Meso");
		case EMyStreamingRuleSource::Gimmick: return TEXT("Gimmick");
		case EMyStreamingRuleSource::Zone: return TEXT("Zone");
		case EMyStreamingRuleSource::ZoneDonation: return TEXT("ZoneDonation");
		case EMyStreamingRuleSource::SmallTalk: return TEXT("SmallTalk");
		}
		return TEXT("Unknown");
	}
}

bool UMyStreamingManagerComponent::RequestRuleSequence(
	const FName SequenceId,
	const EMyStreamingSequenceBusyPolicy BusyPolicy,
	const EMyStreamingRuleSource Source,
	const FGameplayTag SourceEventTag)
{
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(Source);
	bool bNeedsRecipients = false;
	if (!IsRuleSequenceContractValid(SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Rule 거부] Sequence 계약 오류 Source=%s SequenceId=%s"),
			GetRuleSourceName(Source),
			*SequenceId.ToString());
		return false;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = SequenceId;
	Request.BusyPolicy = BusyPolicy;
	Request.SourceEventTag = SourceEventTag;
	if (!TryFillRuleRecipients(Request, Contract, bNeedsRecipients, INDEX_NONE))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Rule 거부] 원인=보상 수령자 없음 Source=%s SequenceId=%s"),
			GetRuleSourceName(Source),
			*SequenceId.ToString());
		return false;
	}

	RequestSequence(Request);
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief 파티 스킬 사용 누계를 갱신하고 문턱을 처음 넘은 Rule을 발동한다.
//! \param Payload 전투 사실이다. 스킬 사용이 아니면 무시한다.
void UMyStreamingManagerComponent::HandleSkillUsePayload(const FMyStreamingCombatPayload& Payload)
{
	if (!Payload.EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_SkillUsed)
		|| !Payload.SkillTag.IsValid()
		|| !SkillUseRuleTable
		|| !Payload.InstigatorTag.MatchesTag(MyGameplayTags::Character_Player))
	{
		return;
	}

	int32& Total = PartySkillUseTotalsBySkillTag.FindOrAdd(Payload.SkillTag);
	++Total;

	// 이 스킬을 요구하는 미사용 Rule의 타이머를 다시 감는다. 방금 썼기 때문이다.
	for (const TPair<FName, uint8*>& Pair : SkillUseRuleTable->GetRowMap())
	{
		const FMyStreamingSkillUseRuleRow* Rule =
			reinterpret_cast<const FMyStreamingSkillUseRuleRow*>(Pair.Value);
		if (Rule
			&& MyStreamingSkillUseRulePolicy::IsIdleRule(Rule->IdleSeconds)
			&& MyStreamingSkillUseRulePolicy::DoesUseMatchSkill(Payload.SkillTag, Rule->SkillTag))
		{
			RearmSkillIdleTimer(Pair.Key, Rule->IdleSeconds);
		}
	}

	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[스킬 사용 누계] SkillTag=%s 누계=%d"),
		*Payload.SkillTag.ToString(),
		Total);

	const FMyStreamingSkillUseRuleRow* Selected = SelectSkillUseRule(Payload.SkillTag);
	if (!Selected)
	{
		return;
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[SkillUse Rule 발동] SkillTag=%s RequiredUses=%d SequenceId=%s"),
		*Selected->SkillTag.ToString(),
		Selected->RequiredUses,
		*Selected->SequenceId.ToString());

	RequestRuleSequence(
		Selected->SequenceId,
		Selected->BusyPolicy,
		EMyStreamingRuleSource::SkillUse,
		MyGameplayTags::Streaming_Event_Combat_SkillUsed);
}

////////////////////////////
//! \author 장효제
//! \brief 이번 사용으로 문턱을 처음 넘은 누적 사용 Rule을 가중치로 하나 뽑는다.
//! \param UsedSkillTag 방금 쓴 스킬 태그다.
//! \return 발동할 Rule이다. 없으면 nullptr다.
const FMyStreamingSkillUseRuleRow* UMyStreamingManagerComponent::SelectSkillUseRule(
	const FGameplayTag UsedSkillTag) const
{
	if (!SkillUseRuleTable)
	{
		return nullptr;
	}

	TArray<const FMyStreamingSkillUseRuleRow*> Candidates;
	int32 TotalWeight = 0;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	for (const TPair<FName, uint8*>& Pair : SkillUseRuleTable->GetRowMap())
	{
		const FMyStreamingSkillUseRuleRow* Rule =
			reinterpret_cast<const FMyStreamingSkillUseRuleRow*>(Pair.Value);
		// 미사용 Rule은 사실이 올 때가 아니라 타이머가 끝날 때 발동한다.
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| MyStreamingSkillUseRulePolicy::IsIdleRule(Rule->IdleSeconds)
			|| !MyStreamingSkillUseRulePolicy::DoesUseMatchSkill(UsedSkillTag, Rule->SkillTag))
		{
			continue;
		}

		// 이번 사용을 뺀 값과 넣은 값을 비교해 문턱을 통과하는 순간만 잡는다.
		const int32 NewCount = CountPartySkillUses(Rule->SkillTag);
		if (!MyStreamingSkillUseRulePolicy::DidCrossThreshold(
				NewCount - 1, NewCount, Rule->RequiredUses))
		{
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
				LastTime && Now < *LastTime + Rule->CooldownSeconds)
			{
				continue;
			}
		}

		Candidates.Add(Rule);
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FMyStreamingSkillUseRuleRow* Rule : Candidates)
	{
		Roll -= Rule->Weight;
		if (Roll <= 0)
		{
			return Rule;
		}
	}

	return Candidates.Last();
}

////////////////////////////
//! \author 장효제
//! \brief Rule이 요구하는 스킬에 해당하는 파티 사용 횟수를 센다.
//! \param RuleSkillTag 셀 스킬이다. 비어 있으면 모든 스킬을 센다.
//! \return 센 사용 횟수다.
int32 UMyStreamingManagerComponent::CountPartySkillUses(const FGameplayTag RuleSkillTag) const
{
	// 계층 매칭이므로 하위 태그 버킷까지 합산한다.
	int32 Total = 0;
	for (const TPair<FGameplayTag, int32>& Bucket : PartySkillUseTotalsBySkillTag)
	{
		if (MyStreamingSkillUseRulePolicy::DoesUseMatchSkill(Bucket.Key, RuleSkillTag))
		{
			Total += Bucket.Value;
		}
	}
	return Total;
}

////////////////////////////
//! \author 장효제
//! \brief 표에 있는 모든 미사용 Rule의 타이머를 처음 건다.
////////////////////////////
//! \author 장효제
//! \brief 조절값 표를 읽어 잠수 판정 시간과 잡담 간격을 덮어쓴다.
//! \details 표가 없거나 행이 빠졌거나 값이 규격 밖이면 C++ 기본값을 그대로 둔다.
//!          기획자가 표만 고쳐도 되게 하되, 잘못된 값으로 기능이 멈추지는 않게 한다.
void UMyStreamingManagerComponent::ApplyTuningTable()
{
	if (!TuningTable)
	{
		return;
	}

	const TCHAR* Context = TEXT("UMyStreamingManagerComponent::ApplyTuningTable");

	const TPair<FName, FMySmallTalkIntervalRange*> Intervals[] = {
		{MyStreamingTuningRowNames::SmallTalkShort, &ShortSmallTalkInterval},
		{MyStreamingTuningRowNames::SmallTalkNormal, &NormalSmallTalkInterval},
		{MyStreamingTuningRowNames::SmallTalkLong, &LongSmallTalkInterval},
		{MyStreamingTuningRowNames::SmallTalkCluster, &ClusterSmallTalkInterval},
	};
	for (const TPair<FName, FMySmallTalkIntervalRange*>& Entry : Intervals)
	{
		const FMyStreamingTuningRow* Row = TuningTable->FindRow<FMyStreamingTuningRow>(
			Entry.Key, Context, false);
		if (!Row)
		{
			continue;
		}

		const bool bIsClusterRow = Entry.Key == MyStreamingTuningRowNames::SmallTalkCluster;
		if (bIsClusterRow && !MyStreamingTuningPolicy::IsClusterChanceValid(Row->Weight))
		{
			UE_LOG(LogStreamingManager, Warning,
				TEXT("[조절값 무시] Row=%s 원인=뭉칠 확률이 0~100 밖 Weight=%d"),
				*Entry.Key.ToString(), Row->Weight);
			continue;
		}

		if (!MyStreamingTuningPolicy::IsIntervalRowValid(*Row))
		{
			UE_LOG(LogStreamingManager, Warning,
				TEXT("[조절값 무시] Row=%s 원인=잡담 간격 규격 밖 Min=%.2f Max=%.2f Weight=%d"),
				*Entry.Key.ToString(), Row->MinSeconds, Row->MaxSeconds, Row->Weight);
			continue;
		}

		Entry.Value->MinSeconds = Row->MinSeconds;
		Entry.Value->MaxSeconds = Row->MaxSeconds;
		Entry.Value->Weight = Row->Weight;
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[조절값 적용] 잡담 짧음=(%.1f~%.1f,%d) 보통=(%.1f~%.1f,%d) 김=(%.1f~%.1f,%d)"),
		ShortSmallTalkInterval.MinSeconds, ShortSmallTalkInterval.MaxSeconds, ShortSmallTalkInterval.Weight,
		NormalSmallTalkInterval.MinSeconds, NormalSmallTalkInterval.MaxSeconds, NormalSmallTalkInterval.Weight,
		LongSmallTalkInterval.MinSeconds, LongSmallTalkInterval.MaxSeconds, LongSmallTalkInterval.Weight);
	UE_LOG(LogStreamingManager, Log,
		TEXT("[조절값 적용] 잡담 뭉침=%.1f~%.1f초 확률=%d%%"),
		ClusterSmallTalkInterval.MinSeconds,
		ClusterSmallTalkInterval.MaxSeconds,
		ClusterSmallTalkInterval.Weight);
}

////////////////////////////
//! \author 장효제
//! \brief 잠수 조건 표에서 판정 시간을 읽어 적용한다.
//! \details 표가 없거나 값이 규격 밖이면 앞서 정해진 값을 그대로 둔다.
void UMyStreamingManagerComponent::ApplyAntiAFKRuleTable()
{
	if (!AntiAFKRuleTable)
	{
		return;
	}

	const FMyStreamingAntiAFKRuleRow* Row =
		AntiAFKRuleTable->FindRow<FMyStreamingAntiAFKRuleRow>(
			MyStreamingAntiAFKRuleNames::Enter,
			TEXT("UMyStreamingManagerComponent::ApplyAntiAFKRuleTable"),
			false);
	if (!Row)
	{
		return;
	}

	if (!MyStreamingAntiAFKRulePolicy::IsIdleSecondsValid(Row->IdleSeconds))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잠수 조건 오류] 원인=판정 시간이 0 이하 IdleSeconds=%.2f"),
			Row->IdleSeconds);
		return;
	}

	AntiAFKTimeoutSeconds = Row->IdleSeconds;
	UE_LOG(LogStreamingManager, Log,
		TEXT("[잠수 조건 적용] 판정 시간=%.1f초"), AntiAFKTimeoutSeconds);
}

////////////////////////////
//! \author 장효제
//! \brief 잠수 조건 표가 가리키는 Sequence를 돌려준다.
//! \details 표가 유일한 출처다. 표가 없거나 행이 비면 재생하지 않는다.
//!          예전에는 이름 하드코딩으로 물러났는데, 그러면 표를 고쳐도 동작이
//!          바뀌지 않아 표가 거짓말이 된다.
//! \param RuleRowName 찾을 조건 행 이름이다.
//! \return 재생할 SequenceId다. 표가 정하지 못하면 NAME_None이다.
FName UMyStreamingManagerComponent::ResolveAntiAFKSequenceId(const FName RuleRowName) const
{
	if (!AntiAFKRuleTable)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잠수 조건 없음] 원인=조건 표가 없음 Row=%s"), *RuleRowName.ToString());
		return NAME_None;
	}

	const FMyStreamingAntiAFKRuleRow* Row =
		AntiAFKRuleTable->FindRow<FMyStreamingAntiAFKRuleRow>(
			RuleRowName,
			TEXT("UMyStreamingManagerComponent::ResolveAntiAFKSequenceId"),
			false);
	if (!Row || Row->SequenceId.IsNone())
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잠수 조건 없음] 원인=행이 없거나 SequenceId가 빔 Row=%s"),
			*RuleRowName.ToString());
		return NAME_None;
	}

	return Row->SequenceId;
}

void UMyStreamingManagerComponent::ArmSkillIdleTimers()
{
	if (!SkillUseRuleTable)
	{
		return;
	}

	for (const TPair<FName, uint8*>& Pair : SkillUseRuleTable->GetRowMap())
	{
		const FMyStreamingSkillUseRuleRow* Rule =
			reinterpret_cast<const FMyStreamingSkillUseRuleRow*>(Pair.Value);
		if (Rule
			&& !Rule->SequenceId.IsNone()
			&& MyStreamingSkillUseRulePolicy::IsIdleRule(Rule->IdleSeconds))
		{
			RearmSkillIdleTimer(Pair.Key, Rule->IdleSeconds);
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 미사용 Rule 하나의 타이머를 처음부터 다시 감는다.
//! \param RuleRowName 다시 감을 Rule의 RowName이다.
//! \param IdleSeconds 기다릴 시간이다.
void UMyStreamingManagerComponent::RearmSkillIdleTimer(
	const FName RuleRowName,
	const float IdleSeconds)
{
	UWorld* World = GetWorld();
	if (!World || IdleSeconds <= 0.0f)
	{
		return;
	}

	FTimerHandle& Handle = SkillIdleTimerHandles.FindOrAdd(RuleRowName);
	World->GetTimerManager().ClearTimer(Handle);

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ThisClass::HandleSkillIdleTimeout, RuleRowName);
	World->GetTimerManager().SetTimer(Handle, Delegate, IdleSeconds, false);
}

////////////////////////////
//! \author 장효제
//! \brief 정해진 시간 동안 그 스킬을 쓰지 않았을 때 Sequence를 요청한다.
//! \param RuleRowName 시간이 끝난 Rule의 RowName이다.
void UMyStreamingManagerComponent::HandleSkillIdleTimeout(const FName RuleRowName)
{
	const AActor* OwnerActor = GetOwner();
	if (!SkillUseRuleTable || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	const FMyStreamingSkillUseRuleRow* Rule =
		SkillUseRuleTable->FindRow<FMyStreamingSkillUseRuleRow>(
			RuleRowName,
			TEXT("UMyStreamingManagerComponent::HandleSkillIdleTimeout"),
			false);
	if (!Rule || Rule->SequenceId.IsNone())
	{
		return;
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[SkillUse 미사용 발동] Rule=%s SkillTag=%s IdleSeconds=%.1f SequenceId=%s"),
		*RuleRowName.ToString(),
		*Rule->SkillTag.ToString(),
		Rule->IdleSeconds,
		*Rule->SequenceId.ToString());

	RequestRuleSequence(
		Rule->SequenceId,
		Rule->BusyPolicy,
		EMyStreamingRuleSource::SkillUse,
		MyGameplayTags::Streaming_Event_Combat_SkillUsed);

	// 계속 쓰지 않으면 다시 알린다. 쿨다운이 반복 간격을 정한다.
	RearmSkillIdleTimer(RuleRowName, FMath::Max(Rule->IdleSeconds, Rule->CooldownSeconds));
}

////////////////////////////
//! \author 장효제
//! \brief 파티 사건 누계 사실 채널을 서버에서 구독한다.
////////////////////////////
//! \author 장효제
//! \brief 사건 누계·아이템 누계 목표를 가진 Mission의 진행도를 올린다.
//! \details 전투 사실은 HandleMissionCombatPayload가 다룬다. 이 두 사건은 다른
//!          채널로 오므로 진행도도 여기서 따로 올린다.
//! \param EventTag 방금 일어난 사건이다.
//! \param ItemId 아이템 사건이면 그 RowName이고, 아니면 비어 있다.
//! \param Amount 이번에 오른 양이다.
void UMyStreamingManagerComponent::AdvanceMissionsForCountEvent(
	const FGameplayTag EventTag,
	const FName ItemId,
	const int32 Amount)
{
	if (!EventTag.IsValid() || Amount <= 0)
	{
		return;
	}

	const float ServerTime = GetMissionServerTime();
	TArray<TPair<FGuid, EMyMissionState>> FinishedMissions;
	bool bProgressChanged = false;

	for (FMyMissionServerState& Mission : MissionStates)
	{
		if (Mission.State != EMyMissionState::Active)
		{
			continue;
		}

		const bool bIsEventObjective =
			Mission.Objective.ConditionType == EMyMissionConditionType::EventCount;
		const bool bIsItemObjective =
			Mission.Objective.ConditionType == EMyMissionConditionType::ItemCount;
		if (!bIsEventObjective && !bIsItemObjective)
		{
			continue;
		}

		const bool bContributes =
			MyStreamingCountRulePolicy::DoesTagMatch(EventTag, Mission.Objective.EventTag)
			&& (!bIsItemObjective
				|| MyStreamingCountRulePolicy::DoesItemMatch(ItemId, Mission.Objective.ItemId));

		if (ServerTime <= Mission.MissionEndsAtServerTime && bContributes)
		{
			Mission.ProgressCount += Amount;
			bProgressChanged = true;
			UE_LOG(LogStreamingManager, Log,
				TEXT("[Mission 진행] InstanceId=%s Progress=%d/%d Event=%s Item=%s"),
				*Mission.MissionInstanceId.ToString(),
				Mission.ProgressCount,
				Mission.Objective.RequiredCount,
				*EventTag.ToString(),
				*ItemId.ToString());
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

	for (const TPair<FGuid, EMyMissionState>& Finished : FinishedMissions)
	{
		FinishActiveMission(Finished.Key, Finished.Value);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 상태 켜짐·꺼짐 사실 채널을 서버에서 구독한다.
void UMyStreamingManagerComponent::RegisterStateListener()
{
	UnregisterStateListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	StateListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingStatePayload>(
			MyGameplayTags::Streaming_Channel_State,
			this,
			&UMyStreamingManagerComponent::HandleStatePayload);
}

////////////////////////////
//! \author 장효제
//! \brief 상태 채널 구독을 멱등 해제한다.
void UMyStreamingManagerComponent::UnregisterStateListener()
{
	if (StateListenerHandle.IsValid())
	{
		StateListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 상태가 켜지거나 꺼지면 출처 목록을 갱신하고 타이머를 다시 맞춘다.
//! \param Channel 상태 채널이다.
//! \param Payload 어떤 출처가 어떤 상태를 켰는지 껐는지다.
void UMyStreamingManagerComponent::HandleStatePayload(
	const FGameplayTag Channel,
	const FMyStreamingStatePayload& Payload)
{
	(void)Channel;
	const AActor* OwnerActor = GetOwner();
	AActor* const SourceActor = Payload.SourceActor.Get();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !Payload.StateTag.IsValid() || !SourceActor)
	{
		return;
	}

	const FObjectKey SourceKey(SourceActor);
	if (Payload.bEntered)
	{
		ActiveStateSources.FindOrAdd(Payload.StateTag).Add(SourceKey);
	}
	else if (TSet<FObjectKey>* Sources = ActiveStateSources.Find(Payload.StateTag))
	{
		Sources->Remove(SourceKey);
		if (Sources->IsEmpty())
		{
			ActiveStateSources.Remove(Payload.StateTag);
		}
	}

	const TSet<FObjectKey>* RemainingSources = ActiveStateSources.Find(Payload.StateTag);
	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[상태 %s] StateTag=%s 출처=%s 이 상태의 출처 수=%d"),
		Payload.bEntered ? TEXT("켜짐") : TEXT("꺼짐"),
		*Payload.StateTag.ToString(),
		*SourceActor->GetName(),
		RemainingSources ? RemainingSources->Num() : 0);

	RefreshStateHoldTimers();
}

////////////////////////////
//! \author 장효제
//! \brief 이 상태를 켜고 있는 출처가 하나라도 있는지 판정한다.
//! \param StateTag Rule이 요구하는 상태다.
//! \return 출처가 하나라도 있으면 true다.
bool UMyStreamingManagerComponent::IsAnyStateActive(const FGameplayTag StateTag) const
{
	TSet<FObjectKey> Sources;
	CollectStateSources(StateTag, Sources);
	return !Sources.IsEmpty();
}

////////////////////////////
//! \author 장효제
//! \brief 이 상태를 켜고 있는 출처들을 모은다.
//! \details 이미 사라진 객체는 건너뛴다. 꺼짐 사실 없이 파괴된 출처가
//!          상태를 영원히 켜진 것으로 남기지 않게 한다.
//! \param StateTag Rule이 요구하는 상태다.
//! \param OutSources 찾은 출처를 담는다.
void UMyStreamingManagerComponent::CollectStateSources(
	const FGameplayTag StateTag,
	TSet<FObjectKey>& OutSources) const
{
	for (const TPair<FGameplayTag, TSet<FObjectKey>>& Pair : ActiveStateSources)
	{
		if (!MyStreamingStateRulePolicy::DoesStateMatch(Pair.Key, StateTag))
		{
			continue;
		}
		for (const FObjectKey& SourceKey : Pair.Value)
		{
			if (SourceKey.ResolveObjectPtr())
			{
				OutSources.Add(SourceKey);
			}
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 같은 조건을 가진 Rule 묶음의 대표 RowName을 정한다.
//! \details 대표 하나만 타이머를 돈다. 묶음 전체가 한 번만 재고,
//!          발동 순간에 Weight로 하나를 고른다.
//! \param Rule 조건을 읽을 Rule이다.
//! \return 묶음에서 이름이 가장 앞선 RowName이다.
FName UMyStreamingManagerComponent::ResolveStateRuleGroupId(
	const FMyStreamingStateRuleRow& Rule) const
{
	FName GroupId = NAME_None;
	if (!StateRuleTable)
	{
		return GroupId;
	}

	for (const TPair<FName, uint8*>& Pair : StateRuleTable->GetRowMap())
	{
		const FMyStreamingStateRuleRow* Other =
			reinterpret_cast<const FMyStreamingStateRuleRow*>(Pair.Value);
		if (!Other || !MyStreamingStateRulePolicy::HaveSameCondition(Rule, *Other))
		{
			continue;
		}
		if (GroupId.IsNone() || Pair.Key.LexicalLess(GroupId))
		{
			GroupId = Pair.Key;
		}
	}
	return GroupId;
}

////////////////////////////
//! \author 장효제
//! \brief 같은 조건을 가진 Rule 중 하나를 Weight로 고른다.
//! \param Condition 조건을 읽을 Rule이다.
//! \return 고른 Rule이다. 후보가 없으면 nullptr다.
const FMyStreamingStateRuleRow* UMyStreamingManagerComponent::PickStateRuleFromGroup(
	const FMyStreamingStateRuleRow& Condition) const
{
	if (!StateRuleTable)
	{
		return nullptr;
	}

	TArray<const FMyStreamingStateRuleRow*> Candidates;
	TArray<int32> CandidateWeights;
	TArray<int32> EligibleIndexes;
	for (const TPair<FName, uint8*>& Pair : StateRuleTable->GetRowMap())
	{
		const FMyStreamingStateRuleRow* Other =
			reinterpret_cast<const FMyStreamingStateRuleRow*>(Pair.Value);
		if (!Other
			|| Other->SequenceId.IsNone()
			|| Other->Weight <= 0
			|| !MyStreamingStateRulePolicy::HaveSameCondition(Condition, *Other))
		{
			continue;
		}
		EligibleIndexes.Add(Candidates.Num());
		Candidates.Add(Other);
		CandidateWeights.Add(Other->Weight);
	}

	int32 TotalWeight = 0;
	for (const int32 Weight : CandidateWeights)
	{
		TotalWeight += Weight;
	}
	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	const int32 SelectedIndex =
		MyStreamingSequencePolicy::SelectWeightedCandidateIndex(
			CandidateWeights,
			EligibleIndexes,
			FMath::RandRange(1, TotalWeight));
	return Candidates.IsValidIndex(SelectedIndex) ? Candidates[SelectedIndex] : nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief 켜진 상태와 그 출처에 맞춰 타이머를 걸거나 취소한다.
//! \details 상태가 이어지는 동안만 시간을 잰다. 상태가 꺼지면 재던 시간을
//!          버리고 발동 기록도 지운다. 이미 재고 있는 짝은 건드리지 않는다.
//!          그래야 무관한 다른 상태가 바뀔 때 엉뚱하게 시간이 초기화되지 않는다.
void UMyStreamingManagerComponent::RefreshStateHoldTimers()
{
	UWorld* World = GetWorld();
	if (!World || !StateRuleTable)
	{
		return;
	}

	TSet<FMyStreamingStateHoldKey> DesiredKeys;
	for (const TPair<FName, uint8*>& Pair : StateRuleTable->GetRowMap())
	{
		const FMyStreamingStateRuleRow* Rule =
			reinterpret_cast<const FMyStreamingStateRuleRow*>(Pair.Value);
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| !MyStreamingStateRulePolicy::IsHoldSecondsValid(Rule->HoldSeconds))
		{
			continue;
		}

		// 대표 Rule 하나만 타이머를 돈다. 나머지는 발동 순간의 후보로만 쓴다.
		const FName GroupId = ResolveStateRuleGroupId(*Rule);
		if (GroupId != Pair.Key)
		{
			continue;
		}

		if (!MyStreamingStateRulePolicy::ShouldHold(
				true, Rule->AndStateTag.IsValid(), IsAnyStateActive(Rule->AndStateTag)))
		{
			continue;
		}

		TSet<FObjectKey> Sources;
		CollectStateSources(Rule->StateTag, Sources);
		for (const FObjectKey& SourceKey : Sources)
		{
			const FMyStreamingStateHoldKey HoldKey{GroupId, SourceKey};
			DesiredKeys.Add(HoldKey);
			if (FiredStateHolds.Contains(HoldKey))
			{
				continue;
			}

			FTimerHandle& Handle = StateHoldTimerHandles.FindOrAdd(HoldKey);
			if (World->GetTimerManager().IsTimerActive(Handle))
			{
				continue;
			}

			FTimerDelegate Delegate;
			Delegate.BindUObject(this, &ThisClass::HandleStateHoldElapsed, HoldKey);
			World->GetTimerManager().SetTimer(Handle, Delegate, Rule->HoldSeconds, false);
		}
	}

	// 더는 조건을 만족하지 않는 짝은 재던 시간과 발동 기록을 함께 버린다.
	for (auto It = StateHoldTimerHandles.CreateIterator(); It; ++It)
	{
		if (DesiredKeys.Contains(It.Key()))
		{
			continue;
		}
		World->GetTimerManager().ClearTimer(It.Value());
		FiredStateHolds.Remove(It.Key());
		It.RemoveCurrent();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 지정한 사실이 오면 그 사실을 리셋 조건으로 쓰는 Rule의 시간을 다시 잰다.
//! \details "전투 중인데 n초 동안 공격하지 않았을 때"가 이 경로를 쓴다.
//!          상태는 그대로 켜져 있고 재던 시간만 처음으로 돌아간다.
//!          한 번 발동한 짝도 여기서 기록이 지워져 다시 잴 수 있게 된다.
//! \param EventTag 방금 일어난 사실이다.
//! \param SourceTag 그 사실을 더 좁게 가리키는 태그다. 없으면 비운다.
void UMyStreamingManagerComponent::ResetStateHoldTimersForEvent(
	const FGameplayTag EventTag,
	const FGameplayTag SourceTag)
{
	UWorld* World = GetWorld();
	if (!World || !StateRuleTable || !EventTag.IsValid())
	{
		return;
	}

	TSet<FName> ResetGroupIds;
	for (const TPair<FName, uint8*>& Pair : StateRuleTable->GetRowMap())
	{
		const FMyStreamingStateRuleRow* Rule =
			reinterpret_cast<const FMyStreamingStateRuleRow*>(Pair.Value);
		if (Rule
			&& MyStreamingStateRulePolicy::ShouldResetOnEvent(
				EventTag, SourceTag, Rule->ResetOnEventTag, Rule->ResetExcludeTag))
		{
			ResetGroupIds.Add(ResolveStateRuleGroupId(*Rule));
		}
	}
	if (ResetGroupIds.IsEmpty())
	{
		return;
	}

	for (auto It = StateHoldTimerHandles.CreateIterator(); It; ++It)
	{
		if (!ResetGroupIds.Contains(It.Key().GroupId))
		{
			continue;
		}
		World->GetTimerManager().ClearTimer(It.Value());
		FiredStateHolds.Remove(It.Key());
		It.RemoveCurrent();
	}

	// 지운 자리에 새 시간을 건다. 상태가 그대로면 처음부터 다시 잰다.
	RefreshStateHoldTimers();
}

////////////////////////////
//! \author 장효제
//! \brief 상태가 정해진 시간만큼 이어지면 Sequence를 요청한다.
//! \details 한 번 발동하면 다시 걸지 않는다. 상태가 꺼졌다 다시 켜지거나
//!          리셋 사실이 와야 다시 잰다. "진입 후 n초"는 진입당 한 번이다.
//! \param HoldKey 시간이 끝난 조건 묶음과 출처의 짝이다.
void UMyStreamingManagerComponent::HandleStateHoldElapsed(
	const FMyStreamingStateHoldKey HoldKey)
{
	const AActor* OwnerActor = GetOwner();
	if (!StateRuleTable || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	const FMyStreamingStateRuleRow* Condition =
		StateRuleTable->FindRow<FMyStreamingStateRuleRow>(
			HoldKey.GroupId,
			TEXT("UMyStreamingManagerComponent::HandleStateHoldElapsed"),
			false);
	if (!Condition)
	{
		return;
	}

	// 타이머가 끝나는 사이에 상태가 꺼졌을 수 있다. 출처까지 다시 확인한다.
	TSet<FObjectKey> Sources;
	CollectStateSources(Condition->StateTag, Sources);
	const bool bStillHolding = Sources.Contains(HoldKey.SourceKey)
		&& MyStreamingStateRulePolicy::ShouldHold(
			true,
			Condition->AndStateTag.IsValid(),
			IsAnyStateActive(Condition->AndStateTag));
	if (!bStillHolding)
	{
		return;
	}

	// 다시 걸지 않는다. 상태가 꺼지거나 리셋 사실이 와야 이 기록이 지워진다.
	FiredStateHolds.Add(HoldKey);

	const FMyStreamingStateRuleRow* Rule = PickStateRuleFromGroup(*Condition);
	if (!Rule)
	{
		return;
	}

	if (Rule->CooldownSeconds > 0.0f)
	{
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.0f;
		if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
			LastTime && Now < *LastTime + Rule->CooldownSeconds)
		{
			return;
		}
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[상태 유지 발동] Group=%s StateTag=%s AndStateTag=%s HoldSeconds=%.1f SequenceId=%s"),
		*HoldKey.GroupId.ToString(),
		*Condition->StateTag.ToString(),
		*Condition->AndStateTag.ToString(),
		Condition->HoldSeconds,
		*Rule->SequenceId.ToString());

	RequestRuleSequence(
		Rule->SequenceId,
		Rule->BusyPolicy,
		EMyStreamingRuleSource::StateHold,
		Condition->StateTag);
}

////////////////////////////
//! \author 장효제
//! \brief 돌고 있는 상태 유지 타이머를 모두 취소한다.
void UMyStreamingManagerComponent::ClearAllStateHoldTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<FMyStreamingStateHoldKey, FTimerHandle>& Pair : StateHoldTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	StateHoldTimerHandles.Reset();
	FiredStateHolds.Reset();
}

void UMyStreamingManagerComponent::RegisterCountEventListener()
{
	UnregisterCountEventListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	CountEventListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingCountEventPayload>(
			MyGameplayTags::Streaming_Channel_CountEvent,
			this,
			&UMyStreamingManagerComponent::HandleCountEventPayload);
}

////////////////////////////
//! \author 장효제
//! \brief 파티 사건 누계 구독을 멱등 해제한다.
void UMyStreamingManagerComponent::UnregisterCountEventListener()
{
	if (CountEventListenerHandle.IsValid())
	{
		CountEventListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 아이템 사건 사실 채널을 서버에서 구독한다.
void UMyStreamingManagerComponent::RegisterItemEventListener()
{
	UnregisterItemEventListener();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	ItemEventListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingItemEventPayload>(
			MyGameplayTags::Streaming_Channel_Item,
			this,
			&UMyStreamingManagerComponent::HandleItemEventPayload);
}

////////////////////////////
//! \author 장효제
//! \brief 아이템 사건 구독을 멱등 해제한다.
void UMyStreamingManagerComponent::UnregisterItemEventListener()
{
	if (ItemEventListenerHandle.IsValid())
	{
		ItemEventListenerHandle.Unregister();
	}
}





////////////////////////////
//! \author 장효제
//! \brief Rule이 요구하는 사건에 해당하는 파티 누계를 센다.
//! \details 시간 필터는 시간 창 Rule에서만 쓴다. 누계 Rule은 던전 전체를 센다.
//! \param RuleEventTag 셀 사건이다.
//! \param RuleSourceTag 사건을 좁히는 태그다. 비우면 좁히지 않는다.
//! \param WindowSeconds 0이면 던전 누계, 0보다 크면 그 시간 안만 센다.
//! \param ExcludeLatest 방금 오른 양만큼 빼고 센다.
//! \return 센 횟수다.
int32 UMyStreamingManagerComponent::CountPartyEvents(
	const FGameplayTag RuleEventTag,
	const FGameplayTag RuleSourceTag,
	const float WindowSeconds,
	const int32 ExcludeLatest) const
{
	const bool bUseWindow = MyStreamingCountRulePolicy::IsWindowRule(WindowSeconds);
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	int32 Count = 0;
	for (const FMyStreamingCountRecord& Record : PartyCountRecords)
	{
		if (!MyStreamingCountRulePolicy::DoesTagMatch(Record.EventTag, RuleEventTag)
			|| !MyStreamingCountRulePolicy::DoesTagMatch(Record.SourceTag, RuleSourceTag))
		{
			continue;
		}
		if (bUseWindow
			&& !MyStreamingCountRulePolicy::IsWithinWindow(
				Record.ServerTimeSeconds, Now, WindowSeconds))
		{
			continue;
		}
		Count += Record.Amount;
	}
	return FMath::Max(Count - ExcludeLatest, 0);
}

////////////////////////////
//! \author 장효제
//! \brief Rule이 요구하는 아이템 사건에 해당하는 파티 누계를 센다.
//! \details 시간 필터는 시간 창 Rule에서만 쓴다. 누계 Rule은 던전 전체를 센다.
//! \param RuleEventTag 사용인지 구매인지다.
//! \param RuleItemId 셀 아이템이다. 비우면 모든 아이템을 센다.
//! \param WindowSeconds 0이면 던전 누계, 0보다 크면 그 시간 안만 센다.
//! \param ExcludeLatest 방금 오른 양만큼 빼고 센다.
//! \return 센 개수다.
int32 UMyStreamingManagerComponent::CountPartyItems(
	const FGameplayTag RuleEventTag,
	const FName RuleItemId,
	const float WindowSeconds,
	const int32 ExcludeLatest) const
{
	const bool bUseWindow = MyStreamingCountRulePolicy::IsWindowRule(WindowSeconds);
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	int32 Count = 0;
	for (const FMyStreamingCountRecord& Record : PartyCountRecords)
	{
		if (!MyStreamingCountRulePolicy::DoesTagMatch(Record.EventTag, RuleEventTag)
			|| !MyStreamingCountRulePolicy::DoesItemMatch(Record.ItemId, RuleItemId))
		{
			continue;
		}
		if (bUseWindow
			&& !MyStreamingCountRulePolicy::IsWithinWindow(
				Record.ServerTimeSeconds, Now, WindowSeconds))
		{
			continue;
		}
		Count += Record.Amount;
	}
	return FMath::Max(Count - ExcludeLatest, 0);
}

////////////////////////////
//! \author 장효제
//! \brief 파티 사건 누계를 갱신하고 문턱을 처음 넘은 Rule을 발동한다.
//! \param Channel 사건 누계 채널이다.
//! \param Payload 무슨 사건인지와 좁히는 태그다.
void UMyStreamingManagerComponent::HandleCountEventPayload(
	const FGameplayTag Channel,
	const FMyStreamingCountEventPayload& Payload)
{
	(void)Channel;
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !CountRuleTable || !Payload.EventTag.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	PartyCountRecords.Add(
		FMyStreamingCountRecord{Now, Payload.EventTag, Payload.SourceTag, NAME_None, 1});
	const int32 Total = CountPartyEvents(Payload.EventTag, FGameplayTag(), 0.0f, 0);

	AdvanceMissionsForCountEvent(Payload.EventTag, NAME_None, 1);

	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[사건 누계] EventTag=%s SourceTag=%s 누계=%d"),
		*Payload.EventTag.ToString(),
		*Payload.SourceTag.ToString(),
		Total);

	TArray<const FMyStreamingCountRuleRow*> Candidates;
	int32 TotalWeight = 0;
	const float NowFloat = static_cast<float>(Now);
	for (const TPair<FName, uint8*>& Pair : CountRuleTable->GetRowMap())
	{
		const FMyStreamingCountRuleRow* Rule =
			reinterpret_cast<const FMyStreamingCountRuleRow*>(Pair.Value);
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| !MyStreamingCountRulePolicy::DoesTagMatch(Payload.EventTag, Rule->EventTag)
			|| !MyStreamingCountRulePolicy::DoesTagMatch(Payload.SourceTag, Rule->SourceTag))
		{
			continue;
		}

		// 이번 사건을 뺀 값과 넣은 값을 비교해 문턱을 통과하는 순간만 잡는다.
		const int32 NewCount = CountPartyEvents(
			Rule->EventTag, Rule->SourceTag, Rule->WindowSeconds, 0);
		const int32 PreviousCount = CountPartyEvents(
			Rule->EventTag, Rule->SourceTag, Rule->WindowSeconds, 1);
		if (!MyStreamingCountRulePolicy::DidCrossThreshold(
				PreviousCount, NewCount, Rule->RequiredCount))
		{
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
				LastTime && NowFloat < *LastTime + Rule->CooldownSeconds)
			{
				continue;
			}
		}

		Candidates.Add(Rule);
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return;
	}

	const FMyStreamingCountRuleRow* Selected = Candidates.Last();
	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FMyStreamingCountRuleRow* Rule : Candidates)
	{
		Roll -= Rule->Weight;
		if (Roll <= 0)
		{
			Selected = Rule;
			break;
		}
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[사건 누계 Rule 발동] EventTag=%s RequiredCount=%d SequenceId=%s"),
		*Selected->EventTag.ToString(),
		Selected->RequiredCount,
		*Selected->SequenceId.ToString());

	RequestRuleSequence(
		Selected->SequenceId,
		Selected->BusyPolicy,
		EMyStreamingRuleSource::CountEvent,
		Payload.EventTag);
}

////////////////////////////
//! \author 장효제
//! \brief 아이템 사건 누계를 갱신하고 문턱을 처음 넘은 Rule을 발동한다.
//! \param Channel 아이템 사건 채널이다.
//! \param Payload 사용/구매·ItemId·UserIndex·개수다.
void UMyStreamingManagerComponent::HandleItemEventPayload(
	const FGameplayTag Channel,
	const FMyStreamingItemEventPayload& Payload)
{
	(void)Channel;
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| !ItemRuleTable || !Payload.EventTag.IsValid()
		|| Payload.ItemId.IsNone() || Payload.Count <= 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// 한 번에 여러 개가 오를 수 있다. 기록을 개수만큼 늘리지 않고 양을 담는다.
	PartyCountRecords.Add(FMyStreamingCountRecord{
		Now, Payload.EventTag, FGameplayTag(), Payload.ItemId, Payload.Count});
	const int32 Total = CountPartyItems(Payload.EventTag, Payload.ItemId, 0.0f, 0);

	AdvanceMissionsForCountEvent(Payload.EventTag, Payload.ItemId, Payload.Count);

	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[아이템 누계] EventTag=%s ItemId=%s UserIndex=%d 개수=%d 누계=%d"),
		*Payload.EventTag.ToString(),
		*Payload.ItemId.ToString(),
		Payload.UserIndex,
		Payload.Count,
		Total);

	TArray<const FMyStreamingItemRuleRow*> Candidates;
	int32 TotalWeight = 0;
	const float NowFloat = static_cast<float>(Now);
	for (const TPair<FName, uint8*>& Pair : ItemRuleTable->GetRowMap())
	{
		const FMyStreamingItemRuleRow* Rule =
			reinterpret_cast<const FMyStreamingItemRuleRow*>(Pair.Value);
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| !MyStreamingCountRulePolicy::DoesTagMatch(Payload.EventTag, Rule->EventTag)
			|| !MyStreamingCountRulePolicy::DoesItemMatch(Payload.ItemId, Rule->ItemId))
		{
			continue;
		}

		const int32 NewCount = CountPartyItems(
			Rule->EventTag, Rule->ItemId, Rule->WindowSeconds, 0);
		const int32 PreviousCount = CountPartyItems(
			Rule->EventTag, Rule->ItemId, Rule->WindowSeconds, Payload.Count);
		if (!MyStreamingCountRulePolicy::DidCrossThreshold(
				PreviousCount, NewCount, Rule->RequiredCount))
		{
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
				LastTime && NowFloat < *LastTime + Rule->CooldownSeconds)
			{
				continue;
			}
		}

		Candidates.Add(Rule);
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return;
	}

	const FMyStreamingItemRuleRow* Selected = Candidates.Last();
	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FMyStreamingItemRuleRow* Rule : Candidates)
	{
		Roll -= Rule->Weight;
		if (Roll <= 0)
		{
			Selected = Rule;
			break;
		}
	}

	UE_LOG(LogStreamingManager, Log,
		TEXT("[아이템 누계 Rule 발동] EventTag=%s ItemId=%s RequiredCount=%d SequenceId=%s"),
		*Selected->EventTag.ToString(),
		*Selected->ItemId.ToString(),
		Selected->RequiredCount,
		*Selected->SequenceId.ToString());

	// 아이템은 UserIndex를 알므로 쓴 사람 개인에게 지급한다.
	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::ItemEvent);
	bool bNeedsRecipients = false;
	if (!IsRuleSequenceContractValid(Selected->SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[아이템 Rule 거부] Sequence 계약 오류 SequenceId=%s"),
			*Selected->SequenceId.ToString());
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = Selected->SequenceId;
	Request.BusyPolicy = Selected->BusyPolicy;
	Request.SourceEventTag = Payload.EventTag;
	if (!TryFillRuleRecipients(Request, Contract, bNeedsRecipients, Payload.UserIndex))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[아이템 Rule 거부] 원인=보상 수령자 없음 SequenceId=%s UserIndex=%d"),
			*Selected->SequenceId.ToString(),
			Payload.UserIndex);
		return;
	}

	RequestSequence(Request);
}

void UMyStreamingManagerComponent::RegisterMesoMessageListener()
{
	UnregisterMesoMessageListener();

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(
			LogStreamingManager,
			Error,
			TEXT("[시스템 오류] Meso listener 등록 실패"));
		return;
	}

	MesoMessageListenerHandle =
		UGameplayMessageSubsystem::Get(this)
		.RegisterListener<FMyStreamingMesoPayload>(
			MyGameplayTags::Streaming_Channel_Meso,
			this,
			&UMyStreamingManagerComponent::HandleMesoPayload);
}

void UMyStreamingManagerComponent::UnregisterMesoMessageListener()
{
	if (MesoMessageListenerHandle.IsValid())
	{
		MesoMessageListenerHandle.Unregister();
	}
}

void UMyStreamingManagerComponent::HandleMesoPayload(
	const FGameplayTag Channel,
	const FMyStreamingMesoPayload& Payload)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !Channel.MatchesTagExact(
			MyGameplayTags::Streaming_Channel_Meso)
		|| !Payload.EventTag.IsValid()
		|| !Payload.SourceTag.IsValid()
		|| Payload.AppliedDelta == 0)
	{
		return;
	}

	const bool bValidEarned =
		Payload.EventTag.MatchesTagExact(
			MyGameplayTags::Streaming_Event_Meso_Earned)
		&& Payload.AppliedDelta > 0;

	const bool bValidSpent =
		Payload.EventTag.MatchesTagExact(
			MyGameplayTags::Streaming_Event_Meso_Spent)
		&& Payload.AppliedDelta < 0;

	if (!bValidEarned && !bValidSpent)
	{
		UE_LOG(
			LogStreamingManager,
			Warning,
			TEXT("[Meso Fact 거부] Event=%s Delta=%d"),
			*Payload.EventTag.ToString(),
			Payload.AppliedDelta);
		return;
	}

	UE_LOG(
		LogStreamingManager,
		Log,
		TEXT("[Meso Fact 수신] Event=%s Source=%s UserIndex=%d Delta=%d Current=%d"),
		*Payload.EventTag.ToString(),
		*Payload.SourceTag.ToString(),
		Payload.UserIndex,
		Payload.AppliedDelta,
		Payload.CurrentMeso);

	TMap<int32, int64>& Totals = bValidEarned
		? EarnedMesoTotalsByUserIndex
		: SpentMesoTotalsByUserIndex;
	int64& Total = Totals.FindOrAdd(Payload.UserIndex);
	const int64 PreviousTotal = Total;
	Total += bValidEarned
		? static_cast<int64>(Payload.AppliedDelta)
		: -static_cast<int64>(Payload.AppliedDelta);

	const FMyStreamingMesoRuleRow* Rule = SelectMesoRule(Payload, PreviousTotal, Total);
	if (!Rule)
	{
		return;
	}

	const FMyStreamingRuleSourceContract Contract =
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::Meso);
	bool bNeedsRecipients = false;
	if (!IsRuleSequenceContractValid(Rule->SequenceId, Contract, bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Meso Rule 거부] Sequence 계약 오류 Event=%s SequenceId=%s"),
			*Payload.EventTag.ToString(),
			*Rule->SequenceId.ToString());
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = Rule->SequenceId;
	Request.BusyPolicy = Rule->BusyPolicy;
	Request.SourceEventTag = Payload.EventTag;
	// 기존 동작 보존: Meso Rule은 Donation 유무와 무관하게 사실을 일으킨 개인을 수령자로 둔다.
	Request.RecipientUserIndex = Payload.UserIndex;
	if (!TryFillRuleRecipients(Request, Contract, bNeedsRecipients, Payload.UserIndex))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Meso Rule 거부] 원인=Donation 수령자 없음 UserIndex=%d SequenceId=%s"),
			Payload.UserIndex,
			*Rule->SequenceId.ToString());
		return;
	}

	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	UE_LOG(
		LogStreamingManager,
		Log,
		TEXT("[Meso Rule] Event=%s Source=%s UserIndex=%d PreviousTotal=%lld NewTotal=%lld RequiredMeso=%d SequenceId=%s Status=%d"),
		*Payload.EventTag.ToString(),
		*Payload.SourceTag.ToString(),
		Payload.UserIndex,
		PreviousTotal,
		Total,
		Rule->RequiredMeso,
		*Rule->SequenceId.ToString(),
		static_cast<int32>(Result.Status));
}

////////////////////////////
//! \author 장효제
//! \brief 이번 Meso 변화로 문턱을 처음 넘은 Rule 중 하나를 Weight로 선택한다.
//! \param Payload 서버가 확정한 Meso 변화 사실이다.
//! \param PreviousTotal 변화 전 플레이어별 Dungeon 누계다.
//! \param NewTotal 변화 후 플레이어별 Dungeon 누계다.
//! \return 선택된 Rule이며 일치 후보가 없으면 nullptr이다.
const FMyStreamingMesoRuleRow* UMyStreamingManagerComponent::SelectMesoRule(
	const FMyStreamingMesoPayload& Payload,
	const int64 PreviousTotal,
	const int64 NewTotal) const
{
	if (!MesoRuleTable)
	{
		return nullptr;
	}

	TArray<FMyStreamingMesoRuleRow*> AllRules;
	MesoRuleTable->GetAllRows(TEXT("UMyStreamingManagerComponent::SelectMesoRule"), AllRules);
	TArray<const FMyStreamingMesoRuleRow*> Candidates;
	int32 TotalWeight = 0;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	for (const FMyStreamingMesoRuleRow* Rule : AllRules)
	{
		if (!Rule
			|| Rule->SequenceId.IsNone()
			|| Rule->Weight <= 0
			|| !DoesTagMatch(Payload.EventTag, Rule->EventTag)
			|| !DoesTagMatch(Payload.SourceTag, Rule->SourceTag)
			|| !MyStreamingMesoRulePolicy::DidCrossThreshold(
				PreviousTotal,
				NewTotal,
				Rule->RequiredMeso))
		{
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId);
				LastTime && Now < *LastTime + Rule->CooldownSeconds)
			{
				continue;
			}
		}

		Candidates.Add(Rule);
		TotalWeight += Rule->Weight;
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 Roll = FMath::RandRange(1, TotalWeight);
	for (const FMyStreamingMesoRuleRow* Rule : Candidates)
	{
		Roll -= Rule->Weight;
		if (Roll <= 0)
		{
			return Rule;
		}
	}

	return Candidates.Last();
}

////////////////////////////
//! \author 장효제
//! \brief Zone Clear 사실을 정확히 한 번 소비해 전용 룰의 Donation을 현재 인증 파티원 전체에 요청한다.
//! \param Channel 수신한 GameplayMessage 채널이다.
//! \param Payload ZoneManager가 확정한 0-based OrderedZones 인덱스다.
//! \return
void UMyStreamingManagerComponent::HandleZoneClearedPayload(
	FGameplayTag Channel,
	const FMyStreamingZoneClearedPayload& Payload)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!MyStreamingSequencePolicy::TryMarkZoneDonationProcessed(
			ProcessedZoneDonationIndexes,
			Payload.ZoneIndex))
	{
		UE_LOG(LogStreamingManager, Log,
			TEXT("[Zone Donation 생략] 원인=잘못됐거나 이미 처리한 ZoneIndex Channel=%s ZoneIndex=%d"),
			*Channel.ToString(),
			Payload.ZoneIndex);
		return;
	}

	FName SequenceId;
	bool bNeedsRecipients = false;
	if (!TryFindZoneDonationSequenceId(Payload.ZoneIndex, SequenceId)
		|| !IsRuleSequenceContractValid(
			SequenceId,
			MyStreamingSequencePolicy::GetRuleSourceContract(
				EMyStreamingRuleSource::ZoneDonation),
			bNeedsRecipients))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Zone Donation 처리 실패] 원인=Rule 또는 Donation Sequence 계약 오류 ZoneIndex=%d SequenceId=%s"),
			Payload.ZoneIndex,
			*SequenceId.ToString());
		return;
	}

	const TArray<int32> RecipientUserIndexes =
		CollectZoneDonationRecipientUserIndexes();

#if !UE_BUILD_SHIPPING
	// D-6 개발 검증: Standalone에는 인증 UserIndex가 없으므로 로컬 PlayerState를 명시적
	// StandaloneTestRecipient로만 사용한다. 네트워크 모드의 인증 계약에는 적용되지 않는다.
	AMyPlayerState* StandaloneTestRecipient = nullptr;
	if (RecipientUserIndexes.IsEmpty()
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode()))
	{
		if (const AGameStateBase* GameState = Cast<AGameStateBase>(OwnerActor))
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
				if (MyPlayerState && MyPlayerState->GetOwningController())
				{
					StandaloneTestRecipient = MyPlayerState;
					break;
				}
			}
		}
	}
	const bool bHasStandaloneTestRecipient = StandaloneTestRecipient != nullptr;
#else
	const bool bHasStandaloneTestRecipient = false;
#endif

	if (RecipientUserIndexes.IsEmpty() && !bHasStandaloneTestRecipient)
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[Zone Donation 처리 실패] 원인=연결·인증된 파티원 없음 ZoneIndex=%d SequenceId=%s"),
			Payload.ZoneIndex,
			*SequenceId.ToString());
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = SequenceId;
	Request.BusyPolicy = EMyStreamingSequenceBusyPolicy::Queue;
	Request.SourceEventTag = MyGameplayTags::Streaming_Event_Zone_Cleared.GetTag();
	Request.RecipientUserIndexes = RecipientUserIndexes;
#if !UE_BUILD_SHIPPING
	Request.StandaloneTestRecipient = StandaloneTestRecipient;
#endif

	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	UE_LOG(LogStreamingManager, Log,
		TEXT("[Zone Donation 요청] ZoneIndex=%d SequenceId=%s ExecutionId=%s RecipientCount=%d RequestStatus=%d"),
		Payload.ZoneIndex,
		*SequenceId.ToString(),
		*Request.SequenceExecutionId.ToString(),
		RecipientUserIndexes.Num() + (bHasStandaloneTestRecipient ? 1 : 0),
		static_cast<int32>(Result.Status));
}

////////////////////////////
//! \author 장효제
//! \brief Zone Donation RuleTable에서 ZoneIndex와 정확히 일치하는 유일한 SequenceId를 찾는다.
//! \param ZoneIndex 찾을 0-based OrderedZones 인덱스다.
//! \param OutSequenceId 유일하게 일치한 SequenceId다.
//! \return 유효한 행이 정확히 하나면 true다.
bool UMyStreamingManagerComponent::TryFindZoneDonationSequenceId(
	const int32 ZoneIndex,
	FName& OutSequenceId) const
{
	OutSequenceId = NAME_None;
	if (!ZoneDonationRuleTable || ZoneIndex < 0)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잘못된 데이터] ZoneDonationRuleTable=%s ZoneIndex=%d"),
			*GetNameSafe(ZoneDonationRuleTable),
			ZoneIndex);
		return false;
	}

	TArray<FMyStreamingZoneDonationRuleRow*> AllRules;
	ZoneDonationRuleTable->GetAllRows(
		TEXT("UMyStreamingManagerComponent::TryFindZoneDonationSequenceId"),
		AllRules);

	int32 MatchCount = 0;
	for (const FMyStreamingZoneDonationRuleRow* Rule : AllRules)
	{
		if (Rule && Rule->ZoneIndex == ZoneIndex)
		{
			++MatchCount;
			OutSequenceId = Rule->SequenceId;
		}
	}

	if (MatchCount != 1 || OutSequenceId.IsNone())
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잘못된 데이터] Zone Donation Rule 일치 개수 오류 Table=%s ZoneIndex=%d MatchCount=%d SequenceId=%s"),
			*ZoneDonationRuleTable->GetPathName(),
			ZoneIndex,
			MatchCount,
			*OutSequenceId.ToString());
		return false;
	}

	return true;
}

////////////////////////////
//! \author 장효제
//! \brief Rule이 참조한 Sequence의 모든 후보가 그 소스 계약에서 허용된 조합인지 검사한다.
//! \param SequenceId 검사할 SequenceId다.
//! \param Contract Rule 소스의 반응 허용 집합과 수령자 전략이다.
//! \param bOutNeedsRecipients 후보 중 Donation Step이 하나라도 있으면 true다. 수령자 확정에 쓴다.
//! \return 후보가 하나 이상이고 모든 후보가 계약을 만족하면 true다.
//! \note bRequireSingleStep이면 후보가 여럿이어도 StepOrder는 하나여야 한다(Zone 클리어 보상 계약).
bool UMyStreamingManagerComponent::IsRuleSequenceContractValid(
	const FName SequenceId,
	const FMyStreamingRuleSourceContract& Contract,
	bool& bOutNeedsRecipients) const
{
	bOutNeedsRecipients = false;
	if (!ChatLineTable || SequenceId.IsNone())
	{
		return false;
	}

	TSet<int32> StepOrders;
	int32 CandidateCount = 0;
	for (const FName RowName : ChatLineTable->GetRowNames())
	{
		const FMyStreamingChatLineRow* Line =
			ChatLineTable->FindRow<FMyStreamingChatLineRow>(
				RowName,
				TEXT("UMyStreamingManagerComponent::IsRuleSequenceContractValid"),
				false);
		if (!Line || Line->SequenceId != SequenceId)
		{
			continue;
		}

		++CandidateCount;
		StepOrders.Add(Line->StepOrder);
		EMyStreamingReaction Reaction = EMyStreamingReaction::Chat;
		if (!MyStreamingSequencePolicy::TryResolveStepReaction(
				Line->PresentationType,
				Line->ActionType,
				Line->MissionTag.IsValid(),
				Line->RewardSource,
				Line->RewardMin,
				Line->RewardMax,
				Line->RewardItemId,
				Reaction)
			|| !MyStreamingSequencePolicy::IsRuleSequenceStepContractValid(
				Line->PresentationType,
				Line->ActionType,
				Line->MissionTag.IsValid(),
				Line->RewardSource,
				Line->RewardMin,
				Line->RewardMax,
				Line->RewardItemId,
				Contract))
		{
			return false;
		}

		bOutNeedsRecipients |= Reaction == EMyStreamingReaction::Reward;
	}

	if (Contract.bRequireSingleStep && StepOrders.Num() != 1)
	{
		return false;
	}

	return CandidateCount > 0;
}

////////////////////////////
//! \author 장효제
//! \brief Gimmick Rule·SmallTalk이 참조한 Sequence가 상태 변경 없는 Chat뿐인지 검사한다.
//! \param SequenceId 검사할 SequenceId다.
//! \return 후보가 하나 이상이고 모든 후보가 Chat/None 계약이면 true다.
bool UMyStreamingManagerComponent::IsChatOnlySequenceContractValid(const FName SequenceId) const
{
	bool bUnusedNeedsRecipients = false;
	return IsRuleSequenceContractValid(
		SequenceId,
		MyStreamingSequencePolicy::GetRuleSourceContract(EMyStreamingRuleSource::SmallTalk),
		bUnusedNeedsRecipients);
}

////////////////////////////
//! \author 장효제
//! \brief 예약 접두사로 SmallTalk Sequence를 식별한다.
//! \param SequenceId 검사할 SequenceId다.
//! \return Seq_SmallTalk_로 시작하면 true다.
bool UMyStreamingManagerComponent::IsSmallTalkSequenceId(const FName SequenceId) const
{
	return SequenceId.ToString().StartsWith(TEXT("Seq_SmallTalk_"), ESearchCase::CaseSensitive);
}

////////////////////////////
//! \author 장효제
//! \brief ChatLineTable에서 Dungeon 중 아직 첫 Line을 출력하지 않은 유효 SmallTalk Sequence를 모은다.
//! \return 중복 없는 SmallTalk SequenceId 목록이다.
TArray<FName> UMyStreamingManagerComponent::CollectUnusedSmallTalkSequenceIds() const
{
	TSet<FName> UniqueIds;
	if (!ChatLineTable)
	{
		return {};
	}

	for (const FName RowName : ChatLineTable->GetRowNames())
	{
		const FMyStreamingChatLineRow* Line =
			ChatLineTable->FindRow<FMyStreamingChatLineRow>(
				RowName,
				TEXT("UMyStreamingManagerComponent::CollectUnusedSmallTalkSequenceIds"),
				false);
		if (Line && IsSmallTalkSequenceId(Line->SequenceId)
			&& !UsedSmallTalkSequenceIds.Contains(Line->SequenceId))
		{
			UniqueIds.Add(Line->SequenceId);
		}
	}

	TArray<FName> Result = UniqueIds.Array();
	Result.RemoveAll([this](const FName SequenceId)
	{
		return !IsChatOnlySequenceContractValid(SequenceId);
	});
	Result.Sort(FNameLexicalLess());
	return Result;
}

////////////////////////////
//! \author 장효제
//! \brief 독립 Weight 추첨과 구간 내 균등 추첨으로 다음 SmallTalk 시각을 예약한다.
void UMyStreamingManagerComponent::ScheduleNextSmallTalk()
{
	UWorld* World = GetWorld();
	if (!bSmallTalkSchedulerEnabled || bSmallTalkExhausted || bIsPartyAFK || !World
		|| IsStoryDialogueBlockingSmallTalk())
	{
		return;
	}

	if (!ChatLineTable)
	{
		bSmallTalkSchedulerEnabled = false;
		UE_LOG(LogStreamingManager, Error, TEXT("[SmallTalk 비활성] ChatLineTable=null"));
		return;
	}

	if (CollectUnusedSmallTalkSequenceIds().IsEmpty())
	{
		bSmallTalkExhausted = true;
		bSmallTalkSchedulerEnabled = false;
		RecordSmallTalkTimeline(EMySmallTalkTimelineEventType::Exhausted);
		return;
	}

	// 방금 잡담이 나갔으면 확률적으로 아주 짧은 간격을 써서 우르르 몰리게 한다.
	// 실제 방송 채팅은 고르게 흐르지 않고 조용하다가 한꺼번에 쏟아진다.
	const bool bCluster = MyStreamingTuningPolicy::ShouldClusterSmallTalk(
		bSmallTalkJustPlayed,
		ClusterSmallTalkInterval.Weight,
		FMath::RandRange(1, 100));
	bSmallTalkJustPlayed = false;

	EMySmallTalkIntervalRangeName SelectedRange = EMySmallTalkIntervalRangeName::Short;
	const FMySmallTalkIntervalRange* Range = nullptr;
	if (bCluster)
	{
		// 뭉침은 구간 추첨을 거치지 않는다. 기록에는 가장 짧은 구간으로 남긴다.
		Range = &ClusterSmallTalkInterval;
	}
	else
	{
		const int32 TotalWeight = ShortSmallTalkInterval.Weight
			+ NormalSmallTalkInterval.Weight
			+ LongSmallTalkInterval.Weight;
		SelectedRange = MyStreamingSequencePolicy::SelectSmallTalkIntervalRange(
			ShortSmallTalkInterval.Weight,
			NormalSmallTalkInterval.Weight,
			LongSmallTalkInterval.Weight,
			FMath::RandRange(1, TotalWeight));

		switch (SelectedRange)
		{
		case EMySmallTalkIntervalRangeName::Short:
			Range = &ShortSmallTalkInterval;
			break;
		case EMySmallTalkIntervalRangeName::Normal:
			Range = &NormalSmallTalkInterval;
			break;
		case EMySmallTalkIntervalRangeName::Long:
			Range = &LongSmallTalkInterval;
			break;
		default:
			bSmallTalkSchedulerEnabled = false;
			UE_LOG(LogStreamingManager, Error, TEXT("[SmallTalk 비활성] Interval Range 추첨 실패"));
			return;
		}
	}

	const float Interval = FMath::FRandRange(Range->MinSeconds, Range->MaxSeconds);
	const float ScheduledTime = World->GetTimeSeconds() + Interval;
	if (Interval <= 0.0f)
	{
		SmallTalkTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&UMyStreamingManagerComponent::HandleSmallTalkTimerElapsed);
	}
	else
	{
		World->GetTimerManager().SetTimer(
			SmallTalkTimerHandle,
			this,
			&UMyStreamingManagerComponent::HandleSmallTalkTimerElapsed,
			Interval,
			false);
	}
	RecordSmallTalkTimeline(
		EMySmallTalkTimelineEventType::Scheduled,
		NAME_None,
		NAME_None,
		SelectedRange,
		Interval,
		ScheduledTime);
}

////////////////////////////
//! \author 장효제
//! \brief Story Dialogue가 시작되면 예약 SmallTalk 또는 활성 SmallTalk의 미래 Step만 취소한다.
void UMyStreamingManagerComponent::CancelSmallTalkForDialogue()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(SmallTalkTimerHandle))
	{
		World->GetTimerManager().ClearTimer(SmallTalkTimerHandle);
		SmallTalkTimerHandle.Invalidate();
		RecordSmallTalkTimeline(EMySmallTalkTimelineEventType::CancelledByDialogue);
	}

	if (!bIsSequencePlaying || !bActiveSequenceIsSmallTalk)
	{
		return;
	}

	const FName CancelledSequenceId = ActiveSequence.SequenceId;
	UpdateExecutionState(
		ActiveSequence.SequenceExecutionId,
		EMyStreamingSequenceExecutionState::FailedInterrupted);
	ResetSequencePlayback();
	RecordSmallTalkTimeline(
		EMySmallTalkTimelineEventType::CancelledByDialogue,
		CancelledSequenceId);
	StartNextPendingSequence();
}

////////////////////////////
//! \author 장효제
//! \brief 파괴된 오벨리스크가 남긴 Dialogue 세션 키를 방어적으로 제거한다.
void UMyStreamingManagerComponent::PruneInvalidStoryDialogueSessions()
{
	for (auto It = ActiveStoryDialogueSessions.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 현재 파티 Story Dialogue 세션이 하나라도 등록돼 있는지 반환한다.
bool UMyStreamingManagerComponent::IsStoryDialogueBlockingSmallTalk() const
{
	return !ActiveStoryDialogueSessions.IsEmpty();
}

////////////////////////////
//! \author 장효제
//! \brief 예약 시각에 Busy를 먼저 검사하고 Idle일 때만 미사용 SmallTalk 하나를 선택해 요청한다.
void UMyStreamingManagerComponent::HandleSmallTalkTimerElapsed()
{
	SmallTalkTimerHandle.Invalidate();
	if (!bSmallTalkSchedulerEnabled || bIsPartyAFK)
	{
		return;
	}

	RecordSmallTalkTimeline(EMySmallTalkTimelineEventType::Attempted);
	if (bIsSequencePlaying)
	{
		RecordSmallTalkTimeline(
			EMySmallTalkTimelineEventType::Dropped,
			NAME_None,
			ActiveSequence.SequenceId);
		ScheduleNextSmallTalk();
		return;
	}

	const TArray<FName> Candidates = CollectUnusedSmallTalkSequenceIds();
	if (Candidates.IsEmpty())
	{
		bSmallTalkExhausted = true;
		bSmallTalkSchedulerEnabled = false;
		RecordSmallTalkTimeline(EMySmallTalkTimelineEventType::Exhausted);
		return;
	}

	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	Request.BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
	Request.SourceEventTag = MyGameplayTags::Streaming_Event_SmallTalk.GetTag();
	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	if (!Result.IsAccepted())
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[SmallTalk 요청 거부] SequenceId=%s RequestStatus=%d"),
			*Request.SequenceId.ToString(),
			static_cast<int32>(Result.Status));
		ScheduleNextSmallTalk();
	}
}

////////////////////////////
//! \author 장효제
//! \brief Non-Shipping에서 SmallTalk 수명 Event를 Dungeon Scope 배열과 한 줄 서버 로그에 남긴다.
void UMyStreamingManagerComponent::RecordSmallTalkTimeline(
	const EMySmallTalkTimelineEventType EventType,
	const FName SequenceId,
	const FName BlockingSequenceId,
	const EMySmallTalkIntervalRangeName SelectedRange,
	const float Interval,
	const float ScheduledTime)
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	FMySmallTalkTimelineEvent& Event = SmallTalkTimelineEvents.AddDefaulted_GetRef();
	Event.EventType = EventType;
	Event.ServerTime = World ? World->GetTimeSeconds() : 0.0f;
	Event.SequenceId = SequenceId;
	Event.BlockingSequenceId = BlockingSequenceId;
	Event.SelectedRange = SelectedRange;
	Event.Interval = Interval;
	Event.ScheduledTime = ScheduledTime;

	UE_LOG(LogStreamingManager, Log,
		TEXT("[SmallTalk Timeline] Event=%s ServerTime=%.3f SequenceId=%s BlockingSequenceId=%s SelectedRange=%s Interval=%.3f ScheduledTime=%.3f"),
		*UEnum::GetValueAsString(EventType),
		Event.ServerTime,
		*SequenceId.ToString(),
		*BlockingSequenceId.ToString(),
		*UEnum::GetValueAsString(SelectedRange),
		Interval,
		ScheduledTime);
#endif
}

////////////////////////////
//! \author 장효제
//! \brief Dungeon GameState에 남아 있는 연결·인증 파티원의 UserIndex를 중복 없이 수집한다.
//! \param
//! \return 오름차순으로 정렬된 유효 UserIndex 목록이다.
////////////////////////////
//! \author 장효제
//! \brief 소스 계약이 요구하는 수령자만 요청에 채운다.
//! \param Request 수령자를 채울 Sequence 요청이다.
//! \param Contract Rule 소스의 수령자 해석 전략이다.
//! \param bNeedsRecipients 이 Sequence에 Donation Step이 있는지 여부다.
//! \param InstigatorUserIndex Instigator 전략에서 쓸 개인 UserIndex다.
//! \return 요청을 계속 진행해도 되면 true다. 수령자가 필요한데 하나도 없으면 false다.
//! \note Donation Step이 없으면 아무것도 채우지 않아 기존 Chat 전용 요청의 필드가 그대로 유지된다.
bool UMyStreamingManagerComponent::TryFillRuleRecipients(
	FMyStreamingSequenceRequest& Request,
	const FMyStreamingRuleSourceContract& Contract,
	const bool bNeedsRecipients,
	const int32 InstigatorUserIndex) const
{
	if (!bNeedsRecipients)
	{
		return true;
	}

	if (Contract.RecipientMode == EMyStreamingRecipientMode::Instigator)
	{
		if (!MyStreamingSequencePolicy::IsAuthenticatedUserIndex(InstigatorUserIndex))
		{
			return false;
		}

		Request.RecipientUserIndex = InstigatorUserIndex;
		return true;
	}

	if (Contract.RecipientMode == EMyStreamingRecipientMode::Party)
	{
		Request.RecipientUserIndexes = CollectZoneDonationRecipientUserIndexes();
		return !Request.RecipientUserIndexes.IsEmpty();
	}

	// RecipientMode::None인 소스는 계약 단계에서 Donation을 거부했어야 한다.
	return false;
}

TArray<int32> UMyStreamingManagerComponent::CollectZoneDonationRecipientUserIndexes() const
{
	TArray<int32> RecipientUserIndexes;
	const AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner());
	if (!GameState)
	{
		return RecipientUserIndexes;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (!MyPlayerState
			|| !MyPlayerState->IsAuthVerified()
			|| !MyStreamingSequencePolicy::IsAuthenticatedUserIndex(MyPlayerState->GetUserIndex())
			|| !MyPlayerState->GetOwningController())
		{
			continue;
		}

		RecipientUserIndexes.AddUnique(MyPlayerState->GetUserIndex());
	}

	RecipientUserIndexes.Sort();
	return RecipientUserIndexes;
}

//! \author 장효제
//! \brief 로컬 UI 채널에 채팅 메시지를 재발행한다.
void UMyStreamingManagerComponent::BroadcastChatMessage(const FMyStreamingChatMessageData& ChatMessage) const
{
	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error, TEXT("[시스템 오류] GameplayMessageSubsystem 인스턴스를 찾을 수 없음 작업=UI 전송 EventTag=%s"),
			*ChatMessage.SourceEventTag.ToString());
		return;
	}

	FMyStreamingChatMessageData LocalChatMessage = ChatMessage;
	ApplyGodPresentation(LocalChatMessage);

	UE_LOG(LogStreamingManager, Verbose, TEXT("[UI 전송] Channel=%s EventTag=%s Message=%s"),
		*MyGameplayTags::Streaming_Channel_UI_Chat.GetTag().ToString(),
		*LocalChatMessage.SourceEventTag.ToString(),
		*LocalChatMessage.Message.ToString());

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_UI_Chat,
		LocalChatMessage);
}

//! \author 장효제
//! \brief 로컬 클라이언트에서 GodTag에 대응하는 표시 이름과 아이콘을 채운다.
void UMyStreamingManagerComponent::ApplyGodPresentation(FMyStreamingChatMessageData& ChatMessage) const
{
	const AActor* OwnerActor = GetOwner();
	if ((OwnerActor && OwnerActor->GetNetMode() == NM_DedicatedServer)
		|| !GodPresentationTable
		|| !ChatMessage.GodTag.IsValid())
	{
		return;
	}

	TArray<FMyGodPresentationRow*> AllPresentations;
	GodPresentationTable->GetAllRows(TEXT("UMyStreamingManagerComponent::ApplyGodPresentation"), AllPresentations);
	for (const FMyGodPresentationRow* Presentation : AllPresentations)
	{
		if (!Presentation || !Presentation->GodTag.MatchesTagExact(ChatMessage.GodTag))
		{
			continue;
		}

		ChatMessage.GodName = Presentation->DisplayName;
		ChatMessage.GodNameColor = Presentation->GetGodLinearColor();
		ChatMessage.GodIcon = Presentation->Icon.LoadSynchronous();
		if (!ChatMessage.GodIcon)
		{
			UE_LOG(LogStreamingManager, Warning, TEXT("[처리 실패] GodTag=%s 원인=GodIcon 로드 실패 Icon=%s"),
				*ChatMessage.GodTag.ToString(),
				*Presentation->Icon.ToSoftObjectPath().ToString());
		}
		return;
	}

	UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] GodPresentation Row 없음 GodTag=%s"),
		*ChatMessage.GodTag.ToString());
}

//! \author 장효제
//! \brief 서버에서 번역한 채팅 메시지를 클라이언트 UI 채널로 넘기는 브릿지다.
void UMyStreamingManagerComponent::MulticastBroadcastChatMessage_Implementation(const FMyStreamingChatMessageData& ChatMessage)
{
	UE_LOG(LogStreamingManager, Verbose, TEXT("[네트워크 수신] Owner=%s EventTag=%s Message=%s"),
		*GetNameSafe(GetOwner()),
		*ChatMessage.SourceEventTag.ToString(),
		*ChatMessage.Message.ToString());

	const AActor* OwnerActor = GetOwner();
	if ((OwnerActor && OwnerActor->GetNetMode() == NM_DedicatedServer))
	{
		return;
	}

	BroadcastChatMessage(ChatMessage);
}

////////////////////////////
//! \author 장효제
//! \brief 같은 SequenceId의 행을 StepOrder별로 묶고 각 Step의 대사 하나를 가중치로 한 번 선택한다.
//! \param BuildInput Sequence 조립에 필요한 공통 입력이다.
//! \param OutSequence Line 선택이 끝난 확정 Sequence를 돌려준다.
//! \return 재생 가능한 Step을 하나 이상 조립했으면 true다.
bool UMyStreamingManagerComponent::BuildResolvedSequence(
	const FMyStreamingSequenceBuildInput& BuildInput,
	FMyResolvedStreamingSequence& OutSequence
) const
{
	OutSequence = FMyResolvedStreamingSequence{};

	if (!ChatLineTable
		|| !BuildInput.SequenceExecutionId.IsValid()
		|| BuildInput.SequenceId.IsNone())
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] ChatLineTable=%s SequenceId=%s ExecutionId=%s"),
			*GetNameSafe(ChatLineTable),
			*BuildInput.SequenceId.ToString(),
			*BuildInput.SequenceExecutionId.ToString());
		return false;
	}

	FMyResolvedStreamingSequence BuiltSequence;
	const bool bIsMissionCompletion =
		BuildInput.SourceEventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Mission_Completed);
	BuiltSequence.SequenceExecutionId = BuildInput.SequenceExecutionId;
	BuiltSequence.SequenceId = BuildInput.SequenceId;
	BuiltSequence.BusyPolicy = BuildInput.BusyPolicy;
	BuiltSequence.RecipientUserIndex = BuildInput.RecipientUserIndex;
	BuiltSequence.RecipientUserIndexes = BuildInput.RecipientUserIndexes;
#if !UE_BUILD_SHIPPING
	BuiltSequence.StandaloneTestRecipient = BuildInput.StandaloneTestRecipient;
#endif

	struct FLineCandidate
	{
		FName RowName;
		const FMyStreamingChatLineRow* Line = nullptr;
	};

	// SequenceId가 같은 유효한 행을 StepOrder별 후보 배열에 모은다.
	TMap<int32, TArray<FLineCandidate>> CandidatesByStep;
	TMap<int32, float> DelayByStep;

	const TArray<FName> AllLineRowNames = ChatLineTable->GetRowNames();
	for (const FName LineRowName : AllLineRowNames)
	{
		const FMyStreamingChatLineRow* Line = ChatLineTable->FindRow<FMyStreamingChatLineRow>(
			LineRowName,
			TEXT("UMyStreamingManagerComponent::BuildResolvedSequence"),
			false);
		if (!Line
			|| Line->SequenceId != BuildInput.SequenceId
			|| Line->StepOrder < 1
			|| !FMath::IsFinite(Line->DelayFromPreviousStepSeconds)
			|| Line->DelayFromPreviousStepSeconds < 0.0f
			|| Line->Weight <= 0
			|| Line->MessageText.IsEmpty())
		{
			continue;
		}

		if (const float* StepDelay = DelayByStep.Find(Line->StepOrder))
		{
			if (!FMath::IsNearlyEqual(*StepDelay, Line->DelayFromPreviousStepSeconds))
			{
				UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] SequenceId=%s StepOrder=%d 원인=Delay 충돌 ExpectedDelay=%.3f ActualDelay=%.3f"),
					*BuildInput.SequenceId.ToString(),
					Line->StepOrder,
					*StepDelay,
					Line->DelayFromPreviousStepSeconds);
				return false;
			}
		}
		else
		{
			DelayByStep.Add(Line->StepOrder, Line->DelayFromPreviousStepSeconds);
		}

		CandidatesByStep.FindOrAdd(Line->StepOrder).Add({LineRowName, Line});
	}

	if (CandidatesByStep.IsEmpty())
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[처리 실패] SequenceId=%s 원인=선택 가능한 Step 없음 RowCount=%d"),
			*BuildInput.SequenceId.ToString(),
			AllLineRowNames.Num());
		return false;
	}

	// TMap에는 순서가 없으므로 StepOrder 키를 꺼내 작은 순서로 정렬한다.
	TArray<int32> SortedStepOrders;
	CandidatesByStep.GetKeys(SortedStepOrders);
	SortedStepOrders.Sort();
	if (bIsMissionCompletion && SortedStepOrders.Num() < 2)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Sequence 거부] Mission Completed는 Chat 뒤 Meso Step이 필요함 SequenceId=%s"),
			*BuildInput.SequenceId.ToString());
		return false;
	}
	for (const int32 StepOrder : SortedStepOrders)
	{
		const TArray<FLineCandidate>& Candidates = CandidatesByStep.FindChecked(StepOrder);
		const bool bIsFinalCompletionStep = bIsMissionCompletion
			&& StepOrder == SortedStepOrders.Last();
		for (const FLineCandidate& Candidate : Candidates)
		{
			const FMyStreamingChatLineRow& Line = *Candidate.Line;
			if (Line.ActionType == EMyStreamingActionType::ApplyMesoDelta && !bIsMissionCompletion)
			{
				UE_LOG(LogStreamingManager, Error,
					TEXT("[Sequence 거부] ApplyMesoDelta는 Mission Completed 전용 SequenceId=%s LineRowName=%s"),
					*BuildInput.SequenceId.ToString(),
					*Candidate.RowName.ToString());
				return false;
			}
			if (bIsMissionCompletion)
			{
				const bool bValid = bIsFinalCompletionStep
					? Line.PresentationType == EMyStreamingPresentationType::Donation
						&& Line.ActionType == EMyStreamingActionType::ApplyMesoDelta
						&& Line.RewardSource == EMyStreamingRewardSource::Payload
					: Line.PresentationType == EMyStreamingPresentationType::Chat
						&& Line.ActionType == EMyStreamingActionType::None
						&& Line.RewardSource == EMyStreamingRewardSource::None;
				if (!bValid)
				{
					UE_LOG(LogStreamingManager, Error,
						TEXT("[Sequence 거부] Mission Completed Line 계약 오류 SequenceId=%s LineRowName=%s"),
						*BuildInput.SequenceId.ToString(),
						*Candidate.RowName.ToString());
					return false;
				}
			}
		}
	}

	for (const int32 StepOrder : SortedStepOrders)
	{
		const TArray<FLineCandidate>* Candidates = CandidatesByStep.Find(StepOrder);
		if (!Candidates || Candidates->IsEmpty())
		{
			UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] SequenceId=%s StepOrder=%d 원인=Step 후보 배열 비어 있음"),
				*BuildInput.SequenceId.ToString(),
				StepOrder);
			return false;
		}

		TArray<FName> CandidateRowNames;
		TArray<int32> CandidateWeights;
		CandidateRowNames.Reserve(Candidates->Num());
		CandidateWeights.Reserve(Candidates->Num());
		for (const FLineCandidate& Candidate : *Candidates)
		{
			CandidateRowNames.Add(Candidate.RowName);
			CandidateWeights.Add(Candidate.Line->Weight);
		}

		FName LastDisplayedLineRowName;
		if (const TMap<int32, FName>* LastByStep =
			LastDisplayedLineRowNamesBySequence.Find(BuildInput.SequenceId))
		{
			if (const FName* LastLine = LastByStep->Find(StepOrder))
			{
				LastDisplayedLineRowName = *LastLine;
			}
		}

		const TArray<int32> EligibleIndexes =
			MyStreamingSequencePolicy::BuildEligibleCandidateIndexes(
				CandidateRowNames,
				LastDisplayedLineRowName);

		int32 TotalWeight = 0;
		for (const int32 CandidateIndex : EligibleIndexes)
		{
			TotalWeight += (*Candidates)[CandidateIndex].Line->Weight;
		}

		if (TotalWeight <= 0)
		{
			UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] SequenceId=%s StepOrder=%d 원인=Weight 총합 오류 TotalWeight=%d"),
				*BuildInput.SequenceId.ToString(),
				StepOrder,
				TotalWeight);
			return false;
		}

		const int32 RandomWeight = FMath::RandRange(1, TotalWeight);
		const int32 SelectedCandidateIndex =
			MyStreamingSequencePolicy::SelectWeightedCandidateIndex(
				CandidateWeights,
				EligibleIndexes,
				RandomWeight);
		if (!Candidates->IsValidIndex(SelectedCandidateIndex))
		{
			UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] SequenceId=%s StepOrder=%d 원인=Weight 후보 선택 실패"),
				*BuildInput.SequenceId.ToString(),
				StepOrder);
			return false;
		}
		const FLineCandidate* SelectedCandidate = &(*Candidates)[SelectedCandidateIndex];
		const FMyStreamingChatLineRow* SelectedLine = SelectedCandidate->Line;
		const FGameplayTag ResolvedGodTag =
			SelectedLine->GodSource == EMyStreamingGodSource::Payload
				? BuildInput.PayloadGodTag
				: SelectedLine->GodTag;
		if (!ResolvedGodTag.IsValid())
		{
			UE_LOG(LogStreamingManager, Warning,
				TEXT("[처리 실패] 원인=God 확정 실패 SequenceId=%s StepOrder=%d GodSource=%s"),
				*BuildInput.SequenceId.ToString(),
				StepOrder,
				*UEnum::GetValueAsString(SelectedLine->GodSource));
			return false;
		}

		FMyResolvedStreamingStep SequenceStep;
		SequenceStep.StepOrder = StepOrder;
		SequenceStep.DelayFromPreviousStepSeconds = SelectedLine->DelayFromPreviousStepSeconds;
		SequenceStep.LineRowName = SelectedCandidate->RowName;
		SequenceStep.ChatMessage.GodName = GetGodDisplayName(ResolvedGodTag);
		SequenceStep.ChatMessage.GodTag = ResolvedGodTag;
		SequenceStep.ChatMessage.SourceEventTag = BuildInput.SourceEventTag;
		SequenceStep.ChatMessage.Message = SelectedLine->MessageText;
		// D-5C: 선택된 Line의 PresentationType을 그대로 담아 UI가 배경 Brush를 명시적으로 고르게 한다.
		SequenceStep.ChatMessage.PresentationType = SelectedLine->PresentationType;
		// DM-3A: 확장 필드 전체를 같은 선택된 Line에서 한 번에 복사한다. 이후 다시 뽑지 않는다.
		SequenceStep.GodSource = SelectedLine->GodSource;
		SequenceStep.PresentationType = SelectedLine->PresentationType;
		SequenceStep.ActionType = SelectedLine->ActionType;
		SequenceStep.MissionTag = SelectedLine->MissionTag;
		SequenceStep.RewardSource = SelectedLine->RewardSource;
		SequenceStep.RewardMin = SelectedLine->RewardMin;
		SequenceStep.RewardMax = SelectedLine->RewardMax;
		SequenceStep.RewardItemId = SelectedLine->RewardItemId;
		SequenceStep.PresentationTier = SelectedLine->PresentationTier;

		// D-2: RollFromLine이면 서버가 이 지점에서 금액을 정확히 한 번 확정해 Resolved Step에 저장한다.
		//      실행부·지급부·UI는 저장된 값을 읽기만 하고 다시 뽑지 않는다.
		{
			const int32 RawRoll =
				(SelectedLine->RewardSource == EMyStreamingRewardSource::RollFromLine
					&& SelectedLine->RewardMin <= SelectedLine->RewardMax)
				? FMath::RandRange(SelectedLine->RewardMin, SelectedLine->RewardMax)
				: 0;
			const int32 ResolvedInputValue =
				SelectedLine->RewardSource == EMyStreamingRewardSource::Payload
					? BuildInput.PayloadMesoAmount
					: RawRoll;
			int32 ResolvedAmount = 0;
			if (!MyStreamingSequencePolicy::TryResolveStepRewardAmount(
				SelectedLine->RewardSource,
				SelectedLine->RewardMin,
				SelectedLine->RewardMax,
				ResolvedInputValue,
				ResolvedAmount))
			{
				UE_LOG(LogStreamingManager, Warning,
					TEXT("[처리 실패] 원인=잘못된 Donation 금액 SequenceId=%s StepOrder=%d LineRowName=%s RewardSource=%s RewardMin=%d RewardMax=%d PayloadMesoAmount=%d"),
					*BuildInput.SequenceId.ToString(),
					StepOrder,
					*SelectedCandidate->RowName.ToString(),
					*UEnum::GetValueAsString(SelectedLine->RewardSource),
					SelectedLine->RewardMin,
					SelectedLine->RewardMax,
					BuildInput.PayloadMesoAmount);
				return false;
			}
			SequenceStep.ResolvedRewardAmount = ResolvedAmount;

			// 금액이 확정된 경우(RollFromLine)에만 한 번 로그를 남긴다. 두 번째 추첨이 없음을 확인할 수 있다.
			if (SelectedLine->RewardSource == EMyStreamingRewardSource::RollFromLine)
			{
				UE_LOG(LogStreamingManager, Log,
					TEXT("[Donation 금액 확정] SequenceId=%s ExecutionId=%s LineRowName=%s GodTag=%s RewardMin=%d RewardMax=%d ResolvedRewardAmount=%d"),
					*BuildInput.SequenceId.ToString(),
					*BuildInput.SequenceExecutionId.ToString(),
					*SelectedCandidate->RowName.ToString(),
					*ResolvedGodTag.ToString(),
					SelectedLine->RewardMin,
					SelectedLine->RewardMax,
					ResolvedAmount);
			}
		}

		// DM-3A/D-1: Chat이 아닌 Presentation(Donation 등)은 확정된 확장 필드를 로그로 남겨
		// 선택된 Line이 Resolved Step까지 정확히 전달됐는지 확인할 수 있게 한다.
		if (SelectedLine->PresentationType != EMyStreamingPresentationType::Chat)
		{
			UE_LOG(LogStreamingManager, Log,
				TEXT("[Resolved Step 확장] SequenceId=%s StepOrder=%d LineRowName=%s GodTag=%s PresentationType=%s ActionType=%s RewardSource=%s RewardMin=%d RewardMax=%d"),
				*BuildInput.SequenceId.ToString(),
				StepOrder,
				*SelectedCandidate->RowName.ToString(),
				*ResolvedGodTag.ToString(),
				*UEnum::GetValueAsString(SelectedLine->PresentationType),
				*UEnum::GetValueAsString(SelectedLine->ActionType),
				*UEnum::GetValueAsString(SelectedLine->RewardSource),
				SelectedLine->RewardMin,
				SelectedLine->RewardMax);
		}

		BuiltSequence.Steps.Add(MoveTemp(SequenceStep));

		UE_LOG(LogStreamingManager, Verbose, TEXT("[Step 선택] SequenceId=%s StepOrder=%d CandidateCount=%d EligibleCount=%d LineRowName=%s GodTag=%s Message=%s"),
			*BuildInput.SequenceId.ToString(),
			StepOrder,
			Candidates->Num(),
			EligibleIndexes.Num(),
			*SelectedCandidate->RowName.ToString(),
			*ResolvedGodTag.ToString(),
			*SelectedLine->MessageText.ToString());
	}

	if (BuiltSequence.Steps.IsEmpty())
	{
		return false;
	}

	const bool bContainsMesoDelta = BuiltSequence.Steps.ContainsByPredicate([](const FMyResolvedStreamingStep& Step)
	{
		return Step.ActionType == EMyStreamingActionType::ApplyMesoDelta;
	});
	if (bContainsMesoDelta && !bIsMissionCompletion)
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[Sequence 거부] ApplyMesoDelta는 Mission Completed 전용 SequenceId=%s SourceEventTag=%s"),
			*BuildInput.SequenceId.ToString(),
			*BuildInput.SourceEventTag.ToString());
		return false;
	}
	if (bIsMissionCompletion)
	{
		const int32 LastIndex = BuiltSequence.Steps.Num() - 1;
		bool bChatPrefixValid = LastIndex >= 1;
		for (int32 StepIndex = 0; bChatPrefixValid && StepIndex < LastIndex; ++StepIndex)
		{
			const FMyResolvedStreamingStep& Step = BuiltSequence.Steps[StepIndex];
			bChatPrefixValid = Step.PresentationType == EMyStreamingPresentationType::Chat
				&& Step.ActionType == EMyStreamingActionType::None
				&& Step.RewardSource == EMyStreamingRewardSource::None;
		}
		const FMyResolvedStreamingStep& LastStep = BuiltSequence.Steps.Last();
		const bool bMesoStepValid =
			LastStep.PresentationType == EMyStreamingPresentationType::Donation
			&& LastStep.ActionType == EMyStreamingActionType::ApplyMesoDelta
			&& LastStep.RewardSource == EMyStreamingRewardSource::Payload;
		if (!bChatPrefixValid || !bMesoStepValid)
		{
			UE_LOG(LogStreamingManager, Error,
				TEXT("[Sequence 거부] Mission Completed는 Chat/None 뒤 Donation/ApplyMesoDelta여야 함 SequenceId=%s"),
				*BuildInput.SequenceId.ToString());
			return false;
		}
	}

	UE_LOG(LogStreamingManager, Verbose, TEXT("[시퀀스 조립] SequenceId=%s ExecutionId=%s StepCount=%d"),
		*BuiltSequence.SequenceId.ToString(),
		*BuiltSequence.SequenceExecutionId.ToString(),
		BuiltSequence.Steps.Num());
	OutSequence = MoveTemp(BuiltSequence);
	return true;
}


////////////////////////////
//! \author 장효제
//! \brief Idle 상태에서 완성된 시퀀스를 현재 재생 대상으로 등록한다.
//! \param Sequence 우측값 참조 - 소유권 이전
//! \return Sequence를 현재 재생 대상으로 시작했으면 true다.
bool UMyStreamingManagerComponent::StartSequence(FMyResolvedStreamingSequence&& Sequence)
{
	if (bIsSequencePlaying)
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 상태] 원인=재생 중 StartSequence 호출 ActiveSequenceId=%s ActiveExecutionId=%s SequenceId=%s ExecutionId=%s"),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString(),
			*Sequence.SequenceId.ToString(),
			*Sequence.SequenceExecutionId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World
		|| !Sequence.SequenceExecutionId.IsValid()
		|| Sequence.SequenceId.IsNone()
		|| Sequence.Steps.IsEmpty())
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[처리 실패] 원인=시퀀스 시작 조건 오류 World=%s SequenceId=%s ExecutionId=%s StepCount=%d"),
			*GetNameSafe(World),
			*Sequence.SequenceId.ToString(),
			*Sequence.SequenceExecutionId.ToString(),
			Sequence.Steps.Num());
		return false;
	}

	ActiveSequence = MoveTemp(Sequence);
	NextStepIndex = 0;
	bIsSequencePlaying = true;
	bActiveSequenceIsSmallTalk = IsSmallTalkSequenceId(ActiveSequence.SequenceId);
	LastSequenceTriggerTimeMap.Add(ActiveSequence.SequenceId, World->GetTimeSeconds());
	UpdateExecutionState(
		ActiveSequence.SequenceExecutionId,
		EMyStreamingSequenceExecutionState::Running);

	UE_LOG(LogStreamingManager, Log, TEXT("[시작] SequenceId=%s ExecutionId=%s StepCount=%d"),
		*ActiveSequence.SequenceId.ToString(),
		*ActiveSequence.SequenceExecutionId.ToString(),
		ActiveSequence.Steps.Num());
	ScheduleNextStep();
	return true;
}


////////////////////////////
//! \author 장효제
//! \brief 완성된 Sequence를 기존 BusyPolicy에 따라 시작, 대기 또는 거부한다.
//! \param Sequence 조립과 Line 선택이 끝난 Sequence다.
//! \return 실행 경로 접수 또는 거부 이유를 담은 동기 결과다.
FMyStreamingSequenceRequestResult UMyStreamingManagerComponent::SubmitSequence(
	FMyResolvedStreamingSequence&& Sequence)
{
	const FGuid SequenceExecutionId = Sequence.SequenceExecutionId;
	const auto StoreResult = [this, SequenceExecutionId](
		const EMyStreamingSequenceRequestStatus Status,
		const EMyStreamingSequenceExecutionState ExecutionState)
	{
		const FMyStreamingSequenceRequestResult Result{
			SequenceExecutionId,
			Status,
			ExecutionState
		};
		if (FSequenceExecutionRecord* Record =
			SequenceExecutionRecords.Find(SequenceExecutionId))
		{
			Record->Result = Result;
		}
		else
		{
			UE_LOG(LogStreamingManager, Error,
				TEXT("[잘못된 상태] 원인=Submit 실행 기록 없음 ExecutionId=%s"),
				*SequenceExecutionId.ToString());
		}
		return Result;
	};

	const bool bIncomingIsSmallTalk = IsSmallTalkSequenceId(Sequence.SequenceId);
	const EMyStreamingSequenceSubmitAction SubmitAction =
		bIsSequencePlaying && bActiveSequenceIsSmallTalk && !bIncomingIsSmallTalk
			? EMyStreamingSequenceSubmitAction::InterruptActive
			: MyStreamingSequencePolicy::ResolveSubmitAction(
				bIsSequencePlaying,
				ActiveSequence.BusyPolicy,
				Sequence.BusyPolicy);

	if (SubmitAction == EMyStreamingSequenceSubmitAction::StartImmediately)
	{
		const bool bStarted = StartSequence(MoveTemp(Sequence));
		const FMyStreamingSequenceRequestResult Result = StoreResult(
			bStarted
				? EMyStreamingSequenceRequestStatus::AcceptedStarted
				: EMyStreamingSequenceRequestStatus::RejectedExecutionUnavailable,
			bStarted
				? EMyStreamingSequenceExecutionState::Running
				: EMyStreamingSequenceExecutionState::FailedTerminal);
		if (!bStarted)
		{
			StartNextPendingSequence();
		}
		return Result;
	}

	if (SubmitAction == EMyStreamingSequenceSubmitAction::InterruptActive)
	{
		const bool bInterruptedSmallTalk = bActiveSequenceIsSmallTalk;
		const FName InterruptedSequenceId = ActiveSequence.SequenceId;
		const FName BlockingSequenceId = Sequence.SequenceId;
		UE_LOG(LogStreamingManager, Log,
			TEXT("[중단 후 시작] ActiveSequenceId=%s ActiveExecutionId=%s IncomingSequenceId=%s IncomingExecutionId=%s PlayedStepCount=%d RemainingStepCount=%d"),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString(),
			*Sequence.SequenceId.ToString(),
			*Sequence.SequenceExecutionId.ToString(),
			NextStepIndex,
			FMath::Max(0, ActiveSequence.Steps.Num() - NextStepIndex));

		// 이미 화면에 보낸 Line과 Cooldown 기록은 유지하고, 남은 Step과 타이머만 버린다.
		UpdateExecutionState(
			ActiveSequence.SequenceExecutionId,
			EMyStreamingSequenceExecutionState::FailedInterrupted);
		ResetSequencePlayback();
		if (bInterruptedSmallTalk)
		{
			RecordSmallTalkTimeline(
				EMySmallTalkTimelineEventType::Interrupted,
				InterruptedSequenceId,
				BlockingSequenceId);
			ScheduleNextSmallTalk();
		}
		const bool bStarted = StartSequence(MoveTemp(Sequence));
		const FMyStreamingSequenceRequestResult Result = StoreResult(
			bStarted
				? EMyStreamingSequenceRequestStatus::AcceptedAfterInterrupt
				: EMyStreamingSequenceRequestStatus::RejectedExecutionUnavailable,
			bStarted
				? EMyStreamingSequenceExecutionState::Running
				: EMyStreamingSequenceExecutionState::FailedTerminal);
		if (!bStarted)
		{
			StartNextPendingSequence();
		}
		return Result;
	}

	if (SubmitAction == EMyStreamingSequenceSubmitAction::DropIncoming)
	{
		UE_LOG(LogStreamingManager, Log,
			TEXT("[폐기] SequenceId=%s ExecutionId=%s ActiveSequenceId=%s ActiveExecutionId=%s"),
			*Sequence.SequenceId.ToString(),
			*Sequence.SequenceExecutionId.ToString(),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString());

		return StoreResult(
			EMyStreamingSequenceRequestStatus::RejectedByBusyPolicy,
			EMyStreamingSequenceExecutionState::RejectedBeforeExecution);
	}

	{
		const int32 QueuePosition = PendingSequences.Num() + 1;
		UE_LOG(LogStreamingManager, Log,
			TEXT("[대기열 등록] SequenceId=%s ExecutionId=%s Position=%d"),
			*Sequence.SequenceId.ToString(),
			*Sequence.SequenceExecutionId.ToString(),
			QueuePosition
		);
	}

	PendingSequences.Add(MoveTemp(Sequence));
	return StoreResult(
		EMyStreamingSequenceRequestStatus::AcceptedQueued,
		EMyStreamingSequenceExecutionState::Pending);
}

////////////////////////////
//! \author 장효제
//! \brief 실행 ID 기록의 현재 또는 최종 상태를 갱신한다.
//! \param SequenceExecutionId 갱신할 한 번의 실행 ID다.
//! \param ExecutionState 새 실행 상태다.
void UMyStreamingManagerComponent::UpdateExecutionState(
	const FGuid& SequenceExecutionId,
	const EMyStreamingSequenceExecutionState ExecutionState)
{
	if (FSequenceExecutionRecord* Record =
		SequenceExecutionRecords.Find(SequenceExecutionId))
	{
		Record->Result.ExecutionState = ExecutionState;
		UE_LOG(LogStreamingManager, Verbose,
			TEXT("[실행 상태 변경] SequenceId=%s ExecutionId=%s ExecutionState=%d"),
			*Record->Request.SequenceId.ToString(),
			*SequenceExecutionId.ToString(),
			static_cast<int32>(ExecutionState));
		return;
	}

	UE_LOG(LogStreamingManager, Error,
		TEXT("[잘못된 상태] 원인=실행 상태 기록 없음 ExecutionId=%s ExecutionState=%d"),
		*SequenceExecutionId.ToString(),
		static_cast<int32>(ExecutionState));
}

////////////////////////////
//! \author 장효제
//! \brief Active 실행을 복구 불가능한 실패로 끝내고 남은 Pending Sequence를 처리한다.
void UMyStreamingManagerComponent::FailActiveSequenceAndStartNextPendingSequence()
{
	const bool bFailedSmallTalk = bActiveSequenceIsSmallTalk;
	if (bIsSequencePlaying && ActiveSequence.SequenceExecutionId.IsValid())
	{
		UpdateExecutionState(
			ActiveSequence.SequenceExecutionId,
			EMyStreamingSequenceExecutionState::FailedTerminal);
	}

	ResetSequencePlayback();
	StartNextPendingSequence();
	if (bFailedSmallTalk)
	{
		ScheduleNextSmallTalk();
	}
}

////////////////////////////
//! \author 장효제
//! \brief Pending Sequence를 앞에서부터 반복 시도하고 실패한 항목은 건너뛴다.
void UMyStreamingManagerComponent::StartNextPendingSequence()
{
	if (bIsSequencePlaying)
	{
		return;
	}

	while (!PendingSequences.IsEmpty())
	{
		// TArray를 큐처럼 사용한다.
		FMyResolvedStreamingSequence NextSequence = MoveTemp(PendingSequences[0]);
		PendingSequences.RemoveAt(0);

		const FGuid SequenceExecutionId = NextSequence.SequenceExecutionId;
		if (StartSequence(MoveTemp(NextSequence)))
		{
			return;
		}

		UpdateExecutionState(
			SequenceExecutionId,
			EMyStreamingSequenceExecutionState::FailedTerminal);
	}
}

//! \author 장효제
//! \brief 다음 Step의 Delay만큼 기다린 뒤 타이머 콜백을 예약한다.
void UMyStreamingManagerComponent::ScheduleNextStep()
{
	if (!bIsSequencePlaying)
	{
		return;
	}

	if (!ActiveSequence.Steps.IsValidIndex(NextStepIndex))
	{
		FinishSequence();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FailActiveSequenceAndStartNextPendingSequence();
		return;
	}

	const float DelaySeconds = FMath::Max(
		0.0f,
		ActiveSequence.Steps[NextStepIndex].DelayFromPreviousStepSeconds);

	if (DelaySeconds <= 0.0f)
	{
		SequenceTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&UMyStreamingManagerComponent::HandleSequenceTimerElapsed);
		return;
	}

	World->GetTimerManager().SetTimer(
		SequenceTimerHandle,
		this,
		&UMyStreamingManagerComponent::HandleSequenceTimerElapsed,
		DelaySeconds,
		false);
}


//! \author 장효제
//! \brief 예약 시간이 지나면 현재 Step을 발행하고 다음 Step으로 이동한다.
void UMyStreamingManagerComponent::HandleSequenceTimerElapsed()
{
	if (!bIsSequencePlaying
		|| !ActiveSequence.Steps.IsValidIndex(NextStepIndex))
	{
		FailActiveSequenceAndStartNextPendingSequence();
		return;
	}

	FMyResolvedStreamingStep& CurrentStep = ActiveSequence.Steps[NextStepIndex];
	const bool bFirstSmallTalkLine = bActiveSequenceIsSmallTalk && NextStepIndex == 0;
	UE_LOG(LogStreamingManager, Log, TEXT("[재생] SequenceId=%s ExecutionId=%s StepOrder=%d StepIndex=%d LineRowName=%s"),
		*ActiveSequence.SequenceId.ToString(),
		*ActiveSequence.SequenceExecutionId.ToString(),
		CurrentStep.StepOrder,
		NextStepIndex,
		*CurrentStep.LineRowName.ToString());

	++NextStepIndex;

	const bool bIsStartMissionAction =
		CurrentStep.ActionType == EMyStreamingActionType::StartMission;
	if (bIsStartMissionAction)
	{
		ExecuteResolvedStepAction(CurrentStep, INDEX_NONE);
	}
	else if (!ActiveSequence.RecipientUserIndexes.IsEmpty()
		&& MyStreamingSequencePolicy::IsStatefulActionType(CurrentStep.ActionType))
	{
		// D-6: Line/God/금액은 CurrentStep에서 한 번만 확정되어 있다.
		// 수령자별 복사본에 독립 실행 결과를 기록하고 성공한 본인에게만 즉시 버블을 보낸다.
		// 상태를 바꾸지 않는 Step은 수령자만큼 반복할 이유가 없어 단일 경로로 보낸다.
		for (const int32 RecipientUserIndex : ActiveSequence.RecipientUserIndexes)
		{
			FMyResolvedStreamingStep RecipientStep = CurrentStep;
			RecipientStep.ActionResult = FMyResolvedStepActionResult{};
			ExecuteResolvedStepAction(RecipientStep, RecipientUserIndex);
			SendDonationBubbleToRecipient(RecipientStep, RecipientUserIndex);
		}
	}
	else
	{
		// 기존 단일 수령자/Standalone 테스트 경로는 그대로 유지한다.
		ExecuteResolvedStepAction(CurrentStep, ActiveSequence.RecipientUserIndex);
		SendDonationBubbleToRecipient(CurrentStep, ActiveSequence.RecipientUserIndex);
	}

	const bool bActionAllowsPresentation =
		!bIsStartMissionAction || CurrentStep.ActionResult.bSucceeded;
	if (MyStreamingSequencePolicy::ShouldBroadcastStepPresentation(
			CurrentStep.PresentationType,
			bIsStartMissionAction,
			CurrentStep.ActionResult.bSucceeded))
	{
		DispatchChatMessage(CurrentStep.ChatMessage);
		if (CurrentStep.PresentationType == EMyStreamingPresentationType::MissionStart)
		{
			SendMissionNoticeToParty(CurrentStep);
		}
	}
	else
	{
		const TCHAR* SkipReason =
			!bActionAllowsPresentation
				? TEXT("StartMission Action 실패")
				: CurrentStep.PresentationType == EMyStreamingPresentationType::Donation
				? TEXT("Donation은 수령자 전용 버블 경로로 처리됨")
				: TEXT("비-Chat Presentation은 일반 Chat 방송 대상이 아님");

		UE_LOG(LogStreamingManager, Log,
			TEXT("[일반 Chat 방송 생략] SequenceId=%s StepOrder=%d LineRowName=%s PresentationType=%s 원인=%s"),
			*ActiveSequence.SequenceId.ToString(),
			CurrentStep.StepOrder,
			*CurrentStep.LineRowName.ToString(),
			*UEnum::GetValueAsString(CurrentStep.PresentationType),
			SkipReason);
	}
	if (bFirstSmallTalkLine)
	{
		UsedSmallTalkSequenceIds.Add(ActiveSequence.SequenceId);
		// 다음 예약이 이 표시를 보고 뭉칠지 정한다.
		bSmallTalkJustPlayed = true;
		RecordSmallTalkTimeline(
			EMySmallTalkTimelineEventType::Played,
			ActiveSequence.SequenceId);
	}
	if (bActionAllowsPresentation)
	{
		LastDisplayedLineRowNamesBySequence
			.FindOrAdd(ActiveSequence.SequenceId)
			.Add(CurrentStep.StepOrder, CurrentStep.LineRowName);
	}

	if (NextStepIndex >= ActiveSequence.Steps.Num())
	{
		FinishSequence();
		return;
	}

	ScheduleNextStep();
}


//! \author 장효제
//! \brief 현재 시퀀스가 마지막 Step까지 끝났음을 기록하고 Idle 상태로 돌아간다.
void UMyStreamingManagerComponent::FinishSequence()
{
	const bool bCompletedSmallTalk = bActiveSequenceIsSmallTalk;
	const FName CompletedSequenceId = ActiveSequence.SequenceId;
	UE_LOG(LogStreamingManager, Log, TEXT("[종료] SequenceId=%s ExecutionId=%s PlayedStepCount=%d"),
		*ActiveSequence.SequenceId.ToString(),
		*ActiveSequence.SequenceExecutionId.ToString(),
		NextStepIndex);

	UpdateExecutionState(
		ActiveSequence.SequenceExecutionId,
		EMyStreamingSequenceExecutionState::Succeeded);
	ResetSequencePlayback();
	StartNextPendingSequence();
	if (bCompletedSmallTalk)
	{
		RecordSmallTalkTimeline(
			EMySmallTalkTimelineEventType::Completed,
			CompletedSequenceId);
		ScheduleNextSmallTalk();
	}
}


//! \author 장효제
//! \brief 재생 타이머와 현재 시퀀스 상태를 초기값으로 되돌린다.
void UMyStreamingManagerComponent::ResetSequencePlayback()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SequenceTimerHandle);
	}

	SequenceTimerHandle.Invalidate();
	bIsSequencePlaying = false;
	bActiveSequenceIsSmallTalk = false;
	ActiveSequence = FMyResolvedStreamingSequence{};
	NextStepIndex = 0;
}
 

//! \author 장효제
//! \brief 실행 환경에 따라 대사 한 줄을 로컬 UI 또는 네트워크 클라이언트로 전달한다.
void UMyStreamingManagerComponent::DispatchChatMessage(
	const FMyStreamingChatMessageData& ChatMessage
)
{
	const AActor* OwnerActor = GetOwner();

	if (!OwnerActor || OwnerActor->GetNetMode() == NM_Standalone)
	{
		BroadcastChatMessage(ChatMessage);
		return;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(
			LogStreamingManager,
			Warning,
			TEXT("[잘못된 상태] 원인=클라이언트에서 대사 전송 시도 Owner=%s"),
			*GetNameSafe(OwnerActor)
		);
		return;
	}

	MulticastBroadcastChatMessage(ChatMessage);
}


namespace
{
	//! \brief [D-3] Donation 지급 실패 사유를 로그용 문자열로 변환한다.
	const TCHAR* DonationFailureReasonToString(EMyStreamingStepActionFailureReason Reason)
	{
		switch (Reason)
		{
		case EMyStreamingStepActionFailureReason::None:					return TEXT("None");
		case EMyStreamingStepActionFailureReason::NoAuthority:			return TEXT("NoAuthority");
		case EMyStreamingStepActionFailureReason::InvalidRecipient:		return TEXT("InvalidRecipient");
		case EMyStreamingStepActionFailureReason::InvalidInventory:		return TEXT("InvalidInventory");
		case EMyStreamingStepActionFailureReason::InvalidContract:		return TEXT("InvalidContract");
		case EMyStreamingStepActionFailureReason::NonPositiveAmount:	return TEXT("NonPositiveAmount");
		case EMyStreamingStepActionFailureReason::Overflow:				return TEXT("Overflow");
		case EMyStreamingStepActionFailureReason::PostconditionMismatch:return TEXT("PostconditionMismatch");
		default:														return TEXT("Unknown");
		}
	}
}


////////////////////////////
//! \author 장효제
//! \brief [D-3] 지급 대상 UserIndex에 해당하는 PlayerState를 소유 GameState의 PlayerArray에서 찾는다.
//! \param RecipientUserIndex 서버가 인증한 지급 대상 identity다.
//! \return 일치하는 AMyPlayerState. 없으면 nullptr. (첫 번째/임의 플레이어를 반환하지 않는다.)
AMyPlayerState* UMyStreamingManagerComponent::FindRecipientPlayerState(int32 RecipientUserIndex) const
{
	// 프로젝트 계약상 UserIndex > 0만 유효하다. 0/음수는 지급 대상으로 선택하지 않는다.
	if (!MyStreamingSequencePolicy::IsAuthenticatedUserIndex(RecipientUserIndex))
	{
		return nullptr;
	}

	const AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner());
	if (!GameState)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (MyPlayerState
			&& MyPlayerState->IsAuthVerified()
			&& MyPlayerState->GetUserIndex() == RecipientUserIndex
			&& MyPlayerState->GetOwningController())
		{
			return MyPlayerState;
		}
	}

	return nullptr;
}


////////////////////////////
//! \author 장효제
//! \brief 확정 Donation Step의 양수 지급 또는 Mission 완료 signed Meso 변화를 한 번 실행한다.
//! \param Step 실행할 확정 Step이다. 실행 결과는 Step.ActionResult에 저장한다(확정 입력값과 분리).
//! \param RecipientUserIndex 지급 대상 플레이어의 서버 identity다.
//! \note BuildResolvedSequence(조립)에서는 지급하지 않으며, 이 함수는 재생 경로에서 Step당 한 번만 호출된다.
////////////////////////////
//! \author 장효제
//! \brief 지급 대상 PlayerState를 확정한다. Standalone 테스트 수령자를 운영 경로보다 우선한다.
//! \param RecipientUserIndex 지급 대상 플레이어의 서버 identity다.
//! \return 확정된 수령자이며 없으면 nullptr이다.
AMyPlayerState* UMyStreamingManagerComponent::ResolveStepRecipient(const int32 RecipientUserIndex) const
{
#if !UE_BUILD_SHIPPING
	const AActor* OwnerActor = GetOwner();
	if (ActiveSequence.StandaloneTestRecipient.IsValid()
		&& OwnerActor
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode()))
	{
		return ActiveSequence.StandaloneTestRecipient.Get();
	}
#endif

	// 운영 경로: 인증된 UserIndex로만 대상을 찾는다(UserIndex>0 계약 유지).
	return FindRecipientPlayerState(RecipientUserIndex);
}

////////////////////////////
//! \author 장효제
//! \brief 경험치와 아이템 보상을 수령자 한 명에게 정확히 한 번 지급한다.
//! \param Step 실행할 확정 Step이다. 실행 결과는 Step.ActionResult에 저장한다.
//! \param RecipientUserIndex 지급 대상 플레이어의 서버 identity다.
//! \note Meso와 달리 사후 잔액 비교를 하지 않는다. 두 API 모두 성공 여부를 직접 돌려준다.
void UMyStreamingManagerComponent::ExecuteRewardGrant(
	FMyResolvedStreamingStep& Step,
	const int32 RecipientUserIndex)
{
	Step.ActionResult.bAttempted = true;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::NoAuthority;
		return;
	}

	AMyPlayerState* Recipient = ResolveStepRecipient(RecipientUserIndex);
	if (!Recipient)
	{
		Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidRecipient;
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[보상 지급 실패] SequenceId=%s StepOrder=%d RecipientUserIndex=%d 원인=수령자 없음"),
			*ActiveSequence.SequenceId.ToString(),
			Step.StepOrder,
			RecipientUserIndex);
		return;
	}

	if (Step.ActionType == EMyStreamingActionType::GrantExp)
	{
		const int32 Amount = Step.ResolvedRewardAmount;
		if (Step.PresentationType != EMyStreamingPresentationType::ExpReward)
		{
			Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidContract;
			return;
		}
		if (Amount <= 0)
		{
			Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::NonPositiveAmount;
			return;
		}

		APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(Recipient->GetPawn());
		if (!PlayerCharacter)
		{
			Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidRecipient;
			UE_LOG(LogStreamingManager, Warning,
				TEXT("[Exp 지급 실패] SequenceId=%s StepOrder=%d RecipientUserIndex=%d 원인=조종 캐릭터 없음"),
				*ActiveSequence.SequenceId.ToString(),
				Step.StepOrder,
				RecipientUserIndex);
			return;
		}

		const int32 BeforeLevel = Recipient->GetCharacterLevel();
		PlayerCharacter->AddExperience(Amount);
		Step.ActionResult.bSucceeded = true;
		Step.ActionResult.AppliedMesoDelta = 0;
		UE_LOG(LogStreamingManager, Log,
			TEXT("[Exp 지급 성공] SequenceId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d Amount=%d BeforeLevel=%d AfterLevel=%d"),
			*ActiveSequence.SequenceId.ToString(),
			Step.StepOrder,
			*Step.LineRowName.ToString(),
			RecipientUserIndex,
			Amount,
			BeforeLevel,
			Recipient->GetCharacterLevel());
		return;
	}

	// GrantItem
	if (Step.PresentationType != EMyStreamingPresentationType::ItemReward
		|| Step.RewardItemId.IsNone()
		|| Step.ResolvedRewardAmount <= 0)
	{
		Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidContract;
		return;
	}

	UMyInventoryComponent* Inventory = Recipient->GetInventoryComponent();
	if (!Inventory)
	{
		Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidInventory;
		return;
	}

	// 알 수 없는 ItemId나 가득 찬 인벤토리는 AddItem이 false를 돌려준다.
	const bool bGranted = Inventory->AddItem(Step.RewardItemId, Step.ResolvedRewardAmount);
    Step.ActionResult.bSucceeded = bGranted;
	Step.ActionResult.FailureReason = bGranted
		? EMyStreamingStepActionFailureReason::None
		: EMyStreamingStepActionFailureReason::PostconditionMismatch;

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Item 지급 %s] SequenceId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d ItemId=%s Count=%d"),
		bGranted ? TEXT("성공") : TEXT("실패"),
		*ActiveSequence.SequenceId.ToString(),
		Step.StepOrder,
		*Step.LineRowName.ToString(),
		RecipientUserIndex,
		*Step.RewardItemId.ToString(),
		Step.ResolvedRewardAmount);
}

void UMyStreamingManagerComponent::ExecuteResolvedStepAction(FMyResolvedStreamingStep& Step, int32 RecipientUserIndex)
{
	Step.ActionResult = FMyResolvedStepActionResult{};
	if (Step.ActionType == EMyStreamingActionType::StartMission)
	{
		Step.ActionResult.bAttempted = true;
		Step.ActionResult.bSucceeded =
			Step.PresentationType == EMyStreamingPresentationType::MissionStart
			&& Step.MissionTag.IsValid()
			&& TryActivateMission(Step.MissionTag, ActiveSequence.SequenceId);
		Step.ActionResult.FailureReason = Step.ActionResult.bSucceeded
			? EMyStreamingStepActionFailureReason::None
			: EMyStreamingStepActionFailureReason::InvalidContract;
		return;
	}

	if (Step.ActionType == EMyStreamingActionType::GrantExp
		|| Step.ActionType == EMyStreamingActionType::GrantItem)
	{
		ExecuteRewardGrant(Step, RecipientUserIndex);
		return;
	}

	const bool bIsDonationGrant =
		Step.PresentationType == EMyStreamingPresentationType::Donation
		&& Step.ActionType == EMyStreamingActionType::GrantMeso
		&& (Step.RewardSource == EMyStreamingRewardSource::RollFromLine
			|| Step.RewardSource == EMyStreamingRewardSource::Payload);
	const bool bIsMissionMesoDelta =
		Step.PresentationType == EMyStreamingPresentationType::Donation
		&& Step.ActionType == EMyStreamingActionType::ApplyMesoDelta
		&& Step.RewardSource == EMyStreamingRewardSource::Payload
		&& Step.ChatMessage.SourceEventTag.MatchesTagExact(
			MyGameplayTags::Streaming_Event_Mission_Completed);

	if (!bIsDonationGrant && !bIsMissionMesoDelta)
	{
		if (Step.ActionType == EMyStreamingActionType::ApplyMesoDelta)
		{
			Step.ActionResult.bAttempted = true;
			Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::InvalidContract;
			UE_LOG(LogStreamingManager, Error,
				TEXT("[Mission Meso 적용 거부] SequenceId=%s StepOrder=%d SourceEventTag=%s 원인=계약 불일치"),
				*ActiveSequence.SequenceId.ToString(),
				Step.StepOrder,
				*Step.ChatMessage.SourceEventTag.ToString());
		}
		return;
	}

	Step.ActionResult.bAttempted = true;

	const AActor* OwnerActor = GetOwner();
	const bool bHasAuthority = (OwnerActor != nullptr) && OwnerActor->HasAuthority();

	AMyPlayerState* Recipient = nullptr;
#if !UE_BUILD_SHIPPING
	// D-4A: 비Shipping·Standalone 전용 명시적 테스트 수령자만 우선한다. 운영 경로는 이 값이 비어 있다.
	if (ActiveSequence.StandaloneTestRecipient.IsValid()
		&& OwnerActor
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode()))
	{
		Recipient = ActiveSequence.StandaloneTestRecipient.Get();
	}
#endif
	if (!Recipient)
	{
		// 운영 경로: 인증된 UserIndex로만 대상을 찾는다(UserIndex>0 계약 유지).
		Recipient = FindRecipientPlayerState(RecipientUserIndex);
	}
	UMyInventoryComponent* Inventory = Recipient ? Recipient->GetInventoryComponent() : nullptr;
	const int32 Amount = Step.ResolvedRewardAmount;
	const int32 BeforeMeso = Inventory ? Inventory->GetMeso() : 0;

	EMyStreamingStepActionFailureReason PrecheckReason = EMyStreamingStepActionFailureReason::None;
	if (!bHasAuthority)
	{
		PrecheckReason = EMyStreamingStepActionFailureReason::NoAuthority;
	}
	else if (!Recipient)
	{
		PrecheckReason = EMyStreamingStepActionFailureReason::InvalidRecipient;
	}
	else if (!Inventory)
	{
		PrecheckReason = EMyStreamingStepActionFailureReason::InvalidInventory;
	}
	else if (bIsDonationGrant || Amount > 0)
	{
		PrecheckReason = MyStreamingSequencePolicy::CheckDonationGrantPreconditions(
			bHasAuthority,
			true,
			true,
			Amount,
			BeforeMeso);
	}
	else if (Amount == 0)
	{
		PrecheckReason = EMyStreamingStepActionFailureReason::InvalidContract;
	}

	if (PrecheckReason != EMyStreamingStepActionFailureReason::None)
	{
		Step.ActionResult.bSucceeded = false;
		Step.ActionResult.FailureReason = PrecheckReason;
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[Donation Meso 적용 실패] SequenceId=%s ExecutionId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d ResolvedRewardAmount=%d FailureReason=%s"),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString(),
			Step.StepOrder,
			*Step.LineRowName.ToString(),
			RecipientUserIndex,
			Amount,
			DonationFailureReasonToString(PrecheckReason));
		return;
	}

	const int32 AppliedMesoDelta =
		MyStreamingSequencePolicy::ResolveAppliedMesoDelta(Amount, BeforeMeso);
	const FGameplayTag MesoSourceTag = bIsMissionMesoDelta
		? MyGameplayTags::Meso_Source_Streaming_Mission
		: MyGameplayTags::Meso_Source_Streaming_Donation;
	bool bMutationSucceeded = true;
	if (AppliedMesoDelta > 0)
	{
		Inventory->AddMeso(AppliedMesoDelta, MesoSourceTag);
	}
	else if (AppliedMesoDelta < 0)
	{
		bMutationSucceeded = Inventory->TryConsumeMeso(-AppliedMesoDelta, MesoSourceTag);
	}
	const int32 AfterMeso = Inventory->GetMeso();

	if (!bMutationSucceeded
		|| !MyStreamingSequencePolicy::IsDonationGrantPostconditionMet(
			BeforeMeso,
			AppliedMesoDelta,
			AfterMeso))
	{
		Step.ActionResult.bSucceeded = false;
		Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::PostconditionMismatch;
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[Donation Meso 적용 실패] SequenceId=%s ExecutionId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d ResolvedRewardAmount=%d FailureReason=%s BeforeMeso=%d AfterMeso=%d"),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString(),
			Step.StepOrder,
			*Step.LineRowName.ToString(),
			RecipientUserIndex,
			Amount,
			DonationFailureReasonToString(EMyStreamingStepActionFailureReason::PostconditionMismatch),
			BeforeMeso,
			AfterMeso);
		return;
	}

	Step.ActionResult.bSucceeded = true;
	Step.ActionResult.AppliedMesoDelta = AppliedMesoDelta;
	Step.ActionResult.FailureReason = EMyStreamingStepActionFailureReason::None;

	// 도네이션을 실제로 받았다는 사실을 남긴다. SourceTag에 준 신을 실어
	// "특정 신에게 도네를 n번" 조건이 신을 구분할 수 있게 한다.
	MyStreamingCountEvent::BroadcastCountEvent(
		this,
		MyGameplayTags::Streaming_Event_Donation_Granted,
		Step.ChatMessage.GodTag);

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Donation Meso 적용 성공] SequenceId=%s ExecutionId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d BeforeMeso=%d ResolvedRewardAmount=%d AppliedMesoDelta=%d AfterMeso=%d"),
		*ActiveSequence.SequenceId.ToString(),
		*ActiveSequence.SequenceExecutionId.ToString(),
		Step.StepOrder,
		*Step.LineRowName.ToString(),
		RecipientUserIndex,
		BeforeMeso,
		Amount,
		AppliedMesoDelta,
		AfterMeso);
}

////////////////////////////
//! \author 장효제
//! \brief 파티 잠수 감시를 시작한다.
void UMyStreamingManagerComponent::StartAntiAFK()
{
	const AActor* OwnerActor = GetOwner();
	if (bAntiAFKStarted || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}
	if (!MyStreamingAntiAFKRulePolicy::IsIdleSecondsValid(AntiAFKTimeoutSeconds))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잠수 방지 시작 실패] 원인=조건 표가 판정 시간을 주지 못함 Timeout=%.1f"),
			AntiAFKTimeoutSeconds);
		return;
	}

	bAntiAFKStarted = true;
	// 미사용 Rule은 사실이 오지 않아야 발동한다. 감시 시작과 같은 시점에 타이머를 건다.
	ArmSkillIdleTimers();
	RegisterPlayerInputListener();
	if (!PlayerInputListenerHandle.IsValid())
	{
		bAntiAFKStarted = false;
		return;
	}

	if (!IsStoryDialogueBlockingSmallTalk())
	{
		ArmAntiAFKTimer();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogStreamingManager, Log,
			TEXT("[잠수 방지] Event=Armed Timeout=%.1f"),
			AntiAFKTimeoutSeconds);
#endif
	}
}

////////////////////////////
//! \author 장효제
//! \brief Dungeon 종료 시 잠수 감시 Timer와 조작 사실 구독을 정리한다.
void UMyStreamingManagerComponent::StopAntiAFK()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AntiAFKTimerHandle);
	}
	AntiAFKTimerHandle.Invalidate();
	UnregisterPlayerInputListener();
	bAntiAFKStarted = false;
	bIsPartyAFK = false;
	bResumePresentationAfterDialogue = false;
}

////////////////////////////
//! \author 장효제
//! \brief 플레이어 조작 사실 채널을 서버에서 구독한다.
void UMyStreamingManagerComponent::RegisterPlayerInputListener()
{
	UnregisterPlayerInputListener();
	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogStreamingManager, Error,
			TEXT("[잠수 방지 시작 실패] GameplayMessageSubsystem 없음"));
		return;
	}

	PlayerInputListenerHandle =
		UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingPlayerInputPayload>(
			MyGameplayTags::Streaming_Channel_PlayerInput,
			this,
			&UMyStreamingManagerComponent::HandlePlayerInput);
}

////////////////////////////
//! \author 장효제
//! \brief 조작 사실 채널 구독을 멱등 해제한다.
void UMyStreamingManagerComponent::UnregisterPlayerInputListener()
{
	if (PlayerInputListenerHandle.IsValid())
	{
		PlayerInputListenerHandle.Unregister();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 조작 사실을 받으면 잠수를 해제하거나 감시 타이머를 다시 감는다.
//! \param Channel 조작 사실 GameplayMessage 채널이다.
//! \param Payload 조작한 플레이어를 담은 최소 Fact다.
void UMyStreamingManagerComponent::HandlePlayerInput(
	FGameplayTag Channel,
	const FMyStreamingPlayerInputPayload& Payload)
{
	(void)Channel;
	const AActor* OwnerActor = GetOwner();
	if (!bAntiAFKStarted || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogStreamingManager, Verbose,
		TEXT("[잠수 방지] Event=InputAccepted UserIndex=%d"), Payload.UserIndex);
#endif

	if (bIsPartyAFK)
	{
		ResumeFromAFK(
			MyGameplayTags::Streaming_Event_Player_Input,
			IsStoryDialogueBlockingSmallTalk());
		return;
	}

	if (!IsStoryDialogueBlockingSmallTalk())
	{
		ArmAntiAFKTimer();
	}
}

////////////////////////////
//! \author 장효제
//! \brief Tick 없이 현재 시각부터 AntiAFKTimeoutSeconds 뒤 한 번 만료되는 Timer를 설정한다.
void UMyStreamingManagerComponent::ArmAntiAFKTimer()
{
	UWorld* World = GetWorld();
	if (!bAntiAFKStarted || bIsPartyAFK || !World || IsStoryDialogueBlockingSmallTalk())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AntiAFKTimerHandle,
		this,
		&UMyStreamingManagerComponent::HandleAntiAFKTimeout,
		AntiAFKTimeoutSeconds,
		false);
}

////////////////////////////
//! \author 장효제
//! \brief 판정 시간 동안 조작이 없으면 파티를 한 번 잠수 상태로 전환한다.
void UMyStreamingManagerComponent::HandleAntiAFKTimeout()
{
	AntiAFKTimerHandle.Invalidate();
	if (!bAntiAFKStarted || bIsPartyAFK || IsStoryDialogueBlockingSmallTalk())
	{
		return;
	}

	EnterPartyAFK();
}

////////////////////////////
//! \author 장효제
//! \brief 파티 잠수를 확정하고 SmallTalk 공급을 멈춘 뒤 AntiAFK Mission을 한 번 요청한다.
void UMyStreamingManagerComponent::EnterPartyAFK()
{
	if (!bAntiAFKStarted || bIsPartyAFK)
	{
		return;
	}

	bIsPartyAFK = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SmallTalkTimerHandle);
	}
	SmallTalkTimerHandle.Invalidate();

#if !UE_BUILD_SHIPPING
	UE_LOG(LogStreamingManager, Log, TEXT("[잠수 방지] Event=EnteredAFK"));
#endif

	const bool bMissionStarted = TryStartAntiAFKMission();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogStreamingManager, Log,
		TEXT("[잠수 방지] Event=AntiAFKMissionTriggered Started=%s"),
		bMissionStarted ? TEXT("true") : TEXT("false"));
#endif
}

////////////////////////////
//! \author 장효제
//! \brief 조작으로 잠수를 해제하고 Resume 연출·SmallTalk·새 감시를 재개한다.
//! \param SourceEventTag 잠수를 해제한 원본 사건이며 Dialogue 직접 해제는 비어 있다.
//! \param bDeferPresentation Story Dialogue가 끝날 때까지 Resume Chat을 보류할지 여부다.
void UMyStreamingManagerComponent::ResumeFromAFK(
	const FGameplayTag SourceEventTag,
	const bool bDeferPresentation)
{
	if (!bAntiAFKStarted || !bIsPartyAFK)
	{
		return;
	}

	bIsPartyAFK = false;
#if !UE_BUILD_SHIPPING
	UE_LOG(LogStreamingManager, Log,
		TEXT("[잠수 방지] Event=ActivityResumed SourceEventTag=%s"),
		SourceEventTag.IsValid() ? *SourceEventTag.ToString() : TEXT("StoryDialogue"));
#endif

	if (bDeferPresentation || IsStoryDialogueBlockingSmallTalk())
	{
		bResumePresentationAfterDialogue = true;
		return;
	}

	RequestAntiAFKResumeSequence();
	ScheduleNextSmallTalk();
	ArmAntiAFKTimer();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogStreamingManager, Log,
		TEXT("[잠수 방지] Event=ReArmed Timeout=%.1f"),
		AntiAFKTimeoutSeconds);
#endif
}

////////////////////////////
//! \author 장효제
//! \brief 잠수 해제 Chat-only Sequence를 기존 Queue 실행 경계에 요청한다.
void UMyStreamingManagerComponent::RequestAntiAFKResumeSequence()
{
	FMyStreamingSequenceRequest Request;
	Request.SequenceExecutionId = FGuid::NewGuid();
	Request.SequenceId = ResolveAntiAFKSequenceId(MyStreamingAntiAFKRuleNames::Resume);
	if (Request.SequenceId.IsNone())
	{
		return;
	}
	Request.BusyPolicy = EMyStreamingSequenceBusyPolicy::Queue;
	Request.SourceEventTag = MyGameplayTags::Streaming_Event_AntiAFK_Resumed;

	const FMyStreamingSequenceRequestResult Result = RequestSequence(Request);
	if (!Result.IsAccepted())
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[잠수 방지 Resume 요청 실패] SequenceId=%s Status=%d"),
			*Request.SequenceId.ToString(),
			static_cast<int32>(Result.Status));
	}
}


////////////////////////////
//! \author 장효제
//! \brief 실제 Meso 변화가 발생한 Donation Step을 수령자에게 Bubble과 Notice로 보낸다.
//! \param Step 방금 ExecuteResolvedStepAction으로 실행 결과가 채워진 확정 Step이다.
//! \param RecipientUserIndex 지급을 받은 플레이어의 서버 identity다.
//! \note 서버 전용. 지급/추첨을 하지 않고, 한 번 조립한 BubbleData.Message를 두 UI가 함께 사용한다.
void UMyStreamingManagerComponent::SendDonationBubbleToRecipient(const FMyResolvedStreamingStep& Step, int32 RecipientUserIndex)
{
	// 지급이 실제로 성공한 Donation만 표시한다. 실패/미시도/0원은 여기서 차단된다.
	if (!MyStreamingSequencePolicy::ShouldSendRewardBubble(
			Step.PresentationType,
			Step.ActionType,
			Step.ActionResult.bAttempted,
			Step.ActionResult.bSucceeded,
			Step.ActionResult.AppliedMesoDelta))
	{
		return;
	}

	// 수령자 전용 전달: 지급받은 UserIndex의 PlayerState가 소유한 컨트롤러에게만 Client RPC로 보낸다.
	AMyPlayerState* Recipient = nullptr;
#if !UE_BUILD_SHIPPING
	const AActor* BubbleOwnerActor = GetOwner();
	if (ActiveSequence.StandaloneTestRecipient.IsValid()
		&& BubbleOwnerActor
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(BubbleOwnerActor->GetNetMode()))
	{
		Recipient = ActiveSequence.StandaloneTestRecipient.Get();
	}
#endif
	if (!Recipient)
	{
		Recipient = FindRecipientPlayerState(RecipientUserIndex);
	}
	ADungeonPC* RecipientController = Recipient ? Cast<ADungeonPC>(Recipient->GetOwningController()) : nullptr;
	if (!RecipientController)
	{
		UE_LOG(LogStreamingManager, Warning,
			TEXT("[Donation 버블 전송 실패] 원인=수령자 컨트롤러 없음 SequenceId=%s ExecutionId=%s StepOrder=%d RecipientUserIndex=%d"),
			*ActiveSequence.SequenceId.ToString(),
			*ActiveSequence.SequenceExecutionId.ToString(),
			Step.StepOrder,
			RecipientUserIndex);
		return;
	}

	// 표시 데이터는 같은 선택된 Resolved Step에서만 가져온다. 표시 금액은 실제 지급 결과값을 사용한다.
	FMyStreamingChatMessageData BubbleData;
	BubbleData.GodTag = Step.ChatMessage.GodTag;
	BubbleData.SourceEventTag = Step.ChatMessage.SourceEventTag;
	// D-5C: UI가 배경 Brush를 고를 수 있도록 명시적 Presentation을 담는다(문자열/bool 추론 없음).
	BubbleData.PresentationType = Step.PresentationType;
	// D-5B: 시스템 결과 문구 + 선택된 Line의 원본 MessageText(작성 대사)를 조합한다.
	const FText GodDisplayName = GetGodDisplayName(Step.ChatMessage.GodTag);
	const FText CharacterDisplayName =
		MyStreamingSequencePolicy::GetDonationRecipientCharacterDisplayName(Recipient->GetSelectedCharacterId());
	// 보상 종류에 따라 표시 수량과 문구가 달라진다. Meso는 실제 적용 변화량, Exp는 확정 획득량,
	// Item은 지급 개수를 쓴다.
	EMyStreamingRewardKind RewardKind = EMyStreamingRewardKind::Meso;
	MyStreamingSequencePolicy::TryResolveRewardKind(Step.PresentationType, RewardKind);

	FText ItemDisplayName;
	int32 DisplayAmount = Step.ActionResult.AppliedMesoDelta;
	if (RewardKind == EMyStreamingRewardKind::Exp)
	{
		DisplayAmount = Step.ResolvedRewardAmount;
	}
	else if (RewardKind == EMyStreamingRewardKind::Item)
	{
		DisplayAmount = Step.ResolvedRewardAmount;
		if (const UMyInventoryComponent* RecipientInventory = Recipient->GetInventoryComponent())
		{
			FMyItemData ItemData;
			if (RecipientInventory->FindItemData(Step.RewardItemId, ItemData))
			{
				ItemDisplayName = ItemData.DisplayName;
			}
		}
		if (ItemDisplayName.IsEmpty())
		{
			// 표시명을 찾지 못해도 지급은 이미 성공했으므로 RowName으로 대체한다.
			ItemDisplayName = FText::FromName(Step.RewardItemId);
		}
	}

	BubbleData.Message = MyStreamingSequencePolicy::FormatRewardMessage(
		Step.ChatMessage.GodTag,
		GodDisplayName,
		CharacterDisplayName,
		RewardKind,
		DisplayAmount,
		ItemDisplayName,
		Step.ChatMessage.Message);

	UE_LOG(LogStreamingManager, Log,
		TEXT("[Donation 버블 전송] SequenceId=%s ExecutionId=%s StepOrder=%d LineRowName=%s RecipientUserIndex=%d GodTag=%s AppliedMesoDelta=%d"),
		*ActiveSequence.SequenceId.ToString(),
		*ActiveSequence.SequenceExecutionId.ToString(),
		Step.StepOrder,
		*Step.LineRowName.ToString(),
		RecipientUserIndex,
		*Step.ChatMessage.GodTag.ToString(),
		Step.ActionResult.AppliedMesoDelta);

	// 같은 지급 성공 결과를 기존 두 클라이언트 표시 경로에 전달한다.
	// Notice는 별도 의미의 문구를 조립하거나 지급을 실행하지 않으며, 0초는 WBP_Notice의 기본 표시 시간을 뜻한다.
	// 버블과 Notice는 서버가 한 번 조립한 같은 RichText 문구를 사용한다.
	RecipientController->ClientReceiveDonationBubble(BubbleData);

	FMyNoticeData DonationNoticeData;
	DonationNoticeData.Message = BubbleData.Message;
	DonationNoticeData.PresentationType = EMyNoticePresentationType::Donation;
	RecipientController->SendNoticeDataToClient(DonationNoticeData);
}

////////////////////////////
//! \author 장효제
//! \brief Mission 시작 문구를 고정 파티의 기존 Notice 경로로 전달한다.
//! \param Step 현재 재생한 Mission Presentation Step이다.
void UMyStreamingManagerComponent::SendMissionNoticeToParty(const FMyResolvedStreamingStep& Step)
{
#if !UE_BUILD_SHIPPING
	const AActor* OwnerActor = GetOwner();
	if (ActiveSequence.MissionNoticeStandaloneRecipient.IsValid()
		&& OwnerActor
		&& MyStreamingSequencePolicy::IsStandaloneDonationTestAllowed(OwnerActor->GetNetMode()))
	{
		AMyPlayerState* Recipient = ActiveSequence.MissionNoticeStandaloneRecipient.Get();
		ADungeonPC* RecipientController =
			Recipient ? Cast<ADungeonPC>(Recipient->GetOwningController()) : nullptr;
		if (RecipientController)
		{
			RecipientController->SendNoticeToClient(Step.ChatMessage.Message, 0.0f);
		}
		return;
	}
#endif

	for (const int32 UserIndex : ActiveSequence.MissionNoticeUserIndexes)
	{
		AMyPlayerState* Recipient = FindRecipientPlayerState(UserIndex);
		ADungeonPC* RecipientController =
			Recipient ? Cast<ADungeonPC>(Recipient->GetOwningController()) : nullptr;
		if (RecipientController)
		{
			RecipientController->SendNoticeToClient(Step.ChatMessage.Message, 0.0f);
		}
	}
}


////////////////////////////
//! \author 장효제
//! \brief [D-4] 수령자 클라이언트에서 기존 God Chat 경로로 Donation 결과 버블을 표시한다.
//! \param BubbleData 서버가 조립해 전달한 표시 데이터다.
void UMyStreamingManagerComponent::ClientDisplayDonationBubble(const FMyStreamingChatMessageData& BubbleData)
{
	UE_LOG(LogStreamingManager, Log,
		TEXT("[Donation 버블 수신] Owner=%s PresentationType=%s GodTag=%s Message=%s"),
		*GetNameSafe(GetOwner()),
		*UEnum::GetValueAsString(BubbleData.PresentationType),
		*BubbleData.GodTag.ToString(),
		*BubbleData.Message.ToString());

	// 기존 일반 Chat 표시 경로(ApplyGodPresentation + UI 채널 브로드캐스트)를 그대로 재사용한다.
	BroadcastChatMessage(BubbleData);
}


//! \author 장효제
//! \brief 전투 Rule 테이블을 순회해 Payload와 일치하는 Rule 중 하나를 가중치로 선택한다.
FMyStreamingCombatRuleMatchResult UMyStreamingManagerComponent::MatchCombatRule(const FMyStreamingCombatPayload& Payload) const
{
	FMyStreamingCombatRuleMatchResult MatchResult;
	if (!CombatRuleTable)
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] CombatRuleTable=null"));
		return MatchResult;
	}

	TArray<FMyStreamingCombatRuleRow*> AllRules;
	CombatRuleTable->GetAllRows(TEXT("UMyStreamingManagerComponent::MatchCombatRule"), AllRules);

	UE_LOG(LogStreamingManager, Verbose, TEXT("[DataTable 로드] Table=%s RowCount=%d"),
		*CombatRuleTable->GetPathName(),
		AllRules.Num());

	if (AllRules.IsEmpty())
	{
		UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] CombatRuleTable RowCount=0 Table=%s"),
			*CombatRuleTable->GetPathName());
		return MatchResult;
	}

	TArray<const FMyStreamingCombatRuleRow*> MatchedRules;
	int32 TotalWeight = 0;
	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	for (int32 RuleIndex = 0; RuleIndex < AllRules.Num(); ++RuleIndex)
	{
		const FMyStreamingCombatRuleRow* Rule = AllRules[RuleIndex];
		if (!Rule || Rule->SequenceId.IsNone() || Rule->Weight <= 0)
		{
			UE_LOG(LogStreamingManager, Warning, TEXT("[잘못된 데이터] Index=%d RulePtr=%s SequenceId=%s Weight=%d"),
				RuleIndex,
				Rule ? TEXT("valid") : TEXT("null"),
				Rule ? *Rule->SequenceId.ToString() : TEXT("None"),
				Rule ? Rule->Weight : 0);
			continue;
		}

		if (Rule->CooldownSeconds > 0.0f)
		{
			if (const float* LastTriggerTime = LastSequenceTriggerTimeMap.Find(Rule->SequenceId))
			{
				const float CooldownEndTime = *LastTriggerTime + Rule->CooldownSeconds;
				if (CurrentTime < CooldownEndTime)
				{
					UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 제외] SequenceId=%s 원인=Cooldown CurrentTime=%.3f LastTriggerTime=%.3f CooldownSeconds=%.3f RemainingSeconds=%.3f"),
						*Rule->SequenceId.ToString(),
						CurrentTime,
						*LastTriggerTime,
						Rule->CooldownSeconds,
						CooldownEndTime - CurrentTime);
					continue;
				}
			}
		}

		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 평가] Index=%d SequenceId=%s Weight=%d EventTag=%s InstigatorTag=%s TargetTag=%s SkillTag=%s DamageMin=%.2f DamageMax=%.2f HPRatioMin=%.3f HPRatioMax=%.3f CriticalMatch=%s KillMatch=%s"),
			RuleIndex,
			*Rule->SequenceId.ToString(),
			Rule->Weight,
			*Rule->EventTag.ToString(),
			*Rule->InstigatorTag.ToString(),
			*Rule->TargetTag.ToString(),
			*Rule->SkillTag.ToString(),
			Rule->MinDamageAmount,
			Rule->MaxDamageAmount,
			Rule->MinTargetCurrentHPRatio,
			Rule->MaxTargetCurrentHPRatio,
			*StaticEnum<EMyStreamingRuleBoolMatch>()->GetNameStringByValue(static_cast<int64>(Rule->CriticalMatch)),
			*StaticEnum<EMyStreamingRuleBoolMatch>()->GetNameStringByValue(static_cast<int64>(Rule->KillMatch)));

		if (!DoesCombatRuleMatch(Payload, *Rule))
		{
			continue;
		}

		MatchedRules.Add(Rule);
		TotalWeight += Rule->Weight;
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 일치] Index=%d SequenceId=%s TotalWeight=%d"),
			RuleIndex,
			*Rule->SequenceId.ToString(),
			TotalWeight);
	}

	if (MatchedRules.IsEmpty() || TotalWeight <= 0)
	{
		return MatchResult;
	}

	int32 RandomWeight = FMath::RandRange(1, TotalWeight);
	UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 선택] MatchedCount=%d TotalWeight=%d Roll=%d"),
		MatchedRules.Num(),
		TotalWeight,
		RandomWeight);

	for (const FMyStreamingCombatRuleRow* Rule : MatchedRules)
	{
		RandomWeight -= Rule->Weight;
		if (RandomWeight <= 0)
		{
			MatchResult.bMatched = true;
			MatchResult.SequenceId = Rule->SequenceId;
			MatchResult.MatchedRule = *Rule;
			UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 선택] SequenceId=%s Weight=%d"),
				*MatchResult.SequenceId.ToString(),
				Rule->Weight);
			return MatchResult;
		}
	}

	const FMyStreamingCombatRuleRow* FallbackRule = MatchedRules.Last();
	MatchResult.bMatched = true;
	MatchResult.SequenceId = FallbackRule->SequenceId;
	MatchResult.MatchedRule = *FallbackRule;
	UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 선택] SequenceId=%s Weight=%d 방식=Fallback"),
		*MatchResult.SequenceId.ToString(),
		FallbackRule->Weight);
	return MatchResult;
}

bool UMyStreamingManagerComponent::DoesCombatRuleMatch(const FMyStreamingCombatPayload& Payload, const FMyStreamingCombatRuleRow& Rule) const
{
	if (!DoesTagMatch(Payload.EventTag, Rule.EventTag))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=EventTag Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			*Payload.EventTag.ToString(),
			*Rule.EventTag.ToString());
		return false;
	}

	if (!DoesTagMatch(Payload.InstigatorTag, Rule.InstigatorTag))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=InstigatorTag Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			*Payload.InstigatorTag.ToString(),
			*Rule.InstigatorTag.ToString());
		return false;
	}

	if (!DoesTagMatch(Payload.TargetTag, Rule.TargetTag))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=TargetTag Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			*Payload.TargetTag.ToString(),
			*Rule.TargetTag.ToString());
		return false;
	}

	if (!DoesTagMatch(Payload.SkillTag, Rule.SkillTag))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=SkillTag Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			*Payload.SkillTag.ToString(),
			*Rule.SkillTag.ToString());
		return false;
	}

	if (!DoesFloatRangeMatch(Payload.DamageAmount, Rule.MinDamageAmount, Rule.MaxDamageAmount))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=Damage Payload=%.2f RuleMin=%.2f RuleMax=%.2f"),
			*Rule.SequenceId.ToString(),
			Payload.DamageAmount,
			Rule.MinDamageAmount,
			Rule.MaxDamageAmount);
		return false;
	}

	if (!DoesFloatRangeMatch(Payload.TargetCurrentHPRatio, Rule.MinTargetCurrentHPRatio, Rule.MaxTargetCurrentHPRatio))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=HPRatio Payload=%.3f RuleMin=%.3f RuleMax=%.3f"),
			*Rule.SequenceId.ToString(),
			Payload.TargetCurrentHPRatio,
			Rule.MinTargetCurrentHPRatio,
			Rule.MaxTargetCurrentHPRatio);
		return false;
	}

	if (!DoesBoolMatch(Payload.bIsCritical, Rule.CriticalMatch))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=CriticalMatch Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			Payload.bIsCritical ? TEXT("true") : TEXT("false"),
			*StaticEnum<EMyStreamingRuleBoolMatch>()->GetNameStringByValue(static_cast<int64>(Rule.CriticalMatch)));
		return false;
	}

	if (!DoesBoolMatch(Payload.bIsKill, Rule.KillMatch))
	{
		UE_LOG(LogStreamingManager, Verbose, TEXT("[규칙 불일치] SequenceId=%s 원인=KillMatch Payload=%s Rule=%s"),
			*Rule.SequenceId.ToString(),
			Payload.bIsKill ? TEXT("true") : TEXT("false"),
			*StaticEnum<EMyStreamingRuleBoolMatch>()->GetNameStringByValue(static_cast<int64>(Rule.KillMatch)));
		return false;
	}

	return true;
}

bool UMyStreamingManagerComponent::DoesTagMatch(FGameplayTag PayloadTag, FGameplayTag RuleTag) const
{
	return !RuleTag.IsValid() || PayloadTag.MatchesTag(RuleTag);
}

bool UMyStreamingManagerComponent::DoesFloatRangeMatch(float PayloadValue, float RuleMinValue, float RuleMaxValue) const
{
	if (RuleMinValue != -1.0f && PayloadValue < RuleMinValue)
	{
		return false;
	}

	if (RuleMaxValue != -1.0f && PayloadValue > RuleMaxValue)
	{
		return false;
	}

	return true;
}

bool UMyStreamingManagerComponent::DoesBoolMatch(bool bPayloadValue, EMyStreamingRuleBoolMatch RuleMatch) const
{
	switch (RuleMatch)
	{
	case EMyStreamingRuleBoolMatch::Any:
		return true;
	case EMyStreamingRuleBoolMatch::MatchTrue:
		return bPayloadValue;
	case EMyStreamingRuleBoolMatch::MatchFalse:
		return !bPayloadValue;
	default:
		return false;
	}
}

FText UMyStreamingManagerComponent::BuildDebugMessageText(const FMyStreamingCombatPayload& Payload, const FMyStreamingCombatRuleMatchResult& MatchResult) const
{
	const FString InstigatorName = GetTagLeafName(Payload.InstigatorTag);
	const FString TargetName = GetTagLeafName(Payload.TargetTag);
	const int32 RoundedDamage = FMath::Max(FMath::RoundToInt(Payload.DamageAmount), 0);
	const FString SequenceId = MatchResult.SequenceId.ToString();

	if (Payload.bIsKill || Payload.EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_Kill))
	{
		return FText::FromString(FString::Printf(TEXT("SequenceId: %s | %s가 %s를 처치했다."), *SequenceId, *InstigatorName, *TargetName));
	}

	if (Payload.bIsCritical)
	{
		return FText::FromString(FString::Printf(TEXT("SequenceId: %s | 치명타! %s가 %s에게 %d 피해를 줬다."), *SequenceId, *InstigatorName, *TargetName, RoundedDamage));
	}

	if (Payload.EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_SkillUsed))
	{
		const FString SkillName = GetTagLeafName(Payload.SkillTag);
		return FText::FromString(FString::Printf(TEXT("SequenceId: %s | %s가 %s을 사용했다."), *SequenceId, *InstigatorName, *SkillName));
	}

	return FText::FromString(FString::Printf(TEXT("SequenceId: %s | %s가 %s에게 %d 피해를 줬다."), *SequenceId, *InstigatorName, *TargetName, RoundedDamage));
}

FText UMyStreamingManagerComponent::GetGodDisplayName(FGameplayTag GodTag) const
{
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Horus))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameHorus", "호루스");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Isis))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameIsis", "이시스");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Anubis))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameAnubis", "아누비스");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Thoth))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameThoth", "토트");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Hathor))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameHathor", "하토르");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Sekhmet))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameSekhmet", "세크메트");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Nephthys))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameNephthys", "네프티스");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Ra))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameRa", "라");
	}
	if (GodTag.MatchesTagExact(MyGameplayTags::God_Set))
	{
		return NSLOCTEXT("ProjectPStreaming", "GodNameSet", "세트");
	}

	return DefaultGodName;
}

FString UMyStreamingManagerComponent::GetTagLeafName(FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return TEXT("Unknown");
	}

	const FString TagString = Tag.ToString();
	int32 LastDotIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), LastDotIndex))
	{
		return TagString.RightChop(LastDotIndex + 1);
	}

	return TagString;
}
