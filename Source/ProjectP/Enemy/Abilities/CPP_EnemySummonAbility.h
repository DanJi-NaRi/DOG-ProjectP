// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_EnemySummonAbility.generated.h"

class ACPP_EnemyBase;

////////////////////////////
//! \class UCPP_EnemySummonAbility
//! \brief 소환 패턴: 생존 플레이어마다 주변 반경 내 무작위 위치에 일반 적을 SummonsPerPlayer마리 소환한다.
//!        소환 종류는 패턴의 SummonEnemyClasses에서 무작위, 공격력은 SummonAttackPowerScale배. 소환몹은 파생 적 플래그로
//!        보상·재소환에서 제외되고 소환자 리스트에 등록되어 동시 생존 수 게이트에 반영된다.
//! \note  게이트(생존 소환몹 ≥ 생존 플레이어 × SummonsPerPlayer)는 패턴 선택 단계(SelectAvailableAttackPattern)에서 이미
//!        걸러지므로 여기서는 재검사 없이 소환한다. 소환 몽타주를 재생하고 소환 노티파이 타이밍에 실제 소환한다
//!        (투사체/장판과 동일). 몽타주가 없으면 즉시 소환 폴백.
UCLASS()
class PROJECTP_API UCPP_EnemySummonAbility : public UMyGameplayAbilityBase
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

	//! \brief 소환 노티파이 도달 시 호출(CPP_EnemyBase::TriggerSummonFromAnimNotify 경유). 쿨다운 커밋 + 실제 소환.
	bool TriggerSummonFromNotify(ACPP_EnemyBase* Summoner);

	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

protected:
	//! \brief 소환/등장 연출용 GameplayCue 태그(선택).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Summon|Cue")
	FGameplayTag SummonCueTag;

private:
	bool DoSummon(ACPP_EnemyBase* Summoner);
	void SpawnMinionsAroundPlayer(ACPP_EnemyBase* Summoner, AActor* PlayerActor);
	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	bool bHasSummoned = false;
};
