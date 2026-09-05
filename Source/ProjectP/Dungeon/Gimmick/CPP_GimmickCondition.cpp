////////////////////////////
//! \file CPP_GimmickCondition.cpp
//! \brief 조건 블록 베이스의 기본 구현 파일이다.
#include "CPP_GimmickCondition.h"

bool UCPP_GimmickCondition::Evaluate(const ACPP_GimmickBase* Owner) const
{
	return false;
}

float UCPP_GimmickCondition::GetProgress(const ACPP_GimmickBase* Owner) const
{
	return Evaluate(Owner) ? 1.0f : 0.0f;
}
