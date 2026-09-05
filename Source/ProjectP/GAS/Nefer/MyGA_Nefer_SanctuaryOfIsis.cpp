// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGA_Nefer_SanctuaryOfIsis.h"

#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "../../MyGameplayTags.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"

////////////////////////////
//! \author HanUl
//! \brief Sanctuary Of Isis Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGA_Nefer_SanctuaryOfIsis::UMyGA_Nefer_SanctuaryOfIsis()
{
	AbilityTag = MyGameplayTags::Skill_Nefer_SanctuaryOfIsis;
	InputTag = MyGameplayTags::Input_Skill_E;
	CooldownTag = MyGameplayTags::Cooldown_Skill_NFR_SK_E;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 Ability가 취소되면 슬로우 부여 기록만 정리한다.
//!        이미 부여한 슬로우 GE는 자체 지속시간이 끝날 때까지 유지해야 하므로 제거하지 않는다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Nefer_SanctuaryOfIsis::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	if (bWasCancelled)
	{
		SlowedTargetsByArea.Empty();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief Definition에 지속 피해 장판에 필요한 값이 있는지 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 발동 가능하면 true
bool UMyGA_Nefer_SanctuaryOfIsis::CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	(void)ActorInfo;

	if (!Context.SkillData.Effects.HitGameplayEffect || Context.SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer sanctuary activation failed - damage data is invalid. Definition: %s, HitGE: %s, Coefficient: %.2f"),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(Context.SkillData.Effects.HitGameplayEffect),
			Context.SkillData.Effects.DamageCoefficient);
		return false;
	}

	if (Context.AreaSpec.Duration <= 0.0f || Context.AreaSpec.TickInterval <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer sanctuary activation failed - timing is invalid. Definition: %s, Duration: %.2f, TickInterval: %.2f"),
			*GetNameSafe(GetActiveSkillDefinition()),
			Context.AreaSpec.Duration,
			Context.AreaSpec.TickInterval);
		return false;
	}

	if (!Context.SkillData.Effects.StatusGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer sanctuary activation warning - status GameplayEffect is null, slow will not be applied. Definition: %s"),
			*GetNameSafe(GetActiveSkillDefinition()));
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Sanctuary는 지속 피해 장판이므로 Tick을 예약한다.
//! \param Context 장판 런타임 컨텍스트
//! \return true
bool UMyGA_Nefer_SanctuaryOfIsis::ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const
{
	(void)Context;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 범위 안 적에게 지속 피해를 적용하고, 아직 슬로우가 걸리지 않은 대상에게 슬로우를 1회 부여한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_SanctuaryOfIsis::ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context)
{
	TArray<AActor*> Targets;
	CollectValidAreaTargets(Context, Targets);

	TSet<TWeakObjectPtr<AActor>>& SlowedTargets = SlowedTargetsByArea.FindOrAdd(Context.AreaInstanceId);

	int32 DamageAppliedCount = 0;
	int32 SlowAppliedCount = 0;
	for (AActor* TargetActor : Targets)
	{
		if (ApplyDamageGameplayEffectToTarget(
				Context,
				TargetActor,
				Context.SkillData.Effects.HitGameplayEffect,
				Context.SkillData.Effects.DamageCoefficient))
		{
			++DamageAppliedCount;
		}

		// 슬로우는 대상별 최초 1회만 부여한다. 지속시간은 슬로우 GE가 관리하므로 장판이 끝나도 남은 시간만큼 유지된다.
		const TWeakObjectPtr<AActor> TargetKey(TargetActor);
		if (Context.SkillData.Effects.StatusGameplayEffect && !SlowedTargets.Contains(TargetKey))
		{
			if (ApplyStatusGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.StatusGameplayEffect, 0.0f))
			{
				SlowedTargets.Add(TargetKey);
				++SlowAppliedCount;
			}
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("Nefer sanctuary tick - InstanceId: %d, Location: %s, Radius: %.2f, Targets: %d, Damaged: %d, Slowed: %d"),
		Context.AreaInstanceId,
		*Context.AreaSpec.TargetLocation.ToCompactString(),
		Context.AreaSpec.Radius,
		Targets.Num(),
		DamageAppliedCount,
		SlowAppliedCount);
}

////////////////////////////
//! \author HanUl
//! \brief 장판 종료 시 슬로우 부여 기록만 정리한다. 부여된 슬로우 GE는 자체 지속시간까지 유지된다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_SanctuaryOfIsis::ApplyAreaEndEffects(const FMyAreaSkillRuntimeContext& Context)
{
	SlowedTargetsByArea.Remove(Context.AreaInstanceId);
	Super::ApplyAreaEndEffects(Context);
}
