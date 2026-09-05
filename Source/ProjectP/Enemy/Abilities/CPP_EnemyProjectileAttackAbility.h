// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemyProjectileAttackAbility.generated.h"

class ACPP_EnemyBase;
class UCPP_EnemyAttackPatternData;

UCLASS()
class PROJECTP_API UCPP_EnemyProjectileAttackAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	bool FireProjectileFromNotify(ACPP_EnemyBase* EnemyAvatar);
	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

private:
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool SpawnProjectile(
		ACPP_EnemyBase* EnemyAvatar,
		AActor* TargetActor,
		const UCPP_EnemyAttackPatternData* AttackPattern
	);

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	bool bHasFiredProjectile = false;
};
