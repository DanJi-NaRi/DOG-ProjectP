// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_ClearComponent_SurviveTime.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"

////////////////////////////
//! \author HanUl
//! \brief Zone Active 시 생존 타이머를 시작한다. Duration이 0이면 즉시 클리어를 통보한다.
//! \param
//! \return
void UCPP_ClearComponent_SurviveTime::ActivateClearCondition_Implementation()
{
	// 부모가 감시 활성화 + (이미 충족 시) 재방송을 처리한다.
	Super::ActivateClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	if (Duration <= 0.0f)
	{
		MarkClearSatisfied();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		SurviveTimerHandle, this, &UCPP_ClearComponent_SurviveTime::HandleSurviveTimeExpired, Duration, false);
}

////////////////////////////
//! \author HanUl
//! \brief 감시 비활성화 시 생존 타이머를 정지한다.
//! \param
//! \return
void UCPP_ClearComponent_SurviveTime::DeactivateClearCondition_Implementation()
{
	Super::DeactivateClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SurviveTimerHandle);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Zone 리셋 시 생존 타이머를 정지하고 상태를 초기화한다.
//! \param
//! \return
void UCPP_ClearComponent_SurviveTime::ResetClearCondition_Implementation()
{
	Super::ResetClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SurviveTimerHandle);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 남은 생존 시간(초)을 반환한다. 타이머가 돌고 있지 않으면 0.
//! \param
//! \return 남은 시간(초)
float UCPP_ClearComponent_SurviveTime::GetRemainingTime() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetTimerManager().IsTimerActive(SurviveTimerHandle))
	{
		return 0.0f;
	}

	return World->GetTimerManager().GetTimerRemaining(SurviveTimerHandle);
}

////////////////////////////
//! \author HanUl
//! \brief 생존 시간 만료 시 Zone에 클리어를 통보한다.
//! \param
//! \return
void UCPP_ClearComponent_SurviveTime::HandleSurviveTimeExpired()
{
	// 감시 비활성/이미 클리어 상태 처리는 부모(MarkClearSatisfied)가 래치로 보장한다.
	MarkClearSatisfied();
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 Owner Actor가 서버 권한을 가지고 있는지 확인한다.
//! \param
//! \return 서버 권한이 있으면 true
bool UCPP_ClearComponent_SurviveTime::HasZoneAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}
