////////////////////////////
//! \page MyStreamingCountRuleTypes.h
//! \brief 파티 사건 누계·아이템 누계와 Streaming Sequence를 연결하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingCountRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingCountRuleRow
//! \author 장효제
//! \brief 파티가 어떤 사건을 몇 번 겪었을 때 실행할 Sequence를 정의한다.
//! \details 항아리 파괴·플레이어 사망·도네이션 수령처럼 UserIndex를 지목할 수 없거나
//!          지목이 뜻이 없는 사건을 담는다. 그래서 보상 수령자는 파티 전원이다.
//!          아이템은 개인 지목이 가능하고 식별자가 태그가 아니라 RowName이라
//!          FMyStreamingItemRuleRow가 따로 담는다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCountRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 셀 사건이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule", meta = (Categories = "Streaming.Event"))
	FGameplayTag EventTag;

	//! \brief 사건을 더 좁히는 태그다. 비우면 그 사건 전부를 센다.
	//! \details 도네이션에서는 어떤 신이 줬는지를 가리킨다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule")
	FGameplayTag SourceTag;

	//! \brief 이 횟수에 처음 도달한 순간 한 번 발동한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	//! \brief 0이면 던전 전체 누계, 0보다 크면 그 시간 안의 사건만 센다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule", meta = (ClampMin = "0.0"))
	float WindowSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CountRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

////////////////////////////
//! \struct FMyStreamingItemRuleRow
//! \author 장효제
//! \brief 파티가 어떤 아이템을 몇 개 쓰거나 샀을 때 실행할 Sequence를 정의한다.
//! \details 아이템 사건은 인벤토리가 PlayerState 소유라 UserIndex를 알 수 있다.
//!          그래서 사건을 일으킨 개인에게 보상할 수 있다.
//!          식별자는 GameplayTag가 아니라 DT_ItemTable의 RowName이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingItemRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 셀 사건이다. 사용과 구매를 가른다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule", meta = (Categories = "Streaming.Event.Item"))
	FGameplayTag EventTag;

	//! \brief 셀 아이템의 DT_ItemTable RowName이다. 비우면 모든 아이템을 센다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule")
	FName ItemId;

	//! \brief 이 개수에 처음 도달한 순간 한 번 발동한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	//! \brief 0이면 던전 전체 누계, 0보다 크면 그 시간 안의 사건만 센다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule", meta = (ClampMin = "0.0"))
	float WindowSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ItemRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

////////////////////////////
//! \struct FMyStreamingCountRecord
//! \author 장효제
//! \brief 파티 사건 하나의 기록이다. 누계 Rule과 시간 창 Rule이 함께 쓴다.
USTRUCT()
struct PROJECTP_API FMyStreamingCountRecord
{
	GENERATED_BODY()

	//! \brief 서버 기준 발생 시각이다.
	UPROPERTY()
	double ServerTimeSeconds = 0.0;

	//! \brief 사건 태그다. 아이템 사건에서는 사용/구매를 가른다.
	UPROPERTY()
	FGameplayTag EventTag;

	//! \brief 사건을 좁히는 태그다. 아이템 사건에서는 쓰지 않는다.
	UPROPERTY()
	FGameplayTag SourceTag;

	//! \brief 아이템 사건의 대상 RowName이다. 그 외 사건에서는 비어 있다.
	UPROPERTY()
	FName ItemId;

	//! \brief 이 기록이 올린 양이다. 아이템은 한 번에 여러 개가 오를 수 있다.
	UPROPERTY()
	int32 Amount = 1;
};

namespace MyStreamingCountRulePolicy
{
	//! \brief 시간 창을 쓰는 Rule인지 판정한다.
	inline bool IsWindowRule(const float WindowSeconds)
	{
		return WindowSeconds > 0.0f;
	}

	//! \brief 증가하는 횟수가 Rule 문턱을 이번 사건에서 처음 통과했는지 반환한다.
	//! \details 처치 누계·스킬 사용과 같은 판정이다. 문턱을 넘은 뒤에도 매번
	//!          발동하면 같은 대사가 쏟아지므로 통과하는 순간만 잡는다.
	inline bool DidCrossThreshold(
		const int32 PreviousCount,
		const int32 NewCount,
		const int32 RequiredCount)
	{
		return RequiredCount > 0 && PreviousCount < RequiredCount && NewCount >= RequiredCount;
	}

	//! \brief 사건이 Rule이 요구하는 태그에 해당하는지 판정한다.
	//! \details 빈 Rule 태그는 모든 값과 일치한다. 계층 매칭을 쓴다.
	inline bool DoesTagMatch(const FGameplayTag EventValue, const FGameplayTag RuleValue)
	{
		return !RuleValue.IsValid() || EventValue.MatchesTag(RuleValue);
	}

	//! \brief 아이템 사건이 Rule이 요구하는 아이템에 해당하는지 판정한다.
	//! \details 빈 Rule ItemId는 모든 아이템과 일치한다. RowName은 계층이 없어
	//!          완전일치로만 비교한다.
	inline bool DoesItemMatch(const FName EventItemId, const FName RuleItemId)
	{
		return RuleItemId.IsNone() || EventItemId == RuleItemId;
	}

	//! \brief 시간 창 안에 남아 있는 기록인지 판정한다.
	inline bool IsWithinWindow(
		const double RecordTimeSeconds,
		const double NowSeconds,
		const float WindowSeconds)
	{
		return NowSeconds - RecordTimeSeconds <= static_cast<double>(WindowSeconds);
	}
}
