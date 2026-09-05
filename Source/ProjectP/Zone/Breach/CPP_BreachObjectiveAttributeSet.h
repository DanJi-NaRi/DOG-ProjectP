#pragma once

#include "CoreMinimal.h"
#include "GAS/MyAttributeSet.h"
#include "CPP_BreachObjectiveAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

////////////////////////////
//! \class UCPP_BreachObjectiveAttributeSet
//! \brief 돌파 목표가 받은 피해량을 체력 차감 대신 돌파 적중 신호로 변환하는 전용 AttributeSet.
UCLASS()
class PROJECTP_API UCPP_BreachObjectiveAttributeSet : public UMyAttributeSet
{
	GENERATED_BODY()

public:
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
