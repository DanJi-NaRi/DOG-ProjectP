// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_EnemyAreaBase.h"

#include "Enemy/Abilities/CPP_EnemyAttackShapeLibrary.h"
#include "Enemy/Actors/CPP_EnemyTelegraphActor.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

ACPP_EnemyAreaBase::ACPP_EnemyAreaBase()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

////////////////////////////
//! \author HanSeul
//! \brief Initializes the fixed enemy area attack and starts its warning timer.
//! \param InSourceASC 설치자(적)의 ASC. 피해 GameplayEffect의 Source가 된다.
//! \param InHitGameplayEffect Instant damage GameplayEffect applied to player targets.
//! \param InDamageType Single or periodic area damage behavior.
//! \param InAreaRadius Circular area radius in centimeters.
//! \param InAreaHalfHeight Vertical half-height used by the shared circle hit-shape query.
//! \param InWarningDuration Delay before the area becomes active.
//! \param InActiveDuration Periodic area lifetime after activation.
//! \param InDamageInterval Interval between periodic damage applications.
//! \param InDamageCoefficient 스킬 피해 계수. 공격력 곱셈은 ExecutionCalculation이 담당한다.
//! \param InTelegraphActorClass Shared filling telegraph actor class used during the warning phase.
//! \param InImpactCueTag Optional GameplayCue executed at the fixed area location whenever damage is evaluated.
//! \return None
void ACPP_EnemyAreaBase::InitializeArea(
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InHitGameplayEffect,
	EEnemyAreaDamageType InDamageType,
	float InAreaRadius,
	float InAreaHalfHeight,
	float InWarningDuration,
	float InActiveDuration,
	float InDamageInterval,
	float InDamageCoefficient,
	TSubclassOf<ACPP_EnemyTelegraphActor> InTelegraphActorClass,
	FGameplayTag InImpactCueTag
)
{
	SourceASC = InSourceASC;
	HitGameplayEffect = InHitGameplayEffect;
	DamageType = InDamageType;
	AreaRadius = FMath::Max(InAreaRadius, 0.0f);
	AreaHalfHeight = FMath::Max(InAreaHalfHeight, 0.0f);
	WarningDuration = FMath::Max(InWarningDuration, 0.0f);
	ActiveDuration = FMath::Max(InActiveDuration, 0.0f);
	DamageInterval = FMath::Max(InDamageInterval, 0.01f);
	DamageCoefficient = FMath::Max(InDamageCoefficient, 0.0f);
	TelegraphActorClass = InTelegraphActorClass;
	ImpactCueTag = InImpactCueTag;

	if (!HasAuthority())
	{
		return;
	}

	SpawnWarningTelegraph();

	if (WarningDuration <= 0.0f)
	{
		ActivateArea();
		return;
	}

	GetWorldTimerManager().SetTimer(WarningTimerHandle, this, &ACPP_EnemyAreaBase::ActivateArea, WarningDuration, false);
}

void ACPP_EnemyAreaBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(WarningTimerHandle);
	GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	GetWorldTimerManager().ClearTimer(ActiveDurationTimerHandle);
	DestroyWarningTelegraph();

	Super::EndPlay(EndPlayReason);
}

float ACPP_EnemyAreaBase::GetWarningServerStartTime() const
{
	return WarningServerStartTime;
}

////////////////////////////
//! \author HanSeul
//! \brief Activates the area after its warning time and starts single or periodic damage.
//! \return None
void ACPP_EnemyAreaBase::ActivateArea()
{
	if (!HasAuthority())
	{
		return;
	}

	DestroyWarningTelegraph();

	ApplyAreaDamage();

	if (DamageType == EEnemyAreaDamageType::Single || ActiveDuration <= 0.0f)
	{
		Destroy();
		return;
	}

	if (DamageType == EEnemyAreaDamageType::Periodic)
	{
		GetWorldTimerManager().SetTimer(DamageTimerHandle, this, &ACPP_EnemyAreaBase::ApplyAreaDamage, DamageInterval, true);
	}

	GetWorldTimerManager().SetTimer(ActiveDurationTimerHandle, this, &ACPP_EnemyAreaBase::HandleActiveDurationFinished, ActiveDuration, false);
}

////////////////////////////
//! \author HanSeul
//! \brief Finds player characters currently inside the area and applies stored damage.
//! \return None
void ACPP_EnemyAreaBase::ApplyAreaDamage()
{
	if (!HasAuthority() || !SourceASC.IsValid() || !HitGameplayEffect || DamageCoefficient <= 0.0f)
	{
		return;
	}

	ExecuteImpactCue();

	TArray<AActor*> Targets;
	CollectPlayerTargets(Targets);
	for (AActor* TargetActor : Targets)
	{
		ApplyDamageToTarget(TargetActor);
	}
}

void ACPP_EnemyAreaBase::HandleActiveDurationFinished()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
		Destroy();
	}
}

void ACPP_EnemyAreaBase::CollectPlayerTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	// Faction 판정에는 설치자(Instigator)를 사용한다.
	AActor* SourceAvatar = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	FBossHitShapeData HitShape;
	HitShape.Shape = EBossAttackShape::Circle;
	HitShape.InnerRadius = 0.0f;
	HitShape.OuterRadius = AreaRadius;
	HitShape.HalfHeight = AreaHalfHeight;

	TSet<AActor*> ShapeTargets;
	UCPP_EnemyAttackShapeLibrary::CollectTargetsFromHitShape(
		this,
		SourceAvatar,
		GetActorTransform(),
		HitShape,
		ShapeTargets
	);

	TSet<AActor*> UniqueTargets;
	for (AActor* TargetActor : ShapeTargets)
	{
		if (!TargetActor || UniqueTargets.Contains(TargetActor))
		{
			continue;
		}

		if (!UMyAbilitySystemLibrary::IsHostile(SourceAvatar, TargetActor))
		{
			continue;
		}

		UniqueTargets.Add(TargetActor);
		OutTargets.Add(TargetActor);
	}
}

bool ACPP_EnemyAreaBase::ApplyDamageToTarget(AActor* TargetActor)
{
	if (!SourceASC.IsValid() || !TargetActor || !HitGameplayEffect || DamageCoefficient <= 0.0f)
	{
		return false;
	}

	return UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
		SourceASC.Get(),
		TargetActor,
		HitGameplayEffect,
		DamageCoefficient
	);
}

////////////////////////////
//! \author HanSeul
//! \brief 고정된 Area 중심에서 선택적인 순간 GameplayCue를 실행한다.
//! \return None
void ACPP_EnemyAreaBase::ExecuteImpactCue() const
{
	if (!HasAuthority() || !SourceASC.IsValid() || !ImpactCueTag.IsValid())
	{
		return;
	}

	AActor* SourceAvatar = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	FGameplayCueParameters CueParameters;
	CueParameters.Location = GetActorLocation();
	CueParameters.Instigator = SourceAvatar;
	CueParameters.EffectCauser = SourceAvatar;
	CueParameters.RawMagnitude = AreaRadius;
	SourceASC->ExecuteGameplayCue(ImpactCueTag, CueParameters);
}

////////////////////////////
//! \author HanSeul
//! \brief 경고 단계에 공용 원형 텔레그래프를 생성하고 서버 판정 중심을 텔레그래프가 찾은 지면 위치에 맞춘다.
//! \return None
void ACPP_EnemyAreaBase::SpawnWarningTelegraph()
{
	if (!HasAuthority() || ActiveTelegraphActor)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSubclassOf<ACPP_EnemyTelegraphActor> SpawnClass = TelegraphActorClass;
	if (!SpawnClass)
	{
		SpawnClass = ACPP_EnemyTelegraphActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveTelegraphActor = World->SpawnActor<ACPP_EnemyTelegraphActor>(
		SpawnClass,
		GetActorTransform(),
		SpawnParameters
	);
	if (!ActiveTelegraphActor)
	{
		return;
	}

	FBossHitShapeData HitShape;
	HitShape.Shape = EBossAttackShape::Circle;
	HitShape.InnerRadius = 0.0f;
	HitShape.OuterRadius = AreaRadius;
	HitShape.HalfHeight = AreaHalfHeight;
	ActiveTelegraphActor->Initialize(GetActorTransform(), HitShape, WarningDuration);
	WarningServerStartTime = ActiveTelegraphActor->GetServerStartTime();

	// Initialize가 서버에서 찾은 지면 중심을 실제 Area 판정 중심으로도 사용한다.
	SetActorLocation(ActiveTelegraphActor->GetActorLocation());
}

////////////////////////////
//! \author HanSeul
//! \brief 활성 경고 텔레그래프를 제거한다.
//! \return None
void ACPP_EnemyAreaBase::DestroyWarningTelegraph()
{
	if (IsValid(ActiveTelegraphActor))
	{
		ActiveTelegraphActor->Destroy();
	}

	ActiveTelegraphActor = nullptr;
}
