#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "CPP_BossTelegraphActor.generated.h"

class UMaterialInterface;
class UDecalComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

////////////////////////////
//! \brief Replicated spawn data that lets every client rebuild the telegraph visual locally.
USTRUCT()
struct FBossTelegraphSpawnData
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform SourceTransform = FTransform::Identity;

	UPROPERTY()
	FBossHitShapeData HitShape;

	//! \brief Telegraph fill duration in seconds. Every machine evaluates it against synchronized server time.
	UPROPERTY()
	float Duration = 0.0f;

	//! \brief Synchronized server-world timestamp at which the telegraph warning began.
	UPROPERTY()
	float ServerStartTime = 0.0f;
};

UCLASS()
class PROJECTP_API ACPP_BossTelegraphActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossTelegraphActor();

	//! \brief Authority-side entry point: stores replicated spawn data and applies the telegraph locally.
	void Initialize(const FTransform& SourceTransform, const FBossHitShapeData& InHitShape, float InDuration = 0.0f);
	float GetServerStartTime() const;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	//! \brief Builds the decal visual from the current spawn data. Runs on every machine (authority + clients).
	void ApplyTelegraph();

	//! \brief Pushes the current fill progress (0..1) into the decal material instance.
	void UpdateFillProgress();
	float GetSynchronizedServerTime() const;

	UFUNCTION()
	void OnRep_SpawnData();

	bool FindGroundLocation(const FVector& ShapeCenter, FVector& OutGroundLocation) const;
	FVector GetDecalCenterLocation(const FVector& ShapeOrigin, const FRotator& ShapeRotation) const;
	void UpdateDecalVisual();
	void ApplyDecalMaterialParameters(UMaterialInstanceDynamic* DecalMaterialInstance) const;
	void SetTelegraphVisible(bool bVisible) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> TelegraphDecalComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> TelegraphMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_GameTraceChannel2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "0.0"))
	float TraceStartHeight = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "0.0"))
	float TraceEndDepth = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true", Units = "cm"))
	float DecalSurfaceOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true", Units = "cm", ClampMin = "1.0"))
	float DecalProjectionDepth = 200.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SpawnData)
	FBossTelegraphSpawnData SpawnData;

	//! \brief Cached dynamic material instance driving the decal, used to update fill progress per tick.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterialInstance;

	//! \brief Elapsed warning time derived from synchronized server time.
	float ElapsedTime = 0.0f;
};
