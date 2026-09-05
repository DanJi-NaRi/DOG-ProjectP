#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CPP_EnemyAttackShapeLibrary.generated.h"

class AActor;
struct FBossHitShapeData;

////////////////////////////
//! \class UCPP_EnemyAttackShapeLibrary
//! \brief 적 공격의 텔레그래프 데이터와 실제 피해 판정이 동일한 도형 계산을 사용하도록 제공하는 공용 라이브러리.
UCLASS()
class PROJECTP_API UCPP_EnemyAttackShapeLibrary : public UObject
{
	GENERATED_BODY()

public:
	static void CollectTargetsFromHitShape(
		const UObject* WorldContextObject,
		const AActor* SourceActor,
		const FTransform& SourceTransform,
		const FBossHitShapeData& HitShape,
		TSet<AActor*>& OutTargets
	);

private:
	static void CollectTargetsFromCircle(
		UWorld* World,
		const AActor* SourceActor,
		const FTransform& SourceTransform,
		const FBossHitShapeData& HitShape,
		TSet<AActor*>& OutTargets
	);

	static void CollectTargetsFromSector(
		UWorld* World,
		const AActor* SourceActor,
		const FTransform& SourceTransform,
		const FBossHitShapeData& HitShape,
		TSet<AActor*>& OutTargets
	);

	static void CollectTargetsFromRectangle(
		UWorld* World,
		const AActor* SourceActor,
		const FTransform& SourceTransform,
		const FBossHitShapeData& HitShape,
		TSet<AActor*>& OutTargets
	);
};
