// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CPP_EnemyWaveData.generated.h"

class ACPP_EnemyBase;

////////////////////////////
//! \enum EWaveTrigger
//! \brief 다음 웨이브가 시작되는 조건. (첫 웨이브는 OnPreviousCleared를 즉시 시작으로 처리)
UENUM(BlueprintType)
enum class EWaveTrigger : uint8
{
	// 직전 웨이브 스폰 완료 후 StartDelay(초) 경과 시 시작.
	Timed,
	// 직전 웨이브에서 스폰된 적이 모두 사망하면 시작.
	OnPreviousCleared,
	// 전멸 또는 StartDelay 중 먼저 도달하는 쪽에서 시작. (StartDelay는 하드락 방지 타임아웃)
	Hybrid
};

////////////////////////////
//! \enum EPreSpawnTiming
//! \brief 프리스폰(초기 배치 적) 스폰이 일어나는 Zone 상태. 모두 플레이어 입장(Active) 이전 시점이다.
UENUM(BlueprintType)
enum class EPreSpawnTiming : uint8
{
	// 가장 이른 시점 (플레이어가 몇 존 앞). 멀리서부터 적이 대기.
	Preparing,
	// 직전 Zone이 Active일 때.
	Ready,
	// 플레이어가 입구 앞에 모일 때 (입장 직전).
	Entering
};

////////////////////////////
//! \enum EWaveSpawnMode
//! \brief 웨이브의 스폰 방식.
UENUM(BlueprintType)
enum class EWaveSpawnMode : uint8
{
	// Entries를 순서대로 전부 스폰. SpawnInterval마다 1마리. (동시 상한 없음)
	FixedList,
	// Entries를 셔플해 만든 총량을, 동시 상한 아래에서 틱마다 배치(Min~Max)로 드립 스폰.
	Budgeted,
	// Budgeted와 동일하되 큐가 소진되면 다시 채워 무한 스폰. Entries의 Count는 총량이 아니라 구성 비율.
	// 스스로 끝나지 않으므로 시간 버티기 클리어(SurviveTime)와 조합하며, 뒤에 다른 웨이브를 둘 수 없다.
	Survival
};

////////////////////////////
//! \enum EEnemySpawnTargetPolicy
//! \brief Wave Entry로 생성된 Enemy가 사용할 전투 타겟 정책.
UENUM(BlueprintType)
enum class EEnemySpawnTargetPolicy : uint8
{
	// 기존 방식. 플레이어별 타겟 수와 거리를 기준으로 균등 분배한다.
	PlayerBalanced UMETA(DisplayName = "Player Balanced"),
	// SpawnerComponent에 등록된 단일 거점만 공격한다.
	DefenseObjective UMETA(DisplayName = "Defense Objective"),
	// SpawnerComponent에 등록된 돌파 목표만 공격한다.
	BreachObjective UMETA(DisplayName = "Breach Objective"),
	// SpawnerComponent에 등록된 의식 목표의 흡수 범위까지 이동한다.
	RitualObjective UMETA(DisplayName = "Ritual Objective")
};

////////////////////////////
//! \struct FEnemyWaveEntry
//! \brief 한 웨이브 안에서 "어떤 적을 몇 마리, 어느 스폰 태그 지점에" 스폰할지 정의하는 단위.
USTRUCT(BlueprintType)
struct FEnemyWaveEntry
{
	GENERATED_BODY()

	// 스폰할 적 클래스.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<ACPP_EnemyBase> EnemyClass;

	// 생성된 Enemy가 사용할 전투 타겟 정책. 기존 WaveData 호환을 위해 플레이어 균등 분배가 기본값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EEnemySpawnTargetPolicy TargetPolicy = EEnemySpawnTargetPolicy::PlayerBalanced;

	// 이 항목에서 스폰할 마릿수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "1"))
	int32 Count = 1;

	// 스폰 위치를 고를 SpawnPoint 태그. 해당 태그를 가진 지점들에 라운드로빈으로 분배.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FName SpawnPointTag;
};

////////////////////////////
//! \struct FEnemyWave
//! \brief 하나의 웨이브 정의. 여러 Entry로 구성되며, 스폰 방식/시작 조건/휴식을 가진다.
USTRUCT(BlueprintType)
struct FEnemyWave
{
	GENERATED_BODY()

	// 이 웨이브에서 스폰할 항목들. (총량 = 모든 Entry의 Count 합)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FEnemyWaveEntry> Entries;

	// 이 웨이브의 스폰 방식.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EWaveSpawnMode SpawnMode = EWaveSpawnMode::FixedList;

	// 이 웨이브가 시작되는 조건.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EWaveTrigger Trigger = EWaveTrigger::OnPreviousCleared;

	// Timed/Hybrid에서 사용. 직전 웨이브 스폰 완료 기준 지연(초). 첫 웨이브는 Zone Active 기준.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float StartDelay = 0.0f;

	// 스폰 틱 간격(초). FixedList=마리당 간격(0이면 동시), Budgeted=배치당 간격.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float SpawnInterval = 0.0f;

	// 이 웨이브를 전멸시킨 뒤 다음 웨이브 시작까지의 휴식(초). 전멸 트리거 경로에만 적용(타이머 경로엔 미적용).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float RestAfterClear = 0.0f;

	// [Budgeted/Survival] 필드에 동시에 존재할 수 있는 이 웨이브 적의 최대 수. 0이면 무제한(배치 레이트로만 제한).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Budgeted", meta = (ClampMin = "0", EditCondition = "SpawnMode != EWaveSpawnMode::FixedList", EditConditionHides))
	int32 MaxConcurrent = 15;

	// [Budgeted/Survival] 한 틱에 스폰할 최소 마릿수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Budgeted", meta = (ClampMin = "1", EditCondition = "SpawnMode != EWaveSpawnMode::FixedList", EditConditionHides))
	int32 SpawnBatchMin = 2;

	// [Budgeted/Survival] 한 틱에 스폰할 최대 마릿수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Budgeted", meta = (ClampMin = "1", EditCondition = "SpawnMode != EWaveSpawnMode::FixedList", EditConditionHides))
	int32 SpawnBatchMax = 3;
};

////////////////////////////
//! \class UCPP_EnemyWaveData
//! \brief 웨이브 시퀀스를 정의하는 DataAsset. SpawnerComponent에 할당하여 재사용한다.
//!        DataAsset이라 레벨 배치 SpawnPoint를 직접 참조할 수 없어 SpawnPointTag로 간접 참조한다.
UCLASS(BlueprintType)
class PROJECTP_API UCPP_EnemyWaveData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 플레이어 입장 전 방에 미리 스폰해둘 초기 배치 적들. (트리거/주기 없이 한 번에 스폰)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PreSpawn")
	TArray<FEnemyWaveEntry> PreSpawnEntries;

	// 프리스폰이 실행되는 Zone 상태. (PreSpawnEntries가 비어 있으면 무시)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PreSpawn")
	EPreSpawnTiming PreSpawnTiming = EPreSpawnTiming::Preparing;

	// Active 진입 후 순서대로 진행되는 웨이브 목록.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FEnemyWave> Waves;
};
