// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_ClearComponent_CombatCleared.h"

#include "CPP_SpawnerComponent.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "GameFramework/Actor.h"

////////////////////////////
//! \author HanUl
//! \brief Zone Active 시 스포너가 스폰한 적(프리스폰 포함)을 끌어와 등록하고 이후 스폰/완료를 구독한다. 구독을 먼저 건 뒤 Zone이 스포너를 기동하므로 첫 웨이브 스폰을 놓치지 않는다.
//! \param
//! \return
void UCPP_ClearComponent_CombatCleared::ActivateClearCondition_Implementation()
{
	// 부모가 감시 활성화 + (이미 충족 시) 재방송을 처리한다.
	Super::ActivateClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	bAllWavesFinished = false;

	// 같은 Zone의 스포너를 찾는다. 모든 적(프리스폰 + 웨이브)은 스포너를 통해 일원화되어 있다.
	if (AActor* Owner = GetOwner())
	{
		Spawner = Owner->FindComponentByClass<UCPP_SpawnerComponent>();
	}

	if (Spawner.IsValid())
	{
		// 이미 스폰된 적(프리스폰) 등록 - 구독 이전 시점(Preparing 등)에 스폰되어 통지를 놓쳤으므로 직접 끌어온다.
		for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : Spawner->GetSpawnedEnemies())
		{
			RegisterEnemy(WeakEnemy.Get());
		}

		// 이후 웨이브 스폰/완료 구독.
		Spawner->OnEnemySpawned.AddUObject(this, &UCPP_ClearComponent_CombatCleared::HandleEnemySpawned);
		Spawner->OnAllWavesFinished.AddUObject(this, &UCPP_ClearComponent_CombatCleared::HandleAllWavesFinished);
		bAllWavesFinished = Spawner->AreAllWavesFinished();
	}
	else
	{
		bAllWavesFinished = true;
	}

	EvaluateClear();
}

////////////////////////////
//! \author HanUl
//! \brief Zone 리셋 시 적 사망 구독과 스포너 구독을 모두 해제하고 추적 상태를 초기화한다.
//! \param
//! \return
void UCPP_ClearComponent_CombatCleared::ResetClearCondition_Implementation()
{
	Super::ResetClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : TrackedEnemies)
	{
		if (ACPP_EnemyBase* Enemy = WeakEnemy.Get())
		{
			Enemy->OnEnemyDeath.RemoveDynamic(this, &UCPP_ClearComponent_CombatCleared::HandleTrackedEnemyDeath);
		}
	}
	TrackedEnemies.Reset();

	if (Spawner.IsValid())
	{
		Spawner->OnEnemySpawned.RemoveAll(this);
		Spawner->OnAllWavesFinished.RemoveAll(this);
	}
	Spawner = nullptr;
	bAllWavesFinished = false;
}

////////////////////////////
//! \author HanUl
//! \brief 적을 중복 없이 추적에 등록하고 사망 이벤트를 구독한다. 이미 죽은 적은 등록하지 않는다.
//! \param Enemy 등록할 적
//! \return
void UCPP_ClearComponent_CombatCleared::RegisterEnemy(ACPP_EnemyBase* Enemy)
{
	if (!IsValid(Enemy) || Enemy->IsDead())
	{
		return;
	}

	bool bAlreadyTracked = false;
	TrackedEnemies.Add(Enemy, &bAlreadyTracked);
	if (bAlreadyTracked)
	{
		return;
	}

	Enemy->OnEnemyDeath.AddUniqueDynamic(this, &UCPP_ClearComponent_CombatCleared::HandleTrackedEnemyDeath);
}

////////////////////////////
//! \author HanUl
//! \brief 추적 중인 적들 중 생존 수를 센다.
//! \param
//! \return 생존한 적의 수
int32 UCPP_ClearComponent_CombatCleared::CountAlive() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : TrackedEnemies)
	{
		const ACPP_EnemyBase* Enemy = WeakEnemy.Get();
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			++Alive;
		}
	}
	return Alive;
}

////////////////////////////
//! \author HanUl
//! \brief 클리어 조건을 평가한다. 모든 웨이브가 소진되고 추적 적이 전멸했으면 Zone에 클리어를 통보한다.
//! \param
//! \return
void UCPP_ClearComponent_CombatCleared::EvaluateClear()
{
	if (!HasZoneAuthority())
	{
		return;
	}

	if (bAllWavesFinished && CountAlive() == 0)
	{
		// 감시 비활성/이미 클리어 상태 처리는 부모(MarkClearSatisfied)가 래치로 보장한다.
		MarkClearSatisfied();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 스포너가 적을 스폰할 때마다 추적에 등록한다.
//! \param SpawnedEnemy 새로 스폰된 적
//! \return
void UCPP_ClearComponent_CombatCleared::HandleEnemySpawned(ACPP_EnemyBase* SpawnedEnemy)
{
	RegisterEnemy(SpawnedEnemy);
}

////////////////////////////
//! \author HanUl
//! \brief 스포너가 모든 웨이브 스폰을 마쳤을 때 호출. 소진 플래그를 세우고 클리어를 재평가한다.
//! \param
//! \return
void UCPP_ClearComponent_CombatCleared::HandleAllWavesFinished()
{
	bAllWavesFinished = true;
	EvaluateClear();
}

////////////////////////////
//! \author HanUl
//! \brief 추적 중인 적이 사망할 때 호출. 파라미터가 없는 델리게이트라 추적 집합을 재계수하여 클리어를 평가한다.
//! \param
//! \return
void UCPP_ClearComponent_CombatCleared::HandleTrackedEnemyDeath()
{
	EvaluateClear();
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 Owner Actor가 서버 권한을 가지고 있는지 확인한다.
//! \param
//! \return 서버 권한이 있으면 true
bool UCPP_ClearComponent_CombatCleared::HasZoneAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}
