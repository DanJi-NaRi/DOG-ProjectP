////////////////////////////
//! \file CPP_GimmickElementBase.cpp
//! \brief 기믹 요소 베이스 구현 파일이다.
//! \editor 준혁 - 클리어 시 요소 연출을 고정하는 SolvedLock 공통 API 추가
#include "CPP_GimmickElementBase.h"
#include "CPP_GimmickBase.h"
#include "Net/UnrealNetwork.h"

ACPP_GimmickElementBase::ACPP_GimmickElementBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ACPP_GimmickElementBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CaptureInitialState();
	}

	// 서버에서만 소속 기믹에 등록한다.
	if (HasAuthority() && OwnerGimmick)
	{
		OwnerGimmick->RegisterElement(this);
	}
}

void ACPP_GimmickElementBase::MarkStateDirty()
{
	if (HasAuthority() && OwnerGimmick)
	{
		OwnerGimmick->NotifyElementChanged(this);
	}
}

bool ACPP_GimmickElementBase::IsSatisfied() const
{
	return false;
}

AActor* ACPP_GimmickElementBase::GetActivator() const
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 실제로 이동한 기믹 요소의 월드 Transform을 최초 상태로 복원하는 함수
void ACPP_GimmickElementBase::ResetElement()
{
	if (!HasAuthority() || !bInitialStateCaptured)
	{
		return;
	}

	// Static 루트에 동일한 Transform을 다시 적용하면 Mobility 경고가 발생하므로 실제 차이가 있을 때만 복원한다.
	if (!GetActorTransform().Equals(InitialActorTransform))
	{
		SetActorTransform(InitialActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 요소의 최초 월드 Transform을 공통 초기 상태로 저장하는 함수
void ACPP_GimmickElementBase::CaptureInitialState()
{
	if (!HasAuthority())
	{
		return;
	}

	InitialActorTransform = GetActorTransform();
	bInitialStateCaptured = true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 생명주기에 따라 요소가 제공하는 상호작용 상태를 변경하는 함수
// bEnabled : 상호작용 활성 여부
void ACPP_GimmickElementBase::SetGimmickInteractionEnabled(bool /*bEnabled*/)
{
}

void ACPP_GimmickElementBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_GimmickElementBase, bSolvedLocked);
}

void ACPP_GimmickElementBase::SetSolvedLock(bool bLocked)
{
	if (!HasAuthority() || bSolvedLocked == bLocked)
	{
		return;
	}

	bSolvedLocked = bLocked;

	// 서버 서브클래스 처리 후 서버/리슨 호스트 연출. 원격 클라이언트는 OnRep_SolvedLocked가 처리한다.
	OnSolvedLockChanged();
	OnSolvedLockChangedFX(bSolvedLocked);
}

void ACPP_GimmickElementBase::OnSolvedLockChanged()
{
}

void ACPP_GimmickElementBase::OnRep_SolvedLocked()
{
	OnSolvedLockChangedFX(bSolvedLocked);
}
