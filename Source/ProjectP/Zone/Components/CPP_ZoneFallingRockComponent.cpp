#include "Zone/Components/CPP_ZoneFallingRockComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GAS/MyPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Zone/Hazards/CPP_ZoneFallingRock.h"
#include "Zone/ZoneBase.h"

UCPP_ZoneFallingRockComponent::UCPP_ZoneFallingRockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 첫 낙석 묶음을 즉시 생성하고 설정된 간격의 반복 타이머를 시작한다.
void UCPP_ZoneFallingRockComponent::StartFallingRocks()
{
	if (!HasZoneAuthority() || bFallingRockActive)
	{
		return;
	}

	if (!FallingRockClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: FallingRockClass is not configured."), *GetNameSafe(GetOwner()));
		return;
	}

	bFallingRockActive = true;
	SpawnRockBatch();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&UCPP_ZoneFallingRockComponent::SpawnRockBatch,
			FMath::Max(SpawnInterval, 0.1f),
			true);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 서버의 반복 생성 타이머를 중지하고 아직 남아 있는 낙석과 텔레그래프를 모두 제거한다.
void UCPP_ZoneFallingRockComponent::StopFallingRocks()
{
	if (!HasZoneAuthority())
	{
		return;
	}

	bFallingRockActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	DestroyActiveRocks();
}

void UCPP_ZoneFallingRockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFallingRocks();
	Super::EndPlay(EndPlayReason);
}

bool UCPP_ZoneFallingRockComponent::HasZoneAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

////////////////////////////
//! \author HanSeul
//! \brief 한 주기마다 각 생존 플레이어 주변에 N개, Zone 전체에 A개의 낙석을 생성한다.
void UCPP_ZoneFallingRockComponent::SpawnRockBatch()
{
	if (!bFallingRockActive || !HasZoneAuthority())
	{
		return;
	}

	ActiveRocks.RemoveAllSwap([](const TObjectPtr<ACPP_ZoneFallingRock>& Rock)
	{
		return !IsValid(Rock);
	});

	TArray<AActor*> LivingPlayers;
	GatherLivingPlayers(LivingPlayers);

	const int32 SafePlayerRockCount = FMath::Max(RockCountPerPlayer, 0);
	for (AActor* LivingPlayer : LivingPlayers)
	{
		for (int32 RockIndex = 0; RockIndex < SafePlayerRockCount; ++RockIndex)
		{
			FVector SpawnLocation;
			if (TryFindPlayerRockLocation(LivingPlayer, SpawnLocation))
			{
				SpawnRockAtLocation(SpawnLocation);
			}
		}
	}

	const int32 SafeGlobalRockCount = FMath::Max(GlobalRockCount, 0);
	for (int32 RockIndex = 0; RockIndex < SafeGlobalRockCount; ++RockIndex)
	{
		FVector SpawnLocation;
		if (TryFindGlobalRockLocation(SpawnLocation))
		{
			SpawnRockAtLocation(SpawnLocation);
		}
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 서버 GameState에서 현재 생존 상태이며 Pawn이 유효한 플레이어만 수집한다.
//! \param OutLivingPlayers 수집된 생존 플레이어 Pawn 배열
void UCPP_ZoneFallingRockComponent::GatherLivingPlayers(TArray<AActor*>& OutLivingPlayers) const
{
	OutLivingPlayers.Reset();

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		AActor* PlayerPawn = IsValid(MyPlayerState) ? MyPlayerState->GetPawn() : nullptr;
		if (IsValid(MyPlayerState) && MyPlayerState->IsAlive() && IsValid(PlayerPawn))
		{
			OutLivingPlayers.Add(PlayerPawn);
		}
	}
}

bool UCPP_ZoneFallingRockComponent::TryFindPlayerRockLocation(const AActor* PlayerActor, FVector& OutLocation) const
{
	if (!IsValid(PlayerActor))
	{
		return false;
	}

	const int32 SafeAttemptCount = FMath::Max(MaxSpawnAttempts, 1);
	for (int32 Attempt = 0; Attempt < SafeAttemptCount; ++Attempt)
	{
		const float RandomAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float RandomDistance = FMath::Sqrt(FMath::FRand()) * FMath::Max(PlayerSpawnRadius, 0.0f);
		const FVector Candidate = PlayerActor->GetActorLocation() + FVector(
			FMath::Cos(RandomAngle) * RandomDistance,
			FMath::Sin(RandomAngle) * RandomDistance,
			0.0f);

		if (TryProjectCandidateToZone(Candidate, OutLocation))
		{
			return true;
		}
	}

	return false;
}

bool UCPP_ZoneFallingRockComponent::TryFindGlobalRockLocation(FVector& OutLocation) const
{
	const AZoneBase* OwnerZone = Cast<AZoneBase>(GetOwner());
	const UBoxComponent* ZoneBoundary = OwnerZone ? OwnerZone->GetZoneBoundary() : nullptr;
	if (!ZoneBoundary)
	{
		return false;
	}

	const FVector BoundaryExtent = ZoneBoundary->GetUnscaledBoxExtent();
	const FTransform BoundaryTransform = ZoneBoundary->GetComponentTransform();
	const int32 SafeAttemptCount = FMath::Max(MaxSpawnAttempts, 1);

	for (int32 Attempt = 0; Attempt < SafeAttemptCount; ++Attempt)
	{
		const FVector LocalCandidate(
			FMath::FRandRange(-BoundaryExtent.X, BoundaryExtent.X),
			FMath::FRandRange(-BoundaryExtent.Y, BoundaryExtent.Y),
			0.0f);
		const FVector Candidate = BoundaryTransform.TransformPosition(LocalCandidate);

		if (TryProjectCandidateToZone(Candidate, OutLocation))
		{
			return true;
		}
	}

	return false;
}

bool UCPP_ZoneFallingRockComponent::TryProjectCandidateToZone(const FVector& Candidate, FVector& OutLocation) const
{
	const AZoneBase* OwnerZone = Cast<AZoneBase>(GetOwner());
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!OwnerZone || !NavigationSystem)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	const FVector SafeProjectionExtent(
		FMath::Max(NavigationProjectionExtent.X, 0.0f),
		FMath::Max(NavigationProjectionExtent.Y, 0.0f),
		FMath::Max(NavigationProjectionExtent.Z, 0.0f));
	if (!NavigationSystem->ProjectPointToNavigation(Candidate, ProjectedLocation, SafeProjectionExtent))
	{
		return false;
	}

	if (!OwnerZone->ContainsLocation(ProjectedLocation.Location))
	{
		return false;
	}

	OutLocation = ProjectedLocation.Location;
	return true;
}

ACPP_ZoneFallingRock* UCPP_ZoneFallingRockComponent::SpawnRockAtLocation(const FVector& SpawnLocation)
{
	UWorld* World = GetWorld();
	if (!World || !FallingRockClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACPP_ZoneFallingRock* SpawnedRock = World->SpawnActor<ACPP_ZoneFallingRock>(
		FallingRockClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (IsValid(SpawnedRock))
	{
		ActiveRocks.Add(SpawnedRock);
	}

	return SpawnedRock;
}

void UCPP_ZoneFallingRockComponent::DestroyActiveRocks()
{
	for (ACPP_ZoneFallingRock* ActiveRock : ActiveRocks)
	{
		if (IsValid(ActiveRock))
		{
			ActiveRock->Destroy();
		}
	}

	ActiveRocks.Reset();
}
