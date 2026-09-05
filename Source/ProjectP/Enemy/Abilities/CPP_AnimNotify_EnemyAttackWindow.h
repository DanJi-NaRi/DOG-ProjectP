// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Abilities/CPP_AnimNotify_BossAttackWindowEvent.h"
#include "CPP_AnimNotify_EnemyAttackWindow.generated.h"

////////////////////////////
//! \class UCPP_AnimNotify_EnemyAttackWindow
//! \brief 일반 적 공격 윈도우 노티파이. 보스 윈도우 노티파이(WindowId 페이로드 GameplayEvent, 서버 전용)를
//!        그대로 상속한 에셋 표면용 클래스 — 적 몽타주가 보스 클래스명을 직접 참조하지 않게 한다.
//!        EventTag를 비워두면 기본 공격 윈도우 이벤트로 동작한다(적 Shape 어빌리티가 리슨).
UCLASS()
class PROJECTP_API UCPP_AnimNotify_EnemyAttackWindow : public UCPP_AnimNotify_BossAttackWindowEvent
{
	GENERATED_BODY()
};
