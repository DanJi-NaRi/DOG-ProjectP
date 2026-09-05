// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "CPP_EnemyCombatCoordinator.generated.h"

class ACPP_EnemyBase;

////////////////////////////
//! \class UEnemyCombatCoordinatorSubsystem
//! \brief 적 전투 중앙 조정자(서버 전용, 틱 없음). 슬롯 격자를 대체한다.
//!        - 타게터 집계: 플레이어별로 자신을 타겟하는 살아있는 적 수(타겟 분산 지표).
//!        - 공격 토큰: 플레이어별 동시 공격자 상한(스웜 페이싱). 토큰은 공격(몽타주) 동안만 점유하고 스윙 사이 반납된다.
//!        위치/대형은 관리하지 않는다 — 이동은 스티어링(타겟 접근+사거리 정지)과 DetourCrowd 회피가 담당한다.
UCLASS()
class PROJECTP_API UEnemyCombatCoordinatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//! 적의 타겟 플레이어를 등록/변경한다. NewPlayer=nullptr면 해제. (AIC SetTargetActor에서 호출)
	void SetEnemyTarget(ACPP_EnemyBase* Enemy, AActor* NewPlayer);

	//! 플레이어를 타겟하는 살아있는 적 수(타겟 분산 지표).
	int32 GetTargeterCount(const AActor* Player) const;

	//! 공격 토큰 가용 여부(순수 판정, 참고용). 이미 보유 중이거나 상한 미만이면 true. (CanAttackTarget 게이트)
	bool CanAcquireAttackToken(const ACPP_EnemyBase* Enemy, const AActor* Player) const;

	//! 공격 토큰 획득(공격 커밋 시). 성공 또는 이미 보유면 true, 상한 초과면 false. (최종 허가 — 경쟁 방지)
	bool TryAcquireAttackToken(ACPP_EnemyBase* Enemy, AActor* Player);

	//! 공격 토큰 반납(공격 종료/중단). 모든 풀에서 제거하므로 타겟이 바뀐 뒤 호출돼도 누수가 없다. (넉넉히 호출해도 안전)
	void ReleaseAttackToken(ACPP_EnemyBase* Enemy);

private:
	static int32 GetMaxAttackersPerPlayer();
	static int32 CountAlive(const TSet<TWeakObjectPtr<ACPP_EnemyBase>>& Set);

	// 플레이어별 타게터/공격자 집합. 사망·소멸 적은 CountAlive가 걸러내므로 명시적 제거가 누락돼도 집계는 자가 회수된다.
	TMap<TObjectKey<AActor>, TSet<TWeakObjectPtr<ACPP_EnemyBase>>> Targeters;
	TMap<TObjectKey<AActor>, TSet<TWeakObjectPtr<ACPP_EnemyBase>>> Attackers;
};
