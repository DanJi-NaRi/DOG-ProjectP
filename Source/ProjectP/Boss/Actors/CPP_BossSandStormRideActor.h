#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BossSandStormRideActor.generated.h"

class USceneComponent;
class USphereComponent;
class UCPP_BossEncounterDirectorComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS()
class PROJECTP_API ACPP_BossSandStormRideActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossSandStormRideActor();

	virtual void Tick(float DeltaSeconds) override;

	void Initialize(
		UCPP_BossEncounterDirectorComponent* InEncounterDirector,
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InPreRideDamageGameplayEffect
	);
	void NotifyCurrentOverlappingRiders();
	void SetTargetActor(AActor* InTargetActor);
	AActor* GetTargetActor() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm")
	TObjectPtr<USphereComponent> RideTrigger;

private:
	void RefreshTargetIfNeeded();
	bool IsTargetAlive() const;
	void MoveTowardTarget(float DeltaSeconds);
	void StartPreRideDamageForCurrentOverlaps();
	void TryStartPreRideDamageForActor(AActor* DamageTargetActor);
	void StartPreRideDamageTimerForActor(AActor* DamageTargetActor);
	void StopPreRideDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor);
	void StopAllPreRideDamageTimers();
	void HandlePreRideDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor);
	void ApplyPreRideDamageToActor(AActor* DamageTargetActor);
	void ApplyRideRootEffect(AActor* Rider);
	void StopAfterSuccessfulRide();

	UFUNCTION()
	void HandleRideTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleRideTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RideRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MoveSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float StopDistanceAfterRideSuccess = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> RideRootGameplayEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float PreRideDamageInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float PreRideDamageHealthRatio = 0.5f;

	UPROPERTY()
	TWeakObjectPtr<UCPP_BossEncounterDirectorComponent> EncounterDirector;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> PreRideDamageGameplayEffect;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	bool bStopWhenReachedTarget = false;

	TMap<TWeakObjectPtr<AActor>, FTimerHandle> PreRideDamageTimerHandlesByActor;
};
