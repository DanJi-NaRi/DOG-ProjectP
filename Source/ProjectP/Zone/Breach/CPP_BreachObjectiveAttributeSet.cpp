#include "Zone/Breach/CPP_BreachObjectiveAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Zone/Breach/CPP_BreachObjective.h"

////////////////////////////
//! \author HanSeul
//! \brief 돌파 목표의 활성 상태에 따라 피해 메타 Attribute 적용 여부를 결정한다.
//! \param Data 적용 직전 GameplayEffect 변경 정보
//! \return 해당 Attribute 변경을 적용할 수 있으면 true
bool UCPP_BreachObjectiveAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const ACPP_BreachObjective* Objective = ASC
		? Cast<ACPP_BreachObjective>(ASC->GetAvatarActor())
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

	if (Attribute == GetIncomingDamageAttribute() || Attribute == GetIncomingCriticalHitAttribute())
	{
		return Objective->CanReceiveDamage();
	}

	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief 유효한 IncomingDamage를 체력 차감 없이 공격자 기반 돌파 적중 신호로 전달한다.
//! \param Data 적용된 GameplayEffect의 실행 결과 정보
//! \return
void UCPP_BreachObjectiveAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	ACPP_BreachObjective* Objective = ASC
		? Cast<ACPP_BreachObjective>(ASC->GetAvatarActor())
		: nullptr;
	if (!Objective || !Objective->CanReceiveDamage())
	{
		return;
	}

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
	AActor* SourceActor = nullptr;
	if (UAbilitySystemComponent* SourceASC = EffectContext.GetOriginalInstigatorAbilitySystemComponent())
	{
		SourceActor = SourceASC->GetAvatarActor();
	}
	if (!SourceActor)
	{
		SourceActor = EffectContext.GetOriginalInstigator();
	}
	if (!SourceActor)
	{
		SourceActor = EffectContext.GetEffectCauser();
	}

	Objective->HandleIncomingDamage(SourceActor);
}
