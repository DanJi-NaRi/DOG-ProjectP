// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Abilities/CPP_AnimNotifyState_BossTelegraphEvent.h"
#include "CPP_AnimNotifyState_EnemyTelegraph.generated.h"

////////////////////////////
//! \class UCPP_AnimNotifyState_EnemyTelegraph
//! \brief 일반 적 텔레그래프 구간 노티파이. 보스 텔레그래프 노티파이(Begin/End GameplayEvent, 서버 전용)를
//!        그대로 상속한 에셋 표면용 클래스 — 적 몽타주가 보스 클래스명을 직접 참조하지 않게 한다.
UCLASS()
class PROJECTP_API UCPP_AnimNotifyState_EnemyTelegraph : public UCPP_AnimNotifyState_BossTelegraphEvent
{
	GENERATED_BODY()
};
