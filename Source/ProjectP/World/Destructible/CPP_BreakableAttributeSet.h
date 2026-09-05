#pragma once

#include "CoreMinimal.h"
#include "GAS/MyAttributeSet.h"
#include "CPP_BreakableAttributeSet.generated.h"

UCLASS()
class PROJECTP_API UCPP_BreakableAttributeSet : public UMyAttributeSet
{
	GENERATED_BODY()

public:
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	float GetLastReceivedDamage() const { return LastReceivedDamage; }

private:
	float LastReceivedDamage = 0.0f;
};
