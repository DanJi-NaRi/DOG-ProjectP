#include "CPP_BossEncounterDirectorComponent.h"

#include "AbilitySystemComponent.h"
#include "Boss/Core/CPP_BossBrainComponent.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Actors/CPP_BossCrossLaserActor.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Actors/CPP_BossRockWarningActor.h"
#include "Boss/Actors/CPP_BossSandStormRideActor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MyPlayerState.h"

UCPP_BossEncounterDirectorComponent::UCPP_BossEncounterDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCPP_BossEncounterDirectorComponent::BeginPlay()
{
	Super::BeginPlay();
}

EBossPhaseTransitionEncounterState UCPP_BossEncounterDirectorComponent::GetPhaseTransitionState() const
{
	return PhaseTransitionState;
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the temporary phase-transition encounter flow.
//! \return true when the encounter timer starts successfully.
bool UCPP_BossEncounterDirectorComponent::StartPhaseTransitionEncounter()
{
	ACPP_BossCharacter* BossOwner = Cast<ACPP_BossCharacter>(GetOwner());
	if (!BossOwner || !BossOwner->HasAuthority() || BossOwner->GetCurrentPhase() != EBossPhase::Transition)
	{
		return false;
	}

	if (bPhaseTransitionEncounterActive)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bPhaseTransitionEncounterActive = true;
	PhaseTransitionState = EBossPhaseTransitionEncounterState::RockWarning;
	CompletedRideCount = 0;
	CompletedRiders.Reset();

	// Gimmick hazards are already swept twice before this point: at the HP threshold and again the moment
	// the in-flight pattern ended (brain HandleAbilityEnded, pending-only), so the encounter starts clean.

	StartRockWarningLoop();

	World->GetTimerManager().ClearTimer(RockPhaseTimerHandle);
	World->GetTimerManager().ClearTimer(RideWarningTimerHandle);
	World->GetTimerManager().ClearTimer(RideWindowTimerHandle);
	World->GetTimerManager().ClearTimer(SuccessDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		RockPhaseTimerHandle,
		this,
		&UCPP_BossEncounterDirectorComponent::BeginRideWarning,
		RockPhaseDuration,
		false
	);

	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Completes the phase-transition encounter successfully and restarts the boss brain.
void UCPP_BossEncounterDirectorComponent::SucceedPhaseTransitionEncounter()
{
	ACPP_BossCharacter* BossOwner = Cast<ACPP_BossCharacter>(GetOwner());
	if (!BossOwner || !BossOwner->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RockPhaseTimerHandle);
		World->GetTimerManager().ClearTimer(RideWarningTimerHandle);
		World->GetTimerManager().ClearTimer(RideWindowTimerHandle);
		World->GetTimerManager().ClearTimer(SuccessDelayTimerHandle);
	}

	bPhaseTransitionEncounterActive = false;
	PhaseTransitionState = EBossPhaseTransitionEncounterState::Inactive;
	ClearRideRootEffects();
	StopRockWarningLoop();
	ClearRockWarnings();
	ClearSandStorms();
	OnBossEncounterSucceeded.Broadcast();

	if (!BossOwner->CompletePhaseTwoTransition())
	{
		return;
	}

	ResetCurseStateForAllPlayers();

	if (UCPP_BossBrainComponent* BrainComponent = BossOwner->GetBossBrainComponent())
	{
		BrainComponent->StartBrain();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 페이즈 전환 성공 종료 시 모든 PlayerState의 저주 게이지와 저주 상태를 초기화한다.
//! \param 없음
//! \return 없음
void UCPP_BossEncounterDirectorComponent::ResetCurseStateForAllPlayers()
{
	UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (!MyPlayerState)
		{
			continue;
		}

		if (UAbilitySystemComponent* PlayerASC = MyPlayerState->GetAbilitySystemComponent())
		{
			PlayerASC->SetNumericAttributeBase(
				UMyAttributeSet::GetCurseGaugeAttribute(),
				0.0f);
		}

		MyPlayerState->SetCurseState(false);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Handles phase-transition encounter failure.
void UCPP_BossEncounterDirectorComponent::FailPhaseTransitionEncounter()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RockPhaseTimerHandle);
		World->GetTimerManager().ClearTimer(RideWarningTimerHandle);
		World->GetTimerManager().ClearTimer(RideWindowTimerHandle);
		World->GetTimerManager().ClearTimer(SuccessDelayTimerHandle);
	}

	bPhaseTransitionEncounterActive = false;
	PhaseTransitionState = EBossPhaseTransitionEncounterState::Inactive;
	ClearRideRootEffects();
	StopRockWarningLoop();
	ClearRockWarnings();
	ClearSandStorms();
	ApplyFailDamage();
	OnBossEncounterFailed.Broadcast();
}

////////////////////////////
//! \author HanSeul
//! \brief Records a rider's successful sandstorm boarding and succeeds when the required count is reached.
//! \param Rider Actor that has entered a valid sandstorm boarding area.
bool UCPP_BossEncounterDirectorComponent::NotifyPhaseTransitionRideSuccess(AActor* Rider)
{
	if (!bPhaseTransitionEncounterActive || PhaseTransitionState != EBossPhaseTransitionEncounterState::RideWindow)
	{
		return false;
	}

	if (!Rider || Rider == GetOwner())
	{
		return false;
	}

	if (!IsLivingPlayerPawn(Rider))
	{
		return false;
	}

	TWeakObjectPtr<AActor> RiderPtr(Rider);
	if (CompletedRiders.Contains(RiderPtr))
	{
		return false;
	}

	CompletedRiders.Add(RiderPtr);
	++CompletedRideCount;

	if (CompletedRideCount >= RequiredRideCount)
	{
		SchedulePhaseTransitionSuccess();
	}

	return true;
}

AActor* UCPP_BossEncounterDirectorComponent::FindLivingSandStormTarget() const
{
	TArray<AActor*> LivingPlayers;
	GetLivingPlayerPawns(LivingPlayers);
	return LivingPlayers.IsEmpty() ? nullptr : LivingPlayers[0];
}

////////////////////////////
//! \author HanSeul
//! \brief Starts sandstorm warning after the rock warning phase ends.
void UCPP_BossEncounterDirectorComponent::BeginRideWarning()
{
	if (!bPhaseTransitionEncounterActive || PhaseTransitionState != EBossPhaseTransitionEncounterState::RockWarning)
	{
		return;
	}

	PhaseTransitionState = EBossPhaseTransitionEncounterState::RideWarning;
	StopRockWarningLoop();
	SpawnSandStorms();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RideWarningTimerHandle);
		World->GetTimerManager().SetTimer(
			RideWarningTimerHandle,
			this,
			&UCPP_BossEncounterDirectorComponent::BeginRideWindow,
			RideWarningDuration,
			false
		);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Opens the sandstorm ride window after the warning period.
void UCPP_BossEncounterDirectorComponent::BeginRideWindow()
{
	if (!bPhaseTransitionEncounterActive || PhaseTransitionState != EBossPhaseTransitionEncounterState::RideWarning)
	{
		return;
	}

	PhaseTransitionState = EBossPhaseTransitionEncounterState::RideWindow;

	for (ACPP_BossSandStormRideActor* SandStorm : ActiveSandStorms)
	{
		if (IsValid(SandStorm))
		{
			SandStorm->NotifyCurrentOverlappingRiders();
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RideWindowTimerHandle);
		World->GetTimerManager().SetTimer(
			RideWindowTimerHandle,
			this,
			&UCPP_BossEncounterDirectorComponent::HandleRideWindowTimerFinished,
			RideWindowDuration,
			false
		);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Schedules delayed success after every required rider boards a sandstorm.
void UCPP_BossEncounterDirectorComponent::SchedulePhaseTransitionSuccess()
{
	if (!bPhaseTransitionEncounterActive)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RideWindowTimerHandle);
		World->GetTimerManager().ClearTimer(SuccessDelayTimerHandle);
		World->GetTimerManager().SetTimer(
			SuccessDelayTimerHandle,
			this,
			&UCPP_BossEncounterDirectorComponent::SucceedPhaseTransitionEncounter,
			SuccessDelay,
			false
		);
	}
}

void UCPP_BossEncounterDirectorComponent::HandleRideWindowTimerFinished()
{
	if (!bPhaseTransitionEncounterActive || PhaseTransitionState != EBossPhaseTransitionEncounterState::RideWindow)
	{
		return;
	}

	FailPhaseTransitionEncounter();
}

////////////////////////////
//! \author HanSeul
//! \brief Applies encounter-failure lethal damage to pawns around the boss.
void UCPP_BossEncounterDirectorComponent::ApplyFailDamage()
{
	ACPP_BossCharacter* BossOwner = Cast<ACPP_BossCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!BossOwner || !World || !BossOwner->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = BossOwner->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayEffect> DamageEffectClass = BossOwner->GetBossDamageGameplayEffect();
	if (!SourceASC || !DamageEffectClass || FailDamageRadius <= 0.0f || FailDamageAmount <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(FailDamageRadius);
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		BossOwner->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		CollisionShape
	);

	if (!bHasOverlap)
	{
		return;
	}

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || TargetActor == BossOwner || DamagedActors.Contains(TargetActor))
		{
			continue;
		}

		DamagedActors.Add(TargetActor);
		UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			DamageEffectClass,
			FailDamageAmount
		);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Removes ride-root effects from riders that successfully boarded sandstorms.
void UCPP_BossEncounterDirectorComponent::ClearRideRootEffects()
{
	const FGameplayTag RideRootTag = FGameplayTag::RequestGameplayTag(TEXT("Boss.Encounter.RideRoot"), false);
	if (!RideRootTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer RideRootTags;
	RideRootTags.AddTag(RideRootTag);

	for (const TWeakObjectPtr<AActor>& RiderPtr : CompletedRiders)
	{
		AActor* Rider = RiderPtr.Get();
		if (!Rider)
		{
			continue;
		}

		UAbilitySystemComponent* RiderASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(Rider);
		if (!RiderASC)
		{
			continue;
		}

		RiderASC->RemoveActiveEffectsWithGrantedTags(RideRootTags);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Starts repeatedly spawning rock warning actors during the rock warning phase.
void UCPP_BossEncounterDirectorComponent::StartRockWarningLoop()
{
	SpawnRockWarningsForLivingPlayers();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RockSpawnTimerHandle);
		World->GetTimerManager().SetTimer(
			RockSpawnTimerHandle,
			this,
			&UCPP_BossEncounterDirectorComponent::SpawnRockWarningsForLivingPlayers,
			RockSpawnInterval,
			true
		);
	}
}

void UCPP_BossEncounterDirectorComponent::StopRockWarningLoop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RockSpawnTimerHandle);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns rock warning actors near every living player.
void UCPP_BossEncounterDirectorComponent::SpawnRockWarningsForLivingPlayers()
{
	SpawnFallingWarningsForLivingPlayers(RockWarningActorClass, RockSpawnRadiusAroundPlayer, ActiveRockWarnings);
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns a falling-warning actor near each living player. Shared by the phase-transition rock warnings and
//!        the clear-encounter red lightning; the per-actor damage coefficient/effect lives on the spawned class.
//! \param WarningActorClass Rock-warning-derived class to spawn (rock or red lightning variant).
//! \param SpawnRadiusAroundPlayer Random horizontal spawn radius around each living player.
//! \param OutSpawnedWarnings Array that receives the spawned warnings for later cleanup.
void UCPP_BossEncounterDirectorComponent::SpawnFallingWarningsForLivingPlayers(TSubclassOf<ACPP_BossRockWarningActor> WarningActorClass, float SpawnRadiusAroundPlayer, TArray<TObjectPtr<ACPP_BossRockWarningActor>>& OutSpawnedWarnings)
{
	ACPP_BossCharacter* BossOwner = Cast<ACPP_BossCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!BossOwner || !World || !WarningActorClass)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = BossOwner->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayEffect> DamageEffectClass = BossOwner->GetBossDamageGameplayEffect();
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	TArray<AActor*> LivingPlayers;
	GetLivingPlayerPawns(LivingPlayers);

	for (AActor* LivingPlayer : LivingPlayers)
	{
		if (!LivingPlayer)
		{
			continue;
		}

		const float RandomAngle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float RandomDistance = FMath::FRandRange(0.0f, SpawnRadiusAroundPlayer);
		const FVector RandomOffset(
			FMath::Cos(RandomAngle) * RandomDistance,
			FMath::Sin(RandomAngle) * RandomDistance,
			0.0f
		);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = BossOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACPP_BossRockWarningActor* Warning = World->SpawnActor<ACPP_BossRockWarningActor>(
			WarningActorClass,
			LivingPlayer->GetActorLocation() + RandomOffset,
			FRotator::ZeroRotator,
			SpawnParameters
		);

		if (!Warning)
		{
			continue;
		}

		Warning->Initialize(SourceASC, DamageEffectClass);
		OutSpawnedWarnings.Add(Warning);
	}
}

void UCPP_BossEncounterDirectorComponent::ClearRockWarnings()
{
	for (ACPP_BossRockWarningActor* RockWarning : ActiveRockWarnings)
	{
		if (IsValid(RockWarning))
		{
			RockWarning->Destroy();
		}
	}

	ActiveRockWarnings.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns temporary sandstorm ride actors around the boss for the phase-transition encounter.
void UCPP_BossEncounterDirectorComponent::SpawnSandStorms()
{
	ClearSandStorms();

	ACPP_BossCharacter* BossOwner = Cast<ACPP_BossCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!BossOwner || !World || !SandStormRideActorClass)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = BossOwner->GetAbilitySystemComponent();
	TSubclassOf<UGameplayEffect> DamageEffectClass = BossOwner->GetBossDamageGameplayEffect();
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	const FVector BossLocation = BossOwner->GetActorLocation();
	const FVector Forward = BossOwner->GetActorForwardVector();
	const FVector Right = BossOwner->GetActorRightVector();
	constexpr float SpawnDistance = 600.0f;

	const TArray<FVector> SpawnLocations = {
		BossLocation + Forward * SpawnDistance,
		BossLocation + Right * SpawnDistance,
		BossLocation - Right * SpawnDistance
	};

	for (const FVector& SpawnLocation : SpawnLocations)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = BossOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACPP_BossSandStormRideActor* SandStorm = World->SpawnActor<ACPP_BossSandStormRideActor>(
			SandStormRideActorClass,
			SpawnLocation,
			FRotator::ZeroRotator,
			SpawnParameters
		);

		if (!SandStorm)
		{
			continue;
		}

		SandStorm->Initialize(this, SourceASC, DamageEffectClass);
		ActiveSandStorms.Add(SandStorm);
	}

	AssignSandStormTargets();
}

void UCPP_BossEncounterDirectorComponent::ClearSandStorms()
{
	for (ACPP_BossSandStormRideActor* SandStorm : ActiveSandStorms)
	{
		if (IsValid(SandStorm))
		{
			SandStorm->Destroy();
		}
	}

	ActiveSandStorms.Reset();
}

////////////////////////////
//! \brief Tracks a gimmick hazard so an encounter phase can force-destroy it. Prunes stale weak refs opportunistically.
void UCPP_BossEncounterDirectorComponent::RegisterGimmickHazard(AActor* GimmickHazard)
{
	if (!GimmickHazard)
	{
		return;
	}

	ActiveGimmickHazards.RemoveAll([](const TWeakObjectPtr<AActor>& Existing)
	{
		return !Existing.IsValid();
	});

	ActiveGimmickHazards.Add(GimmickHazard);
}

////////////////////////////
//! \brief Destroys every active gimmick hazard. Called when the phase-two transition or clear encounter begins.
void UCPP_BossEncounterDirectorComponent::ClearGimmickHazards()
{
	for (const TWeakObjectPtr<AActor>& GimmickHazard : ActiveGimmickHazards)
	{
		if (GimmickHazard.IsValid())
		{
			GimmickHazard->Destroy();
		}
	}

	ActiveGimmickHazards.Reset();
}

void UCPP_BossEncounterDirectorComponent::AssignSandStormTargets()
{
	TArray<AActor*> LivingPlayers;
	GetLivingPlayerPawns(LivingPlayers);

	if (LivingPlayers.IsEmpty())
	{
		for (ACPP_BossSandStormRideActor* SandStorm : ActiveSandStorms)
		{
			if (IsValid(SandStorm))
			{
				SandStorm->SetTargetActor(nullptr);
			}
		}
		return;
	}

	for (int32 Index = 0; Index < ActiveSandStorms.Num(); ++Index)
	{
		ACPP_BossSandStormRideActor* SandStorm = ActiveSandStorms[Index];
		if (!IsValid(SandStorm))
		{
			continue;
		}

		SandStorm->SetTargetActor(LivingPlayers[Index % LivingPlayers.Num()]);
	}
}

void UCPP_BossEncounterDirectorComponent::GetLivingPlayerPawns(TArray<AActor*>& OutLivingPlayers) const
{
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(this, OutLivingPlayers);
}

bool UCPP_BossEncounterDirectorComponent::IsLivingPlayerPawn(AActor* Candidate) const
{
	if (!Candidate || Candidate == GetOwner())
	{
		return false;
	}

	return UMyAbilitySystemLibrary::IsLivingPawn(Candidate);
}

ACPP_BossCharacter* UCPP_BossEncounterDirectorComponent::GetBossOwner() const
{
	return Cast<ACPP_BossCharacter>(GetOwner());
}

bool UCPP_BossEncounterDirectorComponent::IsClearEncounterActive() const
{
	return bClearEncounterActive;
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the clear (final judgment) encounter. Grants a large shield and runs red lightning + cross laser
//!        concurrently. Deliberately does NOT stop the brain or lock the boss, so phase-two patterns keep firing.
//! \return true when the clear encounter starts.
bool UCPP_BossEncounterDirectorComponent::StartClearEncounter()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	UWorld* World = GetWorld();
	if (!BossOwner || !World || !BossOwner->HasAuthority() || bClearEncounterActive || bClearEncounterConsumed)
	{
		return false;
	}

	bClearEncounterActive = true;
	bClearEncounterConsumed = true;

	// Gimmick hazards are already swept twice before this point: at the HP threshold and again the moment
	// the in-flight pattern ended (brain HandleAbilityEnded, pending-only), so the encounter starts clean.

	if (UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent())
	{
		BossASC->AddLooseGameplayTag(BossGameplayTags::Boss_Encounter_FinalJudgment);
	}

	// Grant the shield first, then bind the watcher, so the 0 -> large transition is not mistaken for depletion.
	GrantClearEncounterShield();
	BindBossShieldChangedDelegate();

	StartRedLightningLoop();
	SpawnCrossLaser();

	World->GetTimerManager().ClearTimer(ClearEncounterTimerHandle);
	World->GetTimerManager().SetTimer(
		ClearEncounterTimerHandle,
		this,
		&UCPP_BossEncounterDirectorComponent::FailClearEncounter,
		ClearEncounterDuration,
		false
	);

	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Succeeds the clear encounter (shield removed in time): tears down the encounter, then defeats the boss.
void UCPP_BossEncounterDirectorComponent::SucceedClearEncounter()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner || !BossOwner->HasAuthority() || !bClearEncounterActive)
	{
		return;
	}

	EndClearEncounter();
	BossOwner->HandleBossDefeated();
}

////////////////////////////
//! \author HanSeul
//! \brief Fails the clear encounter (time limit reached): tears down the encounter and wipes all living players.
void UCPP_BossEncounterDirectorComponent::FailClearEncounter()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner || !BossOwner->HasAuthority() || !bClearEncounterActive)
	{
		return;
	}

	EndClearEncounter();

	// Lock the boss so it stops acting once the clear encounter fails.
	if (UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent())
	{
		BossASC->AddLooseGameplayTag(BossGameplayTags::Boss_State_Locked);
	}

	ApplyFailDamage();
}

////////////////////////////
//! \author HanSeul
//! \brief Common teardown shared by clear-encounter success and failure. Clears the health-pin flag first so the
//!        attribute set stops flooring boss health.
void UCPP_BossEncounterDirectorComponent::EndClearEncounter()
{
	bClearEncounterActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClearEncounterTimerHandle);
	}

	StopRedLightningLoop();
	ClearRedLightnings();
	ClearCrossLaser();
	UnbindBossShieldChangedDelegate();

	if (ACPP_BossCharacter* BossOwner = GetBossOwner())
	{
		if (UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent())
		{
			BossASC->RemoveLooseGameplayTag(BossGameplayTags::Boss_Encounter_FinalJudgment);
		}
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Applies the large clear-encounter shield to the boss via the configured SetByCaller shield effect.
void UCPP_BossEncounterDirectorComponent::GrantClearEncounterShield()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner)
	{
		return;
	}

	UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent();
	if (!BossASC || !ClearShieldGameplayEffect || ClearShieldAmount <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = BossASC->MakeEffectContext();
	EffectContext.AddSourceObject(BossOwner);

	FGameplayEffectSpecHandle SpecHandle = BossASC->MakeOutgoingSpec(ClearShieldGameplayEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	UMyAbilitySystemLibrary::AssignSetByCallerShield(SpecHandle, ClearShieldAmount);
	BossASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UCPP_BossEncounterDirectorComponent::BindBossShieldChangedDelegate()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner)
	{
		return;
	}

	UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent();
	if (!BossASC)
	{
		return;
	}

	BossShieldChangedDelegateHandle = BossASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute())
		.AddUObject(this, &UCPP_BossEncounterDirectorComponent::HandleBossShieldChanged);
}

void UCPP_BossEncounterDirectorComponent::UnbindBossShieldChangedDelegate()
{
	if (!BossShieldChangedDelegateHandle.IsValid())
	{
		return;
	}

	if (ACPP_BossCharacter* BossOwner = GetBossOwner())
	{
		if (UAbilitySystemComponent* BossASC = BossOwner->GetAbilitySystemComponent())
		{
			BossASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetShieldAttribute()).Remove(BossShieldChangedDelegateHandle);
		}
	}

	BossShieldChangedDelegateHandle.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief Succeeds the clear encounter as soon as the boss shield is fully depleted.
//! \param ChangeData Shield attribute change payload from the boss Ability System Component.
void UCPP_BossEncounterDirectorComponent::HandleBossShieldChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!bClearEncounterActive)
	{
		return;
	}

	if (ChangeData.NewValue <= 0.0f)
	{
		SucceedClearEncounter();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the repeating red lightning spawn loop for the clear encounter.
void UCPP_BossEncounterDirectorComponent::StartRedLightningLoop()
{
	SpawnRedLightningsForLivingPlayers();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RedLightningTimerHandle);
		World->GetTimerManager().SetTimer(
			RedLightningTimerHandle,
			this,
			&UCPP_BossEncounterDirectorComponent::SpawnRedLightningsForLivingPlayers,
			RedLightningInterval,
			true
		);
	}
}

void UCPP_BossEncounterDirectorComponent::StopRedLightningLoop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RedLightningTimerHandle);
	}
}

void UCPP_BossEncounterDirectorComponent::SpawnRedLightningsForLivingPlayers()
{
	SpawnFallingWarningsForLivingPlayers(RedLightningActorClass, RedLightningSpawnRadiusAroundPlayer, ActiveRedLightnings);
}

void UCPP_BossEncounterDirectorComponent::ClearRedLightnings()
{
	for (ACPP_BossRockWarningActor* RedLightning : ActiveRedLightnings)
	{
		if (IsValid(RedLightning))
		{
			RedLightning->Destroy();
		}
	}

	ActiveRedLightnings.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns the single rotating cross laser at the boss location for the clear encounter.
void UCPP_BossEncounterDirectorComponent::SpawnCrossLaser()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	UWorld* World = GetWorld();
	if (!BossOwner || !World || !CrossLaserActorClass || ActiveCrossLaser)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = BossOwner->GetAbilitySystemComponent();
	const TSubclassOf<UGameplayEffect> DamageEffectClass = BossOwner->GetBossDamageGameplayEffect();
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = BossOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Arena center is owned by the boss (shared with other arena-centered skills); falls back to boss location when unset.
	const FVector SpawnLocation = BossOwner->GetArenaCenterLocation();

	ActiveCrossLaser = World->SpawnActor<ACPP_BossCrossLaserActor>(
		CrossLaserActorClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters
	);

	if (ActiveCrossLaser)
	{
		ActiveCrossLaser->Initialize(SourceASC, DamageEffectClass);
	}
}

void UCPP_BossEncounterDirectorComponent::ClearCrossLaser()
{
	if (IsValid(ActiveCrossLaser))
	{
		ActiveCrossLaser->Destroy();
	}

	ActiveCrossLaser = nullptr;
}
