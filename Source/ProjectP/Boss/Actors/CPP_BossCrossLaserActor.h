#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BossCrossLaserActor.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class UGameplayEffect;
class USceneComponent;
class UPrimitiveComponent;

////////////////////////////
//! \author HanSeul
//! \brief Continuously rotating cross-shaped laser used during the boss clear (final judgment) encounter.
//!        Rotation is derived deterministically from a replicated server-time stamp so the client visual and the
//!        server-side collision stay aligned without streaming movement. Damage is applied on the server only, and
//!        follows the sandstorm model: an immediate hit on contact, then a repeating per-actor tick while overlapping.
UCLASS()
class PROJECTP_API ACPP_BossCrossLaserActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossCrossLaserActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//! \brief Authority-side setup: stores the damage source and effect used by the laser damage ticks.
	void Initialize(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageGameplayEffect);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|CrossLaser")
	TObjectPtr<USceneComponent> SceneRoot;

	//! \brief The two perpendicular arms of the cross. They serve as both the damage overlap volumes and the debug
	//!        wireframe visual; collision is QueryOnly and overlaps pawns only.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|CrossLaser")
	TObjectPtr<UBoxComponent> ArmBoxA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|CrossLaser")
	TObjectPtr<UBoxComponent> ArmBoxB;

private:
	void UpdateRotationFromServerTime();
	float GetCurrentServerTime() const;
	void UpdateArmBoxExtents();

	UFUNCTION()
	void HandleArmBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleArmEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void StartLaserDamageForCurrentOverlaps();
	void TryStartLaserDamageForActor(AActor* DamageTargetActor);
	void StartLaserDamageTimerForActor(AActor* DamageTargetActor);
	void StopLaserDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor);
	void StopAllLaserDamageTimers();
	void HandleLaserDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor);
	void ApplyLaserDamageToActor(AActor* DamageTargetActor);
	bool IsActorInAnyArm(AActor* OtherActor) const;
	bool IsDamageSourceActor(const AActor* OtherActor) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "deg/s"))
	float AngularSpeedDegPerSec = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "deg"))
	float InitialYawDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "1.0"))
	float ArmLength = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "1.0"))
	float ArmHalfWidth = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "1.0"))
	float ArmHalfHeight = 200.0f;

	//! \brief Interval between repeated damage ticks while a target stays inside an arm (contact hit is immediate).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", Units = "s", ClampMin = "0.05"))
	float DamageTickInterval = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DamageCoefficient = 2.5f;

	//! \brief 레이저 피해 틱마다 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|CrossLaser", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float CurseGaugeAmount = 0.0f;

	//! \brief Replicated once at spawn; the shared reference point every machine uses to compute the current angle.
	UPROPERTY(Replicated)
	float StartServerTime = 0.0f;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	//! \brief Per-target repeating damage timers, keyed by the overlapping actor (sandstorm-style).
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> LaserDamageTimerHandlesByActor;
};
