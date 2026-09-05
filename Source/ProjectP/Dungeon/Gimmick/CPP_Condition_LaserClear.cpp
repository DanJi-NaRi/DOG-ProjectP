#include "CPP_Condition_LaserClear.h"

#include "CPP_GimmickBase.h"
#include "Elements/CPP_LaserGimmickElement.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 소유 기믹에 등록된 모든 레이저 요소가 Clear 상태인지 판정하는 함수
// Owner : 조건과 기믹 요소를 소유한 기믹 액터
// Return Value : 레이저 요소가 하나 이상 존재하고 모든 요소가 Clear 상태이면 true
bool UCPP_Condition_LaserClear::Evaluate(const ACPP_GimmickBase* Owner) const
{
    if (!Owner)
    {
        return false;
    }

    bool bFoundLaserElement = false;

    for (const ACPP_GimmickElementBase* Element : Owner->GetElements())
    {
        const ACPP_LaserGimmickElement* LaserElement =
            Cast<ACPP_LaserGimmickElement>(Element);
        if (!LaserElement)
        {
            continue;
        }

        bFoundLaserElement = true;
        if (!LaserElement->IsSatisfied() ||
            LaserElement->GetClearHoldTime() < RequiredHoldTime)
        {
            return false;
        }
    }

    return bFoundLaserElement;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 전체 레이저 요소 중 Clear 상태인 요소의 비율을 반환하는 함수
// Owner : 조건과 기믹 요소를 소유한 기믹 액터
// Return Value : 레이저 요소의 완료 비율, 요소가 없으면 0
float UCPP_Condition_LaserClear::GetProgress(const ACPP_GimmickBase* Owner) const
{
    if (!Owner)
    {
        return 0.0f;
    }

    int32 LaserElementCount = 0;
    float TotalProgress = 0.0f;

    for (const ACPP_GimmickElementBase* Element : Owner->GetElements())
    {
        const ACPP_LaserGimmickElement* LaserElement =
            Cast<ACPP_LaserGimmickElement>(Element);
        if (!LaserElement)
        {
            continue;
        }

        ++LaserElementCount;
        if (!LaserElement->IsSatisfied())
        {
            continue;
        }

        TotalProgress += RequiredHoldTime <= UE_KINDA_SMALL_NUMBER
            ? 1.0f
            : FMath::Clamp(LaserElement->GetClearHoldTime() / RequiredHoldTime, 0.0f, 1.0f);
    }

    return LaserElementCount > 0
        ? TotalProgress / static_cast<float>(LaserElementCount)
        : 0.0f;
}
