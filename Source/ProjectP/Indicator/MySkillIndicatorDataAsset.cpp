////////////////////////////
//! \page MySkillIndicatorDataAsset.cpp
//! \brief MyGAS 스킬 인디케이터 DataAsset 조회 함수를 구현한다.

#include "MySkillIndicatorDataAsset.h"

////////////////////////////
//! \brief DataAsset에 설정된 인디케이터 표시 설정을 반환한다.
//! \return 인디케이터 표시 설정
const FMySkillIndicatorSpec& UMySkillIndicatorDataAsset::GetIndicatorSpec() const
{
	return IndicatorSpec;
}

////////////////////////////
//! \brief DataAsset에 설정된 인디케이터 Actor 클래스를 반환한다.
//! \return 인디케이터 Actor 클래스
TSubclassOf<AMySkillIndicatorActorBase> UMySkillIndicatorDataAsset::GetIndicatorActorClass() const
{
	return IndicatorActorClass;
}
