#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BossBlackHoleActor.generated.h"

class USceneComponent;
class USphereComponent;
class UAbilitySystemComponent;
class UGameplayEffect;
class ACharacter;
class ACPP_BossTelegraphActor;

////////////////////////////
//! \class ACPP_BossBlackHoleActor
//! \brief P2 기믹 패턴(BOS_SET_P2_PAT_04) 검은 구. 전장 중앙에 소환되어 PullRadius 내 플레이어를 계속 끌어당기고,
//!        Lifetime(5초) 후 KillRadius 내 플레이어를 즉사시키며 폭발·소멸한다.
//! \note 서버 권한에서만 흡입/즉사 로직이 진행된다. 끌어당김은 각 플레이어 CMC에 RadialForce 루트모션 소스(Additive)를
//!       적용하는 방식이라 클라이언트 예측과 충돌하지 않고 부드럽게 복제된다. 흡입 범위는 디버그용 와이어 구로 표시하고,
//!       즉사 범위는 블루프린트 텔레그래프 데칼로 표시한다.
UCLASS()
class PROJECTP_API ACPP_BossBlackHoleActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossBlackHoleActor();

	virtual void Tick(float DeltaSeconds) override;

	////////////////////////////
	//! \brief 서버에서 스폰 직후 호출되어 검은 구를 구동한다.
	//! \param InSourceASC 즉사 GameplayEffect를 적용할 보스 ASC.
	//! \param InKillGameplayEffect 즉사에 사용할 SetByCaller 데미지 GameplayEffect(보스 피해 GE 재사용).
	//! \param InKillDamage 즉사용 데미지 수치(확정 사살을 위한 초대량 값).
	void Initialize(
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InKillGameplayEffect,
		float InKillDamage
	);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//! \brief 폭발 시 전 클라이언트에서 재생되는 코스메틱 훅. 블루프린트에서 폭발 VFX를 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|BlackHole")
	void OnBlackHoleExploded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole")
	TObjectPtr<USceneComponent> SceneRoot;

	//! \brief 흡입 범위(600) 디버그 시각화용 와이어 구. 대상 수집에는 쓰지 않는다(살아있는 플레이어를 직접 조회).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole")
	TObjectPtr<USphereComponent> PullRangeSphere;

private:
	void RefreshPull();
	void EnsurePullSourceForCharacter(ACharacter* Character);
	void RemovePullSourceForCharacter(const TWeakObjectPtr<ACharacter>& CharacterPtr);
	void RemoveAllPullSources();

	void SpawnKillTelegraph();
	void DestroyKillTelegraph();

	void HandleLifetimeExpired();
	void HandleExplosionLingerFinished();

	//! \brief 폭발 순간 KillRadius 내 생존 플레이어를 즉사시킨다(스냅샷).
	void ApplyExplosionKill();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayExplosionCosmetic();

	//! \brief 흡입 판정/시각 반경(cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PullRadius = 600.0f;

	//! \brief 폭발 시 즉사 반경(cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float KillRadius = 300.0f;

	//! \brief 스폰 후 폭발까지의 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float Lifetime = 5.0f;

	//! \brief 끌어당기는 RadialForce 세기. 플레이어 이동속도보다 약하게 두면 반대로 달려 탈출 가능하다. BP에서 튜닝.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PullStrength = 300.0f;

	//! \brief 즉사 반경 텔레그래프 액터 클래스(다른 패턴처럼 에디터에서 등록). 데칼 반경은 KillRadius에 자동으로 맞춰진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ACPP_BossTelegraphActor> KillTelegraphActorClass;

	//! \brief 폭발 코스메틱이 클라이언트에 전달되도록 잠깐 유지한 뒤 소멸하기까지의 시간(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ExplosionLingerTime = 0.2f;

	//! \brief 블랙홀 즉사 피해와 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|BlackHole", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float CurseGaugeAmount = 0.0f;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> KillGameplayEffect;

	float KillDamage = 0.0f;

	UPROPERTY()
	TObjectPtr<ACPP_BossTelegraphActor> KillTelegraph;

	FTimerHandle LifetimeTimerHandle;
	FTimerHandle ExplosionLingerTimerHandle;

	//! \brief 현재 끌어당기고 있는 캐릭터별 RadialForce 루트모션 소스 ID. add-once / remove-precise 용도.
	TMap<TWeakObjectPtr<ACharacter>, uint16> PullSourceIdsByCharacter;
};
