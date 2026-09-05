#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossBlackHoleAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
class ACPP_BossBlackHoleActor;
struct FGameplayEventData;

////////////////////////////
//! \class UCPP_BossBlackHoleAbility
//! \brief P2 기믹 패턴(BOS_SET_P2_PAT_04): 소환 몽타주를 재생하고, 몽타주 중간의 노티파이(Event.Boss.AttackWindow)에서
//!        전장 중앙에 검은 구를 소환한다. 몽타주가 끝나면 어빌리티가 종료되어 다음 패턴으로 넘어간다.
//! \note 구는 소환 후 독립적으로 5초 뒤 폭발한다. 전멸기(클리어 인카운터) 시작 시 Director가 구를 즉시 소멸시킨다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UCPP_BossBlackHoleAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCPP_BossBlackHoleAbility();

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

	virtual float GetCooldownSeconds() const override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	//! \brief 소환 몽타주. 중간에 CPP_AnimNotify_BossAttackWindowEvent를 배치해 구 소환 시점을 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|BlackHole")
	TObjectPtr<UAnimMontage> SpawnMontage;

	//! \brief 소환할 검은 구 액터 클래스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|BlackHole")
	TSubclassOf<ACPP_BossBlackHoleActor> BlackHoleActorClass;

	//! \brief 폭발 시 즉사에 사용할 데미지 수치(확정 사살용 초대량 값).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (ClampMin = "0.0"))
	float KillDamage = 9999999.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown")
	TSubclassOf<UGameplayEffect> BossCooldownGameplayEffectClass;

private:
	void SpawnBlackHole();

	UFUNCTION()
	void HandleSpawnEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleMontageBlendOut();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveSpawnEventTask;

	bool bBlackHoleSpawned = false;

	mutable FGameplayTagContainer BossCooldownTags;
};
