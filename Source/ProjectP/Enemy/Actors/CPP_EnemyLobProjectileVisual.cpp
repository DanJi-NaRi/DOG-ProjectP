#include "CPP_EnemyLobProjectileVisual.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

ACPP_EnemyLobProjectileVisual::ACPP_EnemyLobProjectileVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ACPP_EnemyLobProjectileVisual::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_EnemyLobProjectileVisual, VisualData);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버가 확정한 포물선 경로와 시작 시각을 저장하고 모든 클라이언트에서 같은 비행을 시작한다.
//! \param InStartLocation 포탄 연출 시작 위치.
//! \param InEndLocation 텔레그래프와 피해 판정이 공유하는 도착 위치.
//! \param InPeakHeight 시작점과 도착점의 선형 경로 위에 더할 최고 높이.
//! \param InDuration 서버 경고 시간과 동일한 비행 시간.
//! \param InServerStartTime 텔레그래프가 시작된 서버 월드 시각.
//! \return None
void ACPP_EnemyLobProjectileVisual::InitializeLobVisual(
	const FVector& InStartLocation,
	const FVector& InEndLocation,
	float InPeakHeight,
	float InDuration,
	float InServerStartTime
)
{
	if (!HasAuthority())
	{
		return;
	}

	VisualData.StartLocation = InStartLocation;
	VisualData.EndLocation = InEndLocation;
	VisualData.PeakHeight = FMath::Max(InPeakHeight, 0.0f);
	VisualData.Duration = FMath::Max(InDuration, 0.0f);
	VisualData.ServerStartTime = InServerStartTime;
	bHasVisualData = true;
	SetActorHiddenInGame(false);
	ApplyVisualAtServerTime();
	ForceNetUpdate();

	SetLifeSpan(VisualData.Duration + FMath::Max(NetworkDestroyGraceDuration, 0.0f));
}

////////////////////////////
//! \author HanSeul
//! \brief 동기화된 서버 시각으로 현재 포물선 위치와 진행 방향을 갱신한다.
//! \param DeltaSeconds 프레임 델타 시간.
//! \return None
void ACPP_EnemyLobProjectileVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyVisualAtServerTime();
}

void ACPP_EnemyLobProjectileVisual::OnRep_VisualData()
{
	bHasVisualData = true;
	SetActorHiddenInGame(false);
	ApplyVisualAtServerTime();
}

////////////////////////////
//! \author HanSeul
//! \brief 현재 서버 시각에 대응하는 포물선 위치를 적용하고 착탄 시 연출을 숨긴다.
//! \return None
void ACPP_EnemyLobProjectileVisual::ApplyVisualAtServerTime()
{
	if (!bHasVisualData)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float ElapsedTime = FMath::Max(GetSynchronizedServerTime() - VisualData.ServerStartTime, 0.0f);
	const float Alpha = VisualData.Duration > 0.0f
		? FMath::Clamp(ElapsedTime / VisualData.Duration, 0.0f, 1.0f)
		: 1.0f;
	const FVector BaseLocation = FMath::Lerp(VisualData.StartLocation, VisualData.EndLocation, Alpha);
	const float HeightOffset = 4.0f * VisualData.PeakHeight * Alpha * (1.0f - Alpha);
	SetActorLocation(BaseLocation + FVector::UpVector * HeightOffset);

	const FVector Tangent = VisualData.EndLocation - VisualData.StartLocation
		+ FVector::UpVector * (4.0f * VisualData.PeakHeight * (1.0f - 2.0f * Alpha));
	if (!Tangent.IsNearlyZero())
	{
		SetActorRotation(Tangent.Rotation());
	}

	const bool bHasArrived = Alpha >= 1.0f;
	SetActorHiddenInGame(bHasArrived);
	SetActorTickEnabled(!bHasArrived);
}

////////////////////////////
//! \author HanSeul
//! \brief 포탄 연출의 진행률 계산에 사용할 동기화된 서버 월드 시각을 반환한다.
//! \return GameState 서버 월드 시각. GameState가 없으면 현재 월드 시각.
float ACPP_EnemyLobProjectileVisual::GetSynchronizedServerTime() const
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
