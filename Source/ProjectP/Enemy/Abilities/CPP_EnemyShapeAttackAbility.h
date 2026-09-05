// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemyShapeAttackAbility.generated.h"

class ACPP_BossTelegraphActor;
class ACPP_EnemyBase;
class UAbilitySystemComponent;
class UAbilityTask_WaitGameplayEvent;
class UCPP_EnemyAttackPatternData;
class UGameplayEffect;
struct FBossAttackWindowData;
struct FBossHitShapeData;
struct FGameplayEventData;

////////////////////////////
//! \class UCPP_EnemyShapeAttackAbility
//! \brief 일반 적 모양(원/부채꼴/사각) 커스텀 공격. 보스 일반 패턴의 윈도우 이벤트 + 모양 판정 + 텔레그래프 방식을
//!        적 패턴 데이터(AttackWindows)와 적 공통 흐름(멀티캐스트 몽타주, FinishAbilityFromMontage 종료)에 이식했다.
//!        몽타주의 EnemyAttackWindow 노티파이가 판정을, EnemyTelegraph 노티파이 구간이 텔레그래프 표시를 구동한다.
UCLASS()
class PROJECTP_API UCPP_EnemyShapeAttackAbility : public UMyGameplayAbilityBase
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

	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

private:
	UFUNCTION()
	void HandleAttackWindowEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTelegraphBeginEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTelegraphEndEvent(FGameplayEventData Payload);

	bool SetupWindowEventListeners();
	const FBossAttackWindowData* FindAttackWindow(const UCPP_EnemyAttackPatternData* AttackPattern, FName WindowId) const;
	void CollectTargetsFromHitShape(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromCircle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromSector(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromRectangle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void ApplyDamageToTargets(ACPP_EnemyBase* EnemyAvatar, const FBossAttackWindowData& AttackWindow, const TSet<AActor*>& Targets) const;
	bool ApplyStatusEffectToTarget(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> StatusGameplayEffect
	) const;
	void SpawnTelegraphsForWindow(FName WindowId, float TelegraphDuration);
	void RemoveTelegraphsForWindow(FName WindowId);
	void ClearActiveTelegraphs();
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveAttackWindowEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphBeginEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphEndEventTask;

	TMap<FName, TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>> ActiveTelegraphActorsByWindow;
	bool bHasCommittedAttack = false;
};
