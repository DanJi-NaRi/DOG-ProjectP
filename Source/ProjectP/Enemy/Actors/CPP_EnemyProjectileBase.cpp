// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyProjectileBase.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MySkillDebugShape.h"
#include "GameFramework/ProjectileMovementComponent.h"

ACPP_EnemyProjectileBase::ACPP_EnemyProjectileBase()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrimaryActorTick.bCanEverTick = true;
#else
	PrimaryActorTick.bCanEverTick = false;
#endif
	bReplicates = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(24.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;
}

void ACPP_EnemyProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ACPP_EnemyProjectileBase::HandleProjectileOverlap);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 개발 디버그가 활성화되면 현재 투사체 위치에 실제 충돌 반경 구체를 표시한다.
//! \param DeltaSeconds 프레임 델타 시간.
//! \return None
void ACPP_EnemyProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugHitSphere && CollisionComponent)
	{
		MySkillDebugDraw::DrawShape(
			GetWorld(),
			FMySkillDebugShape::MakeSphere(
				CollisionComponent->GetComponentLocation(),
				CollisionComponent->GetScaledSphereRadius(),
				FColor::Red,
				0.0f,
				1.0f
			)
		);
	}
#endif
}

////////////////////////////
//! \author HanSeul
//! \brief Initializes this projectile with its source ASC, target, hit GameplayEffect, and final damage.
//! \param InSourceASC 발사자(적)의 ASC. 피해 GameplayEffect의 Source가 된다.
//! \param InTargetActor Intended target actor.
//! \param InHitGameplayEffect GameplayEffect class applied when the projectile overlaps a valid target.
//! \param InStatusGameplayEffect Optional status GameplayEffect class applied after the hit effect.
//! \param InRange Maximum projectile travel range in centimeters.
//! \param InProjectileSpeed Projectile speed in centimeters per second.
//! \param InProjectileRadius Projectile overlap radius in centimeters.
//! \param InDamageCoefficient 스킬 피해 계수. 공격력 곱셈은 ExecutionCalculation이 담당한다.
//! \return None
void ACPP_EnemyProjectileBase::InitializeProjectile(
	UAbilitySystemComponent* InSourceASC,
	AActor* InTargetActor,
	TSubclassOf<UGameplayEffect> InHitGameplayEffect,
	TSubclassOf<UGameplayEffect> InStatusGameplayEffect,
	float InRange,
	float InProjectileSpeed,
	float InProjectileRadius,
	float InDamageCoefficient
)
{
	SourceASC = InSourceASC;
	TargetActor = InTargetActor;
	HitGameplayEffect = InHitGameplayEffect;
	StatusGameplayEffect = InStatusGameplayEffect;

	Range = FMath::Max(InRange, 0.0f);
	InitialSpeed = FMath::Max(InProjectileSpeed, 0.0f);
	ProjectileRadius = FMath::Max(InProjectileRadius, 0.0f);
	DamageCoefficient = FMath::Max(InDamageCoefficient, 0.0f);
	LifeSeconds = InitialSpeed > 0.0f && Range > 0.0f ? Range / InitialSpeed : LifeSeconds;

	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(ProjectileRadius);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = InitialSpeed;
		ProjectileMovementComponent->MaxSpeed = InitialSpeed;
		ProjectileMovementComponent->Velocity = GetActorForwardVector() * InitialSpeed;
	}

	SetLifeSpan(LifeSeconds);
}

////////////////////////////
//! \author HanSeul
//! \brief Applies the projectile GameplayEffect to the first valid overlapped actor.
//! \param OverlappedComponent Component that received the overlap.
//! \param OtherActor Actor overlapped by this projectile.
//! \param OtherComp Component of OtherActor that overlapped.
//! \param OtherBodyIndex Body index from the overlap event.
//! \param bFromSweep Whether the overlap came from a sweep.
//! \param SweepResult Sweep data for the overlap event.
//! \return None
void ACPP_EnemyProjectileBase::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	// Faction 판정에는 발사자(Instigator)를 사용한다.
	AActor* SourceAvatar = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	if (!HasAuthority() || !OtherActor || OtherActor == this || !UMyAbilitySystemLibrary::IsHostile(SourceAvatar, OtherActor))
	{
		return;
	}

	if (!CanApplyProjectileHitToActor(OtherActor))
	{
		return;
	}

	if (ApplyHitEffectToActor(OtherActor))
	{
		HandleProjectileHitResolved(OtherActor, SweepResult);
		Destroy();
	}
}

bool ACPP_EnemyProjectileBase::CanApplyProjectileHitToActor(AActor*) const
{
	return true;
}

void ACPP_EnemyProjectileBase::HandleProjectileHitResolved(AActor*, const FHitResult&)
{
}

bool ACPP_EnemyProjectileBase::ApplyHitEffectToActor(AActor* HitActor)
{
	if (!SourceASC.IsValid() || !HitGameplayEffect || !HitActor || DamageCoefficient <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(HitActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HitGameplayEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	if (!UMyAbilitySystemLibrary::AssignSetByCallerCoefficient(SpecHandle, DamageCoefficient))
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	ApplyStatusEffectToTarget(TargetASC, EffectContext);
	return true;
}

bool ACPP_EnemyProjectileBase::ApplyStatusEffectToTarget(
	UAbilitySystemComponent* TargetASC,
	const FGameplayEffectContextHandle& EffectContext
)
{
	if (!SourceASC.IsValid() || !TargetASC || !StatusGameplayEffect)
	{
		return false;
	}

	const FGameplayEffectSpecHandle StatusSpecHandle = SourceASC->MakeOutgoingSpec(StatusGameplayEffect, 1.0f, EffectContext);
	if (!StatusSpecHandle.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpecHandle.Data.Get(), TargetASC);
	return true;
}
