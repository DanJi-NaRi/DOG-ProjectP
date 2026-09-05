////////////////////////////
//! \page MyStreamingAntiAFKRuleTypes.h
//! \brief 파티 잠수 판정과 Streaming Sequence를 연결하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MyStreamingAntiAFKRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingAntiAFKRuleRow
//! \author 장효제
//! \brief 잠수에 들어갈 때와 풀릴 때 각각 재생할 Sequence를 정의한다.
//! \details 잠수 방지는 사실이 "오지 않는 것"에 반응하는 조건이다. 조작이
//!          IdleSeconds 동안 없으면 잠수로 보고 진입 Sequence를 재생한다.
//!          해제 행은 시간을 쓰지 않고 활동이 돌아온 순간에 재생된다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingAntiAFKRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 이 시간 동안 조작이 없으면 잠수로 본다. 해제 행은 0이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|AntiAFKRule", meta = (ClampMin = "0.0"))
	float IdleSeconds = 0.0f;

	//! \brief 재생할 Sequence다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|AntiAFKRule")
	FName SequenceId;
};

namespace MyStreamingAntiAFKRuleNames
{
	//! \brief 잠수에 들어갈 때의 행이다. IdleSeconds가 잠수 판정 시간이다.
	inline const FName Enter = TEXT("Rule_AntiAFK_Enter");
	//! \brief 잠수가 풀릴 때의 행이다. 시간을 쓰지 않는다.
	inline const FName Resume = TEXT("Rule_AntiAFK_Resume");
}

namespace MyStreamingAntiAFKRulePolicy
{
	//! \brief 잠수 판정 시간으로 쓸 수 있는 값인지 판정한다.
	//! \details 0 이하는 시작하자마자 잠수가 되어 쓸 수 없다.
	inline bool IsIdleSecondsValid(const float Seconds)
	{
		return FMath::IsFinite(Seconds) && Seconds > 0.0f;
	}
}
