#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "CPP_BossSandStormChaseActor.generated.h"

class USceneComponent;
class USphereComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

////////////////////////////
//! \class ACPP_BossSandStormChaseActor
//! \brief P1 기믹 패턴용 추격 모래폭풍. 무작위 표적을 추격하며 접촉 시 지속 피해를 준다.
//!        페이즈 전환 라이드 폭풍(ACPP_BossSandStormRideActor)과 달리 탑승/전환 로직 없이 순수 피해만 준다.
//! \note 서버 권한에서만 이동/피해/표적 관리가 진행되며, 이동은 SetReplicateMovement로 클라이언트에 복제된다.
//!       표적 발밑 마크는 마킹 GameplayEffect가 부착한 GameplayCue로 전 클라이언트에 표시된다.
UCLASS()
class PROJECTP_API ACPP_BossSandStormChaseActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossSandStormChaseActor();

	virtual void Tick(float DeltaSeconds) override;

	////////////////////////////
	//! \brief 서버에서 스폰 직후 호출되어 추격 폭풍을 구동한다.
	//! \param InSourceASC 피해/마크 GameplayEffect를 적용할 보스 ASC.
	//! \param InInitialTarget 최초 표적(어빌리티가 무작위로 선택한 생존 플레이어).
	//! \param InDamageGameplayEffect 접촉 피해용 SetByCaller GameplayEffect(보스 피해 GE 재사용).
	//! \param InTargetMarkGameplayEffect 표적 상태 태그 + 발밑 마크 Cue를 부여하는 무한 지속 GameplayEffect.
	//! \param InExistingMarkHandle 어빌리티가 이미 최초 표적에게 적용한 마킹 GE 핸들(소유권 이관).
	void Initialize(
		UAbilitySystemComponent* InSourceASC,
		AActor* InInitialTarget,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffect,
		TSubclassOf<UGameplayEffect> InTargetMarkGameplayEffect,
		const FActiveGameplayEffectHandle& InExistingMarkHandle
	);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm")
	TObjectPtr<USphereComponent> DamageSphere;

private:
	void BeginChase();
	void HandleLifetimeExpired();
	void MoveTowardTarget(float DeltaSeconds);

	void RefreshTargetIfNeeded();
	bool IsTargetAlive() const;
	void SetCurrentTarget(AActor* NewTarget);
	void ApplyTargetMark(AActor* MarkTarget);
	void RemoveTargetMark();

	void StartDamageForCurrentOverlaps();
	void TryStartDamageForActor(AActor* DamageTargetActor);
	void StartDamageTimerForActor(AActor* DamageTargetActor);
	void StopDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor);
	void StopAllDamageTimers();
	void HandleDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor);
	void ApplyDamageToActor(AActor* DamageTargetActor);
	bool ShouldDamageActor(const AActor* CandidateActor) const;

	UFUNCTION()
	void HandleDamageSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleDamageSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	//! \brief 접촉 피해 판정 반경(cm). 1.2m 요구사항.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DamageRadius = 120.0f;

	//! \brief 추격 이동 속도(cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MoveSpeed = 350.0f;

	//! \brief 스폰 후 이동을 시작하기까지의 정지 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MoveStartDelay = 1.0f;

	//! \brief 스폰 시점 기준 총 지속 시간(초). 만료 시 스스로 소멸.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float Lifetime = 15.0f;

	//! \brief 접촉 피해 적용 주기(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float DamageInterval = 1.0f;

	//! \brief 1회 피해량 = 표적 현재 체력 * 이 비율.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float DamageHealthRatio = 0.5f;

	//! \brief 접촉 피해 틱마다 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|SandStorm|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float CurseGaugeAmount = 0.0f;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> TargetMarkGameplayEffect;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	FActiveGameplayEffectHandle TargetMarkHandle;

	bool bChasing = false;

	FTimerHandle MoveStartTimerHandle;
	FTimerHandle LifetimeTimerHandle;
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> DamageTimerHandlesByActor;
};
