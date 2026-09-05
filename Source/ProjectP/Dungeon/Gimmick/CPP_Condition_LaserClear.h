#pragma once

#include "CoreMinimal.h"
#include "CPP_GimmickCondition.h"
#include "CPP_Condition_LaserClear.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Laser Clear"))
class PROJECTP_API UCPP_Condition_LaserClear : public UCPP_GimmickCondition
{
    GENERATED_BODY()

public:
    virtual bool Evaluate(const ACPP_GimmickBase* Owner) const override;
    virtual float GetProgress(const ACPP_GimmickBase* Owner) const override;

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Condition", meta = (ClampMin = "0.0", Units = "s", AllowPrivateAccess = "true"))
    float RequiredHoldTime = 0.0f;
};
