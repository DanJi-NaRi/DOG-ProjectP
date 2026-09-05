////////////////////////////
//! \file CPP_GimmickReward.h
//! \brief 기믹 클리어 시 실행할 결과(문 개방·다리 생성 등)의 조립 블록 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CPP_GimmickReward.generated.h"

class ACPP_GimmickBase;

////////////////////////////
//! \class UCPP_GimmickReward
//! \brief 기믹 로컬 결과(문·다리·승강기 등)를 실행/되돌리는 보상 블록의 베이스.
//!        Zone에 대한 "완료 통보"는 보상이 아니라 기믹의 상태 델리게이트가 담당한다.
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UCPP_GimmickReward : public UObject
{
	GENERATED_BODY()

public:
	//! 클리어 시 실행(서버).
	virtual void Execute(ACPP_GimmickBase* Owner);

	//! 지속형 기믹이 클리어에서 벗어날 때 되돌림(서버).
	virtual void Revert(ACPP_GimmickBase* Owner);
};
