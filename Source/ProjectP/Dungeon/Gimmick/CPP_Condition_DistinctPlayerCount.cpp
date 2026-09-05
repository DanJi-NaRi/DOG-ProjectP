////////////////////////////
//! \file CPP_Condition_DistinctPlayerCount.cpp
//! \brief 서로 다른 플레이어 점유 수 판정 조건 구현 파일이다.
#include "CPP_Condition_DistinctPlayerCount.h"
#include "CPP_GimmickBase.h"
#include "CPP_GimmickElementBase.h"

int32 UCPP_Condition_DistinctPlayerCount::CountDistinctActivators(const ACPP_GimmickBase* Owner) const
{
	if (!Owner)
	{
		return 0;
	}

	TSet<AActor*> DistinctActivators;
	for (const ACPP_GimmickElementBase* Element : Owner->GetElements())
	{
		if (!Element || !Element->IsSatisfied())
		{
			continue;
		}

		if (AActor* Activator = Element->GetActivator())
		{
			DistinctActivators.Add(Activator);
		}
	}
	return DistinctActivators.Num();
}

bool UCPP_Condition_DistinctPlayerCount::Evaluate(const ACPP_GimmickBase* Owner) const
{
	return CountDistinctActivators(Owner) >= RequiredCount;
}

float UCPP_Condition_DistinctPlayerCount::GetProgress(const ACPP_GimmickBase* Owner) const
{
	if (RequiredCount <= 0)
	{
		return 1.0f;
	}
	return FMath::Clamp(static_cast<float>(CountDistinctActivators(Owner)) / static_cast<float>(RequiredCount), 0.0f, 1.0f);
}
