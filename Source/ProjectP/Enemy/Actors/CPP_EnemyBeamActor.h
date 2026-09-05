// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_EnemyBeamActor.generated.h"

class ACPP_EnemyBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;

//! \brief 눈빔 페이즈. Aim=타겟 자유 추적(무피해 조준선), Lock=방향 고정(발사 직전 정지 연출),
//!        Fire=최대 각속도로 추적하며 비관통 빔 발사(대상당 1회 피해).
UENUM(BlueprintType)
enum class EEnemyBeamPhase : uint8
{
	Aim UMETA(DisplayName = "Aim"),
	Lock UMETA(DisplayName = "Lock"),
	Fire UMETA(DisplayName = "Fire")
};

////////////////////////////
//! \class ACPP_EnemyBeamActor
//! \brief 시전자 눈 위치에서 나가는 비관통 추적 빔. 서버가 매 틱 회전(페이즈별)·비관통 트레이스(벽/적대 폰에서 잘림,
//!        아군 폰 관통)·대상당 1회 피해를 담당하고, 원점/각도/길이/페이즈를 복제해 각 머신이 비주얼을 로컬 재구성한다.
//! \note  회전이 타겟 위치에 의존(비결정론적)해 보스 십자레이저의 서버시간 방식은 못 쓴다. 대신 상태를 직접 복제한다.
UCLASS()
class PROJECTP_API ACPP_EnemyBeamActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_EnemyBeamActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//! \brief 서버 전용 초기화. 시전자/데미지 소스와 빔 치수·각속도·피해 계수를 저장한다.
	void Initialize(
		ACPP_EnemyBase* InCaster,
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InHitGameplayEffect,
		TSubclassOf<UGameplayEffect> InStatusGameplayEffect,
		float InRange,
		float InHalfWidth,
		float InHalfHeight,
		float InOriginHeight,
		float InDamageCoefficient,
		bool bInDrawDebug
	);

	//! \brief 페이즈 전환(어빌리티가 타이머 경계에서 호출). Fire 진입 시 피해 중복 방지 셋을 초기화한다.
	void SetBeamPhase(EEnemyBeamPhase NewPhase);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Beam")
	TObjectPtr<USceneComponent> SceneRoot;

	//! \brief 원점(눈)에 놓이고 현재 빔 각도로 회전하는 피벗. BP 비주얼(메시/나이아가라)을 +X 전방으로 이 아래에 붙인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Beam")
	TObjectPtr<USceneComponent> BeamPivot;

	//! \brief 비주얼 갱신 훅. Length(현재 잘린 길이)/Phase를 받아 BP에서 메시 스케일·나이아가라 길이를 조정한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Beam")
	void OnBeamVisualUpdated(float Length, EEnemyBeamPhase Phase);

	//! \brief 페이즈 변경 훅. 조준선↔빔 머티리얼/이펙트 전환용.
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Beam")
	void OnBeamPhaseChanged(EEnemyBeamPhase Phase);

private:
	void ServerUpdateBeam(float DeltaSeconds);
	void TraceBeam(float& OutLength, AActor*& OutHostilePawnAtStop) const;
	float TraceWallDistanceUpTo(float MaxDistance) const;
	void ApplyBeamVisual();
	void ApplyDamageToActor(AActor* TargetActor);

	UFUNCTION()
	void OnRep_BeamVisual();

	UFUNCTION()
	void OnRep_BeamPhase();

	TWeakObjectPtr<ACPP_EnemyBase> Caster;
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	UPROPERTY()
	TSubclassOf<UGameplayEffect> HitGameplayEffect;
	UPROPERTY()
	TSubclassOf<UGameplayEffect> StatusGameplayEffect;

	float Range = 800.0f;
	float HalfWidth = 10.0f;
	float HalfHeight = 50.0f;
	float OriginHeight = 60.0f;
	float DamageCoefficient = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamVisual)
	FVector_NetQuantize RepOrigin = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_BeamVisual)
	float RepYaw = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamVisual)
	float RepLength = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_BeamPhase)
	EEnemyBeamPhase RepPhase = EEnemyBeamPhase::Aim;

	UPROPERTY(Replicated)
	bool bDrawDebug = false;

	float CurrentYaw = 0.0f;
	bool bYawInitialized = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;
};
