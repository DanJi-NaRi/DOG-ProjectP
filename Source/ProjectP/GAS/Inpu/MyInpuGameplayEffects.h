////////////////////////////
//! \file MyInpuGameplayEffects.h
//! \brief Inpu 스킬에서 사용하는 SetByCaller GameplayEffect 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MyInpuGameplayEffects.generated.h"

////////////////////////////
//! \class UMyInpuTimedStatusGameplayEffect
//! \author HanUl
//! \brief Data.Duration을 지속시간으로 사용하고 적용 측에서 상태 태그를 동적으로 주입하는 공용 상태 GE다.
UCLASS()
class PROJECTP_API UMyInpuTimedStatusGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMyInpuTimedStatusGameplayEffect();
};

////////////////////////////
//! \class UMyInpuDamageTakenIncreaseGameplayEffect
//! \author HanUl
//! \brief Data.Duration 동안 Data.DamageTakenMultiplier를 대상의 받는 피해 배율에 더하는 GE다.
UCLASS()
class PROJECTP_API UMyInpuDamageTakenIncreaseGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMyInpuDamageTakenIncreaseGameplayEffect();
};
