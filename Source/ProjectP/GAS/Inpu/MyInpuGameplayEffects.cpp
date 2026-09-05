////////////////////////////
//! \file MyInpuGameplayEffects.cpp
//! \brief Inpu 스킬에서 사용하는 SetByCaller GameplayEffect를 구현한다.

#include "MyInpuGameplayEffects.h"

#include "GAS/MyAttributeSet.h"
#include "MyGameplayTags.h"

////////////////////////////
//! \author HanUl
//! \brief Data.Duration을 지속시간으로 사용하는 상태 GameplayEffect로 구성한다.
//! \param 없음
//! \return 없음
UMyInpuTimedStatusGameplayEffect::UMyInpuTimedStatusGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = MyGameplayTags::Data_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);
}

////////////////////////////
//! \author HanUl
//! \brief Data.Duration과 Data.DamageTakenMultiplier를 사용하는 받는 피해 증가 GameplayEffect로 구성한다.
//! \param 없음
//! \return 없음
UMyInpuDamageTakenIncreaseGameplayEffect::UMyInpuDamageTakenIncreaseGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = MyGameplayTags::Data_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	FSetByCallerFloat SetByCallerIncrease;
	SetByCallerIncrease.DataTag = MyGameplayTags::Data_DamageTakenMultiplier;

	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = UMyAttributeSet::GetDamageTakenMultiplierAttribute();
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerIncrease);
	Modifiers.Add(ModifierInfo);
}
