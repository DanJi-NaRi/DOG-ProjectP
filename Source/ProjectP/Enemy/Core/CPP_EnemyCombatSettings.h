// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CPP_EnemyCombatSettings.generated.h"

////////////////////////////
//! \class UEnemyCombatSettings
//! \brief 적 전투 조정 프로젝트 설정(Project Settings → Enemy Combat). 디자이너가 에디터에서 조정, config에 저장.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Enemy Combat"))
class PROJECTP_API UEnemyCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//! 플레이어 한 명을 동시에 공격할 수 있는 적의 최대 수. 나머지는 사거리에서 우글대다 자리가 나면 친다. (스웜 페이싱)
	//! live 튜닝은 콘솔 변수 enemy.MaxAttackersPerPlayer 로 덮어쓸 수 있다.
	UPROPERTY(EditAnywhere, config, Category = "Attack", meta = (ClampMin = "1"))
	int32 MaxAttackersPerPlayer = 5;
};
