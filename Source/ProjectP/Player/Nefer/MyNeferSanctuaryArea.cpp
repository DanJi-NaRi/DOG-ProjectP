////////////////////////////
//! \page MyNeferSanctuaryArea.cpp

#include "MyNeferSanctuaryArea.h"

#include "Components/SphereComponent.h"
#include "TimerManager.h"

AMyNeferSanctuaryArea::AMyNeferSanctuaryArea()
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
//! \brief Sanctuary 장판의 시전자, 반경, 지속시간을 초기화한다. 회복 적용은 GA의 장판 틱이 담당한다.
//! \param InSourceActor 장판을 설치한 Actor
//! \param InAreaRadius 장판 반경
//! \param InDuration 장판 지속시간
//! \return 없음
void AMyNeferSanctuaryArea::InitializeSanctuary(
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
		SourceActor->OnDestroyed.AddDynamic(this, &AMyNeferSanctuaryArea::HandleSourceDestroyed);
	}

	if (HasAuthority())
	{
		UWorld* World = GetWorld();
		if (World && Duration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				DurationTimerHandle,
				this,
				&AMyNeferSanctuaryArea::HandleDurationFinished,
				Duration,
				false
			);
		}

		UE_LOG(LogTemp, Log, TEXT("Nefer sanctuary area initialized - Area: %s, Source: %s, Location: %s, Radius: %.2f, Duration: %.2f"),
			*GetNameSafe(this),
			*GetNameSafe(SourceActor),
			*GetActorLocation().ToCompactString(),
			AreaRadius,
			Duration);
	}
}

void AMyNeferSanctuaryArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DurationTimerHandle);
	}

	if (SourceActor)
	{
		SourceActor->OnDestroyed.RemoveDynamic(this, &AMyNeferSanctuaryArea::HandleSourceDestroyed);
	}

	Super::EndPlay(EndPlayReason);
}

void AMyNeferSanctuaryArea::HandleSourceDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority())
	{
		Destroy();
	}
}

void AMyNeferSanctuaryArea::HandleDurationFinished()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
