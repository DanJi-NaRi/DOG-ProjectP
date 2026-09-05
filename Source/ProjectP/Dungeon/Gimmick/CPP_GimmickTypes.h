////////////////////////////
//! \file CPP_GimmickTypes.h
//! \brief 기믹 시스템 공용 열거형/델리게이트 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "CPP_GimmickTypes.generated.h"

////////////////////////////
//! \enum EGimmickState
//! \brief 기믹의 최소 상태. 시작/종료/봉인 등 라이프사이클은 Zone이 소유하므로 여기서는 판정 관련 상태만 둔다.
UENUM(BlueprintType)
enum class EGimmickState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),	//!< Zone이 아직 활성화하지 않음. 평가하지 않는다.
	Active   UMETA(DisplayName = "Active"),		//!< 활성 상태. 클리어 조건을 평가한다.
	Solved   UMETA(DisplayName = "Solved")		//!< 클리어됨. (지속형이면 조건이 깨질 때 Active로 되돌아간다.)
};

//! 기믹 상태 변경 브로드캐스트. Zone/방 매니저가 구독하여 진행을 추적한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGimmickStateChangedSignature, EGimmickState, OldState, EGimmickState, NewState);
