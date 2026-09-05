////////////////////////////
//! \file CPP_Condition_BalanceTargetWeight.cpp
//! \brief 저울의 목표 무게와 좌우 균형 상태를 검사하는 조건 블록 구현 파일이다.
#include "CPP_Condition_BalanceTargetWeight.h"

#include "CPP_GimmickBase.h"
#include "Elements/CPP_BalanceScaleElement.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 소속된 모든 저울에서 양쪽에 무게추가 존재하고, 좌우 총무게가 같으며 목표 무게 이상인지 판정하는 함수
// Owner : 조건을 보유하고 저울 요소를 집계하는 기믹 액터
// Return Value : 저울을 하나 이상 찾았고 모든 저울이 조건을 충족하면 true, 아니면 false
bool UCPP_Condition_BalanceTargetWeight::Evaluate(const ACPP_GimmickBase* Owner) const
{
    if (!Owner)
    {
        return false;
    }

    bool bFoundBalanceScale = false;

    for (const ACPP_GimmickElementBase* Element : Owner->GetElements())
    {
        const ACPP_BalanceScaleElement* BalanceScale = Cast<ACPP_BalanceScaleElement>(Element);
        if (!BalanceScale)
        {
            continue;
        }

        bFoundBalanceScale = true;

        const int32 LeftOccupiedSlotCount =
            BalanceScale->GetOccupiedSlotCount(EBalanceScaleSide::Left);
        const int32 RightOccupiedSlotCount =
            BalanceScale->GetOccupiedSlotCount(EBalanceScaleSide::Right);
        const bool bHasLeftWeightObject = LeftOccupiedSlotCount > 0;
        const bool bHasRightWeightObject = RightOccupiedSlotCount > 0;
        const int32 LeftTotalWeight = BalanceScale->GetLeftTotalWeight();
        const int32 RightTotalWeight = BalanceScale->GetRightTotalWeight();

        UE_LOG(LogTemp, Warning,
            TEXT("[BalanceScaleCondition] Scale: %s, LeftCount: %d, RightCount: %d, LeftWeight: %d, RightWeight: %d, TargetWeight: %d"),
            *GetNameSafe(BalanceScale),
            LeftOccupiedSlotCount,
            RightOccupiedSlotCount,
            LeftTotalWeight,
            RightTotalWeight,
            TargetWeight);

        if (!bHasLeftWeightObject ||
            !bHasRightWeightObject ||
            LeftTotalWeight != RightTotalWeight ||
            LeftTotalWeight < TargetWeight)
        {
            return false;
        }
    }

    return bFoundBalanceScale;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 저울 목표 무게 조건의 완료 진행도를 반환하는 함수
// Owner : 조건을 보유하고 저울 요소를 집계하는 기믹 액터
// Return Value : 조건을 충족하면 1, 충족하지 않으면 0
float UCPP_Condition_BalanceTargetWeight::GetProgress(const ACPP_GimmickBase* Owner) const
{
    return Evaluate(Owner) ? 1.0f : 0.0f;
}
