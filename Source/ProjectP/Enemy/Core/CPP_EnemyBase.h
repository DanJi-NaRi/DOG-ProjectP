// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GAS/MyAbilitySet.h"
#include "CPP_EnemyBase.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;
class UCPP_EnemyAttackPatternData;
class UCPP_EnemyAreaAttackAbility;
class UCPP_EnemyBeamAttackAbility;
class UCPP_EnemyBlinkNovaAbility;
class UCPP_EnemyDashAttackAbility;
class UCPP_EnemyProjectileAttackAbility;
class UCPP_EnemyShapeAttackAbility;
class UCPP_EnemySplitAbility;
class UCPP_EnemySummonAbility;
class UGameplayEffect;
class UMyAttributeSet;
struct FOnAttributeChangeData;
enum class EEnemySpawnTargetPolicy : uint8;

UENUM(BlueprintType)
enum class EEnemyAttackPatternSelectionMode : uint8
{
	Any UMETA(DisplayName = "Any"),
	Normal UMETA(DisplayName = "Normal"),
	SpecialCondition UMETA(DisplayName = "Special Condition")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyHealthChangedSignature, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyDeathSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyActionFinishedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnemyStatusEffectChangedSignature, FGameplayTag, StatusTag, bool, bActive);

UCLASS()
class PROJECTP_API ACPP_EnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_EnemyBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	bool ActivatePrimaryEnemyAbility();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	bool ActivateEnemyAbilityBySelectionMode(EEnemyAttackPatternSelectionMode SelectionMode);

	UFUNCTION(BlueprintPure, Category = "Enemy|Attack")
	AActor* GetCurrentTargetActor() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	bool PlayPrimaryAttackMontageFromAbility(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Attack")
	void FinishPrimaryAttackFromAbility();

	// 공격 몽타주 일시정지/재개(전 클라 동기). 돌진 동안 '찌른 포즈'를 붙잡아 두는 용도. (서버에서 호출)
	void SetAttackMontagePausedFromAbility(bool bPaused);

	void SetActiveProjectileAttackAbility(UCPP_EnemyProjectileAttackAbility* InAbility);
	void ClearActiveProjectileAttackAbility(UCPP_EnemyProjectileAttackAbility* InAbility);
	bool FirePrimaryProjectileFromAnimNotify();
	void SetActiveAreaAttackAbility(UCPP_EnemyAreaAttackAbility* InAbility);
	void ClearActiveAreaAttackAbility(UCPP_EnemyAreaAttackAbility* InAbility);
	bool TriggerPrimaryAreaFromAnimNotify();
	void SetActiveDashAttackAbility(UCPP_EnemyDashAttackAbility* InAbility);
	void ClearActiveDashAttackAbility(UCPP_EnemyDashAttackAbility* InAbility);
	bool StartDashAttackFromAnimNotify();
	void SetActiveShapeAttackAbility(UCPP_EnemyShapeAttackAbility* InAbility);
	void ClearActiveShapeAttackAbility(UCPP_EnemyShapeAttackAbility* InAbility);
	void SetActiveBlinkNovaAbility(UCPP_EnemyBlinkNovaAbility* InAbility);
	void ClearActiveBlinkNovaAbility(UCPP_EnemyBlinkNovaAbility* InAbility);
	void SetActiveBeamAttackAbility(UCPP_EnemyBeamAttackAbility* InAbility);
	void ClearActiveBeamAttackAbility(UCPP_EnemyBeamAttackAbility* InAbility);
	void SetActiveSplitAbility(UCPP_EnemySplitAbility* InAbility);
	void ClearActiveSplitAbility(UCPP_EnemySplitAbility* InAbility);
	void SetActiveSummonAbility(UCPP_EnemySummonAbility* InAbility);
	void ClearActiveSummonAbility(UCPP_EnemySummonAbility* InAbility);
	bool TriggerSummonFromAnimNotify();

	// 어빌리티가 스폰한 파생 적 여부(분열 분신·소환몹 공용). true면 사망 보상 제외 + bExcludeForSpawnedMinion 패턴 선택 제외.
	bool IsAbilitySpawnedMinion() const { return bIsAbilitySpawnedMinion; }
	void SetAbilitySpawnedMinion(bool bInIsMinion) { bIsAbilitySpawnedMinion = bInIsMinion; }

	int32 GetEnemyLevel() const { return EnemyLevel; }
	void SetEnemyLevel(int32 NewEnemyLevel) { EnemyLevel = FMath::Max(NewEnemyLevel, 1); }
	void ConfigureSpawnTarget(EEnemySpawnTargetPolicy NewPolicy, AActor* NewObjectiveTarget);
	EEnemySpawnTargetPolicy GetSpawnTargetPolicy() const { return SpawnTargetPolicy; }
	AActor* GetAssignedObjectiveTarget() const { return AssignedObjectiveTarget; }

	// 최대/현재 체력을 직접 설정한다(서버). 분신 스폰(절반 HP)·병합 복귀(합산 HP)용. 데미지 반응(경직/사망)은 유발하지 않는다.
	void SetHealthValues(float NewMaxHealth, float NewCurrentHealth);

	// 공격력(AttackPower) 베이스를 Scale배로 설정한다(서버). 소환몹 공격력 80% 등에 사용.
	void SetAttackPowerScale(float Scale);

	// 소환 패턴이 스폰한 적을 추적 등록한다. 게이트(동시 생존 소환몹 수 제한)용.
	void RegisterSummonedMinion(ACPP_EnemyBase* Minion);

	// 이 적이 소환 패턴으로 부른, 현재 살아있는 소환몹 수. 조회 시 무효/사망 항목을 정리한다.
	int32 GetLivingSummonedCount() const;

	// 점멸형 어빌리티용: 액터 숨김+콜리전을 전 클라 동기로 끄거나 복구한다. (서버에서 호출)
	void SetVanishedFromAbility(bool bVanished);

	// 돌진 물리 관통용: 캡슐의 Pawn 채널 응답을 전 클라 동기로 끄거나 복구한다.
	// 정지 판정은 돌진 태스크의 sweep이 전담하므로 벽/플레이어에 막히는 동작은 변하지 않는다. (서버에서 호출)
	void SetPawnPhysicsIgnoredFromAbility(bool bIgnored);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Animation")
	void StopCurrentMontage();

	// 공격 종료 통지 없이 현재 몽타주만 전 클라 정지. 돌진 벽 기절처럼 어빌리티가 마무리 시점을 직접 제어할 때 사용. (서버 전용)
	void StopCurrentMontageWithoutFinish();

	UFUNCTION(BlueprintCallable, Category = "Enemy|State")
	void Dead();

	// 외부(스포너 등)에서 강제 사망 처리. 정상 사망 경로(bIsDead/OnEnemyDeath/BP_OnDeath)를 태운 뒤 사후 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Enemy|State")
	void ForceKill();

	UFUNCTION(BlueprintCallable, Category = "Enemy|State")
	void ForceKillWithoutRewards();

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Attribute")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Attribute")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS")
	UMyAttributeSet* GetMyAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Attack")
	UCPP_EnemyAttackPatternData* GetPrimaryAttackPattern() const;

	// 지금 발동 가능한(쿨다운 안 도는) 공격 패턴이 있는지. ActiveAttackPattern 캐시에 의존하지 않아
	// 공격 종료(캐시 초기화) 후에도 정확하다 — AIC 공격 게이트(쿨다운 동안 슬롯 이동)용.
	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Ability")
	bool HasReadyAttackPattern() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Ability")
	bool HasReadyAttackPatternBySelectionMode(EEnemyAttackPatternSelectionMode SelectionMode) const;

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Ability")
	float GetReadyAttackPatternRange() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|MyGAS|Tag")
	bool HasEnemyGameplayTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|MyGAS|Status")
	void BroadcastCurrentStatusEffectStates();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void UpdateChaseAcceptableRadius();

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Health")
	FEnemyHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|State")
	FEnemyDeathSignature OnEnemyDeath;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Attack")
	FEnemyActionFinishedSignature OnAttackFinished;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|MyGAS|Status")
	FEnemyStatusEffectChangedSignature OnStatusEffectChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "Enemy|State")
	bool bIsDead = false;

	// 공격 슬롯 반경(= 유효 패턴 최소 사거리 − AttackRangeSlotBuffer). UpdateChaseAcceptableRadius가 갱신.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat")
	float ChaseAcceptableRadius = 200.0f;

protected:
	virtual void BeginPlay() override;

	//! 처치 시 파티 전원에게 지급할 메소량 (서버에서만 지급)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Reward", meta = (ClampMin = "0"))
	int32 MesoRewardOnDeath = 10;

	//! 처치 시 파티 전원에게 지급할 경험치량 (서버에서만 지급)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Reward", meta = (ClampMin = "0"))
	int32 ExpRewardOnDeath = 10;

	// 슬롯을 사거리보다 이만큼 안쪽에 잡는다. 타겟 이동으로 인한 헛스윙 방지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRangeSlotBuffer = 60.0f;

	// 슬롯 반경 하한. 캡슐 합(플레이어~40+적~40)보다 여유 있게 커야 물리적으로 설 수 있고 밀착이 안 생긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float MinSlotRadius = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	TArray<TObjectPtr<UCPP_EnemyAttackPatternData>> AttackPatterns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Stagger", meta = (ClampMin = "0.0"))
	float StaggerDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Stagger", meta = (ClampMin = "0.0"))
	float StaggerImmunityDuration = 0.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|MyGAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|MyGAS")
	TObjectPtr<UMyAttributeSet> MyAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|MyGAS|Status")
	FGameplayTagContainer TrackedStatusEffectTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|MyGAS|Ability Set")
	TObjectPtr<UMyAbilitySet> DefaultEnemyAbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|MyGAS|Attributes")
	TSubclassOf<UGameplayEffect> DefaultEnemyAttributeEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|MyGAS|Attributes")
	FDataTableRowHandle EnemyBaseStatRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Identity")
	FGameplayTagContainer DefaultCharacterTags;

	UPROPERTY(Transient)
	FMyAbilitySetGrantedHandles GrantedAbilityHandles;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Health")
	void BP_OnHealthChanged(float NewCurrentHP, float NewMaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|State")
	void BP_OnDeath();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Death", meta = (BlueprintProtected = "true"))
	void FinishDeathDisappear();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackPatternMontage(AActor* TargetActor, UAnimMontage* PatternMontage);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopCurrentMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDeadFX();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetAttackMontagePaused(bool bPaused);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetActorVanished(bool bVanished);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetPawnPhysicsIgnored(bool bIgnored);

	void GrantKillRewards();
	void BroadcastAttackFinished();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

private:
	enum class EDeathRewardPolicy : uint8
	{
		GrantRewards,
		SuppressRewards
	};

	void HandleDeath(EDeathRewardPolicy RewardPolicy);
	bool bDeathDisappearFinished = false;

	UCPP_EnemyAttackPatternData* SelectAvailableAttackPattern(EEnemyAttackPatternSelectionMode SelectionMode) const;
	bool IsSpecialConditionPattern(const UCPP_EnemyAttackPatternData* AttackPattern) const;
	bool DoesAttackPatternMeetConditions(const UCPP_EnemyAttackPatternData* AttackPattern) const;
	void InitializeEnemyAbilitySystem();
	void ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC);
	void ApplyDefaultEnemyAttributes();
	float CalculateLevelScaledStat(float BaseValue, float GrowthRate) const;
	void GrantEnemyAbilities();
	void BindEnemyAttributeDelegates();
	void BindEnemyGameplayTagDelegates();
	void ApplyMoveSpeedToMovement(float NewMoveSpeed);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void HandleStatusEffectTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleDamageReaction(float OldHealth, float NewHealth);
	bool TryRequestForcedSpecialAttack(float OldHealth, float NewHealth);
	bool CanEnterStaggerByGAS() const;
	bool IsStaggerImmuneByGAS() const;
	bool IsStaggerImmuneDuringCurrentAttack() const;
	void BroadcastHealthChangedFromGAS();
	void SendDamageStateTreeEvent(bool bWasFatal);
	bool CanBeStaggered() const;
	void MarkStaggered();
	bool CancelActiveAttackAbilities();
	bool PlayAttackPatternMontage(AActor* TargetActor, UAnimMontage* PatternMontage);
	void FinishAttackMontage(UAnimMontage* Montage);

	TObjectPtr<UAnimMontage> ActiveAttackMontage;
	UPROPERTY(Transient)
	TObjectPtr<UCPP_EnemyAttackPatternData> ActiveAttackPattern;
	UPROPERTY(Transient)
	TObjectPtr<UCPP_EnemyAttackPatternData> PendingForcedAttackPattern;
	FGameplayAbilitySpecHandle ActiveAttackAbilityHandle;
	TObjectPtr<UCPP_EnemyProjectileAttackAbility> ActiveProjectileAttackAbility;
	TObjectPtr<UCPP_EnemyAreaAttackAbility> ActiveAreaAttackAbility;
	TObjectPtr<UCPP_EnemyDashAttackAbility> ActiveDashAttackAbility;
	TObjectPtr<UCPP_EnemyShapeAttackAbility> ActiveShapeAttackAbility;
	TObjectPtr<UCPP_EnemyBlinkNovaAbility> ActiveBlinkNovaAbility;
	TObjectPtr<UCPP_EnemyBeamAttackAbility> ActiveBeamAttackAbility;
	TObjectPtr<UCPP_EnemySplitAbility> ActiveSplitAbility;
	TObjectPtr<UCPP_EnemySummonAbility> ActiveSummonAbility;
	bool bIsAbilitySpawnedMinion = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Level", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 EnemyLevel = 1;
	EEnemySpawnTargetPolicy SpawnTargetPolicy = static_cast<EEnemySpawnTargetPolicy>(0);
	UPROPERTY(Transient)
	TObjectPtr<AActor> AssignedObjectiveTarget;
	bool bSuppressDamageReaction = false;
	mutable TArray<TWeakObjectPtr<ACPP_EnemyBase>> SummonedMinions;
	float LastStaggerTime = -1000.0f;
	bool bHasAttackFinishedBroadcast = false;
};
