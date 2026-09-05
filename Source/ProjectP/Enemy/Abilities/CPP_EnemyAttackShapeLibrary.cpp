#include "CPP_EnemyAttackShapeLibrary.h"

#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

////////////////////////////
//! \author HanSeul
//! \brief 주어진 SourceTransform을 기준으로 공격 도형 안에 들어온 Pawn을 수집한다.
//! \param WorldContextObject 판정을 실행할 월드를 제공하는 객체.
//! \param SourceActor 판정에서 제외할 공격 주체.
//! \param SourceTransform 공격 도형의 기준 위치와 회전.
//! \param HitShape 텔레그래프와 피해 판정이 함께 사용하는 도형 데이터.
//! \param OutTargets 판정 안에서 발견한 액터 집합.
//! \return None
void UCPP_EnemyAttackShapeLibrary::CollectTargetsFromHitShape(
	const UObject* WorldContextObject,
	const AActor* SourceActor,
	const FTransform& SourceTransform,
	const FBossHitShapeData& HitShape,
	TSet<AActor*>& OutTargets
)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	switch (HitShape.Shape)
	{
	case EBossAttackShape::Circle:
		CollectTargetsFromCircle(World, SourceActor, SourceTransform, HitShape, OutTargets);
		break;
	case EBossAttackShape::Sector:
		CollectTargetsFromSector(World, SourceActor, SourceTransform, HitShape, OutTargets);
		break;
	case EBossAttackShape::Rectangle:
		CollectTargetsFromRectangle(World, SourceActor, SourceTransform, HitShape, OutTargets);
		break;
	default:
		break;
	}
}

void UCPP_EnemyAttackShapeLibrary::CollectTargetsFromCircle(
	UWorld* World,
	const AActor* SourceActor,
	const FTransform& SourceTransform,
	const FBossHitShapeData& HitShape,
	TSet<AActor*>& OutTargets
)
{
	if (!World || HitShape.OuterRadius <= 0.0f)
	{
		return;
	}

	const FVector ShapeOrigin = SourceTransform.GetLocation() + SourceTransform.GetRotation().RotateVector(HitShape.LocalOffset);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackCircle), false);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const float HorizontalDistanceSquared = FVector(ToTarget.X, ToTarget.Y, 0.0f).SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;
		if (bInRadius && bInHeight)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

void UCPP_EnemyAttackShapeLibrary::CollectTargetsFromSector(
	UWorld* World,
	const AActor* SourceActor,
	const FTransform& SourceTransform,
	const FBossHitShapeData& HitShape,
	TSet<AActor*>& OutTargets
)
{
	if (!World || HitShape.OuterRadius <= 0.0f || HitShape.SectorAngleDegrees <= 0.0f)
	{
		return;
	}

	const FVector ShapeOrigin = SourceTransform.GetLocation() + SourceTransform.GetRotation().RotateVector(HitShape.LocalOffset);
	const FRotator SourceRotation = SourceTransform.GetRotation().Rotator();
	const FRotator ShapeRotation(0.0f, SourceRotation.Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackSector), false);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HitShape.SectorAngleDegrees * 0.5f));
	const bool bFullCircle = HitShape.SectorAngleDegrees >= 360.0f;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const FVector HorizontalToTarget(ToTarget.X, ToTarget.Y, 0.0f);
		const float HorizontalDistanceSquared = HorizontalToTarget.SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;
		const bool bInAngle = bFullCircle
			|| (HorizontalDistanceSquared > KINDA_SMALL_NUMBER
				&& FVector::DotProduct(ShapeForward, HorizontalToTarget.GetSafeNormal()) >= MinDot);
		if (bInRadius && bInHeight && bInAngle)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

void UCPP_EnemyAttackShapeLibrary::CollectTargetsFromRectangle(
	UWorld* World,
	const AActor* SourceActor,
	const FTransform& SourceTransform,
	const FBossHitShapeData& HitShape,
	TSet<AActor*>& OutTargets
)
{
	if (!World || HitShape.ForwardLength <= 0.0f || HitShape.HalfWidth <= 0.0f || HitShape.HalfHeight <= 0.0f)
	{
		return;
	}

	const FRotator SourceRotation = SourceTransform.GetRotation().Rotator();
	const FRotator ShapeRotation(0.0f, SourceRotation.Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();
	const FVector ShapeStart = SourceTransform.GetLocation() + SourceTransform.GetRotation().RotateVector(HitShape.LocalOffset);
	const FVector ShapeCenter = ShapeStart + ShapeForward * (HitShape.ForwardLength * 0.5f);
	const FVector BoxExtent(HitShape.ForwardLength * 0.5f, HitShape.HalfWidth, HitShape.HalfHeight);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackRectangle), false);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeCenter,
		ShapeRotation.Quaternion(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(BoxExtent),
		QueryParams
	);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (AActor* TargetActor = OverlapResult.GetActor())
		{
			OutTargets.Add(TargetActor);
		}
	}
}
