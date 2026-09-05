#include "Zone/Ritual/CPP_RitualObjectiveAttributeSet.h"

#include "GameplayEffectExtension.h"

////////////////////////////
//! \author HanSeul
//! \brief 의식 목표에 적용되는 체력 관련 메타 Attribute를 차단한다.
//! \param Data 적용 직전 GameplayEffect 변경 정보
//! \return 체력과 보호막 관련 메타 Attribute가 아니면 true
bool UCPP_RitualObjectiveAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	return Attribute != GetIncomingDamageAttribute()
		&& Attribute != GetIncomingCriticalHitAttribute()
		&& Attribute != GetIncomingHealAttribute()
		&& Attribute != GetIncomingShieldAttribute();
}
