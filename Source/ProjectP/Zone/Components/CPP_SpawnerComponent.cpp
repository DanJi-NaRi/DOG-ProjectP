// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_SpawnerComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Spawning/CPP_EnemyWaveData.h"
#include "Enemy/Spawning/CPP_EnemySpawnPoint.h"
#include "Zone/Defense/CPP_DefenseObjective.h"
#include "Zone/Breach/CPP_BreachObjective.h"
#include "Zone/Breach/CPP_BreachChanceObject.h"
#include "Zone/Ritual/CPP_RitualObjective.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/PlayerCharacterBase.h"
#include "Kismet/GameplayStatics.h"

////////////////////////////
//! \author HanUl
//! \brief 스포너 컴포넌트의 기본 Tick을 비활성화한다.
//! \param
//! \return
UCPP_SpawnerComponent::UCPP_SpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

////////////////////////////
//! \author HanUl
//! \brief 종료 시 진행 중인 타이머를 정리한다.
//! \param EndPlayReason 종료 사유
//! \return
void UCPP_SpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(DefenseObjective))
	{
		DefenseObjective->OnObjectiveDestroyed.RemoveDynamic(
			this,
			&UCPP_SpawnerComponent::HandleDefenseObjectiveDestroyed);
	}

	if (IsValid(BreachObjective))
	{
		BreachObjective->OnBreachObjectiveHit.RemoveAll(this);
	}

	if (IsValid(RitualObjective))
	{
		RitualObjective->OnObjectiveEntered.RemoveAll(this);
		RitualObjective->OnGaugeFull.RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IntraSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

namespace
{
	// pre-Active 상태의 진행 순서 랭크. (Preparing < Ready < Entering) 그 외는 -1.
	int32 PreSpawnStateRank(EZoneState State)
	{
		switch (State)
		{
		case EZoneState::Preparing: return 0;
		case EZoneState::Ready:     return 1;
		case EZoneState::Entering:  return 2;
		default:                    return -1;
		}
	}

	// 프리스폰 타이밍 설정의 진행 순서 랭크.
	int32 PreSpawnTimingRank(EPreSpawnTiming Timing)
	{
		switch (Timing)
		{
		case EPreSpawnTiming::Ready:    return 1;
		case EPreSpawnTiming::Entering: return 2;
		case EPreSpawnTiming::Preparing:
		default:                        return 0;
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 설정된 PreSpawnTiming 이후 첫 pre-Active 상태에서 초기 배치 적을 1회 스폰한다. 첫 Zone처럼 창 중간 상태에서 시작해도 놓치지 않도록 "이상" 비교를 쓴다. (서버 전용)
//! \param CurrentState 현재 Zone 상태
//! \return
void UCPP_SpawnerComponent::TryPreSpawn(EZoneState CurrentState)
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	InitializeDefenseObjective();

	if (bPreSpawnDone)
	{
		return;
	}

	if (!WaveData || WaveData->PreSpawnEntries.Num() == 0)
	{
		return;
	}

	const int32 CurrentRank = PreSpawnStateRank(CurrentState);
	if (CurrentRank < 0 || CurrentRank < PreSpawnTimingRank(WaveData->PreSpawnTiming))
	{
		// pre-Active 상태가 아니거나, 아직 설정한 프리스폰 시점 이전.
		return;
	}

	bPreSpawnDone = true;
	BuildSpawnPointMap();

	for (const FEnemyWaveEntry& Entry : WaveData->PreSpawnEntries)
	{
		if (!Entry.EnemyClass)
		{
			continue;
		}
		if (Entry.TargetPolicy == EEnemySpawnTargetPolicy::RitualObjective)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Spawner] RitualObjective is not supported in PreSpawnEntries. Spawn skipped - Zone: %s, EnemyClass: %s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Entry.EnemyClass.Get()));
			continue;
		}

		for (int32 n = 0; n < Entry.Count; ++n)
		{
			SpawnOneEnemy(Entry.EnemyClass, Entry.SpawnPointTag, Entry.TargetPolicy);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 웨이브 진행을 시작한다. 웨이브가 없으면 즉시 완료를 통지한다. 프리스폰 적을 첫 웨이브의 게이팅 대상으로 삼아 첫 웨이브도 일반 웨이브와 동일한 진행 규칙을 탄다.
//! \param
//! \return
void UCPP_SpawnerComponent::ActivateSpawner()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	if (bDeactivated)
	{
		return;
	}

	InitializeDefenseObjective();
	InitializeBreachObjective();
	InitializeRitualObjective();
	if (IsValid(DefenseObjective))
	{
		DefenseObjective->SetDamageEnabled(true);
	}
	if (IsValid(BreachObjective))
	{
		BreachObjective->SetDamageEnabled(HasValidBreachConfiguration());
	}
	if (IsValid(RitualObjective))
	{
		RitualObjective->SetAbsorptionEnabled(HasValidRitualConfiguration());
	}

	BuildSpawnPointMap();

	if (!WaveData || WaveData->Waves.Num() == 0)
	{
		FinishAllWaves();
		return;
	}

	// 프리스폰 적을 첫 웨이브의 "직전 웨이브"로 취급한다.
	// → 첫 웨이브가 OnPreviousCleared/Hybrid면 프리스폰 전멸로 트리거되고(없으면 즉시), Timed면 StartDelay만 적용된다.
	GatingWaveEnemies.Reset();
	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : AllSpawnedEnemies)
	{
		if (ACPP_EnemyBase* Enemy = WeakEnemy.Get())
		{
			GatingWaveEnemies.Add(Enemy);
			Enemy->OnEnemyDeath.AddUniqueDynamic(this, &UCPP_SpawnerComponent::HandleSpawnedEnemyDeath);
		}
	}

	ScheduleNextWave(0);
}

////////////////////////////
//! \author HanUl
//! \brief 스폰을 영구 정지한다. 모든 타이머를 정리하고, 생존 중인 스폰 적을 일괄 사망 처리한다. (Zone Clear 시 호출, Survival 종료 처리 포함)
//! \param
//! \return
void UCPP_SpawnerComponent::DeactivateSpawner()
{
	if (!HasOwnerAuthority() || bDeactivated)
	{
		return;
	}

	StopSpawningWithoutKilling();
	if (IsValid(DefenseObjective))
	{
		DefenseObjective->SetDamageEnabled(false);
	}
	if (IsValid(BreachObjective))
	{
		BreachObjective->SetDamageEnabled(false);
	}
	if (IsValid(RitualObjective))
	{
		RitualObjective->SetAbsorptionEnabled(false);
	}

	// 남은 생존 적 일괄 사망 처리. (전멸 클리어 존은 이 시점에 생존이 없어 무해)
	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : AllSpawnedEnemies)
	{
		ACPP_EnemyBase* Enemy = WeakEnemy.Get();
		if (IsValid(Enemy) && !Enemy->IsDead())
		{
			Enemy->ForceKillWithoutRewards();
		}
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 거점 방어 Zone 재시작을 위해 기존 Enemy와 웨이브 런타임 상태 및 거점을 초기화한다.
void UCPP_SpawnerComponent::ResetSpawner()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	StopSpawningWithoutKilling();

	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : AllSpawnedEnemies)
	{
		if (ACPP_EnemyBase* Enemy = WeakEnemy.Get())
		{
			Enemy->OnEnemyDeath.RemoveDynamic(this, &UCPP_SpawnerComponent::HandleSpawnedEnemyDeath);
			if (!Enemy->IsDead())
			{
				Enemy->ForceKillWithoutRewards();
			}
		}
	}

	AllSpawnedEnemies.Reset();
	CurrentSpawnQueue.Reset();
	CurrentSpawnQueueIndex = 0;
	CurrentWaveIndex = INDEX_NONE;
	GatingWaveEnemies.Reset();
	PendingNextWaveIndex = INDEX_NONE;
	bWaitingForGatingClear = false;
	bPreSpawnDone = false;
	bAllWavesFinished = false;
	bObjectiveFailed = false;
	bDeactivated = false;
	NextBreachChanceIndex = 0;
	ResolvedBreachEnemies.Reset();
	ResolvedRitualEnemies.Reset();

	if (!bDefenseObjectiveInitialized)
	{
		InitializeDefenseObjective();
	}
	else if (IsValid(DefenseObjective))
	{
		DefenseObjective->ResetObjective();
	}

	if (!bBreachObjectiveInitialized)
	{
		InitializeBreachObjective();
	}
	else if (IsValid(BreachObjective))
	{
		BreachObjective->ResetObjective();
		for (const TWeakObjectPtr<ACPP_BreachChanceObject>& WeakChanceObject : RuntimeBreachChanceObjects)
		{
			if (ACPP_BreachChanceObject* ChanceObject = WeakChanceObject.Get())
			{
				ChanceObject->RestoreChance();
			}
		}
	}

	if (!bRitualObjectiveInitialized)
	{
		InitializeRitualObjective();
	}
	else if (IsValid(RitualObjective))
	{
		RitualObjective->ResetObjective();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 치트 전용 즉시 스폰. WaveData의 적 구성(프리스폰+웨이브 순서, 중복 제거)을 순환하며 Count마리를
//!        태그 무관 전체 스폰 지점에 라운드로빈 분배 스폰한다. 지점이 없으면 Zone 위치에 스폰한다.
//!        일반 스폰과 동일한 등록 경로를 타므로 Active 존이면 클리어 집계에 포함되고, 웨이브 게이팅에는 영향을 주지 않는다.
//!        존이 이미 Clear되어 스포너가 정지된 상태여도 스폰은 수행된다. (서버 전용)
//! \param Count 스폰할 마릿수
//! \param FallbackEnemyClass WaveData에 적 클래스가 하나도 없을 때 사용할 폴백 클래스
//! \return 실제 스폰에 성공한 마릿수 (스폰할 클래스가 전혀 없으면 0)
int32 UCPP_SpawnerComponent::CheatSpawnEnemies(int32 Count, TSubclassOf<ACPP_EnemyBase> FallbackEnemyClass)
{
	if (!HasOwnerAuthority() || Count <= 0)
	{
		return 0;
	}

	// 스폰할 클래스 후보: 이 존 WaveData의 적 구성. 비어 있으면 폴백 클래스.
	TArray<TSubclassOf<ACPP_EnemyBase>> CandidateClasses;
	if (WaveData)
	{
		for (const FEnemyWaveEntry& Entry : WaveData->PreSpawnEntries)
		{
			if (Entry.EnemyClass)
			{
				CandidateClasses.AddUnique(Entry.EnemyClass);
			}
		}

		for (const FEnemyWave& Wave : WaveData->Waves)
		{
			for (const FEnemyWaveEntry& Entry : Wave.Entries)
			{
				if (Entry.EnemyClass)
				{
					CandidateClasses.AddUnique(Entry.EnemyClass);
				}
			}
		}
	}

	if (CandidateClasses.Num() == 0 && FallbackEnemyClass)
	{
		CandidateClasses.Add(FallbackEnemyClass);
	}

	if (CandidateClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Spawner] CheatSpawnEnemies: no enemy class to spawn - Zone: %s"), *GetNameSafe(GetOwner()));
		return 0;
	}

	// 스폰 위치 후보: 태그 무관 전체 유효 스폰 지점.
	TArray<ACPP_EnemySpawnPoint*> ValidPoints;
	for (ACPP_EnemySpawnPoint* Point : SpawnPoints)
	{
		if (IsValid(Point))
		{
			ValidPoints.Add(Point);
		}
	}

	int32 SpawnedCount = 0;
	for (int32 n = 0; n < Count; ++n)
	{
		ACPP_EnemySpawnPoint* Point = ValidPoints.Num() > 0 ? ValidPoints[n % ValidPoints.Num()] : nullptr;
		ACPP_EnemyBase* Spawned = SpawnEnemyAt(
			CandidateClasses[n % CandidateClasses.Num()],
			Point,
			EEnemySpawnTargetPolicy::PlayerBalanced);
		RegisterSpawnedEnemy(Spawned);
		if (Spawned)
		{
			++SpawnedCount;
		}
	}

	return SpawnedCount;
}

////////////////////////////
//! \author HanUl
//! \brief 배치된 스폰 지점들을 태그별로 그룹화한다.
//! \param
//! \return
void UCPP_SpawnerComponent::BuildSpawnPointMap()
{
	// 프리스폰과 웨이브가 라운드로빈 커서를 공유하도록 1회만 구성한다.
	if (bSpawnPointMapBuilt)
	{
		return;
	}
	bSpawnPointMapBuilt = true;

	TagToSpawnPoints.Reset();
	TagRoundRobinIndex.Reset();

	for (ACPP_EnemySpawnPoint* Point : SpawnPoints)
	{
		if (!IsValid(Point))
		{
			continue;
		}

		TagToSpawnPoints.FindOrAdd(Point->GetSpawnPointTag()).Add(Point);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 배치된 거점의 초기 상태를 복구하고 파괴 이벤트를 한 번만 구독한다.
void UCPP_SpawnerComponent::InitializeDefenseObjective()
{
	if (bDefenseObjectiveInitialized || !IsValid(DefenseObjective))
	{
		return;
	}

	bDefenseObjectiveInitialized = true;
	bObjectiveFailed = false;
	DefenseObjective->OnObjectiveDestroyed.AddUniqueDynamic(
		this,
		&UCPP_SpawnerComponent::HandleDefenseObjectiveDestroyed);
	DefenseObjective->ResetObjective();
}

////////////////////////////
//! \author HanSeul
//! \brief 돌파 목표와 피격 이벤트를 연결하고 등록된 기회 오브젝트를 초기 상태로 복구한다.
void UCPP_SpawnerComponent::InitializeBreachObjective()
{
	if (bBreachObjectiveInitialized || !IsValid(BreachObjective))
	{
		return;
	}

	bBreachObjectiveInitialized = true;
	bObjectiveFailed = false;
	NextBreachChanceIndex = 0;
	ResolvedBreachEnemies.Reset();
	BuildRuntimeBreachChanceObjects();
	BreachObjective->OnBreachObjectiveHit.AddUObject(
		this,
		&UCPP_SpawnerComponent::HandleBreachObjectiveHit);
	BreachObjective->ResetObjective();

	for (const TWeakObjectPtr<ACPP_BreachChanceObject>& WeakChanceObject : RuntimeBreachChanceObjects)
	{
		if (ACPP_BreachChanceObject* ChanceObject = WeakChanceObject.Get())
		{
			ChanceObject->RestoreChance();
		}
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 의식 목표의 범위 진입과 게이지 완료 이벤트를 연결하고 초기 상태로 복구한다.
void UCPP_SpawnerComponent::InitializeRitualObjective()
{
	if (bRitualObjectiveInitialized || !IsValid(RitualObjective))
	{
		return;
	}

	bRitualObjectiveInitialized = true;
	bObjectiveFailed = false;
	ResolvedRitualEnemies.Reset();
	RitualObjective->OnObjectiveEntered.AddUObject(
		this,
		&UCPP_SpawnerComponent::HandleRitualObjectiveEntered);
	RitualObjective->OnGaugeFull.AddUObject(
		this,
		&UCPP_SpawnerComponent::HandleRitualGaugeFull);
	RitualObjective->ResetObjective();
}

void UCPP_SpawnerComponent::BuildRuntimeBreachChanceObjects()
{
	RuntimeBreachChanceObjects.Reset();
	for (ACPP_BreachChanceObject* ChanceObject : BreachChanceObjects)
	{
		if (IsValid(ChanceObject) && !RuntimeBreachChanceObjects.Contains(ChanceObject))
		{
			RuntimeBreachChanceObjects.Add(ChanceObject);
		}
	}
}

bool UCPP_SpawnerComponent::HasValidBreachConfiguration() const
{
	return IsValid(BreachObjective) && RuntimeBreachChanceObjects.Num() > 0;
}

bool UCPP_SpawnerComponent::HasValidRitualConfiguration() const
{
	return IsValid(RitualObjective);
}

AActor* UCPP_SpawnerComponent::ResolveObjectiveTarget(EEnemySpawnTargetPolicy TargetPolicy) const
{
	if (TargetPolicy == EEnemySpawnTargetPolicy::DefenseObjective)
	{
		return DefenseObjective;
	}
	if (TargetPolicy == EEnemySpawnTargetPolicy::BreachObjective)
	{
		return BreachObjective;
	}
	if (TargetPolicy == EEnemySpawnTargetPolicy::RitualObjective)
	{
		return RitualObjective;
	}
	return nullptr;
}

bool UCPP_SpawnerComponent::ConsumeNextBreachChance()
{
	while (RuntimeBreachChanceObjects.IsValidIndex(NextBreachChanceIndex))
	{
		ACPP_BreachChanceObject* ChanceObject = RuntimeBreachChanceObjects[NextBreachChanceIndex++].Get();
		if (IsValid(ChanceObject) && !ChanceObject->IsConsumed())
		{
			ChanceObject->ConsumeChance();
			return true;
		}
	}
	return false;
}

void UCPP_SpawnerComponent::FailObjectiveEncounter()
{
	if (bObjectiveFailed)
	{
		return;
	}

	bObjectiveFailed = true;
	StopSpawningWithoutKilling();
	if (IsValid(DefenseObjective))
	{
		DefenseObjective->SetDamageEnabled(false);
	}
	if (IsValid(BreachObjective))
	{
		BreachObjective->SetDamageEnabled(false);
	}
	if (IsValid(RitualObjective))
	{
		RitualObjective->SetAbsorptionEnabled(false);
	}
	ForceKillAllPlayers();
}

////////////////////////////
//! \author HanSeul
//! \brief 기존 Enemy는 유지한 채 추가 소환과 웨이브 진행 타이머만 정지한다.
void UCPP_SpawnerComponent::StopSpawningWithoutKilling()
{
	bDeactivated = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IntraSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	PendingNextWaveIndex = INDEX_NONE;
	bWaitingForGatingClear = false;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버의 모든 살아 있는 플레이어에게 기존 ForceKill 사망 경로를 적용한다.
void UCPP_SpawnerComponent::ForceKillAllPlayers() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APlayerCharacterBase* Player = PlayerState
			? Cast<APlayerCharacterBase>(PlayerState->GetPawn())
			: nullptr;
		if (IsValid(Player) && !Player->IsDead())
		{
			Player->ForceKill();
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 지정 웨이브의 Entry들을 개별 스폰 큐로 평탄화하고 스폰을 시작한다.
//! \param WaveIndex 시작할 웨이브 인덱스
//! \return
void UCPP_SpawnerComponent::BeginWave(int32 WaveIndex)
{
	if (!WaveData || !WaveData->Waves.IsValidIndex(WaveIndex))
	{
		FinishAllWaves();
		return;
	}

	CurrentWaveIndex = WaveIndex;
	const FEnemyWave& Wave = WaveData->Waves[WaveIndex];

	FillSpawnQueueFromEntries(Wave);

	// 이 웨이브의 적들을 다음 웨이브 게이팅용으로 새로 수집한다.
	GatingWaveEnemies.Reset();

	if (CurrentSpawnQueue.Num() == 0)
	{
		OnWaveSpawnCompleted(WaveIndex);
		return;
	}

	if (Wave.SpawnMode == EWaveSpawnMode::Budgeted || Wave.SpawnMode == EWaveSpawnMode::Survival)
	{
		ShuffleSpawnQueue();
		SpawnBudgetedTick();
	}
	else
	{
		SpawnNextInQueue();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 웨이브 Entries를 개별 스폰 큐로 평탄화하고 커서를 초기화한다.
//! \param Wave 큐를 만들 웨이브 정의
//! \return
void UCPP_SpawnerComponent::FillSpawnQueueFromEntries(const FEnemyWave& Wave)
{
	CurrentSpawnQueue.Reset();
	for (const FEnemyWaveEntry& Entry : Wave.Entries)
	{
		if (!Entry.EnemyClass)
		{
			continue;
		}

		for (int32 n = 0; n < Entry.Count; ++n)
		{
			CurrentSpawnQueue.Add(FPendingSpawn{ Entry.EnemyClass, Entry.SpawnPointTag, Entry.TargetPolicy });
		}
	}
	CurrentSpawnQueueIndex = 0;
}

////////////////////////////
//! \author HanUl
//! \brief 스폰 큐를 섞어 타입이 고르게 나오도록 한다. (서버 전용 스폰이라 클라 간 불일치 없음)
//! \param
//! \return
void UCPP_SpawnerComponent::ShuffleSpawnQueue()
{
	for (int32 i = CurrentSpawnQueue.Num() - 1; i > 0; --i)
	{
		CurrentSpawnQueue.Swap(i, FMath::RandRange(0, i));
	}
}

////////////////////////////
//! \author HanUl
//! \brief 현재 웨이브 큐에서 다음 적을 스폰한다. SpawnInterval이 0이면 남은 전부를 즉시 스폰하고, 아니면 간격을 두고 예약한다.
//! \param
//! \return
void UCPP_SpawnerComponent::SpawnNextInQueue()
{
	if (bDeactivated || !WaveData || !WaveData->Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FEnemyWave& Wave = WaveData->Waves[CurrentWaveIndex];

	do
	{
		const FPendingSpawn& Item = CurrentSpawnQueue[CurrentSpawnQueueIndex];

		if (ACPP_EnemyBase* Spawned = SpawnOneEnemy(Item.EnemyClass, Item.Tag, Item.TargetPolicy))
		{
			// 웨이브 적만 진행 게이팅 대상에 넣고 사망을 구독한다. (프리스폰 적은 제외)
			GatingWaveEnemies.Add(Spawned);
			Spawned->OnEnemyDeath.AddUniqueDynamic(this, &UCPP_SpawnerComponent::HandleSpawnedEnemyDeath);
		}

		++CurrentSpawnQueueIndex;
	}
	while (Wave.SpawnInterval <= 0.0f && CurrentSpawnQueueIndex < CurrentSpawnQueue.Num());

	if (CurrentSpawnQueueIndex < CurrentSpawnQueue.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(
			IntraSpawnTimerHandle, this, &UCPP_SpawnerComponent::SpawnNextInQueue, Wave.SpawnInterval, false);
	}
	else
	{
		OnWaveSpawnCompleted(CurrentWaveIndex);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Budgeted 모드 스폰 틱. 동시 상한(MaxConcurrent) 아래에서 배치(SpawnBatchMin~Max)로 스폰하고, 큐가 빌 때까지 SpawnInterval마다 반복한다. 상한에 걸리면 이번 틱은 대기한다.
//! \param
//! \return
void UCPP_SpawnerComponent::SpawnBudgetedTick()
{
	if (bDeactivated || !WaveData || !WaveData->Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FEnemyWave& Wave = WaveData->Waves[CurrentWaveIndex];
	int32 Remaining = CurrentSpawnQueue.Num() - CurrentSpawnQueueIndex;

	if (Remaining <= 0)
	{
		// Survival: 큐를 다시 채우고 재셔플해 무한 스폰. (Entries의 Count = 한 사이클의 구성 비율)
		if (Wave.SpawnMode == EWaveSpawnMode::Survival)
		{
			FillSpawnQueueFromEntries(Wave);
			ShuffleSpawnQueue();
			Remaining = CurrentSpawnQueue.Num();
			if (Remaining <= 0)
			{
				return;
			}
		}
		else
		{
			OnWaveSpawnCompleted(CurrentWaveIndex);
			return;
		}
	}

	// 이번 틱에 스폰 가능한 여유. MaxConcurrent<=0이면 상한 없음.
	const int32 Room = (Wave.MaxConcurrent > 0) ? (Wave.MaxConcurrent - CountGatingAlive()) : Remaining;

	if (Room > 0)
	{
		const int32 BatchMin = FMath::Max(1, FMath::Min(Wave.SpawnBatchMin, Wave.SpawnBatchMax));
		const int32 BatchMax = FMath::Max(Wave.SpawnBatchMin, Wave.SpawnBatchMax);
		const int32 BatchN = FMath::Min3(FMath::RandRange(BatchMin, BatchMax), Room, Remaining);

		for (int32 s = 0; s < BatchN; ++s)
		{
			const FPendingSpawn& Item = CurrentSpawnQueue[CurrentSpawnQueueIndex];

			if (ACPP_EnemyBase* Spawned = SpawnOneEnemy(Item.EnemyClass, Item.Tag, Item.TargetPolicy))
			{
				GatingWaveEnemies.Add(Spawned);
				Spawned->OnEnemyDeath.AddUniqueDynamic(this, &UCPP_SpawnerComponent::HandleSpawnedEnemyDeath);
			}

			++CurrentSpawnQueueIndex;
		}
	}
	// Room<=0이면 이번 틱은 스폰 없이 대기(다음 틱에 재확인).

	// Survival은 스폰 완료가 없으므로 항상 다음 틱을 예약한다. (정지는 DeactivateSpawner가 담당)
	if (Wave.SpawnMode == EWaveSpawnMode::Survival || CurrentSpawnQueueIndex < CurrentSpawnQueue.Num())
	{
		GetWorld()->GetTimerManager().SetTimer(
			IntraSpawnTimerHandle, this, &UCPP_SpawnerComponent::SpawnBudgetedTick, FMath::Max(Wave.SpawnInterval, 0.01f), false);
	}
	else
	{
		OnWaveSpawnCompleted(CurrentWaveIndex);
	}
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 등록/통지 마무리를 RegisterSpawnedEnemy로 분리 (치트 스폰 경로와 공유)
//! \brief 태그 지점에 적 1마리를 스폰하고 전체 목록 등록 + OnEnemySpawned 통지를 수행한다. 프리스폰/웨이브 공통 경로.
//! \param EnemyClass 스폰할 적 클래스
//! \param SpawnPointTag 스폰 위치 태그
//! \return 스폰된 적, 실패 시 nullptr
//! \param TargetPolicy 생성된 Enemy에 전달할 전투 타겟 정책
ACPP_EnemyBase* UCPP_SpawnerComponent::SpawnOneEnemy(
	TSubclassOf<ACPP_EnemyBase> EnemyClass,
	FName SpawnPointTag,
	EEnemySpawnTargetPolicy TargetPolicy)
{
	if (TargetPolicy == EEnemySpawnTargetPolicy::DefenseObjective && !IsValid(DefenseObjective))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Spawner] DefenseObjective target is not assigned. Spawn skipped - Zone: %s, EnemyClass: %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(EnemyClass.Get()));
		return nullptr;
	}
	if (TargetPolicy == EEnemySpawnTargetPolicy::BreachObjective && !HasValidBreachConfiguration())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Spawner] BreachObjective target or chance objects are not assigned. Spawn skipped - Zone: %s, EnemyClass: %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(EnemyClass.Get()));
		return nullptr;
	}
	if (TargetPolicy == EEnemySpawnTargetPolicy::RitualObjective && !HasValidRitualConfiguration())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Spawner] RitualObjective target is not assigned. Spawn skipped - Zone: %s, EnemyClass: %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(EnemyClass.Get()));
		return nullptr;
	}

	ACPP_EnemySpawnPoint* Point = PickSpawnPointForTag(SpawnPointTag);
	ACPP_EnemyBase* Spawned = SpawnEnemyAt(EnemyClass, Point, TargetPolicy);
	RegisterSpawnedEnemy(Spawned);
	return Spawned;
}

////////////////////////////
//! \author 준혁
//! \brief 스폰 성공한 적을 전체 목록(AllSpawnedEnemies)에 등록하고 OnEnemySpawned를 통지한다. 일반/치트 스폰의 공통 마무리.
//! \param Spawned 방금 스폰된 적 (nullptr이면 무시)
//! \return
void UCPP_SpawnerComponent::RegisterSpawnedEnemy(ACPP_EnemyBase* Spawned)
{
	if (!Spawned)
	{
		return;
	}

	AllSpawnedEnemies.Add(Spawned);
	OnEnemySpawned.Broadcast(Spawned);
}

////////////////////////////
//! \author HanUl
//! \editor HanSeul - Deferred Spawn 중 Zone 고정 EnemyLevel을 BeginPlay 전에 전달한다.
//! \brief 지정 스폰 지점에 적을 스폰한다. 지점이 없으면 Zone(Owner) 위치에 스폰하며, 겹침은 콜리전 보정으로 처리한다.
//! \param EnemyClass 스폰할 적 클래스
//! \param SpawnPoint 스폰 위치 지점 (nullptr이면 Owner 위치)
//! \return 스폰된 적, 실패 시 nullptr
//! \param TargetPolicy 생성된 Enemy에 전달할 전투 타겟 정책
ACPP_EnemyBase* UCPP_SpawnerComponent::SpawnEnemyAt(
	TSubclassOf<ACPP_EnemyBase> EnemyClass,
	ACPP_EnemySpawnPoint* SpawnPoint,
	EEnemySpawnTargetPolicy TargetPolicy)
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		return nullptr;
	}

	const FTransform SourceTransform =
		SpawnPoint ? SpawnPoint->GetActorTransform() : GetOwner()->GetActorTransform();

	// 스폰 지점 사각형(회전 반영) 안 랜덤 위치로 산포한 뒤, 네비메시 위로 보정한다. (익스텐트 0이면 원위치)
	FVector SpawnLocation = GetRandomizedSpawnLocation(SourceTransform,
		SpawnPoint ? SpawnPoint->GetSpawnExtent() : FVector2D::ZeroVector);

	// NavMesh 표면에서 적 캡슐의 반높이와 5cm 여유만큼 올려 바닥과 겹치지 않은 상태로 스폰한다.
	if (const ACPP_EnemyBase* EnemyDefault = EnemyClass->GetDefaultObject<ACPP_EnemyBase>())
	{
		if (const UCapsuleComponent* Capsule = EnemyDefault->GetCapsuleComponent())
		{
			SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight() + 5.f;
		}
	}

	// 스케일은 전파하지 않는다. Zone/스폰지점 스케일이 적 크기에 영향 주지 않도록 위치/회전만 사용(스케일=1).
	const FTransform SpawnTransform(SourceTransform.GetRotation(), SpawnLocation);

	ACPP_EnemyBase* Spawned = World->SpawnActorDeferred<ACPP_EnemyBase>(
		EnemyClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Spawned)
	{
		return nullptr;
	}

	Spawned->SetEnemyLevel(EnemyLevel);
	Spawned->ConfigureSpawnTarget(
		TargetPolicy,
		ResolveObjectiveTarget(TargetPolicy));
	UGameplayStatics::FinishSpawningActor(Spawned, SpawnTransform);
	return Spawned;
}

////////////////////////////
//! \author HanUl
//! \brief SourceTransform 중심의 XY 사각형(하프익스텐트 Extent, 회전 반영) 안 랜덤 위치를 뽑아 네비메시 위로 투영한다. (서버에서만 호출됨)
//! \param SourceTransform 스폰 지점 트랜스폼 (위치 + 회전)
//! \param Extent 산포 사각형의 XY 하프익스텐트(cm). 둘 다 0 이하면 산포하지 않는다.
//! \return 네비메시 위로 보정된 스폰 위치. 익스텐트 0 또는 투영 실패 시 원위치.
FVector UCPP_SpawnerComponent::GetRandomizedSpawnLocation(const FTransform& SourceTransform, const FVector2D& Extent) const
{
	const FVector Center = SourceTransform.GetLocation();
	if (Extent.X <= 0.f && Extent.Y <= 0.f)
	{
		return Center;
	}

	// 사각형이라 X/Y 각각 균일 분포. 로컬 오프셋을 스폰 지점 회전으로 돌려 월드로 변환.
	const FVector LocalOffset(FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y), 0.f);
	const FVector Candidate = Center + SourceTransform.GetRotation().RotateVector(LocalOffset);

	// 네비메시 위로 투영. 실패하면 낙사/벽낌 방지를 위해 스폰 지점 원위치로 폴백.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		// 수직 탐색 범위는 익스텐트 기반으로 잡되 최소 200cm 보장 (경사/단차 대응).
		const float MaxXY = FMath::Max(Extent.X, Extent.Y);
		const FVector QueryExtent(MaxXY, MaxXY, FMath::Max(MaxXY, 200.f));
		if (NavSys->ProjectPointToNavigation(Candidate, Projected, QueryExtent))
		{
			return Projected.Location;
		}
		return Center;
	}

	// 네비 시스템이 없으면 Z만 원본 유지한 랜덤 위치 사용.
	return Candidate;
}

////////////////////////////
//! \author HanUl
//! \brief 태그에 해당하는 스폰 지점을 라운드로빈으로 하나 선택한다. 무효 항목은 건너뛴다.
//! \param Tag 스폰 지점 그룹 태그
//! \return 선택된 스폰 지점, 없거나 모두 무효면 nullptr
ACPP_EnemySpawnPoint* UCPP_SpawnerComponent::PickSpawnPointForTag(FName Tag)
{
	TArray<TWeakObjectPtr<ACPP_EnemySpawnPoint>>* Points = TagToSpawnPoints.Find(Tag);
	if (!Points || Points->Num() == 0)
	{
		// 지정 태그의 스폰 지점 없음 - 레벨 배치/WaveData 태그 확인 필요
		UE_LOG(LogTemp, Warning, TEXT("[Spawner] No SpawnPoint for tag '%s' - Zone: %s"),
			*Tag.ToString(), *GetNameSafe(GetOwner()));
		return nullptr;
	}

	int32& Cursor = TagRoundRobinIndex.FindOrAdd(Tag);
	for (int32 Tries = 0; Tries < Points->Num(); ++Tries)
	{
		ACPP_EnemySpawnPoint* Point = (*Points)[Cursor % Points->Num()].Get();
		++Cursor;
		if (IsValid(Point))
		{
			return Point;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 한 웨이브의 스폰이 끝났을 때 호출. 다음 웨이브가 있으면 진행을 예약하고, 없으면 전체 완료를 통지한다.
//! \param WaveIndex 스폰이 끝난 웨이브 인덱스
//! \return
void UCPP_SpawnerComponent::OnWaveSpawnCompleted(int32 WaveIndex)
{
	const int32 NextWaveIndex = WaveIndex + 1;
	if (!WaveData || !WaveData->Waves.IsValidIndex(NextWaveIndex))
	{
		FinishAllWaves();
		return;
	}

	ScheduleNextWave(NextWaveIndex);
}

////////////////////////////
//! \author HanUl
//! \brief 다음 웨이브의 Trigger에 따라 진행을 예약한다. Hybrid는 전멸/타이머 중 먼저 오는 쪽이 진행한다.
//! \param NextWaveIndex 예약할 다음 웨이브 인덱스
//! \return
void UCPP_SpawnerComponent::ScheduleNextWave(int32 NextWaveIndex)
{
	const FEnemyWave& NextWave = WaveData->Waves[NextWaveIndex];
	PendingNextWaveIndex = NextWaveIndex;

	const bool bUseTimer = (NextWave.Trigger == EWaveTrigger::Timed || NextWave.Trigger == EWaveTrigger::Hybrid);
	const bool bUseClear = (NextWave.Trigger == EWaveTrigger::OnPreviousCleared || NextWave.Trigger == EWaveTrigger::Hybrid);

	if (bUseClear)
	{
		bWaitingForGatingClear = true;
		// 스폰 도중/직후 이미 전멸했다면 휴식 후 진행.
		if (CountGatingAlive() == 0)
		{
			AdvanceAfterClear();
			return;
		}
	}

	if (bUseTimer)
	{
		GetWorld()->GetTimerManager().SetTimer(
			NextWaveTimerHandle, this, &UCPP_SpawnerComponent::HandleNextWaveTimer,
			FMath::Max(NextWave.StartDelay, 0.01f), false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 전멸 트리거 경로. 방금 클리어된 웨이브의 RestAfterClear만큼 쉰 뒤 다음 웨이브로 진행한다. 휴식이 0이면 즉시 진행한다. (타이머 경로는 이 함수를 거치지 않아 휴식이 붙지 않는다)
//! \param
//! \return
void UCPP_SpawnerComponent::AdvanceAfterClear()
{
	if (bDeactivated || PendingNextWaveIndex == INDEX_NONE)
	{
		return;
	}

	// 전멸로 넘어가는 경로: 더 이상 사망 재계수로 재진입하지 않도록 잠그고, Hybrid 타임아웃 타이머를 취소한다.
	bWaitingForGatingClear = false;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	// 방금 클리어된 웨이브(= 현재 게이팅 대상 웨이브)의 휴식 시간. 프리스폰→첫 웨이브 전환 시엔 유효 인덱스가 없어 0.
	const float Rest = (WaveData && WaveData->Waves.IsValidIndex(CurrentWaveIndex))
		? WaveData->Waves[CurrentWaveIndex].RestAfterClear
		: 0.0f;

	if (Rest > 0.0f && World)
	{
		World->GetTimerManager().SetTimer(
			NextWaveTimerHandle, this, &UCPP_SpawnerComponent::HandleNextWaveTimer, Rest, false);
	}
	else
	{
		AdvanceToPendingWave();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 예약된 다음 웨이브로 진행한다. 타이머와 전멸 콜백이 중복 호출되어도 한 번만 진행되도록 방어한다.
//! \param
//! \return
void UCPP_SpawnerComponent::AdvanceToPendingWave()
{
	if (PendingNextWaveIndex == INDEX_NONE)
	{
		return;
	}

	const int32 Next = PendingNextWaveIndex;
	PendingNextWaveIndex = INDEX_NONE;
	bWaitingForGatingClear = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	BeginWave(Next);
}

////////////////////////////
//! \author HanUl
//! \brief 모든 웨이브 스폰 완료를 1회 통지한다. (사망과 무관하게 "더 이상 스폰 없음"을 의미)
//! \param
//! \return
void UCPP_SpawnerComponent::FinishAllWaves()
{
	if (bAllWavesFinished || bObjectiveFailed)
	{
		return;
	}

	bAllWavesFinished = true;
	OnAllWavesFinished.Broadcast();
}

////////////////////////////
//! \author HanUl
//! \brief 게이팅 대상(직전 웨이브) 적들 중 생존 수를 센다.
//! \param
//! \return 생존한 적의 수
int32 UCPP_SpawnerComponent::CountGatingAlive() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<ACPP_EnemyBase>& WeakEnemy : GatingWaveEnemies)
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
//! \brief 스폰된 적 사망 시 호출. 전멸 게이팅 중이고 직전 웨이브가 전멸했다면 다음 웨이브로 진행한다.
//! \param
//! \return
void UCPP_SpawnerComponent::HandleSpawnedEnemyDeath()
{
	if (!bWaitingForGatingClear)
	{
		return;
	}

	if (CountGatingAlive() == 0)
	{
		AdvanceAfterClear();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 거점 파괴를 실패로 확정하고 기존 Enemy를 유지한 채 소환을 정지한 뒤 플레이어를 전멸시킨다.
void UCPP_SpawnerComponent::HandleDefenseObjectiveDestroyed()
{
	if (!HasOwnerAuthority() || bObjectiveFailed)
	{
		return;
	}

	FailObjectiveEncounter();
}

////////////////////////////
//! \author HanSeul
//! \brief 돌파 목표에 유효한 Enemy 공격이 적중하면 해당 Enemy당 기회 하나만 소모하고 보상 없이 사망시킨다.
//! \param SourceActor 돌파 목표에 피해를 전달한 Actor
void UCPP_SpawnerComponent::HandleBreachObjectiveHit(AActor* SourceActor)
{
	if (!HasOwnerAuthority() || bObjectiveFailed || !HasValidBreachConfiguration())
	{
		return;
	}

	ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(SourceActor);
	if (!IsValid(Enemy)
		|| Enemy->IsDead()
		|| Enemy->GetSpawnTargetPolicy() != EEnemySpawnTargetPolicy::BreachObjective
		|| Enemy->GetAssignedObjectiveTarget() != BreachObjective
		|| ResolvedBreachEnemies.Contains(Enemy))
	{
		return;
	}

	ResolvedBreachEnemies.Add(Enemy);
	if (!ConsumeNextBreachChance())
	{
		return;
	}

	while (RuntimeBreachChanceObjects.IsValidIndex(NextBreachChanceIndex)
		&& !RuntimeBreachChanceObjects[NextBreachChanceIndex].IsValid())
	{
		++NextBreachChanceIndex;
	}

	if (!RuntimeBreachChanceObjects.IsValidIndex(NextBreachChanceIndex))
	{
		FailObjectiveEncounter();
	}

	Enemy->ForceKillWithoutRewards();
}

////////////////////////////
//! \author HanSeul
//! \brief 의식 범위에 진입한 유효한 Ritual Enemy를 한 번만 흡수하고 보상 없이 사망시킨다.
//! \param SourceActor 의식 목표의 흡수 범위에 진입한 Actor
void UCPP_SpawnerComponent::HandleRitualObjectiveEntered(AActor* SourceActor)
{
	if (!HasOwnerAuthority() || bObjectiveFailed || !HasValidRitualConfiguration())
	{
		return;
	}

	ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(SourceActor);
	if (!IsValid(Enemy)
		|| Enemy->IsDead()
		|| Enemy->GetSpawnTargetPolicy() != EEnemySpawnTargetPolicy::RitualObjective
		|| Enemy->GetAssignedObjectiveTarget() != RitualObjective
		|| ResolvedRitualEnemies.Contains(Enemy))
	{
		return;
	}

	ResolvedRitualEnemies.Add(Enemy);
	if (RitualObjective->ApplyAbsorption(Enemy))
	{
		Enemy->ForceKillWithoutRewards();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 의식 게이지 100% 도달을 공통 전투 실패 처리로 전달한다.
void UCPP_SpawnerComponent::HandleRitualGaugeFull()
{
	if (HasOwnerAuthority())
	{
		FailObjectiveEncounter();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 웨이브 주기/휴식 타이머 만료 시 다음 웨이브로 진행한다. (Timed/Hybrid 타임아웃, 또는 전멸 후 휴식)
//! \param
//! \return
void UCPP_SpawnerComponent::HandleNextWaveTimer()
{
	AdvanceToPendingWave();
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 Owner Actor가 서버 권한을 가지고 있는지 확인한다.
//! \param
//! \return 서버 권한이 있으면 true
bool UCPP_SpawnerComponent::HasOwnerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}
