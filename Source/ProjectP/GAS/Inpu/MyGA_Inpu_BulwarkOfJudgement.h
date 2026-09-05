////////////////////////////
//! \file MyGA_Inpu_BulwarkOfJudgement.h
//! \brief Inpu의 심판의 방벽 궁극기 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Inpu_BulwarkOfJudgement.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Inpu_BulwarkOfJudgement
//! \author HanUl
//! \brief 플레이어를 따라다니는 방패 돔을 세워 범위 내 아군에게 보호막을 부여하고, 유지 후 원형 강타로 적을 타격하는 궁극기다.
//! \note 반경(Targeting.Radius)/돔 유지시간(Timing.ActiveDuration)/강타 계수(Effects.DamageCoefficient)/보호막 GE(Effects.BuffGameplayEffect)/돔 액터(Area.AreaClass)는
//!       SkillDefinition에서, 보호막 비율·보호막당 추가 계수는 UMyBulwarkOfJudgementFragment에서 온다.
//!       판정과 효과는 서버에서만 수행하고, 러너는 어빌리티 수명과 분리되어 강타까지 완주한다.
UCLASS()
class PROJECTP_API UMyGA_Inpu_BulwarkOfJudgement : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Inpu_BulwarkOfJudgement();

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

	//! \brief true면 서버가 판정한 강타 반경을 소유자 화면에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Bulwark Of Judgement|Debug")
	bool bDrawDebugBulwarkOfJudgement = true;

	//! \brief 디버그 도형 표시 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Bulwark Of Judgement|Debug", meta = (ClampMin = "0.0"))
	float DebugShapeLifeTime = 1.0f;
};
