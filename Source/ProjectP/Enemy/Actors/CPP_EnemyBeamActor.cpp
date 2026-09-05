// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyBeamActor.h"

#include "Enemy/Core/CPP_EnemyBase.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

ACPP_EnemyBeamActor::ACPP_EnemyBeamActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(60.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BeamPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamPivot"));
	BeamPivot->SetupAttachment(SceneRoot);
}

void ACPP_EnemyBeamActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_EnemyBeamActor, RepOrigin);
	DOREPLIFETIME(ACPP_EnemyBeamActor, RepYaw);
	DOREPLIFETIME(ACPP_EnemyBeamActor, RepLength);
	DOREPLIFETIME(ACPP_EnemyBeamActor, RepPhase);
	DOREPLIFETIME(ACPP_EnemyBeamActor, bDrawDebug);
}

////////////////////////////
//! \author HanUl
//! \brief 서버 전용 초기화. 시전자/데미지 소스와 빔 치수·각속도·피해 계수를 저장한다.
//! \param InCaster 빔을 쏘는 적
//! \param InSourceASC 데미지 소스 ASC
//! \param InHitGameplayEffect 피격 데미지 GE
//! \param InStatusGameplayEffect 부가 상태이상 GE(선택)
//! \param InRange 빔 최대 길이(cm)
//! \param InHalfWidth 빔 폭 절반(cm)
//! \param InHalfHeight 빔 높이 절반(cm)
//! \param InOriginHeight 시전자 위치로부터 빔 원점의 Z 오프셋(눈 높이)
//! \param InDamageCoefficient 공격력 대비 피해 계수
//! \param bInDrawDebug 디버그 라인 표시 여부
//! \return
void ACPP_EnemyBeamActor::Initialize(
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
)
{
	Caster = InCaster;
	SourceASC = InSourceASC;
	HitGameplayEffect = InHitGameplayEffect;
	StatusGameplayEffect = InStatusGameplayEffect;
	Range = FMath::Max(InRange, 1.0f);
	HalfWidth = FMath::Max(InHalfWidth, 1.0f);
	HalfHeight = FMath::Max(InHalfHeight, 1.0f);
	OriginHeight = InOriginHeight;
	DamageCoefficient = InDamageCoefficient;
	bDrawDebug = bInDrawDebug;
}

void ACPP_EnemyBeamActor::SetBeamPhase(EEnemyBeamPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	RepPhase = NewPhase;
	if (NewPhase == EEnemyBeamPhase::Fire)
	{
		AlreadyHitActors.Reset();
	}

	OnBeamPhaseChanged(NewPhase);
	ForceNetUpdate();
}

void ACPP_EnemyBeamActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		ServerUpdateBeam(DeltaSeconds);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버 매 틱: 원점(눈)·각도(페이즈별)·비관통 길이를 갱신하고, Fire면 정지 지점의 적대 폰에 1회 피해를 준다.
//! \param DeltaSeconds 프레임 시간
//! \return
void ACPP_EnemyBeamActor::ServerUpdateBeam(float DeltaSeconds)
{
	ACPP_EnemyBase* CasterActor = Caster.Get();
	if (!IsValid(CasterActor) || CasterActor->IsDead())
	{
		// 시전자 소실/사망: 피해 없이 빔을 접는다(어빌리티가 곧 파괴).
		RepLength = 0.0f;
		ApplyBeamVisual();
		return;
	}

	const FVector Origin = CasterActor->GetActorLocation() + FVector(0.0f, 0.0f, OriginHeight);

	float DesiredYaw = CurrentYaw;
	if (const AActor* TargetActor = CasterActor->GetCurrentTargetActor())
	{
		const FVector ToTarget = TargetActor->GetActorLocation() - Origin;
		const FVector Horizontal(ToTarget.X, ToTarget.Y, 0.0f);
		if (!Horizontal.IsNearlyZero())
		{
			DesiredYaw = Horizontal.Rotation().Yaw;
		}
	}

	if (!bYawInitialized)
	{
		CurrentYaw = DesiredYaw;
		bYawInitialized = true;
	}

	switch (RepPhase)
	{
	case EEnemyBeamPhase::Aim:
		CurrentYaw = DesiredYaw; // 자유 추적(각속도 제한 없음)
		break;
	case EEnemyBeamPhase::Lock:
	case EEnemyBeamPhase::Fire:
		// 발사 시점에 방향 확정(가디언 레이저 방식): Lock에서 고정된 각도를 Fire 내내 유지 —
		// 추적하지 않으므로 플레이어가 예고된 직선 밖으로 이탈하면 회피된다. 원점(눈)만 시전자를 따라간다.
		break;
	default:
		break;
	}

	RepOrigin = Origin;
	RepYaw = CurrentYaw;

	if (RepPhase == EEnemyBeamPhase::Fire)
	{
		// 발사: 비관통 트레이스로 길이 결정(Range 상한, 벽/적대 폰에서 잘림) + 정지 지점 적대 폰 1회 피해.
		float BeamLength = Range;
		AActor* HostilePawnAtStop = nullptr;
		TraceBeam(BeamLength, HostilePawnAtStop);
		RepLength = BeamLength;

		if (HostilePawnAtStop && !AlreadyHitActors.Contains(HostilePawnAtStop))
		{
			AlreadyHitActors.Add(HostilePawnAtStop);
			ApplyDamageToActor(HostilePawnAtStop);
		}
	}
	else if (RepPhase == EEnemyBeamPhase::Aim)
	{
		// 조준: 기본은 타겟까지 잇되(Range 무관, 허공 절단/오버슈트 방지), 사이에 벽이 있으면 벽에서 자른다.
		const AActor* TargetActor = CasterActor->GetCurrentTargetActor();
		const float DistanceToTarget = TargetActor ? FVector::Dist2D(Origin, TargetActor->GetActorLocation()) : Range;
		RepLength = TraceWallDistanceUpTo(DistanceToTarget);
	}
	// Lock: RepLength를 그대로 유지 — 조준 마지막 프레임의 길이·각도를 스냅샷으로 고정(발사 예고 확정).

	ApplyBeamVisual();
}

////////////////////////////
//! \author HanUl
//! \brief 발사 페이즈 전용. 원점에서 현재 각도로 폭 있는 박스를 Range까지 sweep해 가장 가까운 차단물까지 길이를 자른다.
//!        벽(폰 채널 Block, Instigator/Owner 없는 지오메트리)과 적대 폰이 차단. 아군/중립 폰은 관통(무시).
//! \param OutLength 잘린 빔 길이
//! \param OutHostilePawnAtStop 정지 지점이 적대 폰이면 그 폰(피해 대상), 아니면 nullptr
//! \return
void ACPP_EnemyBeamActor::TraceBeam(float& OutLength, AActor*& OutHostilePawnAtStop) const
{
	OutLength = Range;
	OutHostilePawnAtStop = nullptr;

	ACPP_EnemyBase* CasterActor = Caster.Get();
	UWorld* World = GetWorld();
	if (!IsValid(CasterActor) || !World)
	{
		return;
	}

	const FVector Direction = FRotator(0.0f, RepYaw, 0.0f).Vector();
	const FVector Start = RepOrigin;
	const FVector End = Start + Direction * Range;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBeamTrace), false, CasterActor);
	QueryParams.AddIgnoredActor(CasterActor);

	const FCollisionShape BeamBox = FCollisionShape::MakeBox(FVector(1.0f, HalfWidth, HalfHeight));

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FRotator(0.0f, RepYaw, 0.0f).Quaternion(), ObjectQueryParams, BeamBox, QueryParams);

	float BestDistance = Range;
	AActor* BestActor = nullptr;
	bool bBestIsHostile = false;

	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		AActor* HitActor = Hit.GetActor();
		if (!HitComponent || !HitActor)
		{
			continue;
		}

		const float Distance = Hit.Distance;

		if (Cast<APawn>(HitActor))
		{
			// 적대 폰만 차단(피해 대상). 아군/중립 폰은 관통.
			if (UMyAbilitySystemLibrary::IsHostile(CasterActor, HitActor, /*bRequireAlive=*/false))
			{
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					BestActor = HitActor;
					bBestIsHostile = true;
				}
			}
			continue;
		}

		// 비폰: 폰을 물리적으로 막는 지오메트리만 벽으로 취급. Instigator/Owner 있는 전투 산물은 관통(돌진과 동일 규칙).
		if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
			if (HitActor->GetInstigator() || HitActor->GetOwner())
			{
				continue;
			}

			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestActor = HitActor;
				bBestIsHostile = false;
			}
		}
	}

	OutLength = BestDistance;
	if (bBestIsHostile)
	{
		OutHostilePawnAtStop = BestActor;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 조준선용. 원점에서 현재 각도로 MaxDistance까지 sweep해 가장 가까운 벽까지의 거리를 반환한다.
//!        벽(폰 채널 Block, Instigator/Owner 없는 지오메트리)만 본다 — 폰은 조준선을 자르지 않는다.
//! \param MaxDistance 조준선이 닿을 최대 거리(보통 타겟까지의 거리, Range 상한 없음)
//! \return 벽이 있으면 벽까지 거리, 없으면 MaxDistance
float ACPP_EnemyBeamActor::TraceWallDistanceUpTo(float MaxDistance) const
{
	UWorld* World = GetWorld();
	ACPP_EnemyBase* CasterActor = Caster.Get();
	if (!World || !IsValid(CasterActor) || MaxDistance <= 0.0f)
	{
		return FMath::Max(MaxDistance, 0.0f);
	}

	const FVector Direction = FRotator(0.0f, RepYaw, 0.0f).Vector();
	const FVector Start = RepOrigin;
	const FVector End = Start + Direction * MaxDistance;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBeamAimWallTrace), false, CasterActor);
	QueryParams.AddIgnoredActor(CasterActor);

	const FCollisionShape BeamBox = FCollisionShape::MakeBox(FVector(1.0f, HalfWidth, HalfHeight));

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FRotator(0.0f, RepYaw, 0.0f).Quaternion(), ObjectQueryParams, BeamBox, QueryParams);

	float ClosestWall = MaxDistance;
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		AActor* HitActor = Hit.GetActor();
		if (!HitComponent || !HitActor)
		{
			continue;
		}

		if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}

		// 전투 산물(투사체·FX 등 Instigator/Owner 보유)은 벽이 아니다 — 관통(돌진/발사와 동일 규칙).
		if (HitActor->GetInstigator() || HitActor->GetOwner())
		{
			continue;
		}

		ClosestWall = FMath::Min(ClosestWall, Hit.Distance);
	}

	return ClosestWall;
}

void ACPP_EnemyBeamActor::ApplyBeamVisual()
{
	if (BeamPivot)
	{
		BeamPivot->SetWorldLocationAndRotation(RepOrigin, FRotator(0.0f, RepYaw, 0.0f));
	}

	OnBeamVisualUpdated(RepLength, RepPhase);

	if (bDrawDebug)
	{
		if (UWorld* World = GetWorld())
		{
			const FVector Direction = FRotator(0.0f, RepYaw, 0.0f).Vector();
			const FColor DebugColor = (RepPhase == EEnemyBeamPhase::Fire)
				? FColor::Red
				: (RepPhase == EEnemyBeamPhase::Lock ? FColor::Orange : FColor::Yellow);
			const float Thickness = (RepPhase == EEnemyBeamPhase::Fire) ? 4.0f : 1.0f;
			DrawDebugLine(World, RepOrigin, RepOrigin + Direction * RepLength, DebugColor, false, -1.0f, 0, Thickness);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 정지 지점의 적대 폰에 피해 계수 데미지 GE와 부가 상태이상을 적용한다.
//! \param TargetActor 피해 대상
//! \return
void ACPP_EnemyBeamActor::ApplyDamageToActor(AActor* TargetActor)
{
	UAbilitySystemComponent* SourceAbilitySystem = SourceASC.Get();
	if (!SourceAbilitySystem || !HitGameplayEffect || !IsValid(TargetActor))
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
		SourceAbilitySystem,
		TargetActor,
		HitGameplayEffect,
		DamageCoefficient
	);

	if (StatusGameplayEffect)
	{
		if (UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor))
		{
			FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
			EffectContext.AddSourceObject(Caster.Get());

			const FGameplayEffectSpecHandle StatusSpecHandle = SourceAbilitySystem->MakeOutgoingSpec(StatusGameplayEffect, 1.0f, EffectContext);
			if (StatusSpecHandle.IsValid())
			{
				SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*StatusSpecHandle.Data.Get(), TargetASC);
			}
		}
	}
}

void ACPP_EnemyBeamActor::OnRep_BeamVisual()
{
	ApplyBeamVisual();
}

void ACPP_EnemyBeamActor::OnRep_BeamPhase()
{
	OnBeamPhaseChanged(RepPhase);
	ApplyBeamVisual();
}
