#include "World/Destructible/CPP_BreakableAttributeSet.h"

#include "GameplayEffectExtension.h"

////////////////////////////
//! \author HanSeul
//! \brief 파괴 오브젝트의 IncomingDamage를 Health에만 반영하고 전투 처치 이벤트는 발생시키지 않는다.
//! \param Data 적용된 GameplayEffect의 실행 결과 데이터
void UCPP_BreakableAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		Super::PostGameplayEffectExecute(Data);
		return;
	}

	const float DamageAmount = FMath::Max(GetIncomingDamage(), 0.0f);
	SetIncomingDamage(0.0f);
	SetIncomingCriticalHit(0.0f);

	if (DamageAmount <= 0.0f)
	{
		return;
	}

	LastReceivedDamage = DamageAmount;
	SetHealth(FMath::Clamp(GetHealth() - DamageAmount, 0.0f, GetMaxHealth()));
}
