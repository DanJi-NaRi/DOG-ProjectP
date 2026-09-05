////////////////////////////
//! \page MyCooldownGameplayEffect.h
//! \brief SetByCaller(Data.Cooldown) 지속시간을 사용하는 공용 쿨다운 GameplayEffect 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MyCooldownGameplayEffect.generated.h"

////////////////////////////
//! \class UMyCooldownGameplayEffect
//! \author HanUl
//! \brief Duration = SetByCaller(Data.Cooldown)인 공용 쿨다운 GameplayEffect다.
//! \note 쿨다운 태그는 에셋에 고정하지 않고, 적용하는 쪽이 Spec의 DynamicGrantedTags로 주입한다.
//!       스킬/아이템 등 어떤 쿨다운이든 이 클래스 하나를 공유할 수 있다.
UCLASS()
class PROJECTP_API UMyCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMyCooldownGameplayEffect();
};
