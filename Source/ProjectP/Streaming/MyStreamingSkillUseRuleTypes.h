////////////////////////////
//! \page MyStreamingSkillUseRuleTypes.h
//! \brief 파티의 스킬 사용 누계·미사용 시간과 Streaming Sequence를 연결하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingSkillUseRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingSkillUseRuleRow
//! \author 장효제
//! \brief 파티가 특정 스킬을 몇 번 썼을 때, 또는 얼마 동안 쓰지 않았을 때 실행할 Sequence를 정의한다.
//! \details 스킬 사용 사실은 Combat Payload로 오며 UserIndex가 없다. 따라서 사용 횟수는
//!          플레이어별이 아니라 파티 누계로 센다. 보상 수령자도 파티 전원이다.
//!          IdleSeconds가 0이면 누적 사용 Rule이고, 0보다 크면 미사용 Rule이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingSkillUseRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 셀 스킬이다. 계층 매칭이므로 Skill.Common.Jump는 하위 태그를 모두 포함한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	//! \brief 누적 사용 Rule이 요구하는 사용 횟수다. 미사용 Rule에서는 쓰지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule", meta = (ClampMin = "1"))
	int32 RequiredUses = 1;

	//! \brief 0이면 누적 사용 Rule이고, 0보다 크면 그 시간 동안 쓰지 않았을 때 발동한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule", meta = (ClampMin = "0.0"))
	float IdleSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|SkillUseRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

namespace MyStreamingSkillUseRulePolicy
{
	//! \brief 미사용 시간을 재는 Rule인지 판정한다.
	inline bool IsIdleRule(const float IdleSeconds)
	{
		return IdleSeconds > 0.0f;
	}

	//! \brief 증가하는 사용 횟수가 Rule 문턱을 이번 사용에서 처음 통과했는지 반환한다.
	//! \details 처치 누계와 같은 판정이다. 문턱을 넘은 뒤에도 매번 발동하면
	//!          같은 대사가 쏟아지므로 통과하는 순간만 잡는다.
	inline bool DidCrossThreshold(
		const int32 PreviousCount,
		const int32 NewCount,
		const int32 RequiredUses)
	{
		return RequiredUses > 0 && PreviousCount < RequiredUses && NewCount >= RequiredUses;
	}

	//! \brief Rule이 요구하는 스킬에 이번 사용이 해당하는지 판정한다.
	//! \details 빈 Rule 태그는 모든 스킬과 일치한다. 계층 매칭이므로
	//!          Skill.Common.Jump Rule은 제자리 점프와 이동 점프를 모두 센다.
	inline bool DoesUseMatchSkill(const FGameplayTag UsedSkillTag, const FGameplayTag RuleSkillTag)
	{
		return !RuleSkillTag.IsValid() || UsedSkillTag.MatchesTag(RuleSkillTag);
	}
}
