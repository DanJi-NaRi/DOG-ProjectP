// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemyDashAttackAbility.generated.h"

class ACPP_EnemyBase;
class ACPP_EnemyTelegraphActor;
class UAbilitySystemComponent;
class UAbilityTask_WaitGameplayEvent;
class UCPP_AbilityTask_EnemyDash;
class UCPP_EnemyAttackPatternData;
class UGameplayEffect;
struct FGameplayEventData;

UCLASS()
class PROJECTP_API UCPP_EnemyDashAttackAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
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

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	bool StartDashFromNotify(ACPP_EnemyBase* EnemyAvatar);
	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

private:
	UFUNCTION()
	void HandleDashFinished(const TArray<AActor*>& HitPawns, bool bHitWall);

	UFUNCTION()
	void HandleDashWallStunFinished();

	UFUNCTION()
	void HandleTelegraphBeginEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTelegraphEndEvent(FGameplayEventData Payload);

	void SetupTelegraphEventListeners();
	void SpawnDashTelegraph(float TelegraphDuration);
	void DestroyDashTelegraph();
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool ApplyDashHitToActor(
		ACPP_EnemyBase* EnemyAvatar,
		AActor* HitActor,
		const UCPP_EnemyAttackPatternData* AttackPattern
	);
	bool ApplyStatusEffectToTarget(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> StatusGameplayEffect,
		const FGameplayEffectContextHandle& EffectContext
	);

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	UPROPERTY(Transient)
	TObjectPtr<UCPP_AbilityTask_EnemyDash> ActiveDashTask;
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphBeginEventTask;
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphEndEventTask;
	TWeakObjectPtr<ACPP_EnemyTelegraphActor> ActiveDashTelegraph;
	bool bHasStartedDash = false;
};
