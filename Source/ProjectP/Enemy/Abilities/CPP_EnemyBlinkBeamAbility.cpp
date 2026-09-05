// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyBlinkBeamAbility.h"

#include "Enemy/Abilities/CPP_AbilityTask_EnemyBlink.h"
#include "Enemy/Core/CPP_EnemyBase.h"

////////////////////////////
//! \author HanUl
//! \brief 발동 준비 후: 부모의 즉시 발사 대신 순간이동을 먼저 수행한다. 재등장(OnBlinkFinished) 시 부모의 빔 시퀀스를 시작.
//!        순간이동 태스크가 실패하면 곧바로 빔 시퀀스를 돌려 행을 막는다.
//! \param
//! \return
void UCPP_EnemyBlinkBeamAbility::OnBeamActivated()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar)
	{
		BeginBeamSequence();
		return;
	}

	ActiveBlinkTask = UCPP_AbilityTask_EnemyBlink::EnemyBlink(
		this, EnemyAvatar, VanishDuration, ReappearBehindGap, InvincibleDuration, VanishCueTag, ReappearCueTag);
	if (!ActiveBlinkTask)
	{
		BeginBeamSequence();
		return;
	}

	ActiveBlinkTask->OnBlinkFinished.AddDynamic(this, &UCPP_EnemyBlinkBeamAbility::HandleBlinkFinished);
	ActiveBlinkTask->ReadyForActivation();
}

////////////////////////////
//! \author HanUl
//! \brief 재등장 완료: 단축 조준 눈빔 시퀀스를 시작한다. (무적 창은 순간이동 태스크가 조준 초반 동안 유지)
//! \param
//! \return
void UCPP_EnemyBlinkBeamAbility::HandleBlinkFinished()
{
	BeginBeamSequence();
}
