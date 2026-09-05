////////////////////////////
//! \page MyStreamingCombatRuleTypes.h
//! \brief Streaming System 전투 조건 테이블에서 사용하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.generated.h"

////////////////////////////
//! \enum EMyStreamingRuleBoolMatch
//! \author 장효제
//! \brief 전투 Rule에서 bool Payload 값을 비교하는 방식이다.
UENUM(BlueprintType)
enum class EMyStreamingRuleBoolMatch : uint8
{
	Any UMETA(DisplayName = "Any"),
	MatchTrue UMETA(DisplayName = "True"),
	MatchFalse UMETA(DisplayName = "False")
};

////////////////////////////
//! \enum EMyStreamingSequenceBusyPolicy
//! \author 장효제
//! \brief 다른 채팅 시퀀스가 재생 중일 때 새 요청을 처리하는 방식을 정의한다.
UENUM(BlueprintType)
enum class EMyStreamingSequenceBusyPolicy : uint8
{
	Drop,		// 재생 중이면 폐기
	Queue		// 재생 중이면 끝난 뒤에 재생
};


////////////////////////////
//! \struct FMyStreamingCombatRuleRow
//! \author 장효제
//! \brief 전투 Payload를 SequenceId로 매칭하기 위한 DataTable 행 구조체다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCombatRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//--- 누가 누구를 어떤 이벤트로 스킬을 때렸는가? ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (Categories = "Streaming.Event.Combat"))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (Categories = "Character"))
	FGameplayTag InstigatorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (Categories = "Character"))
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	//--- 데미지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "-1.0"))
	float MinDamageAmount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "-1.0"))
	float MaxDamageAmount = -1.0f;

	//--- 타겟의 hp 비율

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinTargetCurrentHPRatio = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MaxTargetCurrentHPRatio = -1.0f;

	//--- 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	EMyStreamingRuleBoolMatch CriticalMatch = EMyStreamingRuleBoolMatch::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	EMyStreamingRuleBoolMatch KillMatch = EMyStreamingRuleBoolMatch::Any;

	//--- 리스폰스 아이디 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	FName SequenceId;

	//---가중치와 쿨타임.  

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	//--- 시퀀스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;

};

////////////////////////////
//! \struct FMyStreamingCombatRuleBucket
//! \author 장효제
//! \brief EventTag별로 미리 분류된 전투 Rule 묶음이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCombatRuleBucket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	TArray<FMyStreamingCombatRuleRow> Rules;
};

////////////////////////////
//! \struct FMyStreamingCombatRuleMatchResult
//! \author 장효제
//! \brief 전투 Rule 매칭 결과로 선택된 SequenceId와 원본 Rule 정보를 담는다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingCombatRuleMatchResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	bool bMatched = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|CombatRule")
	FMyStreamingCombatRuleRow MatchedRule;
};

