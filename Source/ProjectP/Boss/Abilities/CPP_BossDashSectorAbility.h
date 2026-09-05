#pragma once

#include "CoreMinimal.h"
#include "Boss/Abilities/CPP_BossAttackAbility.h"
#include "CPP_BossDashSectorAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UCPP_AbilityTask_BossDash;
class ACPP_BossTelegraphActor;
struct FGameplayEventData;

////////////////////////////
//! \class UCPP_BossDashSectorAbility
//! \brief P2 일반 패턴(BOS_SET_P2_PAT_03): 전방 돌진 후 후방 부채꼴. 후방 부채꼴/텔레그래프/데미지/쿨다운/몽타주는
//!        UCPP_BossAttackAbility(AttackData 공격 윈도우)로 그대로 재사용하고, 돌진 단계만 추가한다.
//! \note 돌진은 UCPP_AbilityTask_BossDash로 서버에서 캡슐 sweep 이동(비관통). 돌진 트리거는 Event.Boss.Dash 이벤트를
//!       WindowId로 구분한다(Aim=방향 고정+직선 텔레그래프, Go=돌진 시작). 돌진 종료 시 몽타주를 후려치기 섹션으로 점프.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UCPP_BossDashSectorAbility : public UCPP_BossAttackAbility
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

protected:
	//! \brief 돌진 거리(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0"))
	float DashDistance = 600.0f;

	//! \brief 돌진 속도(cm/s). 6m/0.35s ≈ 1714. 에디터에서 조절.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0"))
	float DashSpeed = 1714.0f;

	//! \brief 돌진 충돌 캡슐 폭(cm). 반경 = 폭/2. 에디터에서 조절.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0"))
	float DashCapsuleWidth = 80.0f;

	//! \brief 돌진 충돌 캡슐 절반 높이(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0"))
	float DashCapsuleHalfHeight = 90.0f;

	//! \brief 돌진 피해 계수(공격력 × 이 값).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0"))
	float DashDamageCoefficient = 1.7f;

	//! \brief 돌진 피해와 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DashCurseGaugeAmount = 0.0f;

	//! \brief 돌진 직선 텔레그래프 액터 클래스. 크기는 캡슐 폭/돌진 거리에 자동으로 맞춰진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash")
	TSubclassOf<ACPP_BossTelegraphActor> DashLineTelegraphActorClass;

	//! \brief 돌진 방향 고정 + 직선 텔레그래프 표시 트리거로 쓰는 WindowId(Event.Boss.Dash 페이로드).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash")
	FName DashAimWindowId = TEXT("Aim");

	//! \brief 실제 돌진 시작 트리거로 쓰는 WindowId(Event.Boss.Dash 페이로드).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash")
	FName DashGoWindowId = TEXT("Go");

	//! \brief 돌진 종료 시 점프할 몽타주 섹션 이름. 섹션이 없으면 몽타주는 그대로 진행(무해).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Dash")
	FName RearAttackSectionName = TEXT("RearAttack");

private:
	UFUNCTION()
	void HandleDashEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleDashFinished(const TArray<AActor*>& HitPawns);

	void LockDashDirectionAndShowTelegraph();
	void StartDash();
	void DestroyDashTelegraph();

	//! \brief 돌진 몽타주를 일시정지/재개한다(돌진 동안 '지른 포즈' 프레임을 붙잡아 두기 위함).
	void SetDashMontagePaused(bool bPaused);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveDashEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UCPP_AbilityTask_BossDash> ActiveDashTask;

	UPROPERTY(Transient)
	TObjectPtr<ACPP_BossTelegraphActor> DashTelegraph;

	FVector LockedDashDirection = FVector::ForwardVector;
	bool bDashDirectionLocked = false;
};
