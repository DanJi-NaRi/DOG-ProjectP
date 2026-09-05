////////////////////////////
//! \file MyPiercingProjectile.h
//! \brief 경로의 모든 적을 한 번씩 관통 타격하는 범용 투사체 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "MyPiercingProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;

////////////////////////////
//! \class AMyPiercingProjectile
//! \author HanUl
//! \brief 전방으로 날아가며 경로에 닿는 모든 적을 한 번씩 관통 타격하는 투사체다.
//!        적중해도 소멸하지 않고 지형(WorldStatic)도 관통하며, 최대 사거리 수명에서 종료한다. 판정/피해는 서버에서만 수행한다.
UCLASS()
class PROJECTP_API AMyPiercingProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMyPiercingProjectile();

	virtual void Tick(float DeltaSeconds) override;

	////////////////////////////
	//! \brief 발사 소스와 데미지/사거리/속도/반경/계수, 처치·카메라 피드백용 태그를 설정하고 발사한다.
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		AActor* InSourceActor,
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
		float InRange,
		float InProjectileSpeed,
		float InProjectileRadius,
		float InDamageCoefficient,
		FGameplayTag InSkillCooldownTag,
		FGameplayTag InSkillInputTag
	);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float LifeSeconds = 5.0f;

	//! \brief 종료 연출 이후 Actor 제거까지 걸리는 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	float FinishLifeSeconds = 0.8f;

	//! \brief true이고 /debugline 치트가 켜지면 현재 위치의 실제 판정 구체 1개를 매 프레임 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Debug")
	bool bDrawDebugHitSphere = true;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float Range = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float ProjectileRadius = 20.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float DamageCoefficient = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	//! \brief 처치 스킬 식별용 쿨다운 태그(피해 Spec 꼬리표).
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SkillCooldownTag;

	//! \brief Basic/Q/E/R/C 구분용 스킬 입력 태그(카메라 피드백 등급 해석에 사용).
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SkillInputTag;

	UPROPERTY(Transient)
	bool bProjectileInitialized = false;

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
	void FinishProjectile();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_LaunchProjectileVisual(FVector_NetQuantize LaunchLocation, FVector_NetQuantizeNormal LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FinishProjectileVisual(float NewFinishLifeSeconds);

	//! \brief 종료 연출을 BP에서 처리하기 위한 훅.
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void OnProjectileFinished();

private:
	//! \brief 이미 관통 타격한 대상(중복 적중 방지).
	TArray<TWeakObjectPtr<AActor>> HitActors;
};
