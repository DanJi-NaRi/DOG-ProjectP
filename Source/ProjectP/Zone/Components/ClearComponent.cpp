#include "ClearComponent.h"

#include "GameFramework/Actor.h"
#include "Zone/ZoneBase.h"

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 컴포넌트의 기본 Tick을 비활성화한다.
//! \param 
//! \return 
UClearComponent::UClearComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건을 소유한 Zone을 등록한다.
//! \param InOwnerZone Clear 결과를 전달받을 Zone
//! \return 
void UClearComponent::InitializeClearComponent(AZoneBase* InOwnerZone)
{
	OwnerZone = InOwnerZone;
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 감시를 활성화한다.
//! \return 
void UClearComponent::ActivateClearCondition_Implementation()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	bClearConditionActive = true;

	if (bClearSatisfied)
	{
		BroadcastClearSatisfied();
	}
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 감시를 비활성화한다.
//! \return 
void UClearComponent::DeactivateClearCondition_Implementation()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	bClearConditionActive = false;
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 상태를 초기화한다.
//! \return 
void UClearComponent::ResetClearCondition_Implementation()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	bClearConditionActive = false;
	bClearSatisfied = false;
	bClearSatisfiedBroadcasted = false;
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건이 충족되었음을 Zone에 알릴 준비를 한다.
//! \return 
void UClearComponent::MarkClearSatisfied()
{
	if (!HasOwnerAuthority()
		|| bClearSatisfied
		|| (IsValid(OwnerZone) && OwnerZone->HasEncounterFailed()))
	{
		return;
	}

	bClearSatisfied = true;

	if (bClearConditionActive)
	{
		BroadcastClearSatisfied();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 Owner Actor가 서버 권한을 가지고 있는지 확인한다.
//! \return 서버 권한이 있으면 true
bool UClearComponent::HasOwnerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

////////////////////////////
//! \author HanUl
//! \brief Clear 조건 만족 이벤트를 한 번만 방송한다.
//! \return 
void UClearComponent::BroadcastClearSatisfied()
{
	if (bClearSatisfiedBroadcasted)
	{
		return;
	}

	bClearSatisfiedBroadcasted = true;
	OnClearConditionSatisfied.Broadcast(this);
}

