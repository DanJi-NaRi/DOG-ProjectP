// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClearComponent.h"
#include "CPP_ClearComponent_CombatCleared.generated.h"

class ACPP_EnemyBase;
class UCPP_SpawnerComponent;

////////////////////////////
//! \class UCPP_ClearComponent_CombatCleared
//! \brief 전투 Zone 클리어 조건 어댑터. Zone Active 시 SpawnerComponent가 스폰한 적(프리스폰 포함)을 끌어와 등록하고
//!        이후 스폰/완료를 구독하여, (모든 웨이브 소진 && 생존 0)이면 클리어를 통보한다. 모든 적은 스포너를 통해 일원화된다.
UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Clear Component (Combat Cleared)"))
class PROJECTP_API UCPP_ClearComponent_CombatCleared : public UClearComponent
{
	GENERATED_BODY()

public:
	//! Zone Active 시: 사전배치 적 수집 + 스포너 구독 시작.
	virtual void ActivateClearCondition_Implementation() override;

	//! Zone 재시작 등 리셋 시: 추적/구독을 모두 해제한다.
	virtual void ResetClearCondition_Implementation() override;

private:
	bool HasZoneAuthority() const;

	void RegisterEnemy(ACPP_EnemyBase* Enemy);
	int32 CountAlive() const;
	void EvaluateClear();

	void HandleEnemySpawned(ACPP_EnemyBase* SpawnedEnemy);
	void HandleAllWavesFinished();

	UFUNCTION()
	void HandleTrackedEnemyDeath();

	// 사전배치 + 스폰으로 추적 중인 적들.
	TSet<TWeakObjectPtr<ACPP_EnemyBase>> TrackedEnemies;
	// 더 이상 스폰이 없는지 여부 (스포너 통지 또는 스포너 부재 시 true).
	bool bAllWavesFinished = false;

	TWeakObjectPtr<UCPP_SpawnerComponent> Spawner;
};
