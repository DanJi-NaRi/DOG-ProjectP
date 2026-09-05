////////////////////////////
//! \file MyGA_Inpu_AegisVortex.h
//! \brief Inpu의 이지스 소용돌이 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Inpu_AegisVortex.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Inpu_AegisVortex
//! \author HanUl
//! \brief 캐릭터 중심 반경에 여러 번의 원형 파동을 일으켜 일반 몬스터를 당기거나 밀어내고, 마지막에 지면을 강타하는 스킬이다.
//! \note 당김/밀어냄은 UMyAegisVortexFragment의 MoveMode가 결정하므로, 같은 어빌리티로 흡입형(E)과 밀어내기형(R)을
//!       Definition만 달리해 함께 쓸 수 있다.
//!       파동 반경(Targeting.Radius)/지속(Timing.ActiveDuration)/파동 피해(Effects.DamageCoefficient)/피니셔 피해(Effects.SecondaryDamageCoefficient)는
//!       SkillDefinition에서, 파동 횟수·이동 방향/거리·피니셔 반경은 UMyAegisVortexFragment에서 온다.
//!       판정과 효과는 서버에서만 수행하고, 파동 러너는 어빌리티 수명과 분리되어 몽타주 조기 종료와 무관하게 완주한다.
UCLASS()
class PROJECTP_API UMyGA_Inpu_AegisVortex : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Inpu_AegisVortex();

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

	//! \brief true면 서버가 판정한 파동·피니셔 반경을 소유자 화면에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Aegis Vortex|Debug")
	bool bDrawDebugAegisVortex = true;

	//! \brief 디버그 도형 표시 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Aegis Vortex|Debug", meta = (ClampMin = "0.0"))
	float DebugShapeLifeTime = 0.5f;
};
