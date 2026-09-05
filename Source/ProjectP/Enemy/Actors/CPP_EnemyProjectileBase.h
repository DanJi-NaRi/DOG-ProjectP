// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_EnemyProjectileBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
struct FGameplayEffectContextHandle;

UCLASS()
class PROJECTP_API ACPP_EnemyProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	ACPP_EnemyProjectileBase();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Projectile")
	void InitializeProjectile(
		UAbilitySystemComponent* InSourceASC,
		AActor* InTargetActor,
		TSubclassOf<UGameplayEffect> InHitGameplayEffect,
		TSubclassOf<UGameplayEffect> InStatusGameplayEffect,
		float InRange,
		float InProjectileSpeed,
		float InProjectileRadius,
		float InDamageCoefficient
	);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	//! \brief 발사자(적)의 ASC. 피해/상태이상 GameplayEffect Spec을 이 ASC로 생성해 Source가 발사자가 되게 한다.
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile", meta = (ClampMin = "0.0"))
	float LifeSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Projectile|Debug")
	bool bDrawDebugHitSphere = false;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	float Range = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	float ProjectileRadius = 24.0f;

	//! \brief 스킬 피해 계수. 공격력 곱셈은 ExecutionCalculation이 캡처한 Source AttackPower로 수행한다.
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	float DamageCoefficient = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	TSubclassOf<UGameplayEffect> HitGameplayEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Projectile")
	TSubclassOf<UGameplayEffect> StatusGameplayEffect;

	UFUNCTION()
	void HandleProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	bool ApplyStatusEffectToTarget(
		UAbilitySystemComponent* TargetASC,
		const FGameplayEffectContextHandle& EffectContext
	);

	virtual bool CanApplyProjectileHitToActor(AActor* HitActor) const;
	virtual void HandleProjectileHitResolved(AActor* HitActor, const FHitResult& SweepResult);

private:
	bool ApplyHitEffectToActor(AActor* HitActor);
};
