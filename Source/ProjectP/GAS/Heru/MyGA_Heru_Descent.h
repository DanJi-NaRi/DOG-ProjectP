////////////////////////////
//! \file MyGA_Heru_Descent.h
//! \brief Heru의 자기 강화 + 발동 시 전체 쿨 초기화 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Heru_Descent.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Heru_Descent
//! \author HanUl
//! \brief 시전(슈퍼아머) 후 자기 자신에게 버프 GE를 적용하고, 발동 시 1회 자신(C)을 제외한
//!        Heru 모든 스킬의 쿨타임을 즉시 초기화한다.
//!        Definition에 원형 타격 데이터가 채워져 있으면(레벨2) 시전 지점 원형 범위 1회 타격을 추가로 수행한다.
//! \note 버프 내용(+피해/+이속)은 Effects.BuffGameplayEffect(GE 에셋)가, 지속시간은 Timing.ActiveDuration이,
//!       시전 시간은 Timing.CastTime이 결정한다.
//!       원형 타격은 Targeting.Radius / Effects.HitGameplayEffect / Effects.DamageCoefficient가 모두 유효할 때만 발동하므로,
//!       레벨1 Definition은 이 값들을 비워 두고 레벨2 Definition에서 채워 타격을 추가한다(어빌리티는 레벨을 직접 보지 않는다).
UCLASS()
class PROJECTP_API UMyGA_Heru_Descent : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Heru_Descent();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillCommitted(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	//! \brief true면 원형 타격 판정 범위를 소유자 화면에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Descent|Debug")
	bool bDrawDebugDescentImpact = true;

	//! \brief 디버그 도형 표시 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Descent|Debug", meta = (ClampMin = "0.0"))
	float DebugShapeLifeTime = 1.0f;

private:
	//! \brief 시전 동안 부여한 슈퍼아머 태그를 제거한다.
	void RemoveSuperArmorTag(const FGameplayAbilityActorInfo* ActorInfo);

	//! \brief Definition에 원형 타격 데이터가 있을 때만 시전 지점 원형 범위 적에게 1회 피해를 적용한다(서버 전용).
	void ApplyDescentImpact(const FGameplayAbilityActorInfo* ActorInfo, const FMySkillDataEntry& SkillData) const;

	//! \brief 시전 슈퍼아머 태그를 부여했는지 여부(중복 제거 방지).
	bool bSuperArmorApplied = false;
};
