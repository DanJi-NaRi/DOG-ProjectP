// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CPP_EnemyStatTypes.generated.h"

////////////////////////////
//! \struct FCPP_EnemyBaseStatRow
//! \brief 적 종류별 기본 GAS 스탯을 DataTable의 한 행으로 관리한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FCPP_EnemyBaseStatRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat|Growth", meta = (ClampMin = "0.0"))
	float MaxHealthGrowthRate = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat|Growth", meta = (ClampMin = "0.0"))
	float AttackPowerGrowthRate = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat", meta = (ClampMin = "0.0"))
	float Defense = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat|Growth", meta = (ClampMin = "0.0"))
	float DefenseGrowthRate = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat", meta = (ClampMin = "0.0"))
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stat|Growth", meta = (ClampMin = "0.0"))
	float MoveSpeedGrowthRate = 0.0f;
};
