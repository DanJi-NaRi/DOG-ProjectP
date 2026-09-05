#pragma once

#include "CoreMinimal.h"
#include "GAS/MyAttributeSet.h"
#include "CPP_BossAttributeSet.generated.h"

struct FGameplayEffectModCallbackData;

UCLASS()
class PROJECTP_API UCPP_BossAttributeSet : public UMyAttributeSet
{
	GENERATED_BODY()

public:
	UCPP_BossAttributeSet();

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
