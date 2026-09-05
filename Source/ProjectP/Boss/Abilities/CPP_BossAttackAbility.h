#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "CPP_BossAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class ACPP_BossTelegraphActor;
class UCPP_BossAttackData;
struct FBossAttackWindowData;
struct FBossHitShapeData;
struct FGameplayEventData;
class UGameplayEffect;

UCLASS(Abstract, Blueprintable)
class PROJECTP_API UCPP_BossAttackAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCPP_BossAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	virtual float GetCooldownSeconds() const override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	UFUNCTION(BlueprintCallable, Category = "Boss|Attack")
	bool ExecuteAttackWindow(FName WindowId);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attack")
	TObjectPtr<UCPP_BossAttackData> AttackData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Cooldown")
	TSubclassOf<UGameplayEffect> BossCooldownGameplayEffectClass;

private:
	void CollectTargetsFromAttackWindow(const FBossAttackWindowData& AttackWindow, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromHitShape(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromCircle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromSector(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	void CollectTargetsFromRectangle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const;
	bool ApplyDamageToTargets(const FBossAttackWindowData& AttackWindow, const TSet<AActor*>& Targets) const;
	void SpawnTelegraphsForWindow(FName WindowId, float TelegraphDuration);
	void RemoveTelegraphsForWindow(FName WindowId);
	void ClearActiveTelegraphs();

	UFUNCTION()
	void HandleAttackWindowEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTelegraphBeginEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTelegraphEndEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleMontageBlendOut();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveAttackWindowEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphBeginEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ActiveTelegraphEndEventTask;

	mutable FGameplayTagContainer BossCooldownTags;

	TMap<FName, TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>> ActiveTelegraphActorsByWindow;
};
