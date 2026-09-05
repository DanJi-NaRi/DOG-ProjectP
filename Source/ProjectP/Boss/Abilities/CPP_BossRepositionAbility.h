#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossRepositionAbility.generated.h"

class ACPP_BossCharacter;
class ACPP_BossTelegraphActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UCPP_AbilityTask_BossDash;
struct FGameplayEventData;

////////////////////////////
//! \class UCPP_BossRepositionAbility
//! \brief 패턴 사이에 먼 타겟과의 거리를 좁히는 전진 이동 패턴. 이동은 BossDash 태스크를 재사용한다.
//!        돌진(DashSector)과 같은 몽타주 구조 — Aim 노티파이에서 방향 고정+직선 텔레그래프,
//!          Go 노티파이에서 이동 시작+몽타주 일시정지. 명중한 적대 폰에 공격력×계수 소피해+넉백.
//!          몽타주 미지정 시 텔레그래프 없는 즉시 직진으로 안전 강하.
//! \note 쿨다운 없음 — 연속 제한은 브레인의 전진 카운터 규칙이 담당한다.
UCLASS(Blueprintable)
class PROJECTP_API UCPP_BossRepositionAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCPP_BossRepositionAbility();

	//! \brief 타겟이 전진 대역(TooFarDistance 초과)인지. 브레인이 CDO로 조회한다.
	bool ShouldAdvanceAtDistance(float DistanceToTarget) const;

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

protected:
	//! \brief 타겟이 이보다 멀면 전진 스텝을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition", meta = (ClampMin = "0.0"))
	float TooFarDistance = 1600.0f;

	//! \brief 전진 스텝 거리(고정). 위협 가독성을 위해 매번 같은 거리·속도로 이동한다
	//!        (과접근 방지로 마지막 스텝만 잔여 거리에 맞춰 짧아질 수 있음).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.0"))
	float AdvanceStepDistance = 1000.0f;

	//! \brief 전진 후 타겟과 유지할 최소 거리. 마지막 스텝은 이 거리를 넘지 않도록 짧아질 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.0"))
	float AdvanceStopDistance = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "1.0"))
	float AdvanceStepSpeed = 1400.0f;

	//! \brief 전진 명중 피해 계수(공격력 × 이 값). 본 돌진 패턴보다 가볍게 유지. 0이면 피해 없음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.0"))
	float AdvanceDamageCoefficient = 0.3f;

	//! \brief 전진 명중 피해와 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float AdvanceCurseGaugeAmount = 0.0f;

	//! \brief 전진 명중 시 수평 넉백 세기(LaunchCharacter 속도). 0이면 넉백 없음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.0"))
	float AdvanceKnockbackStrength = 600.0f;

	//! \brief 전진 몽타주(돌진 몽타주 재사용 가능). Aim/Go 노티파이(Event.Boss.Dash)가 있어야 한다.
	//!        미지정이면 무모션·무텔레그래프 즉시 직진으로 강하.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance")
	TObjectPtr<UAnimMontage> AdvanceMontage;

	//! \brief 전진 몽타주 재생 속도. 본 돌진과 같은 몽타주를 쓸 때 1.3~1.5로 올려 가벼운 스텝 느낌으로 구분.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance", meta = (ClampMin = "0.1"))
	float AdvanceMontagePlayRate = 1.3f;

	//! \brief 전진 직선 텔레그래프 액터 클래스. 크기는 보스 캡슐 폭/스텝 거리에 자동으로 맞춰진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance")
	TSubclassOf<ACPP_BossTelegraphActor> AdvanceLineTelegraphActorClass;

	//! \brief 방향 고정 + 텔레그래프 표시 트리거 WindowId(Event.Boss.Dash 페이로드).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance")
	FName AdvanceAimWindowId = TEXT("Aim");

	//! \brief 이동 시작 트리거 WindowId(Event.Boss.Dash 페이로드).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Reposition|Advance")
	FName AdvanceGoWindowId = TEXT("Go");

private:
	void StartMontageAdvance(ACPP_BossCharacter* BossAvatar);
	void StartAdvanceStep();
	void LockAdvanceDirectionAndShowTelegraph();
	void DestroyAdvanceTelegraph();
	void PauseAdvanceMontage();
	float ComputeAdvanceStepDistance(float DistanceToTarget) const;
	UCPP_AbilityTask_BossDash* StartStepTask(ACPP_BossCharacter* BossAvatar, const FVector& StepDirection, float StepDistance, float StepSpeed);

	UFUNCTION()
	void HandleAdvanceWindowEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAdvanceStepFinished(const TArray<AActor*>& HitPawns);

	UFUNCTION()
	void HandleAdvanceMontageFinished();

	UFUNCTION()
	void HandleAdvanceMontageAborted();

	UPROPERTY(Transient)
	TObjectPtr<UCPP_AbilityTask_BossDash> ActiveStepTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveAdvanceMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveAdvanceEventTask;

	UPROPERTY(Transient)
	TObjectPtr<ACPP_BossTelegraphActor> AdvanceTelegraph;

	TWeakObjectPtr<AActor> PendingAdvanceTarget;
	FVector LockedAdvanceDirection = FVector::ForwardVector;
	float LockedAdvanceDistance = 0.0f;
	bool bAdvanceDirectionLocked = false;
	bool bAdvanceStepStarted = false;
};
