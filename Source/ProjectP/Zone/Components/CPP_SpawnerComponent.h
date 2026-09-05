// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Zone/Types/ZoneDataTypes.h"
#include "CPP_SpawnerComponent.generated.h"

class UCPP_EnemyWaveData;
class ACPP_EnemySpawnPoint;
class ACPP_EnemyBase;
class ACPP_DefenseObjective;
class ACPP_BreachObjective;
class ACPP_BreachChanceObject;
class ACPP_RitualObjective;
struct FEnemyWave;
enum class EEnemySpawnTargetPolicy : uint8;

// 적 한 마리가 스폰될 때마다 통지 (ClearComponent가 구독하여 전체 집계).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemySpawnedSignature, ACPP_EnemyBase* /*SpawnedEnemy*/);
// 모든 웨이브의 스폰이 끝났을 때 통지 (마지막 웨이브 스폰 완료 시점, 사망과 무관).
DECLARE_MULTICAST_DELEGATE(FOnAllWavesFinishedSignature);

////////////////////////////
//! \class UCPP_SpawnerComponent
//! \brief Zone에 부착하여 웨이브 스폰을 진행하는 서버 전용 오케스트레이터.
//!        Zone이 Active가 되면 ActivateSpawner()로 기동되며, WaveData 순서대로 웨이브를 스폰한다.
UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Spawner Component"))
class PROJECTP_API UCPP_SpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPP_SpawnerComponent();

	// Zone의 Preparing/Ready/Entering 상태에서 호출. WaveData의 PreSpawnTiming과 일치하는 상태면 초기 배치 적을 스폰한다. (서버 전용, 1회)
	void TryPreSpawn(EZoneState CurrentState);

	// Zone Active 시 호출. 웨이브 진행을 시작한다. (서버 전용)
	void ActivateSpawner();

	// Zone Clear 시 호출. 모든 스폰 타이머를 정지하고 생존 중인 스폰 적을 일괄 사망 처리한다. (서버 전용, Survival 종료 처리)
	void DeactivateSpawner();
	void ResetSpawner();

	// 치트 전용: 이 Zone의 스폰 지점 전체에 몬스터 Count마리를 즉시 분배 스폰한다. (서버 전용, 웨이브 진행과 무관)
	// 클래스는 WaveData의 적 구성(프리스폰+웨이브)을 순환 사용하고, 없으면 FallbackEnemyClass를 사용한다.
	// 스폰된 적은 일반 스폰과 동일하게 등록되어 클리어 집계/존 클리어 시 일괄 정리 대상이 된다.
	int32 CheatSpawnEnemies(int32 Count, TSubclassOf<ACPP_EnemyBase> FallbackEnemyClass);

	// 모든 웨이브 스폰이 완료되었는지 여부. (ClearComponent가 활성화 시점에 초기 상태 확인용)
	bool AreAllWavesFinished() const { return bAllWavesFinished; }
	bool HasDefenseFailed() const { return bObjectiveFailed; }
	bool HasObjectiveFailed() const { return bObjectiveFailed; }
	bool HasAnyEncounterObjective() const { return DefenseObjective != nullptr || BreachObjective != nullptr || RitualObjective != nullptr; }

	// 지금까지 이 스포너가 스폰한 모든 적(프리스폰 포함). ClearComponent가 Active 시점에 끌어가 등록한다.
	const TArray<TWeakObjectPtr<ACPP_EnemyBase>>& GetSpawnedEnemies() const { return AllSpawnedEnemies; }
	ACPP_DefenseObjective* GetDefenseObjective() const { return DefenseObjective; }
	ACPP_BreachObjective* GetBreachObjective() const { return BreachObjective; }
	ACPP_RitualObjective* GetRitualObjective() const { return RitualObjective; }

	// 스폰/완료 통지 델리게이트 (서버 전용 구독).
	FOnEnemySpawnedSignature OnEnemySpawned;
	FOnAllWavesFinishedSignature OnAllWavesFinished;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 스폰할 웨이브 시퀀스 정의.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UCPP_EnemyWaveData> WaveData;

	//! \brief 이 Zone의 Spawner가 생성하는 Enemy에게 적용할 고정 레벨.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn|Level", meta = (ClampMin = "1"))
	int32 EnemyLevel = 1;

	// 이 Zone에서 사용할 스폰 지점들. 태그별로 그룹화되어 위치 선택에 쓰인다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn")
	TArray<TObjectPtr<ACPP_EnemySpawnPoint>> SpawnPoints;

	//! \brief 거점 공격 정책에서 사용할 단일 배치 거점. 일반 Battle Zone에서는 비워둔다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Defense")
	TObjectPtr<ACPP_DefenseObjective> DefenseObjective;

	//! \brief 돌파 저지 Entry가 공격할 고정 목표 지점.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Breach")
	TObjectPtr<ACPP_BreachObjective> BreachObjective;

	//! \brief 남은 기회를 표시하고 등록 순서대로 소모되는 오브젝트 목록.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Breach")
	TArray<TObjectPtr<ACPP_BreachChanceObject>> BreachChanceObjects;

	//! \brief 의식 방해 Entry가 흡수될 고정 목표와 게이지 소유 Actor.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn|Ritual")
	TObjectPtr<ACPP_RitualObjective> RitualObjective;

private:
	bool HasOwnerAuthority() const;

	void BuildSpawnPointMap();
	void InitializeDefenseObjective();
	void InitializeBreachObjective();
	void InitializeRitualObjective();
	void BuildRuntimeBreachChanceObjects();
	bool HasValidBreachConfiguration() const;
	bool HasValidRitualConfiguration() const;
	AActor* ResolveObjectiveTarget(EEnemySpawnTargetPolicy TargetPolicy) const;
	bool ConsumeNextBreachChance();
	void FailObjectiveEncounter();
	void StopSpawningWithoutKilling();
	void ForceKillAllPlayers() const;
	// 웨이브 Entries를 개별 스폰 큐로 평탄화한다. (Survival 재충전에서도 재사용)
	void FillSpawnQueueFromEntries(const FEnemyWave& Wave);
	void ShuffleSpawnQueue();
	void BeginWave(int32 WaveIndex);
	void SpawnNextInQueue();
	// Budgeted 모드: 매 틱 동시 상한 아래에서 배치(Min~Max)로 스폰한다.
	void SpawnBudgetedTick();
	// 적 1마리를 스폰하고 전체 목록 등록 + OnEnemySpawned 통지까지 처리하는 공통 경로.
	ACPP_EnemyBase* SpawnOneEnemy(
		TSubclassOf<ACPP_EnemyBase> EnemyClass,
		FName SpawnPointTag,
		EEnemySpawnTargetPolicy TargetPolicy);
	// 스폰 성공한 적을 전체 목록에 등록하고 OnEnemySpawned를 통지한다. (일반/치트 스폰 공통 마무리)
	void RegisterSpawnedEnemy(ACPP_EnemyBase* Spawned);
	ACPP_EnemyBase* SpawnEnemyAt(
		TSubclassOf<ACPP_EnemyBase> EnemyClass,
		ACPP_EnemySpawnPoint* SpawnPoint,
		EEnemySpawnTargetPolicy TargetPolicy);
	// SourceTransform 기준 Extent(XY 하프익스텐트) 사각형(회전 반영) 안 랜덤 위치를 구해 네비메시 위로 투영한다. 투영 실패/익스텐트 0이면 원위치 반환.
	FVector GetRandomizedSpawnLocation(const FTransform& SourceTransform, const FVector2D& Extent) const;
	ACPP_EnemySpawnPoint* PickSpawnPointForTag(FName Tag);
	void OnWaveSpawnCompleted(int32 WaveIndex);
	void ScheduleNextWave(int32 NextWaveIndex);
	// 전멸 트리거 경로. 방금 클리어된 웨이브의 RestAfterClear만큼 쉬고 다음 웨이브로 진행한다.
	void AdvanceAfterClear();
	void AdvanceToPendingWave();
	void FinishAllWaves();
	int32 CountGatingAlive() const;

	// 웨이브 사이 진행 게이팅용 콜백.
	UFUNCTION()
	void HandleSpawnedEnemyDeath();
	UFUNCTION()
	void HandleDefenseObjectiveDestroyed();
	void HandleBreachObjectiveHit(AActor* SourceActor);
	void HandleRitualObjectiveEntered(AActor* SourceActor);
	void HandleRitualGaugeFull();
	void HandleNextWaveTimer();

	// 평탄화된 개별 스폰 항목.
	struct FPendingSpawn
	{
		TSubclassOf<ACPP_EnemyBase> EnemyClass;
		FName Tag;
		EEnemySpawnTargetPolicy TargetPolicy = static_cast<EEnemySpawnTargetPolicy>(0);
	};

	// 태그 → 해당 태그를 가진 스폰 지점들.
	TMap<FName, TArray<TWeakObjectPtr<ACPP_EnemySpawnPoint>>> TagToSpawnPoints;
	// 태그별 라운드로빈 커서.
	TMap<FName, int32> TagRoundRobinIndex;
	// 스폰 지점 맵을 1회만 구성하기 위한 플래그.
	bool bSpawnPointMapBuilt = false;

	// 프리스폰 1회 실행 보장 플래그.
	bool bPreSpawnDone = false;

	// 프리스폰 + 웨이브로 스폰된 모든 적 (ClearComponent가 끌어감).
	TArray<TWeakObjectPtr<ACPP_EnemyBase>> AllSpawnedEnemies;

	// 현재 웨이브의 스폰 큐.
	TArray<FPendingSpawn> CurrentSpawnQueue;
	int32 CurrentSpawnQueueIndex = 0;
	int32 CurrentWaveIndex = INDEX_NONE;

	// 다음 웨이브 진행을 게이팅하는 직전 웨이브의 적들 (전멸 판정용).
	TArray<TWeakObjectPtr<ACPP_EnemyBase>> GatingWaveEnemies;
	bool bWaitingForGatingClear = false;
	int32 PendingNextWaveIndex = INDEX_NONE;

	bool bAllWavesFinished = false;
	bool bDefenseObjectiveInitialized = false;
	bool bBreachObjectiveInitialized = false;
	bool bRitualObjectiveInitialized = false;
	bool bObjectiveFailed = false;
	int32 NextBreachChanceIndex = 0;
	TArray<TWeakObjectPtr<ACPP_BreachChanceObject>> RuntimeBreachChanceObjects;
	TSet<TWeakObjectPtr<ACPP_EnemyBase>> ResolvedBreachEnemies;
	TSet<TWeakObjectPtr<ACPP_EnemyBase>> ResolvedRitualEnemies;

	// Zone Clear 등으로 정지된 뒤 잔여 콜백이 웨이브를 재개하지 못하도록 잠근다.
	bool bDeactivated = false;

	FTimerHandle IntraSpawnTimerHandle;
	FTimerHandle NextWaveTimerHandle;
};
