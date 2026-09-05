// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "CPP_EnemyAttackPatternData.generated.h"

class ACPP_EnemyBase;
class ACPP_EnemyLobProjectileVisual;
class ACPP_EnemyTelegraphActor;
class UAnimMontage;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EEnemyAreaTargetType : uint8
{
	Player UMETA(DisplayName = "Player"),
	Self UMETA(DisplayName = "Self")
};

UENUM(BlueprintType)
enum class EEnemyAreaDamageType : uint8
{
	Single UMETA(DisplayName = "Single"),
	Periodic UMETA(DisplayName = "Periodic")
};

UCLASS(BlueprintType)
class PROJECTP_API UCPP_EnemyAttackPatternData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common", meta = (ClampMin = "0.0"))
	float Range = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 0.44f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common", meta = (ClampMin = "0.0"))
	float CooldownDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	TSubclassOf<UGameplayEffect> HitGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	TSubclassOf<UGameplayEffect> StatusGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	bool bStaggerImmuneDuringAttack = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common|Condition")
	bool bUseHealthCondition = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common|Condition",
		meta = (ClampMin = "0.0", ClampMax = "100.0", EditCondition = "bUseHealthCondition", EditConditionHides))
	float HealthPercentAtOrBelow = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common|Condition",
		meta = (EditCondition = "bUseHealthCondition", EditConditionHides))
	bool bInterruptAttackOnHealthCondition = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common|Condition")
	bool bUseDistanceCondition = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common|Condition",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bUseDistanceCondition", EditConditionHides))
	float TargetDistanceAtOrAbove = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileRadius = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Dash", meta = (ClampMin = "0.0"))
	float DashSpeed = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Dash", meta = (ClampMin = "0.0"))
	float DashCollisionRadius = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Dash", meta = (ClampMin = "0.0"))
	float DashCollisionHalfHeight = 100.0f;

	// 벽에 막혀 정지했을 때 이 시간(초)만큼 찌른 포즈로 굳는다(기절). 0이면 즉시 후딜 재생.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Dash", meta = (ClampMin = "0.0"))
	float DashWallStunDuration = 0.5f;

	// 플레이어를 맞혔을 때 적이 돌진 반대 방향으로 밀려나는 거리(cm). 0이면 반동 없음(기본).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Dash", meta = (ClampMin = "0.0"))
	float DashSelfKnockback = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area")
	EEnemyAreaTargetType AreaTargetType = EEnemyAreaTargetType::Player;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area")
	EEnemyAreaDamageType AreaDamageType = EEnemyAreaDamageType::Single;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (ClampMin = "0.0"))
	float AreaRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (ClampMin = "0.0", Units = "cm"))
	float AreaHalfHeight = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (ClampMin = "0.0"))
	float AreaWarningDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (ClampMin = "0.0"))
	float AreaActiveDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (ClampMin = "0.01"))
	float AreaDamageInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area", meta = (Categories = "GameplayCue"))
	FGameplayTag AreaImpactCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area|Lob")
	TSubclassOf<ACPP_EnemyLobProjectileVisual> LobProjectileVisualClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area|Lob")
	FName LobLaunchSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Area|Lob", meta = (ClampMin = "0.0", Units = "cm"))
	float LobPeakHeight = 400.0f;

	// Shape(모양 커스텀) 패턴 전용. 몽타주의 EnemyAttackWindow/EnemyTelegraph 노티파이 WindowId와 매칭해
	// 원/부채꼴/사각형 판정을 실행한다. 피해 계수는 윈도우별 DamageCoefficient를 사용한다(공용 DamageCoefficient
	// 미사용, 다단히트 대응). CurseGaugeAmount는 보스 전용 필드라 무시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Shape")
	TArray<FBossAttackWindowData> AttackWindows;

	// 텔레그래프 액터(Shape 패턴 + 돌진 패턴 겸용). 돌진은 Range×캡슐 치수의 직선 사각으로 자동 표시된다.
	// 비우면 기본 ACPP_EnemyTelegraphActor(머티리얼 없음 = 안 보임)로 스폰되므로 BP 자식을 지정할 것.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Shape")
	TSubclassOf<ACPP_EnemyTelegraphActor> TelegraphActorClass;

	// Summon(소환) 패턴 전용. true면 소환 패턴으로 취급되어 소환 게이트(동시 생존 소환몹 수 제한)가 적용된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Summon")
	bool bIsSummonPattern = false;

	// 소환할 적 클래스 목록. 소환 시 이 중 무작위로 선택한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Summon")
	TArray<TSubclassOf<ACPP_EnemyBase>> SummonEnemyClasses;

	// 생존 플레이어 1명당 소환 수. 게이트 배수로도 사용(생존 소환몹 ≥ 생존 플레이어 × 이 값이면 소환 안 함).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Summon", meta = (ClampMin = "0"))
	int32 SummonsPerPlayer = 5;

	// 각 플레이어 주변 소환 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Summon", meta = (ClampMin = "0.0"))
	float SummonRadius = 400.0f;

	// 소환몹 공격력 배수(기존 공격력 대비).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Summon", meta = (ClampMin = "0.0"))
	float SummonAttackPowerScale = 0.8f;

	// Shape 타격 대상 수 제한. 0이면 범위 내 전원, 1 이상이면 최근접 순으로 N명만 타격(단일 대상 스킬용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Shape", meta = (ClampMin = "0"))
	int32 MaxShapeTargets = 0;

	// true면 어빌리티 스폰 파생 적(분신·소환몹)은 이 패턴을 선택하지 않는다(무한 분열/소환 방지). 분열·소환 패턴에 체크.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack|Common")
	bool bExcludeForSpawnedMinion = false;
};
