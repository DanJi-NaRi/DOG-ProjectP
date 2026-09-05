#include "CPP_DefenseObjectiveAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "CPP_DefenseObjective.h"

////////////////////////////
//! \author HanSeul
//! \brief 거점의 현재 상태에 따라 피해를 허용하고 회복·보호막 효과는 항상 차단한다.
//! \param Data 적용 직전의 GameplayEffect 변경 정보
//! \return 해당 Attribute 변경을 적용할 수 있으면 true
bool UCPP_DefenseObjectiveAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const ACPP_DefenseObjective* Objective = ASC
		? Cast<ACPP_DefenseObjective>(ASC->GetAvatarActor())
		: nullptr;

	if (!Objective)
	{
		return true;
	}

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetIncomingHealAttribute() || Attribute == GetIncomingShieldAttribute())
	{
		return false;
	}

	if (Attribute == GetIncomingCriticalHitAttribute() && !Objective->CanReceiveDamage())
	{
		return false;
	}

	if (Attribute == GetIncomingDamageAttribute())
	{
		return Objective->CanReceiveDamage();
	}

	return true;
}
