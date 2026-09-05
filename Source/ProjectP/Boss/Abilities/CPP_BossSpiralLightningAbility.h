#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossSpiralLightningAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
class ACPP_BossRockWarningActor;
struct FGameplayEventData;

////////////////////////////
//! \class UCPP_BossSpiralLightningAbility
//! \brief P2 일반 패턴(BOS_SET_P2_PAT_01): 회오리 번개. 왼손/오른손 중 하나를 무작위로 골라, 보스 위치를 중심으로 한
//!        나선 경로를 따라 SpawnInterval마다 번개(경고→낙하)를 순차 생성한다.
//!        왼손=시계방향·바깥→안쪽, 오른손=반시계방향·안쪽→바깥. 마지막 번개가 낙하한 시점에 어빌리티가 끝난다.
//! \note 번개 타격은 기존 ACPP_BossRockWarningActor(경고 후 범위 피해)를 재사용한다. 피해 계수(1.2)·경고시간·반경은
//!       그 액터 BP에서 세팅한다. 저주 게이지는 프로젝트 미구현이라 실제 적용되지 않는다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UCPP_BossSpiralLightningAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCPP_BossSpiralLightningAbility();

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
	//! \brief 왼손(시계방향·바깥→안쪽) 패턴 시작 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning")
	TObjectPtr<UAnimMontage> LeftHandMontage;

	//! \brief 오른손(반시계방향·안쪽→바깥) 패턴 시작 몽타주.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning")
	TObjectPtr<UAnimMontage> RightHandMontage;

	//! \brief 각 번개 타격 액터(ACPP_BossRockWarningActor 파생 BP). 피해 계수(1.2)/경고시간(1s)/반경(50)은 이 BP에서 세팅.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning")
	TSubclassOf<ACPP_BossRockWarningActor> LightningActorClass;

	//! \brief 나선 안쪽 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "0.0"))
	float InnerRadius = 200.0f;

	//! \brief 나선 바깥 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "0.0"))
	float OuterRadius = 1000.0f;

	//! \brief 생성할 번개 총 개수.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "1"))
	int32 LightningCount = 10;

	//! \brief 나선 회전 수(바퀴). 총 회전 각 = Revolutions × 360°.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "0.0"))
	float Revolutions = 1.5f;

	//! \brief 번개 생성 간격(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "0.05"))
	float SpawnInterval = 0.7f;

	//! \brief 마지막 번개 생성 후 실제 낙하까지의 대기(어빌리티 종료 타이밍). 번개 액터의 WarningDuration과 일치시킬 것.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SpiralLightning", meta = (ClampMin = "0.0"))
	float LightningWarningDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown")
	TSubclassOf<UGameplayEffect> BossCooldownGameplayEffectClass;

private:
	void ComputeSpiralPoints(bool bLeftHand, const FVector& Center, const FVector& ForwardDirection);
	void StartSpiralSpawning();
	void SpawnNextLightning();
	void HandleFinished();

	//! \brief 몽타주 노티파이(Event.Boss.AttackWindow)가 도달하면 나선 번개 생성을 시작한다.
	UFUNCTION()
	void HandleSpawnStartEvent(FGameplayEventData Payload);

	//! \brief 몽타주 종료(완료/중단) 폴백: 노티파이가 없었으면 이때 나선을 시작한다.
	UFUNCTION()
	void HandleMontageEnded();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveSpawnEventTask;

	TArray<FVector> SpiralPoints;
	int32 NextLightningIndex = 0;
	bool bSpiralStarted = false;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle FinishTimerHandle;

	mutable FGameplayTagContainer BossCooldownTags;
};
