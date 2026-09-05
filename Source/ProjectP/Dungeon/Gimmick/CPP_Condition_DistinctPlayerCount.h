////////////////////////////
//! \file CPP_Condition_DistinctPlayerCount.h
//! \brief 서로 다른 플레이어가 N개의 요소를 동시에 점유했는지 판정하는 조건 블록 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "CPP_GimmickCondition.h"
#include "CPP_Condition_DistinctPlayerCount.generated.h"

////////////////////////////
//! \class UCPP_Condition_DistinctPlayerCount
//! \brief 소속 요소들 중 충족 상태인 것의 Activator를 모아 "서로 다른" 액터 수가 RequiredCount 이상인지 본다.
//!        압력 발판 기믹(3명이 각기 다른 발판)에 사용한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Distinct Player Count"))
class PROJECTP_API UCPP_Condition_DistinctPlayerCount : public UCPP_GimmickCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate(const ACPP_GimmickBase* Owner) const override;
	virtual float GetProgress(const ACPP_GimmickBase* Owner) const override;

private:
	int32 CountDistinctActivators(const ACPP_GimmickBase* Owner) const;

	//! 필요한 서로 다른 플레이어 수.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Condition", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 RequiredCount = 3;
};
