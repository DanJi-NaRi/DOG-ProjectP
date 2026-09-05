// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemyBlinkNovaAbility.generated.h"

class ACPP_EnemyBase;
class ACPP_EnemyTelegraphActor;
class UAbilitySystemComponent;
class UCPP_AbilityTask_EnemyBlink;
class UCPP_EnemyAttackPatternData;
class UGameplayEffect;

////////////////////////////
//! \class UCPP_EnemyBlinkNovaAbility
//! \brief 점멸 자폭(MON_SLIME_001): 순간이동(UCPP_AbilityTask_EnemyBlink — 소멸→후방 재등장→무적) 후 재등장 시점부터
//!        ExplodeDelay 후 반경 ExplodeRadius 원형 폭발(패턴 HitGE × DamageCoefficient, 전원) → 즉시 사망(ForceKill).
//! \note  몽타주 없이 구동(스펙이 절대 시간 기준). 순간이동/무적은 공용 태스크가 담당. 연출은 GameplayCue 태그로 재생.
//!        폭발 전에 죽으면(무적 종료~폭발 사이) 폭발 없이 정리 — 카운터플레이.
UCLASS()
class PROJECTP_API UCPP_EnemyBlinkNovaAbility : public UMyGameplayAbilityBase
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

protected:
	//! \brief 소멸 유지 시간(초). 이후 재등장.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova", meta = (ClampMin = "0.0"))
	float VanishDuration = 0.8f;

	//! \brief 재등장 시 타겟 후방으로 벌릴 캡슐 표면 간 간격(cm). 중심 거리 = 이 값 + 양쪽 캡슐 반경.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova", meta = (ClampMin = "0.0"))
	float ReappearBehindGap = 50.0f;

	//! \brief 재등장 직후 무적 시간(초). ExplodeDelay 안에 포함된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova", meta = (ClampMin = "0.0"))
	float InvincibleDuration = 0.15f;

	//! \brief 재등장부터 폭발까지 시간(초). 텔레그래프 채움 시간으로도 사용.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova", meta = (ClampMin = "0.0"))
	float ExplodeDelay = 1.0f;

	//! \brief 폭발 반경(cm). 자신 중심 원형, 범위 내 적대 폰 전원 타격.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova", meta = (ClampMin = "0.0"))
	float ExplodeRadius = 200.0f;

	//! \brief 소멸/재등장/폭발 연출용 GameplayCue 태그(선택). 비우면 생략.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova|Cue")
	FGameplayTag VanishCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova|Cue")
	FGameplayTag ReappearCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkNova|Cue")
	FGameplayTag ExplodeCueTag;

private:
	UFUNCTION()
	void HandleBlinkFinished();

	UFUNCTION()
	void HandleExplodeTimeReached();

	void ExecuteExplosion(ACPP_EnemyBase* EnemyAvatar);
	bool ApplyStatusEffectToTarget(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> StatusGameplayEffect
	) const;
	void SpawnExplodeTelegraph(ACPP_EnemyBase* EnemyAvatar);
	void DestroyExplodeTelegraph();
	void ExecuteCosmeticCue(const FGameplayTag& CueTag) const;
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	UPROPERTY(Transient)
	TObjectPtr<UCPP_AbilityTask_EnemyBlink> ActiveBlinkTask;
	TWeakObjectPtr<ACPP_EnemyTelegraphActor> ActiveExplodeTelegraph;
};
