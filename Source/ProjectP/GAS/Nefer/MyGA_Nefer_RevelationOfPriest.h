// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../MyGameplayAbility_AreaSkillBase.h"
#include "MyGA_Nefer_RevelationOfPriest.generated.h"

UCLASS()
class PROJECTP_API UMyGA_Nefer_RevelationOfPriest : public UMyGameplayAbility_AreaSkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Nefer_RevelationOfPriest();

private:
	virtual bool CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context) override;
	virtual void ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context) override;
};
