// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_EnemyCombatCoordinator.h"

#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Core/CPP_EnemyCombatSettings.h"
#include "HAL/IConsoleManager.h"

// 플레이어당 동시 공격자 상한 live 오버라이드. -1이면 Project Settings(Enemy Combat) 값을 쓴다.
// PIE 도중 콘솔에서 즉시 조정 가능: enemy.MaxAttackersPerPlayer 3
static TAutoConsoleVariable<int32> CVarEnemyMaxAttackers(
	TEXT("enemy.MaxAttackersPerPlayer"),
	-1,
	TEXT("플레이어당 동시 공격자 상한. -1 = Project Settings 값 사용."),
	ECVF_Default);

////////////////////////////
//! \author HanUl
//! \brief 플레이어당 동시 공격자 상한을 반환한다. CVar(>=1)이 있으면 우선, 없으면 Project Settings 값.
//! \param
//! \return 동시 공격자 상한(>=1)
int32 UEnemyCombatCoordinatorSubsystem::GetMaxAttackersPerPlayer()
{
	const int32 Override = CVarEnemyMaxAttackers.GetValueOnGameThread();
	if (Override >= 1)
	{
		return Override;
	}

	const UEnemyCombatSettings* Settings = GetDefault<UEnemyCombatSettings>();
	return Settings ? FMath::Max(1, Settings->MaxAttackersPerPlayer) : 5;
}

////////////////////////////
//! \author HanUl
//! \brief 집합에서 살아있는(유효+비사망) 적 수를 센다. 사망·소멸 점유자는 무시해 자동 회수한다.
//! \param Set 대상 집합
//! \return 살아있는 적 수
int32 UEnemyCombatCoordinatorSubsystem::CountAlive(const TSet<TWeakObjectPtr<ACPP_EnemyBase>>& Set)
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ACPP_EnemyBase>& Ptr : Set)
	{
		const ACPP_EnemyBase* Enemy = Ptr.Get();
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

////////////////////////////
//! \author HanUl
//! \brief 적의 타겟 플레이어를 등록/변경한다. 모든 타게터 풀에서 뺀 뒤 새 플레이어 풀에 넣는다.
//! \param Enemy 대상 적
//! \param NewPlayer 새 타겟 플레이어(nullptr면 해제)
//! \return
void UEnemyCombatCoordinatorSubsystem::SetEnemyTarget(ACPP_EnemyBase* Enemy, AActor* NewPlayer)
{
	if (!Enemy)
	{
		return;
	}

	for (TPair<TObjectKey<AActor>, TSet<TWeakObjectPtr<ACPP_EnemyBase>>>& Pair : Targeters)
	{
		Pair.Value.Remove(Enemy);
	}

	if (IsValid(NewPlayer))
	{
		Targeters.FindOrAdd(TObjectKey<AActor>(NewPlayer)).Add(Enemy);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 플레이어를 타겟하는 살아있는 적 수를 반환한다.
//! \param Player 대상 플레이어
//! \return 타게터 수
int32 UEnemyCombatCoordinatorSubsystem::GetTargeterCount(const AActor* Player) const
{
	const TSet<TWeakObjectPtr<ACPP_EnemyBase>>* Set = Targeters.Find(TObjectKey<AActor>(Player));
	return Set ? CountAlive(*Set) : 0;
}

////////////////////////////
//! \author HanUl
//! \brief 공격 토큰 가용 여부(순수 판정). 이미 보유 중이거나 살아있는 공격자가 상한 미만이면 true.
//! \param Enemy 확인 적
//! \param Player 타겟 플레이어
//! \return 획득 가능하면 true
bool UEnemyCombatCoordinatorSubsystem::CanAcquireAttackToken(const ACPP_EnemyBase* Enemy, const AActor* Player) const
{
	if (!Enemy || !Player)
	{
		return false;
	}

	const TSet<TWeakObjectPtr<ACPP_EnemyBase>>* Set = Attackers.Find(TObjectKey<AActor>(Player));
	if (Set && Set->Contains(Enemy))
	{
		return true;
	}

	const int32 Current = Set ? CountAlive(*Set) : 0;
	return Current < GetMaxAttackersPerPlayer();
}

////////////////////////////
//! \author HanUl
//! \brief 공격 토큰을 획득한다. 이미 보유면 true, 상한 미만이면 등록 후 true, 초과면 false.
//!        CanAcquireAttackToken의 참고 판정과 달리 이 함수의 반환값이 최종 허가다 — 같은 틱 경쟁을 막는다.
//! \param Enemy 요청 적
//! \param Player 타겟 플레이어
//! \return 공격 허가면 true
bool UEnemyCombatCoordinatorSubsystem::TryAcquireAttackToken(ACPP_EnemyBase* Enemy, AActor* Player)
{
	if (!IsValid(Enemy) || !IsValid(Player))
	{
		return false;
	}

	TSet<TWeakObjectPtr<ACPP_EnemyBase>>& Set = Attackers.FindOrAdd(TObjectKey<AActor>(Player));
	if (Set.Contains(Enemy))
	{
		return true;
	}

	if (CountAlive(Set) >= GetMaxAttackersPerPlayer())
	{
		return false;
	}

	Set.Add(Enemy);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 공격 토큰을 반납한다. 모든 플레이어 풀에서 제거해 타겟 전환 후 호출돼도 누수가 없다.
//! \param Enemy 반납 적
//! \return
void UEnemyCombatCoordinatorSubsystem::ReleaseAttackToken(ACPP_EnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	for (TPair<TObjectKey<AActor>, TSet<TWeakObjectPtr<ACPP_EnemyBase>>>& Pair : Attackers)
	{
		Pair.Value.Remove(Enemy);
	}
}
