// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "../MyGameplayAbility_SkillBase.h"
#include "MyGA_Nefer_PowerOfDecay.generated.h"

class AMyNeferProjectile;
class UAnimMontage;
class UAnimInstance;
class UGameplayEffect;
class UAbilityTask_WaitDelay;
class UMySkillDefinitionDataAsset;

////////////////////////////
//! \class UMyGA_Nefer_PowerOfDecay
//! \brief Nefer의 General1 지속피해 투사체 스킬 GameplayAbility이다.
UCLASS()
class PROJECTP_API UMyGA_Nefer_PowerOfDecay : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Nefer_PowerOfDecay();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

private:
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	bool CacheAndValidateSkillDefinition(const FGameplayAbilityActorInfo* ActorInfo, AActor* AvatarActor);
	void FirePendingProjectile();
	FVector GetSpawnLocation(AActor* AvatarActor, const FVector& FireDirection) const;
	FVector GetFireDirection(AActor* AvatarActor) const;
	bool SpawnPowerOfDecayProjectile(AActor* AvatarActor, const FVector& FireDirection) const;
	const UMySkillDefinitionDataAsset* GetActiveSkillDefinition() const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingAvatarActor;

	UPROPERTY(Transient)
	float PendingAimYaw = 0.0f;

	UPROPERTY(Transient)
	FVector PendingFireDirection = FVector::ForwardVector;

	UPROPERTY(Transient)
	bool bHasPendingFireDirection = false;

	UPROPERTY(Transient)
	bool bPendingProjectileFired = false;

	const UMySkillDefinitionDataAsset* ActiveSkillDefinition = nullptr;
};
