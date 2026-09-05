#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPP_BossEncounterDirectorComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossEncounterFailedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossEncounterSucceededSignature);

UENUM(BlueprintType)
enum class EBossPhaseTransitionEncounterState : uint8
{
	Inactive,
	RockWarning,
	RideWarning,
	RideWindow
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UCPP_BossEncounterDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPP_BossEncounterDirectorComponent();

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	bool StartPhaseTransitionEncounter();

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	void SucceedPhaseTransitionEncounter();

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	void FailPhaseTransitionEncounter();

	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter")
	bool NotifyPhaseTransitionRideSuccess(AActor* Rider);

	AActor* FindLivingSandStormTarget() const;

	//! \brief Registers a gimmick hazard actor (e.g. chasing sandstorm, black hole) so it can be force-destroyed when
	//!        an encounter phase (phase transition or clear/final-judgment) begins.
	void RegisterGimmickHazard(AActor* GimmickHazard);

	//! \brief Destroys every active gimmick hazard. Called when the phase-two transition or clear encounter begins.
	void ClearGimmickHazards();

	//! \brief Begins the clear (final judgment) encounter: grants a large shield and runs red lightning + cross laser
	//!        concurrently with the still-running boss brain. Returns true when it starts.
	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter|Clear")
	bool StartClearEncounter();

	//! \brief Ends the clear encounter as a success (shield removed in time) and defeats the boss.
	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter|Clear")
	void SucceedClearEncounter();

	//! \brief Ends the clear encounter as a failure (time limit reached) and wipes all living players.
	UFUNCTION(BlueprintCallable, Category = "Boss|Encounter|Clear")
	void FailClearEncounter();

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter|Clear")
	bool IsClearEncounterActive() const;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FBossEncounterFailedSignature OnBossEncounterFailed;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter")
	FBossEncounterSucceededSignature OnBossEncounterSucceeded;

	UFUNCTION(BlueprintPure, Category = "Boss|Encounter")
	EBossPhaseTransitionEncounterState GetPhaseTransitionState() const;

protected:
	virtual void BeginPlay() override;

private:
	void BeginRideWarning();
	void BeginRideWindow();
	void HandleRideWindowTimerFinished();
	void SchedulePhaseTransitionSuccess();
	void ResetCurseStateForAllPlayers();
	void ApplyFailDamage();
	void ClearRideRootEffects();
	void StartRockWarningLoop();
	void StopRockWarningLoop();
	void SpawnRockWarningsForLivingPlayers();
	void ClearRockWarnings();
	void SpawnFallingWarningsForLivingPlayers(TSubclassOf<class ACPP_BossRockWarningActor> WarningActorClass, float SpawnRadiusAroundPlayer, TArray<TObjectPtr<class ACPP_BossRockWarningActor>>& OutSpawnedWarnings);
	void StartRedLightningLoop();
	void StopRedLightningLoop();
	void SpawnRedLightningsForLivingPlayers();
	void ClearRedLightnings();
	void SpawnCrossLaser();
	void ClearCrossLaser();
	void GrantClearEncounterShield();
	void BindBossShieldChangedDelegate();
	void UnbindBossShieldChangedDelegate();
	void HandleBossShieldChanged(const FOnAttributeChangeData& ChangeData);
	void EndClearEncounter();
	class ACPP_BossCharacter* GetBossOwner() const;
	void SpawnSandStorms();
	void ClearSandStorms();
	void AssignSandStormTargets();
	void GetLivingPlayerPawns(TArray<AActor*>& OutLivingPlayers) const;
	bool IsLivingPlayerPawn(AActor* Candidate) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RockPhaseDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RideWarningDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RideWindowDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SuccessDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Fail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FailDamageRadius = 50000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Fail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FailDamageAmount = 999999.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Rock", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class ACPP_BossRockWarningActor> RockWarningActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Rock", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float RockSpawnInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Rock", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RockSpawnRadiusAroundPlayer = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|SandStorm", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class ACPP_BossSandStormRideActor> SandStormRideActorClass;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true"))
	EBossPhaseTransitionEncounterState PhaseTransitionState = EBossPhaseTransitionEncounterState::Inactive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 RequiredRideCount = 3;

	int32 CompletedRideCount = 0;
	TSet<TWeakObjectPtr<AActor>> CompletedRiders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class ACPP_BossRockWarningActor>> ActiveRockWarnings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class ACPP_BossSandStormRideActor>> ActiveSandStorms;

	//! \brief Active gimmick hazard actors spawned by gimmick pattern abilities. Weak refs since they self-destruct on their own timers.
	TArray<TWeakObjectPtr<AActor>> ActiveGimmickHazards;

	bool bPhaseTransitionEncounterActive = false;

	FTimerHandle RockPhaseTimerHandle;
	FTimerHandle RockSpawnTimerHandle;
	FTimerHandle RideWarningTimerHandle;
	FTimerHandle RideWindowTimerHandle;
	FTimerHandle SuccessDelayTimerHandle;

	//====================
	// Clear (final judgment) encounter

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ClearEncounterDuration = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> ClearShieldGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ClearShieldAmount = 100000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear|RedLightning", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class ACPP_BossRockWarningActor> RedLightningActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear|RedLightning", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float RedLightningInterval = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear|RedLightning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RedLightningSpawnRadiusAroundPlayer = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Encounter|Clear|CrossLaser", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class ACPP_BossCrossLaserActor> CrossLaserActorClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class ACPP_BossRockWarningActor>> ActiveRedLightnings;

	UPROPERTY(Transient)
	TObjectPtr<class ACPP_BossCrossLaserActor> ActiveCrossLaser;

	bool bClearEncounterActive = false;

	//! \brief One-shot guard: the clear encounter is a final mechanic that may only ever start once.
	bool bClearEncounterConsumed = false;

	FDelegateHandle BossShieldChangedDelegateHandle;

	FTimerHandle ClearEncounterTimerHandle;
	FTimerHandle RedLightningTimerHandle;
};
