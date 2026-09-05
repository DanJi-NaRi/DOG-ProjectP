#include "CPP_BossSandStormRideActor.h"

#include "Boss/Encounter/CPP_BossEncounterDirectorComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"

ACPP_BossSandStormRideActor::ACPP_BossSandStormRideActor()
{
	bReplicates = true;
	SetReplicateMovement(true);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	RideTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("RideTrigger"));
	RideTrigger->SetupAttachment(SceneRoot);
	RideTrigger->SetSphereRadius(RideRadius);
	RideTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RideTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	RideTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	RideTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RideTrigger->SetHiddenInGame(false);
}

void ACPP_BossSandStormRideActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshTargetIfNeeded();
	if (!TargetActor.IsValid())
	{
		SetActorTickEnabled(false);
		return;
	}

	MoveTowardTarget(DeltaSeconds);
}

void ACPP_BossSandStormRideActor::Initialize(
	UCPP_BossEncounterDirectorComponent* InEncounterDirector,
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InPreRideDamageGameplayEffect
)
{
	EncounterDirector = InEncounterDirector;
	SourceASC = InSourceASC;
	PreRideDamageGameplayEffect = InPreRideDamageGameplayEffect;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ACPP_BossSandStormRideActor::StartPreRideDamageForCurrentOverlaps);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Notifies the encounter director about actors already overlapping the ride trigger.
void ACPP_BossSandStormRideActor::NotifyCurrentOverlappingRiders()
{
	if (!RideTrigger || !EncounterDirector.IsValid())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	RideTrigger->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor && OverlappingActor != this)
		{
			if (EncounterDirector->NotifyPhaseTransitionRideSuccess(OverlappingActor))
			{
				ApplyRideRootEffect(OverlappingActor);
				StopAfterSuccessfulRide();
			}
		}
	}
}

void ACPP_BossSandStormRideActor::SetTargetActor(AActor* InTargetActor)
{
	TargetActor = InTargetActor;
	bStopWhenReachedTarget = false;
	SetActorTickEnabled(TargetActor.IsValid());
}

AActor* ACPP_BossSandStormRideActor::GetTargetActor() const
{
	return TargetActor.Get();
}

void ACPP_BossSandStormRideActor::RefreshTargetIfNeeded()
{
	if (IsTargetAlive())
	{
		return;
	}

	AActor* NewTarget = EncounterDirector.IsValid() ? EncounterDirector->FindLivingSandStormTarget() : nullptr;
	SetTargetActor(NewTarget);
}

bool ACPP_BossSandStormRideActor::IsTargetAlive() const
{
	AActor* CurrentTarget = TargetActor.Get();
	if (!CurrentTarget)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(CurrentTarget);
	if (!TargetASC)
	{
		return false;
	}

	return TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) > 0.0f;
}

void ACPP_BossSandStormRideActor::MoveTowardTarget(float DeltaSeconds)
{
	AActor* CurrentTarget = TargetActor.Get();
	if (!CurrentTarget)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation = CurrentTarget->GetActorLocation();
	TargetLocation.Z = CurrentLocation.Z;

	const FVector NewLocation = FMath::VInterpConstantTo(
		CurrentLocation,
		TargetLocation,
		DeltaSeconds,
		MoveSpeed
	);

	SetActorLocation(NewLocation);

	if (bStopWhenReachedTarget && FVector::DistSquared2D(NewLocation, TargetLocation) <= FMath::Square(StopDistanceAfterRideSuccess))
	{
		TargetActor = nullptr;
		bStopWhenReachedTarget = false;
		SetActorTickEnabled(false);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Starts pre-ride damage for actors already overlapping this sandstorm after spawn overlap state is ready.
void ACPP_BossSandStormRideActor::StartPreRideDamageForCurrentOverlaps()
{
	if (!HasAuthority() || !RideTrigger || !EncounterDirector.IsValid())
	{
		return;
	}

	if (EncounterDirector->GetPhaseTransitionState() != EBossPhaseTransitionEncounterState::RideWarning)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	RideTrigger->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryStartPreRideDamageForActor(OverlappingActor);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Applies immediate pre-ride damage and starts a per-actor timer if the actor is not already tracked.
//! \param DamageTargetActor Actor that should start receiving pre-ride sandstorm damage.
void ACPP_BossSandStormRideActor::TryStartPreRideDamageForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || DamageTargetActor == this || !EncounterDirector.IsValid())
	{
		return;
	}

	if (EncounterDirector->GetPhaseTransitionState() != EBossPhaseTransitionEncounterState::RideWarning)
	{
		return;
	}

	const TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	if (PreRideDamageTimerHandlesByActor.Contains(DamageTargetPtr))
	{
		return;
	}

	ApplyPreRideDamageToActor(DamageTargetActor);
	StartPreRideDamageTimerForActor(DamageTargetActor);
}

////////////////////////////
//! \author HanSeul
//! \brief Starts a per-actor pre-ride damage timer for an actor that entered this sandstorm.
//! \param DamageTargetActor Actor that entered the sandstorm during RideWarning.
void ACPP_BossSandStormRideActor::StartPreRideDamageTimerForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || PreRideDamageInterval <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	FTimerHandle& TimerHandle = PreRideDamageTimerHandlesByActor.FindOrAdd(DamageTargetPtr);
	World->GetTimerManager().ClearTimer(TimerHandle);

	World->GetTimerManager().SetTimer(
		TimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ACPP_BossSandStormRideActor::HandlePreRideDamageTimerTick,
			DamageTargetPtr
		),
		PreRideDamageInterval,
		true
	);
}

void ACPP_BossSandStormRideActor::StopPreRideDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (FTimerHandle* TimerHandle = PreRideDamageTimerHandlesByActor.Find(DamageTargetActor))
	{
		World->GetTimerManager().ClearTimer(*TimerHandle);
		PreRideDamageTimerHandlesByActor.Remove(DamageTargetActor);
	}
}

void ACPP_BossSandStormRideActor::StopAllPreRideDamageTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& TimerPair : PreRideDamageTimerHandlesByActor)
	{
		World->GetTimerManager().ClearTimer(TimerPair.Value);
	}

	PreRideDamageTimerHandlesByActor.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief Applies a pre-ride damage tick for one actor and stops that actor's timer when it is no longer valid.
//! \param DamageTargetActor Weak pointer to the actor that owns this timer.
void ACPP_BossSandStormRideActor::HandlePreRideDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor)
{
	AActor* DamageTarget = DamageTargetActor.Get();
	if (!DamageTarget || !RideTrigger || !EncounterDirector.IsValid())
	{
		StopPreRideDamageTimerForActor(DamageTargetActor);
		return;
	}

	if (EncounterDirector->GetPhaseTransitionState() != EBossPhaseTransitionEncounterState::RideWarning)
	{
		StopAllPreRideDamageTimers();
		return;
	}

	if (!RideTrigger->IsOverlappingActor(DamageTarget))
	{
		StopPreRideDamageTimerForActor(DamageTargetActor);
		return;
	}

	ApplyPreRideDamageToActor(DamageTarget);
}

////////////////////////////
//! \author HanSeul
//! \brief Applies one current-health-ratio pre-ride damage tick to a target actor.
//! \param DamageTargetActor Actor that should receive the pre-ride sandstorm damage.
void ACPP_BossSandStormRideActor::ApplyPreRideDamageToActor(AActor* DamageTargetActor)
{
	if (!DamageTargetActor || DamageTargetActor == this || !SourceASC || !PreRideDamageGameplayEffect)
	{
		return;
	}

	if (!UMyAbilitySystemLibrary::IsHostile(SourceASC->GetAvatarActor(), DamageTargetActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(DamageTargetActor);
	if (!TargetASC)
	{
		return;
	}

	const float CurrentHealth = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
	if (CurrentHealth <= 0.0f)
	{
		return;
	}

	const float DamageAmount = CurrentHealth * PreRideDamageHealthRatio;
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
		SourceASC,
		DamageTargetActor,
		PreRideDamageGameplayEffect,
		DamageAmount
	);
}

void ACPP_BossSandStormRideActor::BeginPlay()
{
	Super::BeginPlay();

	if (RideTrigger)
	{
		RideTrigger->OnComponentBeginOverlap.AddDynamic(this, &ACPP_BossSandStormRideActor::HandleRideTriggerBeginOverlap);
		RideTrigger->OnComponentEndOverlap.AddDynamic(this, &ACPP_BossSandStormRideActor::HandleRideTriggerEndOverlap);
	}
}

void ACPP_BossSandStormRideActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllPreRideDamageTimers();

	Super::EndPlay(EndPlayReason);
}

void ACPP_BossSandStormRideActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (RideTrigger)
	{
		RideTrigger->SetSphereRadius(RideRadius);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Handles actors entering the sandstorm ride trigger.
//! \param OverlappedComponent Component that received the overlap.
//! \param OtherActor Actor that entered the trigger.
//! \param OtherComp Component from the other actor.
//! \param OtherBodyIndex Body index from the overlap event.
//! \param bFromSweep Whether the overlap came from a sweep.
//! \param SweepResult Sweep result payload.
void ACPP_BossSandStormRideActor::HandleRideTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	if (EncounterDirector.IsValid())
	{
		if (EncounterDirector->GetPhaseTransitionState() == EBossPhaseTransitionEncounterState::RideWarning)
		{
			TryStartPreRideDamageForActor(OtherActor);
		}

		if (EncounterDirector->NotifyPhaseTransitionRideSuccess(OtherActor))
		{
			ApplyRideRootEffect(OtherActor);
			StopAfterSuccessfulRide();
		}
	}
}

void ACPP_BossSandStormRideActor::HandleRideTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	StopPreRideDamageTimerForActor(TWeakObjectPtr<AActor>(OtherActor));
}

////////////////////////////
//! \author HanSeul
//! \brief Applies the ride-root GameplayEffect to a rider that successfully boarded this sandstorm.
//! \param Rider Actor that should receive the root effect.
void ACPP_BossSandStormRideActor::ApplyRideRootEffect(AActor* Rider)
{
	if (!Rider || !RideRootGameplayEffect)
	{
		return;
	}

	UAbilitySystemComponent* RiderASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(Rider);
	if (!RiderASC)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = RiderASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = RiderASC->MakeOutgoingSpec(RideRootGameplayEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	RiderASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ACPP_BossSandStormRideActor::StopAfterSuccessfulRide()
{
	bStopWhenReachedTarget = true;
	SetActorTickEnabled(TargetActor.IsValid());
}
