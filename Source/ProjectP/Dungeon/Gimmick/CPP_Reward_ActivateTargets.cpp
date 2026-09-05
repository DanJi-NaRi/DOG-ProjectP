////////////////////////////
//! \file CPP_Reward_ActivateTargets.cpp
//! \brief 타겟 활성/비활성 보상 구현 파일이다.
//! \editor 준혁 - ICPP_Activatable 대신 Zone의 IZoneSignalReceiver로 신호 전달(문 인터페이스 통일)
#include "CPP_Reward_ActivateTargets.h"
#include "../../Zone/ZoneSignalReceiver.h"

void UCPP_Reward_ActivateTargets::ApplyToTargets(bool bActive) const
{
	for (AActor* Target : Targets)
	{
		if (!Target || !Target->Implements<UZoneSignalReceiver>())
		{
			continue;
		}

		// Zone과 신호 의미를 통일: OnZoneOpen = 열림, OnZoneClose = 닫힘. 기믹은 Zone이 아니므로 SourceZone은 nullptr.
		if (bActive)
		{
			IZoneSignalReceiver::Execute_OnZoneOpen(Target, nullptr);
		}
		else
		{
			IZoneSignalReceiver::Execute_OnZoneClose(Target, nullptr);
		}
	}
}

void UCPP_Reward_ActivateTargets::Execute(ACPP_GimmickBase* /*Owner*/)
{
	ApplyToTargets(true);
}

void UCPP_Reward_ActivateTargets::Revert(ACPP_GimmickBase* /*Owner*/)
{
	ApplyToTargets(false);
}
