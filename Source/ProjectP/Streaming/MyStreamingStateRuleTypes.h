////////////////////////////
//! \page MyStreamingStateRuleTypes.h
//! \brief 상태가 일정 시간 유지될 때 Streaming Sequence를 실행하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "UObject/ObjectKey.h"
#include "MyStreamingStateRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingStateRuleRow
//! \author 장효제
//! \brief 어떤 상태가 정해진 시간 동안 이어지면 실행할 Sequence를 정의한다.
//! \details 사실(Fact)은 한 번 일어나고 끝이지만 상태는 켜짐과 꺼짐이 있다.
//!          상태가 켜지면 시간을 재기 시작하고, 꺼지면 재던 시간을 버린다.
//!          "전투 진입 후 n초"처럼 상태가 이어지는 것 자체를 조건으로 쓴다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingStateRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 재기 시작할 상태다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (Categories = "Streaming.State"))
	FGameplayTag StateTag;

	//! \brief 함께 켜져 있어야 하는 다른 상태다. 비우면 StateTag만 본다.
	//! \details 서로 다른 시스템이 각자 켜는 두 상태의 교집합을 조건으로 쓸 때 필요하다.
	//!          예로 "기믹이 켜져 있고 동시에 움직이지 않는" 상태가 그렇다.
	//!          이 열이 없으면 조합마다 전용 상태 태그를 새로 만들어야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (Categories = "Streaming.State"))
	FGameplayTag AndStateTag;

	//! \brief 상태가 이만큼 이어지면 발동한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (ClampMin = "0.1"))
	float HoldSeconds = 1.0f;

	//! \brief 이 사실이 오면 재던 시간을 처음부터 다시 잰다. 비우면 리셋하지 않는다.
	//! \details 상태와 사실 부재를 함께 쓰는 조건에 필요하다. 예로 "전투 중인데
	//!          n초 동안 공격하지 않았을 때"는 전투 상태를 재다가 공격 사실이 오면
	//!          다시 잰다. 이 열이 없으면 전투 코드가 미공격 여부까지 알아야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (Categories = "Streaming.Event"))
	FGameplayTag ResetOnEventTag;

	//! ResetOnEventTag에 걸리더라도 이 태그에 해당하면 리셋으로 치지 않는다.
	//! 사실의 EventTag와 SourceTag 어느 쪽이든 이 태그 아래면 제외한다.
	//! "스킬을 쓰면 리셋하되 점프는 빼고" 같은 조건이 이 열로 표현된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule")
	FGameplayTag ResetExcludeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|StateRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

////////////////////////////
//! \struct FMyStreamingStateHoldKey
//! \author 장효제
//! \brief 시간을 재는 단위다. 조건 묶음 하나와 그 상태를 켠 출처 하나의 짝이다.
//! \details 같은 상태를 켜는 객체가 여럿일 수 있다. 발판 두 개, 기믹 두 개,
//!          존 전환 중 겹치는 두 존이 그렇다. 출처를 구분하지 않으면 하나가
//!          꺼질 때 나머지가 켜져 있는데도 상태 전체가 꺼진 것으로 보인다.
//!          "한 발판을 10초 유지"도 출처별로 재야 뜻이 맞는다.
struct FMyStreamingStateHoldKey
{
	//! 같은 조건을 가진 Rule 묶음의 대표 RowName이다.
	FName GroupId;
	//! 그 상태를 켜고 있는 객체다.
	FObjectKey SourceKey;

	bool operator==(const FMyStreamingStateHoldKey& Other) const
	{
		return GroupId == Other.GroupId && SourceKey == Other.SourceKey;
	}
};

inline uint32 GetTypeHash(const FMyStreamingStateHoldKey& Key)
{
	return HashCombine(GetTypeHash(Key.GroupId), GetTypeHash(Key.SourceKey));
}

namespace MyStreamingStateRulePolicy
{
	//! \brief 상태가 Rule이 요구하는 상태에 해당하는지 판정한다.
	//! \details 빈 Rule 태그는 아무 상태와도 짝짓지 않는다. 사실 조건과 달리
	//!          "모든 상태"라는 뜻이 성립하지 않기 때문이다.
	inline bool DoesStateMatch(const FGameplayTag ActiveState, const FGameplayTag RuleState)
	{
		return RuleState.IsValid() && ActiveState.MatchesTag(RuleState);
	}

	//! \brief 유지 시간으로 쓸 수 있는 값인지 판정한다.
	inline bool IsHoldSecondsValid(const float Seconds)
	{
		return FMath::IsFinite(Seconds) && Seconds > 0.0f;
	}

	//! \brief 두 Rule이 같은 조건을 재는지 판정한다.
	//! \details 조건이 같은 Rule들은 한 묶음으로 시간을 재고, 발동 순간에
	//!          Weight로 하나만 고른다. 묶지 않으면 후보가 여럿일 때 모두
	//!          따로 발동해 Weight가 아무 뜻도 갖지 못한다.
	inline bool HaveSameCondition(
		const FMyStreamingStateRuleRow& Left,
		const FMyStreamingStateRuleRow& Right)
	{
		return Left.StateTag == Right.StateTag
			&& Left.AndStateTag == Right.AndStateTag
			&& Left.ResetOnEventTag == Right.ResetOnEventTag
			&& Left.ResetExcludeTag == Right.ResetExcludeTag
			&& FMath::IsNearlyEqual(Left.HoldSeconds, Right.HoldSeconds);
	}

	//! \brief 방금 온 사실이 이 Rule의 시간을 다시 재게 하는지 판정한다.
	//! \details ResetOnEventTag에 걸려도 ResetExcludeTag에 해당하면 리셋하지 않는다.
	//!          사실 하나가 여러 뜻을 겸할 때 쓴다. 점프도 스킬 사용 사실로 오지만
	//!          "이동만 했을 때"에서는 스킬을 쓴 것으로 치지 않는다.
	//! \param EventTag 방금 일어난 사실이다.
	//! \param SourceTag 그 사실을 더 좁게 가리키는 태그다. 없으면 비운다.
	//! \param ResetOnEventTag 리셋으로 삼을 사실이다.
	//! \param ResetExcludeTag 리셋에서 뺄 태그다. 비우면 아무것도 빼지 않는다.
	//! \return 시간을 다시 재야 하면 true다.
	inline bool ShouldResetOnEvent(
		const FGameplayTag EventTag,
		const FGameplayTag SourceTag,
		const FGameplayTag ResetOnEventTag,
		const FGameplayTag ResetExcludeTag)
	{
		if (!ResetOnEventTag.IsValid() || !EventTag.MatchesTag(ResetOnEventTag))
		{
			return false;
		}
		if (!ResetExcludeTag.IsValid())
		{
			return true;
		}
		return !EventTag.MatchesTag(ResetExcludeTag)
			&& !SourceTag.MatchesTag(ResetExcludeTag);
	}

	//! \brief 이 Rule이 지금 시간을 재도 되는 상태인지 판정한다.
	//! \param bStateActive StateTag 상태가 켜져 있는지다.
	//! \param bAndStateRequired AndStateTag가 지정되어 있는지다.
	//! \param bAndStateActive AndStateTag 상태가 켜져 있는지다.
	//! \return 두 상태 조건을 모두 만족하면 true다.
	inline bool ShouldHold(
		const bool bStateActive,
		const bool bAndStateRequired,
		const bool bAndStateActive)
	{
		return bStateActive && (!bAndStateRequired || bAndStateActive);
	}
}
