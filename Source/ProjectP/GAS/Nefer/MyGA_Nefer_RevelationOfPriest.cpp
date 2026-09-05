// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGA_Nefer_RevelationOfPriest.h"

#include "../../MyGameplayTags.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanUl
//! \brief Revelation Of Priest Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGA_Nefer_RevelationOfPriest::UMyGA_Nefer_RevelationOfPriest()
{
	AbilityTag = MyGameplayTags::Skill_Nefer_RevelationOfPriest;
	InputTag = MyGameplayTags::Input_Skill_R;
	CooldownTag = MyGameplayTags::Cooldown_Skill_NFR_SK_R;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief Definition에 Revelation 즉시 장판에 필요한 효과와 수치가 있는지 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 발동 가능하면 true
bool UMyGA_Nefer_RevelationOfPriest::CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	(void)ActorInfo;

	const FMySkillEffectSpec& Effects = Context.SkillData.Effects;
	if (!Effects.HitGameplayEffect || !Effects.StatusGameplayEffect)
	{
		/*UE_LOG(LogTemp, Warning, TEXT("Nefer revelation activation failed - GE is invalid. Definition: %s, HitGE: %s, StatusGE: %s"),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(Effects.HitGameplayEffect),
			*GetNameSafe(Effects.StatusGameplayEffect));*/
		return false;
	}

	if (Effects.DamageCoefficient <= 0.0f)
	{
		/*UE_LOG(LogTemp, Warning, TEXT("Nefer revelation activation failed - damage coefficient is invalid. Definition: %s, DamageCoefficient: %.2f"),
			*GetNameSafe(GetActiveSkillDefinition()),
			Effects.DamageCoefficient);*/
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Revelation 생성 즉시 범위 내 대상에게 즉시 피해와 Decay를 적용한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_RevelationOfPriest::ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context)
{
	TArray<AActor*> Targets;
	CollectValidAreaTargets(Context, Targets);

	// 즉시 피해는 계수만 전달하고 공격력 곱셈은 ExecCalc가 담당한다. Decay 틱은 상태 GE라 완성값(Data.Damage)을 유지한다.
	const float DamageCoefficient = Context.SkillData.Effects.DamageCoefficient;
	const float DecayTickDamage = GetSourceAttackPower(Context) * FMath::Max(Context.SkillData.Effects.StatusDamageCoefficient, 0.0f);

	int32 DamageAppliedCount = 0;
	int32 DecayAppliedCount = 0;
	for (AActor* TargetActor : Targets)
	{
		if (ApplyDamageGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.HitGameplayEffect, DamageCoefficient))
		{
			++DamageAppliedCount;
		}

		if (ApplyStatusGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.StatusGameplayEffect, DecayTickDamage))
		{
			++DecayAppliedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Nefer revelation applied - InstanceId: %d, Location: %s, Radius: %.2f, Targets: %d, DamageCoefficient: %.2f, DecayTickDamage: %.2f, DamageApplied: %d, DecayApplied: %d"),
		Context.AreaInstanceId,
		*Context.AreaSpec.TargetLocation.ToCompactString(),
		Context.AreaSpec.Radius,
		Targets.Num(),
		DamageCoefficient,
		DecayTickDamage,
		DamageAppliedCount,
		DecayAppliedCount);
}
