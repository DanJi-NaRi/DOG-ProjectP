#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossSandStormChaseAbility.generated.h"

class ACPP_BossSandStormChaseActor;
class UGameplayEffect;

////////////////////////////
//! \class UCPP_BossSandStormChaseAbility
//! \brief P1 기믹 패턴(BOS_SET_P1_PAT_04): 무작위 생존 아군 하나를 표적으로 마킹하고, 0.5초 뒤 보스 앞에
//!        추격 모래폭풍을 소환한다. 폭풍은 스폰 1초 뒤 이동을 시작하며 어빌리티는 스폰 1초 뒤(총 1.5초)에 종료된다.
//! \note 몬타주 없는 즉발형. 폭풍은 소환 후 독립적으로 15초간 지속되며, 표적 마크의 수명 소유권은 폭풍으로 이관된다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UCPP_BossSandStormChaseAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCPP_BossSandStormChaseAbility();

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
	//! \brief 소환할 추격 모래폭풍 액터 클래스.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm")
	TSubclassOf<ACPP_BossSandStormChaseActor> SandStormActorClass;

	//! \brief 표적 상태 태그 + 발밑 마크 Cue를 부여하는 무한 지속 GameplayEffect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm")
	TSubclassOf<UGameplayEffect> TargetMarkGameplayEffect;

	//! \brief 표적 마킹 후 폭풍이 스폰되기까지의 선행 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (ClampMin = "0.0"))
	float MarkLeadTime = 0.5f;

	//! \brief 폭풍 스폰 후 어빌리티가 종료(다음 패턴으로 전환)되기까지의 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (ClampMin = "0.0"))
	float SpawnToEndTime = 1.0f;

	//! \brief 보스 전방으로 폭풍을 스폰할 거리(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (ClampMin = "0.0"))
	float SpawnForwardDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown")
	TSubclassOf<UGameplayEffect> BossCooldownGameplayEffectClass;

private:
	void HandleSpawnStorm();
	void HandleFinish();
	void ClearActiveTimers();

	FActiveGameplayEffectHandle PendingMarkHandle;
	TWeakObjectPtr<AActor> PendingTarget;

	//! \brief true가 되면 표적 마크 수명은 폭풍이 소유하므로 어빌리티는 EndAbility에서 마크를 제거하지 않는다.
	bool bHandedOffToStorm = false;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle FinishTimerHandle;

	mutable FGameplayTagContainer BossCooldownTags;
};
