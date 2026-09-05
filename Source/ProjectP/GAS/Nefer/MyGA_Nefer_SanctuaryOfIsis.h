// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../MyGameplayAbility_AreaSkillBase.h"
#include "MyGA_Nefer_SanctuaryOfIsis.generated.h"

////////////////////////////
//! \class UMyGA_Nefer_SanctuaryOfIsis
//! \brief Nefer의 위치 지정 지속 피해 + 슬로우 장판 GameplayAbility이다.
//! \note 범위 안 적에게 TickInterval마다 지속 피해를 주고, 범위에 처음 들어온 적에게 슬로우 상태 GE를 1회 부여한다.
//!       슬로우 지속시간과 감속률은 슬로우 GE 애셋이 관리하므로, 장판이 끝나도 남은 시간만큼 그대로 유지된다.
UCLASS()
class PROJECTP_API UMyGA_Nefer_SanctuaryOfIsis : public UMyGameplayAbility_AreaSkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Nefer_SanctuaryOfIsis();

private:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;
	virtual bool CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context) override;
	virtual bool ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const override;
	virtual void ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context) override;
	virtual void ApplyAreaEndEffects(const FMyAreaSkillRuntimeContext& Context) override;

	//! \brief 장판 인스턴스별로 이미 슬로우를 부여한 대상 기록(대상당 최초 1회만 부여하기 위함).
	TMap<int32, TSet<TWeakObjectPtr<AActor>>> SlowedTargetsByArea;
};
