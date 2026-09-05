#include "CPP_AbilityTask_BossDash.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

UCPP_AbilityTask_BossDash::UCPP_AbilityTask_BossDash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UCPP_AbilityTask_BossDash* UCPP_AbilityTask_BossDash::BossDash(
	UGameplayAbility* OwningAbility,
	ACharacter* InDashCharacter,
	FVector InDashDirection,
	float InDashDistance,
	float InDashSpeed,
	float InCapsuleRadius,
	float InCapsuleHalfHeight
)
{
	UCPP_AbilityTask_BossDash* Task = NewAbilityTask<UCPP_AbilityTask_BossDash>(OwningAbility);
	Task->DashCharacter = InDashCharacter;
	Task->DashDirection = InDashDirection.GetSafeNormal();
	Task->RemainingDistance = FMath::Max(InDashDistance, 0.0f);
	Task->DashSpeed = FMath::Max(InDashSpeed, 0.0f);
	Task->CapsuleRadius = FMath::Max(InCapsuleRadius, 1.0f);
	Task->CapsuleHalfHeight = FMath::Max(InCapsuleHalfHeight, 1.0f);
	return Task;
}

void UCPP_AbilityTask_BossDash::Activate()
{
	ACharacter* Character = DashCharacter.Get();
	if (!Character || DashDirection.IsNearlyZero() || RemainingDistance <= 0.0f || DashSpeed <= 0.0f)
	{
		FinishDash(TArray<AActor*>());
		return;
	}

	// Disable character movement so the per-tick SetActorLocation is authoritative (no CMC velocity/floor fighting).
	if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
	{
		CharacterMovement->StopMovementImmediately();
		CharacterMovement->SetMovementMode(MOVE_None);
		bMovementModeOverridden = true;
	}
}

void UCPP_AbilityTask_BossDash::TickTask(float DeltaTime)
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossDashSweep), false, Character);
	QueryParams.AddIgnoredActor(Character);

	const FCollisionShape DashCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectQueryParams, DashCapsule, QueryParams);

	// The object-type sweep also reports overlap-only volumes (telegraphs, laser arms, warning areas,
	// gimmick hazards) — those must not stop the dash. A blocker is a pawn, or geometry that would
	// physically block a pawn (walls).
	TArray<FHitResult> BlockingHits;
	BlockingHits.Reserve(Hits.Num());
	for (const FHitResult& Hit : Hits)
	{
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!HitComponent)
		{
			continue;
		}

		if (Cast<APawn>(Hit.GetActor()) || HitComponent->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
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

	// Non-penetrating: stop at the closest blocker (wall or pawn) along this step.
	float ClosestTime = 1.0f;
	for (const FHitResult& Hit : BlockingHits)
	{
		ClosestTime = FMath::Min(ClosestTime, Hit.Time);
	}

	const FVector StopLocation = Start + DashDirection * (Step * ClosestTime);
	Character->SetActorLocation(StopLocation, false);

	// Damage only pawns encountered up to the stop point (a wide capsule may catch several side by side).
	TArray<AActor*> HitPawns;
	for (const FHitResult& Hit : BlockingHits)
	{
		if (Hit.Time <= ClosestTime + KINDA_SMALL_NUMBER)
		{
			AActor* HitActor = Hit.GetActor();
			if (Cast<APawn>(HitActor) && !HitPawns.Contains(HitActor))
			{
				HitPawns.Add(HitActor);
			}
		}
	}

	FinishDash(HitPawns);
}

void UCPP_AbilityTask_BossDash::FinishDash(const TArray<AActor*>& HitPawns)
{
	if (bDashFinished)
	{
		return;
	}

	bDashFinished = true;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnDashFinished.Broadcast(HitPawns);
	}

	EndTask();
}

void UCPP_AbilityTask_BossDash::OnDestroy(bool bInOwnerFinished)
{
	if (bMovementModeOverridden)
	{
		if (ACharacter* Character = DashCharacter.Get())
		{
			if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
			{
				CharacterMovement->SetMovementMode(MOVE_Walking);
			}
		}
		bMovementModeOverridden = false;
	}

	Super::OnDestroy(bInOwnerFinished);
}
