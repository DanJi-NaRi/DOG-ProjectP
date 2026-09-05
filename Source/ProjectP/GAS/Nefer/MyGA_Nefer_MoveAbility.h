// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../MyGameplayAbility_SkillBase.h"
#include "GameplayTagContainer.h"
#include "MyGA_Nefer_MoveAbility.generated.h"

class AActor;
class UAbilitySystemComponent;
class UAbilityTask_ApplyRootMotionConstantForce;
class UMySkillDefinitionDataAsset;
struct FGameplayEventData;
struct FGameplayAbilityTargetDataHandle;

UCLASS()
class PROJECTP_API UMyGA_Nefer_MoveAbility : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()


public:
	UMyGA_Nefer_MoveAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

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
	UFUNCTION()
	void HandleDashMontageInterrupted();
	UFUNCTION()
	void HandleDashMovementFinished();

	bool TryGetMoveDefinitionData(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UMySkillDefinitionDataAsset*& OutSkillDefinition,
		FMySkillMovementSpec& OutMovementSpec,
		FMySkillTimingSpec& OutTimingSpec
	) const;
	bool ResolveDashDirection(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillMovementSpec& MovementSpec,
		FVector& OutDashDirection
	) const;
	bool TryGetDashDirectionFromEventData(const FGameplayEventData* TriggerEventData, FVector& OutDashDirection) const;
	bool TryGetFacingDashDirection(const AActor* AvatarActor, FVector& OutDashDirection) const;
	UAbilityTask_ApplyRootMotionConstantForce* CreateDashMovementTask(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FMySkillMovementSpec& MovementSpec,
		const FMySkillTimingSpec& TimingSpec,
		const FVector& DashDirection
	);
	void BeginDash(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const UMySkillDefinitionDataAsset* SkillDefinition,
		const FMySkillMovementSpec& MovementSpec,
		const FMySkillTimingSpec& TimingSpec,
		const FVector& DashDirection
	);
	void StartDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo);
	void StopDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo);
	void ApplyDashMovementState(const FGameplayAbilityActorInfo* ActorInfo, const FVector& DashDirection);
	void RestoreDashMovementState(const FGameplayAbilityActorInfo* ActorInfo);
	bool HasMoveCharge(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool ConsumeMoveCharge(const FGameplayAbilityActorInfo* ActorInfo) const;
	void StartMoveRecharge(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UMySkillDefinitionDataAsset* SkillDefinition,
		const FMySkillMovementSpec& MovementSpec
	);
	static void ScheduleMoveRecharge(
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC,
		float BaseRechargeSeconds,
		FGameplayTag RechargeStateTag
	);
	static float GetEffectiveRechargeSeconds(UAbilitySystemComponent* ASC, float BaseRechargeSeconds);

	bool bDashMovementEndHandled = false;
	bool bDashTrailCueActive = false;
	bool bDashMovementStateModified = false;
	TEnumAsByte<ECollisionResponse> CachedPawnCollisionResponse = ECR_Block;
};
