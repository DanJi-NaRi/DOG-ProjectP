////////////////////////////
//! \page MyNeferJudgementArea.cpp

#include "MyNeferJudgementArea.h"

#include "Components/SphereComponent.h"
#include "TimerManager.h"

AMyNeferJudgementArea::AMyNeferJudgementArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AreaComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AreaComponent"));
	SetRootComponent(AreaComponent);
	AreaComponent->InitSphereRadius(AreaRadius);
	AreaComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AreaComponent->SetGenerateOverlapEvents(false);
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 장판의 시전자, 반경, 지속시간을 초기화한다. 피해/회복 적용은 GA가 담당한다.
//! \param InSourceActor 장판을 설치한 Actor
//! \param InAreaRadius 장판 반경
//! \param InDuration 장판 지속시간
//! \return 없음
void AMyNeferJudgementArea::InitializeJudgementArea(
	AActor* InSourceActor,
	float InAreaRadius,
	float InDuration
)
{
	SourceActor = InSourceActor;
	AreaRadius = FMath::Max(InAreaRadius, 0.0f);
	Duration = FMath::Max(InDuration, 0.0f);

	if (AreaComponent)
	{
		AreaComponent->SetSphereRadius(AreaRadius);
	}

	if (SourceActor)
	{
		SourceActor->OnDestroyed.AddDynamic(this, &AMyNeferJudgementArea::HandleSourceDestroyed);
	}

	if (HasAuthority() && Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DurationTimerHandle,
				this,
				&AMyNeferJudgementArea::HandleDurationFinished,
				Duration,
				false
			);
		}
	}
}

void AMyNeferJudgementArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	if (SourceActor)
	{
		SourceActor->OnDestroyed.RemoveDynamic(this, &AMyNeferJudgementArea::HandleSourceDestroyed);
	}

	Super::EndPlay(EndPlayReason);
}

void AMyNeferJudgementArea::HandleSourceDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority())
	{
		Destroy();
	}
}

void AMyNeferJudgementArea::HandleDurationFinished()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
