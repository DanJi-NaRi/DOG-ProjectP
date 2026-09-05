#include "CPP_EnemySplitProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

ACPP_EnemySplitProjectile::ACPP_EnemySplitProjectile()
{
	SetReplicateMovement(true);

	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

void ACPP_EnemySplitProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		if (CollisionComponent)
		{
			CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return;
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(
			this,
			&ACPP_EnemySplitProjectile::HandleProjectileStopped
		);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 최대 사거리에 도달한 최초 탄을 현재 위치에서 분열시키고 액터를 제거한다.
//! \return None
void ACPP_EnemySplitProjectile::LifeSpanExpired()
{
	if (HasAuthority() && !bIsFragment)
	{
		TrySplit(GetActorLocation(), GetActorForwardVector());
	}

	Super::LifeSpanExpired();
}

////////////////////////////
//! \author HanSeul
//! \brief 분열탄이 생성 지점의 플레이어 안전 거리를 벗어난 경우에만 피해를 허용한다.
//! \param HitActor 피해 후보 액터.
//! \return 플레이어 피해를 적용할 수 있으면 true.
bool ACPP_EnemySplitProjectile::CanApplyProjectileHitToActor(AActor* HitActor) const
{
	if (!HitActor)
	{
		return false;
	}

	if (!bIsFragment || FragmentPlayerSafeDistance <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(GetActorLocation(), FragmentStartLocation)
		>= FMath::Square(FragmentPlayerSafeDistance);
}

////////////////////////////
//! \author HanSeul
//! \brief 최초 탄이 플레이어에게 피해를 적용한 위치에서 한 번만 분열한다.
//! \param HitActor 최초 탄에 맞은 플레이어.
//! \param SweepResult 플레이어 Overlap Sweep 결과.
//! \return None
void ACPP_EnemySplitProjectile::HandleProjectileHitResolved(AActor* HitActor, const FHitResult&)
{
	if (HitActor && !bIsFragment)
	{
		TrySplit(GetActorLocation(), GetActorForwardVector());
	}
}

////////////////////////////
//! \author HanSeul
//! \brief WorldStatic 벽에 멈춘 최초 탄은 벽 법선 방향으로 미세 보정한 위치에서 분열하고, 분열탄은 제거한다.
//! \param ImpactResult ProjectileMovement가 전달한 벽 충돌 결과.
//! \return None
void ACPP_EnemySplitProjectile::HandleProjectileStopped(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsFragment)
	{
		FVector ImpactLocation = GetActorLocation();
		if (ImpactResult.bBlockingHit)
		{
			ImpactLocation = FVector(ImpactResult.Location);
		}
		const FVector ImpactNormal = ImpactResult.ImpactNormal.GetSafeNormal();
		const FVector SplitLocation = ImpactLocation + ImpactNormal * FMath::Max(WallSplitSpawnOffset, 0.0f);
		TrySplit(SplitLocation, GetActorForwardVector());
	}

	Destroy();
}

////////////////////////////
//! \author HanSeul
//! \brief 최초 진행 방향을 XY 평면에 투영하고 전후좌우 네 방향으로 분열탄을 생성한다.
//! \param SplitLocation 네 분열탄이 공유하는 생성 위치.
//! \param IncomingDirection 최초 탄의 진행 방향.
//! \return None
void ACPP_EnemySplitProjectile::TrySplit(const FVector& SplitLocation, const FVector& IncomingDirection)
{
	if (!HasAuthority() || bIsFragment || bHasSplit)
	{
		return;
	}

	bHasSplit = true;

	FVector ForwardDirection(IncomingDirection.X, IncomingDirection.Y, 0.0f);
	if (!ForwardDirection.Normalize())
	{
		ForwardDirection = GetActorForwardVector();
		ForwardDirection.Z = 0.0f;
		ForwardDirection.Normalize();
	}

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::ForwardVector;
	}

	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, ForwardDirection).GetSafeNormal();
	SpawnFragment(SplitLocation, ForwardDirection);
	SpawnFragment(SplitLocation, -ForwardDirection);
	SpawnFragment(SplitLocation, RightDirection);
	SpawnFragment(SplitLocation, -RightDirection);
}

////////////////////////////
//! \author HanSeul
//! \brief 같은 피해 정보와 속도·반지름을 사용하는 단일 분열탄을 서버에서 Deferred Spawn한다.
//! \param SplitLocation 분열탄 생성 위치.
//! \param FragmentDirection 분열탄 이동 방향.
//! \return None
void ACPP_EnemySplitProjectile::SpawnFragment(const FVector& SplitLocation, const FVector& FragmentDirection)
{
	UWorld* World = GetWorld();
	if (!World || !SourceASC.IsValid() || FragmentRange <= 0.0f || InitialSpeed <= 0.0f || ProjectileRadius <= 0.0f)
	{
		return;
	}

	TSubclassOf<ACPP_EnemySplitProjectile> SpawnClass = FragmentProjectileClass;
	if (!SpawnClass)
	{
		SpawnClass = GetClass();
	}

	const FVector SafeDirection = FragmentDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	const FTransform SpawnTransform(SafeDirection.Rotation(), SplitLocation);
	ACPP_EnemySplitProjectile* Fragment = World->SpawnActorDeferred<ACPP_EnemySplitProjectile>(
		SpawnClass,
		SpawnTransform,
		GetOwner(),
		GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!Fragment)
	{
		return;
	}

	Fragment->ConfigureAsFragment(SplitLocation);
	Fragment->FinishSpawning(SpawnTransform);
	Fragment->InitializeProjectile(
		SourceASC.Get(),
		TargetActor,
		HitGameplayEffect,
		StatusGameplayEffect,
		FragmentRange,
		InitialSpeed,
		ProjectileRadius,
		DamageCoefficient
	);
}

void ACPP_EnemySplitProjectile::ConfigureAsFragment(const FVector& InFragmentStartLocation)
{
	bIsFragment = true;
	bHasSplit = true;
	FragmentStartLocation = InFragmentStartLocation;
}
