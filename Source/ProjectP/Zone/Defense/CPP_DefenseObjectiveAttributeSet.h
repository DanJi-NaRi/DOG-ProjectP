#pragma once

#include "CoreMinimal.h"
#include "GAS/MyAttributeSet.h"
#include "CPP_DefenseObjectiveAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

////////////////////////////
//! \class UCPP_DefenseObjectiveAttributeSet
//! \brief 거점의 피해 활성 상태를 반영하고 회복·보호막 효과를 차단하는 전용 AttributeSet.
UCLASS()
class PROJECTP_API UCPP_DefenseObjectiveAttributeSet : public UMyAttributeSet
{
	GENERATED_BODY()

public:
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
};
