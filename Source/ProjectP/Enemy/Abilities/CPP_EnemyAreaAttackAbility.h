// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemyAreaAttackAbility.generated.h"

class ACPP_EnemyBase;
class ACPP_EnemyLobProjectileVisual;
class UCPP_EnemyAttackPatternData;

UCLASS()
class PROJECTP_API UCPP_EnemyAreaAttackAbility : public UMyGameplayAbilityBase
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

	bool TriggerAreaFromNotify(ACPP_EnemyBase* EnemyAvatar);
	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

private:
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool SpawnArea(
		ACPP_EnemyBase* EnemyAvatar,
		const UCPP_EnemyAttackPatternData* AttackPattern
	);
	void SpawnLobProjectileVisual(
		ACPP_EnemyBase* EnemyAvatar,
		const UCPP_EnemyAttackPatternData* AttackPattern,
		const FVector& EndLocation,
		float ServerStartTime
	) const;

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	bool bHasTriggeredArea = false;
};
