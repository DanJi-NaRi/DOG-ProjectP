////////////////////////////
//! \file CPP_GimmickCondition.h
//! \brief 기믹의 클리어 여부를 판정하는 조립 블록의 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CPP_GimmickCondition.generated.h"

class ACPP_GimmickBase;

////////////////////////////
//! \class UCPP_GimmickCondition
//! \brief 기믹 액터에 인스턴스로 꽂아 클리어 조건을 조립하는 판정 블록의 베이스.
//!        새로운 판정이 필요하면 이 클래스를 상속해 Evaluate만 구현하면 에디터 드롭다운에 자동 등장한다.
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UCPP_GimmickCondition : public UObject
{
	GENERATED_BODY()

public:
	//! 서버에서 호출. 조건이 충족되었는가?
	virtual bool Evaluate(const ACPP_GimmickBase* Owner) const;

	//! 진행도(0..1). UI/연출용. 기본값은 충족 여부를 그대로 반환한다.
	virtual float GetProgress(const ACPP_GimmickBase* Owner) const;
};
