#pragma once

#include "CoreMinimal.h"
#include "GAS/MyAttributeSet.h"
#include "CPP_RitualObjectiveAttributeSet.generated.h"

////////////////////////////
//! \class UCPP_RitualObjectiveAttributeSet
//! \brief 의식 목표가 GAS 타깃으로 동작하되 체력 피해나 회복은 받지 않도록 차단하는 전용 AttributeSet.
UCLASS()
class PROJECTP_API UCPP_RitualObjectiveAttributeSet : public UMyAttributeSet
{
	GENERATED_BODY()

public:
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
};
