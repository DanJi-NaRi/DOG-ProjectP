#pragma once

#include "CoreMinimal.h"
#include "Enemy/Actors/CPP_EnemyProjectileBase.h"
#include "CPP_EnemySplitProjectile.generated.h"

////////////////////////////
//! \class ACPP_EnemySplitProjectile
//! \brief 최초 탄의 종료 위치에서 진행 방향 기준 전후좌우로 네 개의 분열탄을 생성하는 적 투사체.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_EnemySplitProjectile : public ACPP_EnemyProjectileBase
{
	GENERATED_BODY()

public:
	ACPP_EnemySplitProjectile();

protected:
	virtual void BeginPlay() override;
	virtual void LifeSpanExpired() override;
	virtual bool CanApplyProjectileHitToActor(AActor* HitActor) const override;
	virtual void HandleProjectileHitResolved(AActor* HitActor, const FHitResult& SweepResult) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile|Split", meta = (ClampMin = "0.0", Units = "cm"))
	float FragmentRange = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile|Split", meta = (ClampMin = "0.0", Units = "cm"))
	float FragmentPlayerSafeDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile|Split", meta = (ClampMin = "0.0", Units = "cm"))
	float WallSplitSpawnOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile|Split")
	TSubclassOf<ACPP_EnemySplitProjectile> FragmentProjectileClass;

private:
	UFUNCTION()
	void HandleProjectileStopped(const FHitResult& ImpactResult);

	void TrySplit(const FVector& SplitLocation, const FVector& IncomingDirection);
	void SpawnFragment(const FVector& SplitLocation, const FVector& FragmentDirection);
	void ConfigureAsFragment(const FVector& InFragmentStartLocation);

	FVector FragmentStartLocation = FVector::ZeroVector;
	bool bIsFragment = false;
	bool bHasSplit = false;
};
