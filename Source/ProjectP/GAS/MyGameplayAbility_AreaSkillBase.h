////////////////////////////
//! \file MyGameplayAbility_AreaSkillBase.h
//! \brief SkillDefinition 기반 장판형 GameplayAbility 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "MyGameplayAbility_SkillBase.h"
#include "MyGameplayAbility_AreaSkillBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

////////////////////////////
//! \struct FMyAreaSkillRuntimeSpec
//! \author HanUl
//! \brief Definition에서 해석된 장판 런타임 위치, 크기, 지속시간, VFX 클래스를 담는다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyAreaSkillRuntimeSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	float Radius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	float ImpactDelay = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	float TickInterval = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Area Skill")
	TSubclassOf<AActor> AreaVisualClass;
};

////////////////////////////
//! \struct FMyAreaSkillRuntimeContext
//! \author HanUl
//! \brief 장판 효과 적용 시 필요한 시전자, ASC, Definition 스냅샷, 장판 런타임 값을 묶는다.
struct PROJECTP_API FMyAreaSkillRuntimeContext
{
	int32 AreaInstanceId = INDEX_NONE;
	AActor* AvatarActor = nullptr;
	UAbilitySystemComponent* SourceASC = nullptr;
	FMySkillDataEntry SkillData;
	FMyAreaSkillRuntimeSpec AreaSpec;
};

////////////////////////////
//! \class UMyGameplayAbility_AreaSkillBase
//! \author HanUl
//! \brief 장판 위치 해석, VFX 생성, 범위 수집, 지속 틱을 공통 처리하는 장판형 스킬 기반 클래스다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyGameplayAbility_AreaSkillBase : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGameplayAbility_AreaSkillBase();

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

	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillEndAttack(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual bool CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context);
	virtual void OnAreaSkillCommitted(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context);
	virtual void OnAreaRuntimeStarted(const FMyAreaSkillRuntimeContext& Context, AActor* AreaVisualActor);
	virtual bool ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const;
	virtual void ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context);
	virtual void ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context);
	virtual void ApplyAreaEndEffects(const FMyAreaSkillRuntimeContext& Context);
	virtual bool ShouldIgnoreAreaSourceActor(const FMyAreaSkillRuntimeContext& Context) const;
	virtual bool ShouldAreaAffectTarget(const FMyAreaSkillRuntimeContext& Context, AActor* Candidate) const;
	virtual ECollisionChannel GetAreaOverlapChannel() const;

	void CollectValidAreaTargets(const FMyAreaSkillRuntimeContext& Context, TArray<AActor*>& OutTargets) const;
	UAbilitySystemComponent* GetTargetAbilitySystemComponent(AActor* TargetActor) const;
	float GetSourceAttackPower(const FMyAreaSkillRuntimeContext& Context) const;
	float GetTargetHealth(AActor* TargetActor) const;
	float GetTargetMaxHealth(AActor* TargetActor) const;
	static float PercentValueToRatio(float PercentValue);
	bool TargetHasAnyGameplayTag(AActor* TargetActor, const FGameplayTagContainer& Tags) const;
	int32 GetRemainingEffectTickCountByTags(AActor* TargetActor, const FGameplayTagContainer& Tags, float TickInterval) const;
	bool ApplyDamageGameplayEffectToTarget(
		const FMyAreaSkillRuntimeContext& Context,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
		float DamageCoefficient,
		bool* bOutKilled = nullptr
	) const;
	bool ApplyHealGameplayEffectToTarget(
		const FMyAreaSkillRuntimeContext& Context,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> HealGameplayEffectClass,
		float Heal
	) const;
	bool ApplyStatusGameplayEffectToTarget(
		const FMyAreaSkillRuntimeContext& Context,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> StatusGameplayEffectClass,
		float DamageSetByCaller
	) const;

private:
	struct FMyActiveAreaRuntimeInstance
	{
		int32 AreaInstanceId = INDEX_NONE;
		TWeakObjectPtr<AActor> AvatarActor;
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TWeakObjectPtr<AActor> AreaVisualActor;
		FMySkillDataEntry SkillData;
		FMyAreaSkillRuntimeSpec AreaSpec;
		FTimerHandle EffectStartTimerHandle;
		FTimerHandle TickTimerHandle;
		FTimerHandle EndTimerHandle;
	};

	void ResetPendingAreaState();
	bool BuildAreaRuntimeSpec(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData,
		FMyAreaSkillRuntimeSpec& OutAreaSpec
	) const;
	bool ResolveTargetLocation(const FGameplayEventData* TriggerEventData, FVector& OutTargetLocation) const;
	bool BuildPendingAreaContext(FMyAreaSkillRuntimeContext& OutContext) const;
	bool BuildActiveAreaContext(int32 AreaInstanceId, FMyAreaSkillRuntimeContext& OutContext) const;
	int32 StartAreaRuntime(const FMyAreaSkillRuntimeContext& Context);
	AActor* SpawnAreaVisualActor(const FMyAreaSkillRuntimeContext& Context) const;
	void ConfigureAreaVisualActor(AActor* AreaVisualActor, const FMyAreaSkillRuntimeSpec& AreaSpec) const;
	void DrawDebugAreaEffectRadius(const FMyAreaSkillRuntimeContext& Context) const;
	void BeginAreaEffects(int32 AreaInstanceId);
	void HandleAreaTick(int32 AreaInstanceId);
	void HandleAreaFinished(int32 AreaInstanceId);
	void RemoveAreaRuntimeInstance(int32 AreaInstanceId, bool bDestroyVisualActor);
	void RemoveAllAreaRuntimeInstances(bool bDestroyVisualActor);
	bool ShouldDeferEndAbilityForAreaRuntime(bool bWasCancelled) const;
	void FinishAbilityIfAreaRuntimeComplete();

	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingAvatarActor;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> PendingSourceASC;

	UPROPERTY(Transient)
	FMyAreaSkillRuntimeSpec PendingAreaSpec;

	UPROPERTY(Transient)
	bool bHasPendingAreaSpec = false;

	UPROPERTY(Transient)
	bool bWaitingForAreaRuntimeCompletion = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area Skill|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDrawDebugEffectRadius = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area Skill|Debug", meta = (AllowPrivateAccess = "true"))
	FLinearColor DebugEffectRadiusColor = FLinearColor::Yellow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area Skill|Debug", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DebugEffectRadiusLifeTime = 0.0f;

	int32 NextAreaInstanceId = 1;
	TMap<int32, FMyActiveAreaRuntimeInstance> ActiveAreaRuntimeInstances;
};
