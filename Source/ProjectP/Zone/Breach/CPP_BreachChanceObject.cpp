#include "Zone/Breach/CPP_BreachChanceObject.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

ACPP_BreachChanceObject::ACPP_BreachChanceObject()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ACPP_BreachChanceObject::BeginPlay()
{
	Super::BeginPlay();

	if (bConsumed)
	{
		BP_ApplyConsumedState();
	}
	else
	{
		BP_ApplyRestoredState();
	}
}

void ACPP_BreachChanceObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BreachChanceObject, bConsumed);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 기회 오브젝트를 소모 상태로 전환하고 현재 접속자에게 순간 연출을 재생한다.
//! \param
//! \return
void ACPP_BreachChanceObject::ConsumeChance()
{
	if (!HasAuthority() || bConsumed)
	{
		return;
	}

	bConsumed = true;
	BP_ApplyConsumedState();
	Multicast_PlayConsumeEffects();
	ForceNetUpdate();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 기회 오브젝트를 미소모 상태로 복구한다.
//! \param
//! \return
void ACPP_BreachChanceObject::RestoreChance()
{
	if (!HasAuthority())
	{
		return;
	}

	bConsumed = false;
	BP_ApplyRestoredState();
	ForceNetUpdate();
}

void ACPP_BreachChanceObject::OnRep_Consumed()
{
	if (bConsumed)
	{
		BP_ApplyConsumedState();
	}
	else
	{
		BP_ApplyRestoredState();
	}
}

void ACPP_BreachChanceObject::Multicast_PlayConsumeEffects_Implementation()
{
	BP_PlayConsumeEffects();
}
