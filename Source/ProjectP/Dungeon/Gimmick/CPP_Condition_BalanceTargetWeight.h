////////////////////////////
//! \file CPP_Condition_BalanceTargetWeight.h
//! \brief 저울 양쪽의 무게추 존재 여부와 목표 무게 이상의 균형 상태를 판정하는 조건 블록 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "CPP_GimmickCondition.h"
#include "CPP_Condition_BalanceTargetWeight.generated.h"

////////////////////////////
//! \class UCPP_Condition_BalanceTargetWeight
//! \brief 소속 저울마다 양쪽에 무게추가 존재하고, 좌우 총무게가 같으며 목표 무게 이상인지 판정한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Balance Target Weight"))
class PROJECTP_API UCPP_Condition_BalanceTargetWeight : public UCPP_GimmickCondition
{
    GENERATED_BODY()

public:
    virtual bool Evaluate(const ACPP_GimmickBase* Owner) const override;
    virtual float GetProgress(const ACPP_GimmickBase* Owner) const override;

private:
    //! 양쪽이 각각 도달해야 하는 최소 총무게다. 기믹 인스턴스의 조건 항목에서 설정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Condition", meta = (ClampMin = "1", UIMin = "1", AllowPrivateAccess = "true"))
    int32 TargetWeight = 1;
};
