////////////////////////////
//! \page MyStreamingKillCountRuleTypes.h
//! \brief 파티의 몬스터 처치 누계와 Streaming Sequence를 연결하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingKillCountRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingKillCountRuleRow
//! \author 장효제
//! \brief 파티가 특정 몬스터를 몇 마리 잡았을 때 실행할 Sequence를 정의한다.
//! \details Kill 사실은 Combat Payload로 오며 UserIndex가 없다. 따라서 처치 수는
//!          플레이어별이 아니라 파티 누계로 센다. 보상 수령자도 파티 전원이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingKillCountRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 셀 몬스터다. 비우면 모든 적을, Character.Enemy.Boss면 보스만 센다.
	//! \details 계층 매칭이므로 Character.Enemy는 보스도 포함한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule", meta = (Categories = "Character"))
	FGameplayTag TargetTag;

	//! \brief 이 마릿수에 처음 도달한 순간 한 번 발동한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule", meta = (ClampMin = "1"))
	int32 RequiredKills = 1;

	//! \brief 0이면 던전 전체 누계, 0보다 크면 그 시간 안의 처치만 센다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule", meta = (ClampMin = "0.0"))
	float WindowSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|KillCountRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

////////////////////////////
//! \struct FMyStreamingKillRecord
//! \author 장효제
//! \brief 시간 창 Rule이 셀 처치 하나의 기록이다.
USTRUCT()
struct PROJECTP_API FMyStreamingKillRecord
{
	GENERATED_BODY()

	//! \brief 서버 기준 처치 시각이다.
	UPROPERTY()
	double ServerTimeSeconds = 0.0;

	//! \brief 처치당한 대상의 태그다. Payload가 준 값을 그대로 둔다.
	UPROPERTY()
	FGameplayTag TargetTag;
};

namespace MyStreamingKillCountRulePolicy
{
	//! \brief 시간 창을 쓰는 Rule인지 판정한다.
	inline bool IsWindowRule(const float WindowSeconds)
	{
		return WindowSeconds > 0.0f;
	}

	//! \brief 증가하는 처치 수가 Rule 문턱을 이번 처치에서 처음 통과했는지 반환한다.
	//! \details 누계 Rule과 시간 창 Rule이 같은 판정을 쓴다. 문턱을 넘은 뒤에도
	//!          매번 발동하면 같은 대사가 쏟아지므로 통과하는 순간만 잡는다.
	inline bool DidCrossThreshold(
		const int32 PreviousCount,
		const int32 NewCount,
		const int32 RequiredKills)
	{
		return RequiredKills > 0 && PreviousCount < RequiredKills && NewCount >= RequiredKills;
	}

	//! \brief Rule이 요구하는 대상 태그에 이 처치가 해당하는지 판정한다.
	//! \details 빈 Rule 태그는 모든 대상과 일치한다. 계층 매칭이므로
	//!          Character.Enemy Rule은 Character.Enemy.Boss 처치도 센다.
	inline bool DoesKillMatchTarget(const FGameplayTag KillTargetTag, const FGameplayTag RuleTargetTag)
	{
		return !RuleTargetTag.IsValid() || KillTargetTag.MatchesTag(RuleTargetTag);
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
