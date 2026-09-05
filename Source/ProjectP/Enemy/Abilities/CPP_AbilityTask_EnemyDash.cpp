// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_AbilityTask_EnemyDash.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GAS/MyAbilitySystemLibrary.h"

UCPP_AbilityTask_EnemyDash* UCPP_AbilityTask_EnemyDash::EnemyDash(
	UGameplayAbility* OwningAbility,
	ACharacter* InDashCharacter,
	FVector InDashDirection,
	float InDashDistance,
	float InDashSpeed,
	float InCapsuleRadius,
	float InCapsuleHalfHeight
)
{
	UCPP_AbilityTask_EnemyDash* Task = NewAbilityTask<UCPP_AbilityTask_EnemyDash>(OwningAbility);
	Task->DashCharacter = InDashCharacter;
	Task->DashDirection = InDashDirection.GetSafeNormal();
	Task->RemainingDistance = FMath::Max(InDashDistance, 0.0f);
	Task->DashSpeed = FMath::Max(InDashSpeed, 0.0f);
	Task->CapsuleRadius = FMath::Max(InCapsuleRadius, 1.0f);
	Task->CapsuleHalfHeight = FMath::Max(InCapsuleHalfHeight, 1.0f);
	return Task;
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 시작 시 캡슐의 Pawn 물리 응답을 끈다 — 경로상의 폰(특히 공중에 뜬 아군)을 밀어 날리지 않게.
//!        정지 판정은 sweep이 전담하므로 벽/플레이어에 막히는 동작은 그대로다.
//! \param
//! \return
void UCPP_AbilityTask_EnemyDash::Activate()
{
	Super::Activate();

	// 부모 Activate가 파라미터 불량으로 즉시 종료했다면(OnDestroy 선행) 건드리지 않는다.
	if (!bDashFinished)
	{
		if (ACPP_EnemyBase* EnemyCharacter = Cast<ACPP_EnemyBase>(DashCharacter.Get()))
		{
			EnemyCharacter->SetPawnPhysicsIgnoredFromAbility(true);
		}
	}
}

void UCPP_AbilityTask_EnemyDash::OnDestroy(bool bInOwnerFinished)
{
	// 어떤 경로로 끝나도(완주/벽/어빌리티 강제 종료) Pawn 물리 응답을 복구한다.
	if (ACPP_EnemyBase* EnemyCharacter = Cast<ACPP_EnemyBase>(DashCharacter.Get()))
	{
		EnemyCharacter->SetPawnPhysicsIgnoredFromAbility(false);
	}

	Super::OnDestroy(bInOwnerFinished);
}

////////////////////////////
//! \author HanUl
//! \brief 보스 TickTask와 동일한 sweep 전진이되, 폰 분류만 다르다: 같은 진영(Faction 태그, IsFriendly) 폰은
//!        차단물에서 제외해 관통시키고, 벽(폰이 아닌 지오메트리) 정지 여부를 기록한다.
//! \param DeltaTime 프레임 시간
//! \return
void UCPP_AbilityTask_EnemyDash::TickTask(float DeltaTime)
{
	if (bDashFinished)
	{
		return;
	}

	ACharacter* Character = DashCharacter.Get();
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		FinishDash(TArray<AActor*>());
		return;
	}

	const float Step = FMath::Min(DashSpeed * DeltaTime, RemainingDistance);
	const FVector Start = Character->GetActorLocation();
	const FVector End = Start + DashDirection * Step;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyDashSweep), false, Character);
	QueryParams.AddIgnoredActor(Character);

	// 돌진 캡슐이 캐릭터 캡슐보다 크면 바닥에 파묻혀 첫 틱에 벽 정지가 난다. 캡슐은 반높이가 반경보다
	// 작아질 수 없으므로(작으면 구로 취급) 실효 반높이 = max(반높이, 반경). sweep 중심을 들어올려
	// 캡슐 바닥을 캐릭터 캡슐 바닥(+10cm 여유)에 정렬하면 어떤 데이터 치수든 지면과 겹치지 않는다.
	float SweepZOffset = 0.0f;
	const float EffectiveHalfHeight = FMath::Max(CapsuleHalfHeight, CapsuleRadius);
	if (const UCapsuleComponent* CharacterCapsule = Character->GetCapsuleComponent())
	{
		SweepZOffset = FMath::Max(0.0f, EffectiveHalfHeight - (CharacterCapsule->GetScaledCapsuleHalfHeight() - 10.0f));
	}
	const FVector SweepOffset(0.0f, 0.0f, SweepZOffset);

	const FCollisionShape DashCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start + SweepOffset, End + SweepOffset, FQuat::Identity, ObjectQueryParams, DashCapsule, QueryParams);

	// 오버랩 볼륨(경고 장판 등)은 통과. 폰은 같은 진영(Faction 태그)이면 관통 — 자기들끼리 엉켜 돌진이
	// 막히지 않게 한다. 관통 폰의 캡슐이 아래 '벽' 분기로 재분류되지 않도록 폰 판정은 여기서 끝낸다.
	TArray<FHitResult> BlockingHits;
	BlockingHits.Reserve(Hits.Num());
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!HitComponent)
		{
			continue;
		}

		if (Cast<APawn>(Hit.GetActor()))
		{
			if (!UMyAbilitySystemLibrary::IsFriendly(Character, Hit.GetActor(), /*bRequireAlive=*/false))
			{
				BlockingHits.Add(Hit);
			}
			continue;
		}

		if (HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
			// Instigator/Owner가 있는 액터는 전투 산물(투사체·타격 FX·스킬 액터)이지 지형이 아니다.
			// 벽으로 오분류되면 돌진이 끊기고 벽 기절까지 유발되므로 관통시킨다. (레벨 지오메트리는 둘 다 없다)
			const AActor* HitActor = Hit.GetActor();
			if (HitActor && (HitActor->GetInstigator() || HitActor->GetOwner()))
			{
				continue;
			}

			BlockingHits.Add(Hit);
		}
	}

	if (BlockingHits.Num() == 0)
	{
		Character->SetActorLocation(End, false);
		RemainingDistance -= Step;
		if (RemainingDistance <= KINDA_SMALL_NUMBER)
		{
			FinishDash(TArray<AActor*>());
		}
		return;
	}

	// 비관통: 이 스텝에서 가장 가까운 차단물(벽/적대 폰) 앞에 정지한다.
	float ClosestTime = 1.0f;
	for (const FHitResult& Hit : BlockingHits)
	{
		ClosestTime = FMath::Min(ClosestTime, Hit.Time);
	}

	const FVector StopLocation = Start + DashDirection * (Step * ClosestTime);
	Character->SetActorLocation(StopLocation, false);

	// 정지 지점까지 걸린 폰만 피해 대상으로 보고하고, 폰이 아닌 차단물이 있으면 벽 정지로 기록한다.
	TArray<AActor*> HitPawns;
	for (const FHitResult& Hit : BlockingHits)
	{
		if (Hit.Time <= ClosestTime + KINDA_SMALL_NUMBER)
		{
			AActor* HitActor = Hit.GetActor();
			if (Cast<APawn>(HitActor))
			{
				HitPawns.AddUnique(HitActor);
			}
			else
			{
				bStoppedByWall = true;
			}
		}
	}

	FinishDash(HitPawns);
}

////////////////////////////
//! \author HanUl
//! \brief 적 전용 종료 델리게이트(벽 정지 여부 포함)를 먼저 알리고, 가드/기본 델리게이트/EndTask 정리는 부모에 맡긴다.
//!        부모의 모든 종료 경로(Activate 실패 포함)가 이 오버라이드를 지나므로 통지 누락이 없다.
//! \param HitPawns 정지 지점에서 캡슐에 맞은 폰들
//! \return
void UCPP_AbilityTask_EnemyDash::FinishDash(const TArray<AActor*>& HitPawns)
{
	if (!bDashFinished && ShouldBroadcastAbilityTaskDelegates())
	{
		OnEnemyDashFinished.Broadcast(HitPawns, bStoppedByWall);
	}

	Super::FinishDash(HitPawns);
}
