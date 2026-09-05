////////////////////////////
//! \page MyNeferProjectile.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MyNeferProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;

////////////////////////////
//! \class AMyNeferProjectile
//! \brief Nefer 스킬들이 사용하는 기본 비관통 투사체이다.
UCLASS()
class PROJECTP_API AMyNeferProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMyNeferProjectile();

	UFUNCTION(BlueprintCallable, Category = "Nefer|Projectile")
	void InitializeProjectile(
		AActor* InSourceActor,
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
		float InRange,
		float InProjectileSpeed,
		float InProjectileRadius,
		float InDamageCoefficient,
		TSubclassOf<UGameplayEffect> InDecayGameplayEffectClass = nullptr,
		float InDecayTickDamageCoefficient = 0.10f,
		float InKnockbackDistance = 0.0f
	);

	////////////////////////////
	//! \brief 적중 시 적중 지점에서 원형 범위로 폭발하도록 설정한다. 호출하지 않으면 폭발하지 않는다(기존 동작 유지).
	//! \param InExplosionRadius 폭발 판정 반경(cm). 0 이하이면 폭발하지 않는다
	//! \param InExplosionDamageCoefficient 폭발 피해 계수(공격력에 곱해짐)
	void ConfigureExplosion(float InExplosionRadius, float InExplosionDamageCoefficient);

	void ConfigureChain(int32 InMaxAdditionalTargets, float InSearchRadius, bool bInHitEachTargetOnce);
	void ConfigureExistingStatusBonus(const FGameplayTagContainer& InStatusTags, float InBonusDamageCoefficient);
	void ConfigureAttackerHitCameraFeedback(FGameplayTag InSkillInputTag);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nefer|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nefer|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile", meta = (ClampMin = "0.0"))
	float LifeSeconds = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	float Range = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	float ProjectileRadius = 20.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	float DamageCoefficient = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	float DecayTickDamageCoefficient = 0.10f;

	//! \brief 적중 시 대상을 진행 방향으로 밀어낼 거리(cm). 0이면 넉백 없음
	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	float KnockbackDistance = 0.0f;

	//! \brief 넉백 거리(cm)를 LaunchCharacter 속도(cm/s)로 바꾸는 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile", meta = (ClampMin = "0.0"))
	float KnockbackVelocityPerDistance = 6.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile")
	TSubclassOf<UGameplayEffect> DecayGameplayEffectClass;

	//! \brief 적중 지점 폭발 반경(cm). 0이면 폭발하지 않는다(Fragment 미등록 스킬 기본값).
	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Explosion")
	float ExplosionRadius = 0.0f;

	//! \brief 폭발 피해 계수(공격력에 곱해짐).
	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Explosion")
	float ExplosionDamageCoefficient = 0.0f;

	//! \brief true면 폭발 판정 반경을 /debugline 치트가 켜졌을 때 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile|Explosion|Debug")
	bool bDrawDebugExplosion = true;

	//! \brief 폭발 디버그 도형 표시 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile|Explosion|Debug", meta = (ClampMin = "0.0"))
	float DebugExplosionLifeTime = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Chain")
	int32 RemainingChainCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Chain")
	float ChainSearchRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Chain")
	bool bHitEachTargetOnce = true;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Status Interaction")
	FGameplayTagContainer ExistingStatusTags;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Status Interaction")
	float ExistingStatusBonusDamageCoefficient = 0.0f;

	//! \brief 직접 적중 피해 EffectSpec에만 부착할 공격자 본인 카메라 피드백 등급 태그
	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Projectile|Camera Feedback")
	FGameplayTag AttackerHitCameraFeedbackTag;

	UPROPERTY(Transient)
	bool bProjectileInitialized = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nefer|Projectile", meta = (ClampMin = "0.0"))
	float FinishLifeSeconds = 0.8f;

	UPROPERTY(Transient)
	bool bProjectileFinished = false;


	UFUNCTION()
	void HandleProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void ConfigureCollisionForProjectile();
	void ApplyProjectileLaunchVisual(const FVector& LaunchLocation, const FVector& LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds);
	void ApplyProjectileMovementVisual(const FVector& NewLocation, const FVector& NewDirection, float NewSpeed, float NewLifeSeconds);
	void ApplyProjectileFinishedVisual(float NewFinishLifeSeconds);
	void ApplyProjectileCollisionState(bool bEnableAuthorityCollision);
	bool ApplyDamageToActor(AActor* HitActor);
	void ApplyExplosionDamage(const FVector& ExplosionLocation);
	void ApplyKnockbackToActor(AActor* HitActor) const;
	bool ApplyDamageSpecToTarget(UAbilitySystemComponent* TargetASC, float InDamageCoefficient, const FGameplayEffectContextHandle& EffectContext) const;
	bool HasActorAlreadyBeenHit(AActor* TargetActor) const;
	bool HasExistingStatus(UAbilitySystemComponent* TargetASC) const;
	AActor* FindNextChainTarget() const;
	void ContinueChainToTarget(AActor* TargetActor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_LaunchProjectileVisual(FVector_NetQuantize LaunchLocation, FVector_NetQuantizeNormal LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ContinueChainVisual(FVector_NetQuantize ChainLocation, FVector_NetQuantizeNormal ChainDirection, float ChainSpeed, float ChainLifeSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FinishProjectileVisual(float NewFinishLifeSeconds);
	
	//! \brief 연출을 쓰이기 위해서 bp로 뺐음
	UFUNCTION(BlueprintImplementableEvent, Category = "Nefer|Projectile")
	void OnProjectileFinished();

	void FinishProjectile();

private:
	TArray<TWeakObjectPtr<AActor>> HitActors;
};
