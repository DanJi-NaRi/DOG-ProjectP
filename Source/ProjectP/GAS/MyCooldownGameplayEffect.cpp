////////////////////////////
//! \page MyCooldownGameplayEffect.cpp
//! \brief 공용 쿨다운 GameplayEffect를 구현한다.
#include "MyCooldownGameplayEffect.h"

#include "../MyGameplayTags.h"

////////////////////////////
//! \author HanUl
//! \brief 지속시간을 SetByCaller(Data.Cooldown)로 받는 HasDuration GameplayEffect로 구성한다.
//! \param 없음
//! \return 없음
UMyCooldownGameplayEffect::UMyCooldownGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = MyGameplayTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);
}
