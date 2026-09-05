#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "CPP_BossTargetingComponent.generated.h"

class APawn;

////////////////////////////
//! \class UCPP_BossTargetingComponent
//! \brief 어그로 점수(최근 피해 지분 × 가중치 + 근접도 × 가중치)로 보스의 현재 타겟을 선정하는
//!        서버 전용 컴포넌트. 교체 마진(히스테리시스)으로 점수가 비등할 때의 잦은 타겟 교체를 막는다.
//!        재평가는 브레인이 패턴 경계에서 호출할 때만 수행한다(자체 tick 없음).
//!        타겟은 복제하지 않는다 — 보스의 행동(회전/이동)으로만 드러난다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UCPP_BossTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPP_BossTargetingComponent();

	//! \brief 마지막 재평가에서 선정된 타겟. 죽었거나 무효면 nullptr.
	UFUNCTION(BlueprintPure, Category = "Boss|Targeting")
	AActor* GetCurrentTarget() const;

	//! \brief 패턴 경계에서 타겟을 다시 선정한다. 생존 플레이어 전원을 어그로 점수로 평가하고,
	//!        현재 타겟은 교체 마진을 넘는 도전자가 있을 때만 바뀐다.
	AActor* ReevaluateTarget();

	//! \brief 피해 실행 지점(BossAttributeSet::PostGameplayEffectExecute)에서 호출 — 가해자를 위협 기록에 쌓는다.
	//!        실드 흡수분을 포함한 총 피해 기준(전멸기 실드 DPS 체크 중에도 어그로가 쌓이도록).
	void RecordThreatDamage(const FGameplayEffectContextHandle& EffectContext, float DamageAmount);

private:
	struct FBossThreatRecord
	{
		TWeakObjectPtr<AActor> InstigatorPawn;
		float Damage = 0.0f;
		double RecordTime = 0.0;
	};

	static APawn* ResolveInstigatorPawn(const FGameplayEffectContextHandle& EffectContext);
	void PruneExpiredRecords();

	//! \brief 타겟 선정에 반영하는 최근 피해 집계 구간(초).
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Targeting", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float RecentDamageWindowSeconds = 8.0f;

	//! \brief 딜 지분(내 피해 ÷ 전체 피해, 0~1)의 가중치.
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Targeting", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DamageWeight = 0.7f;

	//! \brief 근접도(1 − 거리 ÷ MaxScoringDistance, 0~1)의 가중치.
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Targeting", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProximityWeight = 0.3f;

	//! \brief 근접도가 0이 되는 기준 거리. 전장 반지름 수준으로 설정(지름 70m 원형 → 3500).
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Targeting", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MaxScoringDistance = 3500.0f;

	//! \brief 타겟 교체 마진: 도전자 점수가 현재 타겟 점수 × 이 값을 넘어야 교체된다.
	//!        1.0이면 마진 없음(항상 최고점으로 교체). 점수가 비등할 때의 타겟 흔들림 방지.
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Targeting", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float TargetSwitchScoreMargin = 1.15f;

	TArray<FBossThreatRecord> ThreatRecords;
	TWeakObjectPtr<AActor> CurrentTarget;
};
