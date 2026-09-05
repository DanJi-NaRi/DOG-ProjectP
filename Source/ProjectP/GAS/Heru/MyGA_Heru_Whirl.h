////////////////////////////
//! \file MyGA_Heru_Whirl.h
//! \brief Heru의 회전 베기(칼날 스윕 다단 히트 + 표식 부여) 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Heru_Whirl.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Heru_Whirl
//! \author HanUl
//! \brief 시전자 중심 원형 범위를 수학적으로 회전하는 칼날이 쓸고 지나가는 순간에만 타격하는 스킬이다.
//!        타격마다 피해와 표식 1스택을 부여하며, 동일 대상은 ICD(한 바퀴 시간) 동안 다시 맞지 않는다.
//!        판정 러너는 어빌리티 수명과 분리되어 몽타주 조기 종료와 무관하게 전체 회전을 완주한다.
//! \note 반경(Targeting.Radius), 타당 계수(Effects.DamageCoefficient), 표식 GE(Effects.StatusGameplayEffect),
//!       바퀴당 시간=ICD(Timing.TickInterval), 쿨타임은 모두 SkillDefinition에서 온다.
UCLASS()
class PROJECTP_API UMyGA_Heru_Whirl : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Heru_Whirl();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

protected:
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	//! \brief 회전 바퀴 수. 총 지속시간 = TickInterval x 바퀴 수.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Whirl", meta = (ClampMin = "1"))
	int32 WhirlSpinCount = 3;

	//! \brief 칼날 스윕 판정의 서브틱 간격(초). 스윕 구간 검사라 값이 커도 누락은 없고 타격 시점 정밀도만 낮아진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Whirl", meta = (ClampMin = "0.01"))
	float SweepSubTickInterval = 0.05f;

	//! \brief 회전 방향 반전 여부(기본: +Yaw 방향). 애니메이션 회전 방향과 맞춘다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Whirl")
	bool bReverseSpinDirection = false;

	//! \brief 판정 반경/칼날 각도 디버그 시각화 여부(임시). 배포 전 제거 예정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Whirl|Debug")
	bool bDrawDebugWhirl = true;
};
