#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GAS/MyAbilitySet.h"
#include "Boss/Core/CPP_BossTypes.h"
#include "CPP_BossCharacter.generated.h"

class UAbilitySystemComponent;
class UCPP_BossAttributeSet;
class UCPP_BossBrainComponent;
class UCPP_BossEncounterDirectorComponent;
class UCPP_BossTargetingComponent;
class UGameplayEffect;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossPhaseChangedSignature, EBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossClearedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossStagedTeleportFinishedSignature);

UCLASS()
class PROJECTP_API ACPP_BossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_BossCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Boss|GAS|Attribute")
	UCPP_BossAttributeSet* GetBossAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "Boss|GAS|Attribute")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Boss|GAS|Attribute")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Boss|GAS|Damage")
	TSubclassOf<UGameplayEffect> GetBossDamageGameplayEffect() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Brain")
	UCPP_BossBrainComponent* GetBossBrainComponent() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Targeting")
	UCPP_BossTargetingComponent* GetBossTargetingComponent() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	UCPP_BossEncounterDirectorComponent* GetBossEncounterDirectorComponent() const;

	//! \brief Arena (boss-room) center location, shared by arena-centered skills (cross laser, black hole, ...).
	//!        Returns ArenaCenterActor's location, or the boss location when unset.
	UFUNCTION(BlueprintPure, Category = "Boss|Arena")
	FVector GetArenaCenterLocation() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	EBossPhase GetCurrentPhase() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	bool IsPhaseTransitionPending() const;

	float GetPhase2HPThreshold() const;
	bool RequestPhase2ByHP();

	//! \brief Starts the clear (final judgment) encounter when the boss health reaches zero in phase two.
	bool RequestClearEncounter();

	//! \brief True while the clear encounter is running; used to pin health above zero during the DPS check.
	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	bool IsClearEncounterActive() const;

	//! \brief True after HP hit zero but before the clear encounter actually starts (deferred to the next pattern boundary).
	//!        Health is pinned above zero while pending, same as while active.
	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	bool IsClearEncounterPending() const;

	//! \brief Starts the pending clear encounter. Called by the boss brain at a decision boundary (after the current pattern ends).
	bool BeginPendingClearEncounter();

	//! \brief Starts the pending clear encounter with staging: teleports to the arena center first, then begins
	//!        the encounter and restarts the brain when the teleport finishes. Falls back to an immediate start
	//!        when the staged teleport cannot run.
	bool BeginPendingClearEncounterStaged();

	//! \brief 범용 연출 텔레포트(서버 전용): 숨김+콜리전 해제 → 즉시 이동(숨김 동안 클라 스무딩 소화) →
	//!        VanishDuration 후 도착 반경 플레이어 밀어내기 → 등장. 연출은 Vanish/Appear BP 훅으로 얹는다.
	UFUNCTION(BlueprintCallable, Category = "Boss|Teleport")
	bool BeginStagedTeleport(const FVector& Destination);

	UFUNCTION(BlueprintPure, Category = "Boss|Teleport")
	bool IsStagedTeleportActive() const;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Teleport")
	FBossStagedTeleportFinishedSignature OnStagedTeleportFinished;

	//! \brief Kills the boss and broadcasts the clear hook. Called when the clear encounter shield is depleted.
	void HandleBossDefeated();

	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FBossClearedSignature OnBossCleared;

	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool RequestPhaseTwoTransition();

	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool BeginPhaseTwoTransition();

	//! \brief Starts the pending phase-two transition with staging: teleports to the arena center first,
	//!        then enters the transition when the teleport finishes. Falls back to an immediate start
	//!        when the staged teleport cannot run.
	bool BeginPhaseTwoTransitionStaged();

	UFUNCTION(BlueprintCallable, Category = "Boss|Phase")
	bool CompletePhaseTwoTransition();

	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FBossPhaseChangedSignature OnBossPhaseChanged;

protected:
	virtual void PostInitializeComponents() override;

	//! \brief 사라짐 연출 BP 훅. 각 머신에서 로컬 실행된다(멀티캐스트 경유) — 이펙트는 여기서 로컬 스폰.
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Teleport")
	void OnTeleportVanishCosmetics(const FVector& VanishLocation);

	//! \brief 등장 연출 BP 훅. 각 머신에서 로컬 실행된다(멀티캐스트 경유).
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Teleport")
	void OnTeleportAppearCosmetics(const FVector& AppearLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HandleTeleportVanish(FVector VanishLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HandleTeleportAppear(FVector AppearLocation);

	//! \brief 사라짐~등장 사이의 연출 시간(초). 클라 이동 스무딩이 이 안에서 소화된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Teleport", meta = (ClampMin = "0.05"))
	float TeleportVanishDuration = 0.5f;

	//! \brief 등장 지점 기준 밀어내기 판정 반경. 크게 잡으면 등장 충격파처럼 쓸 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Teleport", meta = (ClampMin = "0.0"))
	float TeleportPushRadius = 300.0f;

	//! \brief 수평 밀어내기 세기(LaunchCharacter 속도). 자리 비켜주기~확 밀쳐내기 튜닝용.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Teleport", meta = (ClampMin = "0.0"))
	float TeleportPushStrength = 800.0f;

	//! \brief 수직 밀어내기 성분. 0이면 띄우지 않는다(기본).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Teleport", meta = (ClampMin = "0.0"))
	float TeleportPushUpwardStrength = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|GAS")
	TObjectPtr<UCPP_BossAttributeSet> BossAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Brain")
	TObjectPtr<UCPP_BossBrainComponent> BossBrainComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Targeting")
	TObjectPtr<UCPP_BossTargetingComponent> BossTargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Encounter")
	TObjectPtr<UCPP_BossEncounterDirectorComponent> BossEncounterDirectorComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|GAS|Ability Set")
	TObjectPtr<UMyAbilitySet> DefaultBossAbilitySet;

	//! \brief Boss-room center marker (e.g. a TargetPoint placed in the level). Arena-centered skills spawn here;
	//!        falls back to the boss location when unset. Instance-editable so it can reference a level actor.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Arena")
	TObjectPtr<AActor> ArenaCenterActor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|GAS|Attributes")
	TSubclassOf<UGameplayEffect> DefaultBossAttributeEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|GAS|Attributes")
	FDataTableRowHandle BossBaseStatRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Identity")
	FGameplayTagContainer DefaultCharacterTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase")
	TSubclassOf<UGameplayEffect> Phase1GameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase")
	TSubclassOf<UGameplayEffect> Phase2GameplayEffect;

	UPROPERTY(Transient)
	FMyAbilitySetGrantedHandles GrantedAbilityHandles;

	UFUNCTION()
	void OnRep_CurrentPhase(EBossPhase PreviousPhase);

private:
	void InitializeBossAbilitySystem();
	void ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC);
	void ApplyDefaultBossAttributes();
	void GrantBossAbilities();
	void BindHealthChangedDelegate();
	void BindMoveSpeedChangedDelegate();
	void ApplyMoveSpeedToMovement(float NewMoveSpeed);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& ChangeData);
	void TryRequestPhase2ByHP();
	void HandleStagedTeleportAppear();
	void PushPlayersFromTeleportDestination(const FVector& Center) const;
	void RemoveActiveBossDebuffs();
	bool SetPhaseInternal(EBossPhase NewPhase);
	FActiveGameplayEffectHandle ApplyPhaseGameplayEffect(TSubclassOf<UGameplayEffect> PhaseGameplayEffect);
	void RemoveActivePhaseGameplayEffect();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentPhase, Category = "Boss|Phase", meta = (AllowPrivateAccess = "true"))
	EBossPhase CurrentPhase = EBossPhase::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Phase", meta = (AllowPrivateAccess = "true"))
	bool bPhaseTransitionPending = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true"))
	bool bClearEncounterPending = false;

	bool bStagedTeleportActive = false;
	//! \brief 진행 중인 텔레포트가 전멸기 무대 세팅용인지 — 등장 시 전멸기 시작 + 브레인 재시작.
	bool bClearEncounterStagingActive = false;
	//! \brief 진행 중인 텔레포트가 페이즈 전환 무대 세팅용인지 — 등장 시 전환 시작(브레인은 전환 완료 경로가 재시작).
	bool bPhaseTransitionStagingActive = false;
	FTimerHandle StagedTeleportTimerHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|GAS|Damage", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> BossDamageGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase", meta = (AllowPrivateAccess = "true"))
	float Phase2HPThreshold = 0.4f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Phase", meta = (AllowPrivateAccess = "true"))
	bool bPhase2RequestedByHP = false;

	FActiveGameplayEffectHandle ActivePhaseEffectHandle;
};
