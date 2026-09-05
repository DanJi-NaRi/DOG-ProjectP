// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "CPP_EnemyTelegraphActor.generated.h"

////////////////////////////
//! \class ACPP_EnemyTelegraphActor
//! \brief 일반 적 텔레그래프. 보스 텔레그래프(복제 spawn-data + 로컬 fill)를 그대로 상속한 에셋 표면용 클래스.
//!        적 에셋/데이터가 보스 클래스명을 직접 참조하지 않게 하고, 이후 공용화 리팩터 시 C++만 고치면 되게 한다.
UCLASS()
class PROJECTP_API ACPP_EnemyTelegraphActor : public ACPP_BossTelegraphActor
{
	GENERATED_BODY()
};
