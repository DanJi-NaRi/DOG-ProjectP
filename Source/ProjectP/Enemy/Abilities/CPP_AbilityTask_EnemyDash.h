// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Abilities/CPP_AbilityTask_BossDash.h"
#include "CPP_AbilityTask_EnemyDash.generated.h"

class ACharacter;

//! \brief 적 돌진 종료 시 브로드캐스트. bHitWall은 폰이 아닌 지오메트리(벽)에 막혀 조기 정지했으면 true(완주/폰 정지는 false).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyDashFinishedSignature, const TArray<AActor*>&, HitPawns, bool, bHitWall);

////////////////////////////
//! \class UCPP_AbilityTask_EnemyDash
//! \brief 일반 적 돌진 태스크. 보스 돌진의 sweep 이동/정리 골격을 상속하고 적 전용 규칙만 더한다:
//!        같은 진영(Faction 게임플레이 태그) 폰은 관통(자기들끼리 엉킴 방지), 벽 정지 여부를 보고(벽 기절용).
UCLASS()
class PROJECTP_API UCPP_AbilityTask_EnemyDash : public UCPP_AbilityTask_BossDash
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEnemyDashFinishedSignature OnEnemyDashFinished;

	//! \brief 적 돌진 태스크 생성. 방향은 정규화되며, 거리/속도/캡슐 치수로 매 tick 전진한다.
	static UCPP_AbilityTask_EnemyDash* EnemyDash(
		UGameplayAbility* OwningAbility,
		ACharacter* InDashCharacter,
		FVector InDashDirection,
		float InDashDistance,
		float InDashSpeed,
		float InCapsuleRadius,
		float InCapsuleHalfHeight
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	virtual void FinishDash(const TArray<AActor*>& HitPawns) override;

private:
	bool bStoppedByWall = false;
};
