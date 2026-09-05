// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../MyGameplayAbility_AreaSkillBase.h"
#include "GameplayEffectTypes.h"
#include "MyGA_Nefer_JudgementOfIsis.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS()
class PROJECTP_API UMyGA_Nefer_JudgementOfIsis : public UMyGameplayAbility_AreaSkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Nefer_JudgementOfIsis();

	virtual void CancelAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateCancelAbility
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

private:
	virtual bool CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context) override;
	virtual void OnAreaSkillCommitted(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context) override;
	virtual bool ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const override;
	virtual void ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context) override;
	virtual void ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context) override;

	UFUNCTION()
	void HandleAvatarDestroyed(AActor* DestroyedActor);

	void HandleSourceHealthChanged(const FOnAttributeChangeData& Data);
	void BeginCastCancelWatches(AActor* AvatarActor, UAbilitySystemComponent* SourceASC);
	void StopCastCancelWatches();
	void CheckMovementCancel();
	void CancelCastFromCondition(const TCHAR* Reason);
	void ApplySuperArmor(const FMyAreaSkillRuntimeContext& Context);
	void RemoveSuperArmor(UAbilitySystemComponent* SourceASC);
	bool IsDecayTarget(AActor* TargetActor) const;
	int32 GetRemainingDecayTickCount(AActor* TargetActor, float TickInterval) const;
	void ApplyKillHealIfNeeded(const FMyAreaSkillRuntimeContext& Context, bool bKilled) const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CastingAvatarActor;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> CastingSourceASC;

	FActiveGameplayEffectHandle SuperArmorEffectHandle;
	FTimerHandle MovementCancelTimerHandle;
	FDelegateHandle HealthChangedDelegateHandle;
};
