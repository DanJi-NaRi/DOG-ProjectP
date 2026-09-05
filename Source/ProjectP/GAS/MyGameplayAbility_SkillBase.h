////////////////////////////
//! \file MyGameplayAbility_SkillBase.h
//! \brief MyGAS 액티브 스킬 GameplayAbility 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "MyGameplayAbilityBase.h"
#include "SkillData/MySkillSetDataAsset.h"
#include "MyGameplayAbility_SkillBase.generated.h"

class UAnimInstance;
class UAbilityTask_WaitDelay;
class UMySkillDefinitionDataAsset;
struct FMySkillAnimationSpec;

////////////////////////////
//! \class UMyGameplayAbility_SkillBase
//! \author HanUl
//! \brief SkillDefinition 기반 액티브 스킬 공통 동작을 제공하는 GameplayAbility 기반 클래스다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyGameplayAbility_SkillBase : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UMyGameplayAbility_SkillBase();

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	FGameplayTag GetSkillGroupTag() const;
	FGameplayTag GetSkillCategoryTag() const;

protected:
	static const FName StandardCastingSectionName;
	static const FName StandardExecuteSectionName;
	static const FName StandardShootNotifyName;
	static const FName StandardEndAttackNotifyName;

	//! \brief 연결된 SkillDefinition의 AbilityTag를 먼저 쓴다.
	virtual FGameplayTag GetStreamingSkillTag() const override;

	const UMySkillDefinitionDataAsset* GetSkillDefinitionDataAssetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const;
	const UMySkillDefinitionDataAsset* GetActiveSkillDefinition() const;
	const FMySkillDataEntry* FindSkillDataEntryFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo);
	bool CheckSkillDefinitionCooldown(const UMySkillDefinitionDataAsset* SkillDefinition, const FGameplayAbilityActorInfo* ActorInfo) const;
	void ApplySkillDefinitionCooldown(const UMySkillDefinitionDataAsset* SkillDefinition, const FGameplayAbilityActorInfo* ActorInfo) const;
	void ApplyMoveInputBlockFromSkillData(const FMySkillDataEntry& SkillData, const FGameplayAbilityActorInfo* ActorInfo);
	void ClearMoveInputBlock(const FGameplayAbilityActorInfo* ActorInfo);
	bool IsMoveInputBlockApplied() const;
	void ApplySkillInputBlock(const FGameplayAbilityActorInfo* ActorInfo);
	void ClearSkillInputBlock(const FGameplayAbilityActorInfo* ActorInfo);
	bool IsSkillInputBlockApplied() const;
	bool ActivateStandardSkill(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	);
	virtual void OnStandardSkillDataReady(const FMySkillDataEntry& SkillData);
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	);
	virtual void OnStandardSkillCommitted(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	);
	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	);
	virtual void OnStandardSkillEndAttack(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	);
	virtual void OnStandardSkillMontageInterrupted();
	virtual bool ShouldBlockStandardSkillWhileFalling() const;
	float GetAttackSpeedMultiplier(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Tags")
	FGameplayTag SkillGroupTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Tags")
	FGameplayTag SkillCategoryTag;

	UPROPERTY(Transient)
	bool bMoveInputBlockApplied = false;

	UPROPERTY(Transient)
	bool bSkillInputBlockApplied = false;

	UPROPERTY(Transient)
	FMySkillDataEntry CachedSkillDataEntry;

private:
	UFUNCTION()
	void OnStandardCastTimeFinished();

	UFUNCTION()
	void OnStandardShootFallback();

	UFUNCTION()
	void OnStandardEndAttackFallback();

	UFUNCTION()
	void OnStandardMontageCompleted();

	UFUNCTION()
	void OnStandardMontageInterrupted();

	UFUNCTION()
	void OnStandardMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	void ResetStandardSkillState();
	bool PlayStandardSkillMontage(const FMySkillAnimationSpec& AnimationSpec);
	void BindStandardMontageNotify();
	void UnbindStandardMontageNotify();
	void StartStandardMontageFallbacks();
	void HandleStandardSkillShoot();
	void HandleStandardSkillEndAttack();
	void FinishStandardSkillAfterPostDelay();
	void ResetStandardMontagePlayRateToBase() const;
	float GetScaledStandardMontageDelay(float RelativeNotifyTime, float RelativeShootTime) const;
	bool TryGetStandardMontageSectionStartTime(FName SectionName, float& OutStartTime) const;
	bool TryGetStandardMontageNotifyTime(FName NotifyName, float& OutNotifyTime) const;
	bool ValidateStandardMontageSpec(const FMySkillAnimationSpec& AnimationSpec) const;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> StandardBoundAnimInstance;

	const UMySkillDefinitionDataAsset* ActiveSkillDefinition = nullptr;
	FGameplayEventData ActiveStandardTriggerEventData;
	float CurrentBaseMontagePlayRate = 1.0f;
	float CurrentCastingMontagePlayRate = 1.0f;
	bool bHasActiveStandardTriggerEventData = false;
	bool bStandardSkillShootHandled = false;
	bool bStandardSkillUsingMontage = false;
};
