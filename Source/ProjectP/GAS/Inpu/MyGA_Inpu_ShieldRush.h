////////////////////////////
//! \file MyGA_Inpu_ShieldRush.h
//! \brief Inpu의 방패 돌진 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Inpu_ShieldRush.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Inpu_ShieldRush
//! \author HanUl
//! \brief 마우스 방향으로 고정 거리를 돌진하며 경로 적을 한 번씩 타격·넉백하고, 도착 지점에 충격파를 발생시킨다.
//! \note 속도/시간/폭/반경/피해는 SkillDefinition에서 읽고, 기획상 고정된 넉백·피해 증가 수치는 Ability 내부 규칙으로 둔다.
UCLASS()
class PROJECTP_API UMyGA_Inpu_ShieldRush : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Inpu_ShieldRush();

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

protected:
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillCommitted(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillEndAttack(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	//! \brief true면 서버 판정 경로와 도착 충격파를 /debugline 설정에 따라 소유자 화면에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Shield Rush|Debug")
	bool bDrawDebugShieldRush = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Shield Rush|Debug", meta = (ClampMin = "0.0"))
	float DebugShapeLifeTime = 1.0f;

private:
	UFUNCTION()
	void HandleShieldRushFinished();

	bool ResolveDashDirection(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		FVector& OutDirection
	) const;
	void BeginShieldRush(const FGameplayAbilityActorInfo* ActorInfo, const FMySkillDataEntry& SkillData);
	void UpdatePathCollision();
	void SweepPathSegment(const FVector& SegmentStart, const FVector& SegmentEnd);
	void ApplyPathHit(AActor* TargetActor);
	void CollectImpactTargets(const FVector& ImpactLocation, TArray<AActor*>& OutTargets) const;
	void ApplyPhysicalKnockback(AActor* TargetActor) const;
	bool ApplyTimedStatusEffect(
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> EffectClass,
		float Duration,
		FGameplayTag StatusTag
	) const;
	bool ApplyDamageTakenIncrease(UAbilitySystemComponent* TargetASC) const;
	void StartDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo);
	void StopDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo);
	void ExecuteImpactCue(const FVector& ImpactLocation) const;
	void RestoreMovementState(const FGameplayAbilityActorInfo* ActorInfo);
	void DrawDebugShieldRush(const FVector& EndLocation, const TArray<AActor*>& ImpactTargets) const;
	void ResetShieldRushState();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> ActiveDashTask;

	FVector CachedDashDirection = FVector::ZeroVector;
	FVector DashStartLocation = FVector::ZeroVector;
	FVector LastPathSampleLocation = FVector::ZeroVector;
	float CachedDashDistance = 0.0f;
	float CachedPathWidth = 0.0f;
	float CachedImpactRadius = 0.0f;
	TSet<TWeakObjectPtr<AActor>> PathHitActors;
	FTimerHandle PathCollisionTimerHandle;
	TEnumAsByte<ECollisionResponse> CachedPawnCollisionResponse = ECR_Block;
	bool bMovementStateModified = false;
	bool bDashTrailCueActive = false;
	bool bShieldRushActive = false;
	bool bEndAttackRequested = false;
};
