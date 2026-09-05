#include "CPP_BossTelegraphActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

ACPP_BossTelegraphActor::ACPP_BossTelegraphActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// Tick is only enabled while a fill animation is running (see ApplyTelegraph); static telegraphs never tick.
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	TelegraphDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("TelegraphDecalComponent"));
	TelegraphDecalComponent->SetupAttachment(SceneRoot);
	TelegraphDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	TelegraphDecalComponent->DecalSize = FVector(DecalProjectionDepth, 100.0f, 100.0f);
	TelegraphDecalComponent->SetVisibility(false);
	TelegraphDecalComponent->SetHiddenInGame(true);
}

void ACPP_BossTelegraphActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BossTelegraphActor, SpawnData);
}

////////////////////////////
//! \author HanSeul
//! \brief Advances the local fill timer and pushes the resulting progress into the decal material. Runs on every machine.
//! \param DeltaSeconds Frame delta time.
void ACPP_BossTelegraphActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime = FMath::Max(GetSynchronizedServerTime() - SpawnData.ServerStartTime, 0.0f);
	UpdateFillProgress();

	// The fill is done — nothing further changes, so stop ticking entirely.
	if (ElapsedTime >= SpawnData.Duration)
	{
		SetActorTickEnabled(false);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Computes normalized fill progress (0..1) from the local elapsed time and applies it to the decal material.
void ACPP_BossTelegraphActor::UpdateFillProgress()
{
	if (!TelegraphMaterialInstance)
	{
		return;
	}

	const float Progress = SpawnData.Duration > 0.0f
		? FMath::Clamp(ElapsedTime / SpawnData.Duration, 0.0f, 1.0f)
		: 1.0f;

	TelegraphMaterialInstance->SetScalarParameterValue(TEXT("FillProgress"), Progress);
}

////////////////////////////
//! \author HanSeul
//! \brief 멀티플레이에서 동일한 경고 진행률을 계산할 수 있도록 동기화된 서버 월드 시각을 반환한다.
//! \return GameState 서버 월드 시각. GameState가 없으면 현재 월드 시각.
float ACPP_BossTelegraphActor::GetSynchronizedServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

////////////////////////////
//! \author HanSeul
//! \brief Authority-side entry point. Stores the replicated spawn data and applies the telegraph on the server;
//!        clients rebuild the same visual locally via OnRep_SpawnData.
//! \param SourceTransform Boss or warning actor transform captured when the telegraph begins.
//! \param InHitShape Shape data used to size and orient the telegraph decal.
//! \param InDuration Telegraph window length in seconds used to drive the fill from 0 to 1.
void ACPP_BossTelegraphActor::Initialize(const FTransform& SourceTransform, const FBossHitShapeData& InHitShape, float InDuration)
{
	SpawnData.SourceTransform = SourceTransform;
	SpawnData.HitShape = InHitShape;
	SpawnData.Duration = FMath::Max(InDuration, 0.0f);
	SpawnData.ServerStartTime = GetSynchronizedServerTime();

	ApplyTelegraph();
	ForceNetUpdate();
}

float ACPP_BossTelegraphActor::GetServerStartTime() const
{
	return SpawnData.ServerStartTime;
}

void ACPP_BossTelegraphActor::OnRep_SpawnData()
{
	ApplyTelegraph();
}

////////////////////////////
//! \author HanSeul
//! \brief Fixes this telegraph to the traced ground position and updates its decal visual. Runs on every machine.
void ACPP_BossTelegraphActor::ApplyTelegraph()
{
	// 복제를 늦게 받은 클라이언트도 서버의 현재 경고 진행률부터 표시한다.
	// Tick stays off unless this apply ends with a running fill (re-enabled at the bottom of UpdateDecalVisual).
	ElapsedTime = FMath::Max(GetSynchronizedServerTime() - SpawnData.ServerStartTime, 0.0f);
	SetActorTickEnabled(false);

	const FBossHitShapeData& HitShape = SpawnData.HitShape;

	// Location + rotation only (no scale) so the telegraph matches the scale-independent damage query.
	const FVector ShapeOrigin = SpawnData.SourceTransform.GetLocation() + SpawnData.SourceTransform.GetRotation().RotateVector(HitShape.LocalOffset);
	const FRotator SourceRotation = SpawnData.SourceTransform.GetRotation().Rotator();
	const FRotator ShapeRotation(0.0f, SourceRotation.Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector DecalCenter = GetDecalCenterLocation(ShapeOrigin, ShapeRotation);

	FVector GroundLocation = DecalCenter;
	if (!FindGroundLocation(DecalCenter, GroundLocation))
	{
		SetTelegraphVisible(false);
		return;
	}

	GroundLocation.Z += DecalSurfaceOffset;
	SetActorLocationAndRotation(GroundLocation, ShapeRotation);
	UpdateDecalVisual();
}

////////////////////////////
//! \author HanSeul
//! \brief Traces down on the configured boss ground channel to find the decal surface.
//! \param ShapeCenter World-space center point used as the vertical trace origin.
//! \param OutGroundLocation World-space impact point found by the trace.
//! \return true when a blocking ground hit is found.
bool ACPP_BossTelegraphActor::FindGroundLocation(const FVector& ShapeCenter, FVector& OutGroundLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector TraceStart = ShapeCenter + FVector(0.0f, 0.0f, TraceStartHeight);
	const FVector TraceEnd = ShapeCenter - FVector(0.0f, 0.0f, TraceEndDepth);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossTelegraphGroundTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		GroundTraceChannel.GetValue(),
		QueryParams
	);

	if (!bHit || !HitResult.bBlockingHit)
	{
		return false;
	}

	OutGroundLocation = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
	return true;
}

FVector ACPP_BossTelegraphActor::GetDecalCenterLocation(const FVector& ShapeOrigin, const FRotator& ShapeRotation) const
{
	const FBossHitShapeData& HitShape = SpawnData.HitShape;

	if (HitShape.Shape == EBossAttackShape::Rectangle)
	{
		return ShapeOrigin + ShapeRotation.Vector() * (HitShape.ForwardLength * 0.5f);
	}

	return ShapeOrigin;
}

////////////////////////////
//! \author HanSeul
//! \brief Applies decal size, material, and shape parameters for the current hit shape.
void ACPP_BossTelegraphActor::UpdateDecalVisual()
{
	if (!TelegraphDecalComponent)
	{
		return;
	}

	const FBossHitShapeData& HitShape = SpawnData.HitShape;

	switch (HitShape.Shape)
	{
	case EBossAttackShape::Circle:
	case EBossAttackShape::Sector:
		if (HitShape.OuterRadius <= 0.0f)
		{
			SetTelegraphVisible(false);
			return;
		}
		TelegraphDecalComponent->DecalSize = FVector(DecalProjectionDepth, HitShape.OuterRadius, HitShape.OuterRadius);
		break;
	case EBossAttackShape::Rectangle:
		if (HitShape.ForwardLength <= 0.0f || HitShape.HalfWidth <= 0.0f)
		{
			SetTelegraphVisible(false);
			return;
		}
		TelegraphDecalComponent->DecalSize = FVector(DecalProjectionDepth, HitShape.HalfWidth, HitShape.ForwardLength * 0.5f);
		break;
	default:
		SetTelegraphVisible(false);
		return;
	}

	TelegraphDecalComponent->SetRelativeLocation(FVector::ZeroVector);
	TelegraphDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	if (TelegraphMaterial)
	{
		UMaterialInstanceDynamic* DecalMaterialInstance = UMaterialInstanceDynamic::Create(TelegraphMaterial, this);
		if (DecalMaterialInstance)
		{
			ApplyDecalMaterialParameters(DecalMaterialInstance);
			TelegraphDecalComponent->SetDecalMaterial(DecalMaterialInstance);
			TelegraphMaterialInstance = DecalMaterialInstance;
			UpdateFillProgress();

			// Only animate while there is a fill to drive; Duration <= 0 renders as instantly full and never ticks.
			SetActorTickEnabled(SpawnData.Duration > 0.0f && ElapsedTime < SpawnData.Duration);
		}
	}

	SetTelegraphVisible(true);
}

void ACPP_BossTelegraphActor::ApplyDecalMaterialParameters(UMaterialInstanceDynamic* DecalMaterialInstance) const
{
	if (!DecalMaterialInstance)
	{
		return;
	}

	const FBossHitShapeData& HitShape = SpawnData.HitShape;

	DecalMaterialInstance->SetScalarParameterValue(TEXT("Shape"), static_cast<float>(HitShape.Shape));
	DecalMaterialInstance->SetScalarParameterValue(TEXT("InnerRadius"), HitShape.InnerRadius);
	DecalMaterialInstance->SetScalarParameterValue(TEXT("OuterRadius"), HitShape.OuterRadius);
	DecalMaterialInstance->SetScalarParameterValue(TEXT("SectorAngleDegrees"), HitShape.SectorAngleDegrees);
	DecalMaterialInstance->SetScalarParameterValue(TEXT("ForwardLength"), HitShape.ForwardLength);
	DecalMaterialInstance->SetScalarParameterValue(TEXT("HalfWidth"), HitShape.HalfWidth);
}

void ACPP_BossTelegraphActor::SetTelegraphVisible(bool bVisible) const
{
	if (!TelegraphDecalComponent)
	{
		return;
	}

	TelegraphDecalComponent->SetVisibility(bVisible);
	TelegraphDecalComponent->SetHiddenInGame(!bVisible);
}
