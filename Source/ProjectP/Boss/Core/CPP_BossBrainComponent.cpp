#include "CPP_BossBrainComponent.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Boss/Abilities/CPP_BossAttackAbility.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Encounter/CPP_BossEncounterDirectorComponent.h"
#include "Boss/Abilities/CPP_BossPatternSetData.h"
#include "Boss/Abilities/CPP_BossRepositionAbility.h"
#include "Boss/Core/CPP_BossTargetingComponent.h"
#include "Engine/World.h"
#include "GAS/MyGameplayAbilityBase.h"

UCPP_BossBrainComponent::UCPP_BossBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCPP_BossPatternSetData* UCPP_BossBrainComponent::GetPatternSetData() const
{
	return PatternSetData;
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the boss brain and tries to activate the first non-gimmick pattern for the current phase.
//! \return true when a first pattern is selected and activation is requested.
bool UCPP_BossBrainComponent::StartBrain()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner || !BossOwner->HasAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = GetBossAbilitySystemComponent();
	if (!ASC || !PatternSetData)
	{
		return false;
	}

	if (!CanMakeDecision())
	{
		return false;
	}

	if (!bAbilityEndedDelegateBound)
	{
		ASC->OnAbilityEnded.AddUObject(this, &UCPP_BossBrainComponent::HandleAbilityEnded);
		bAbilityEndedDelegateBound = true;
	}

	bBrainRunning = true;
	return TrySelectAndActivateNextPattern();
}

ACPP_BossCharacter* UCPP_BossBrainComponent::GetBossOwner() const
{
	return Cast<ACPP_BossCharacter>(GetOwner());
}

UAbilitySystemComponent* UCPP_BossBrainComponent::GetBossAbilitySystemComponent() const
{
	const ACPP_BossCharacter* BossOwner = GetBossOwner();
	return BossOwner ? BossOwner->GetAbilitySystemComponent() : nullptr;
}

////////////////////////////
//! \author HanSeul
//! \brief Selects a pattern for the current phase using first-pick and cooldown-aware rules, then activates it.
//! \return true when a pattern is selected and activation is requested successfully.
bool UCPP_BossBrainComponent::TrySelectAndActivateNextPattern()
{
	if (!CanMakeDecision())
	{
		StopBrainDecisionLoop();
		return false;
	}

	if (TryBeginPendingPhaseTransition())
	{
		StopBrainDecisionLoop();
		return false;
	}

	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner || !PatternSetData)
	{
		return false;
	}

	// Clear (final judgment) encounter is deferred to this pattern boundary. It now opens with a staged
	// teleport to the arena center, so the brain pauses here; the boss restarts it when the teleport lands
	// (the boss keeps attacking during the clear encounter itself). If staging fell back to an immediate
	// start (no teleport running), keep selecting patterns like before.
	if (TryBeginPendingClearEncounterStaging() && BossOwner->IsStagedTeleportActive())
	{
		StopBrainDecisionLoop();
		return false;
	}

	// Advance toward a distant target before selecting the next attack pattern.
	if (TryActivateRepositionPattern(BossOwner))
	{
		return true;
	}

	const EBossPhase CurrentPhase = BossOwner->GetCurrentPhase();
	const bool bFirstSelection = IsFirstSelectionForPhase(CurrentPhase);
	const TArray<FBossPatternEntry>& PhasePatterns = PatternSetData->GetPatternsForPhase(CurrentPhase);
	const TArray<const FBossPatternEntry*> CandidatePatterns = BuildAvailablePatternCandidates(PhasePatterns, bFirstSelection);
	const FBossPatternEntry* SelectedPattern = SelectPatternCandidate(CandidatePatterns, bFirstSelection);
	if (!SelectedPattern)
	{
		return false;
	}

	if (!TryActivatePatternAbility(SelectedPattern->AbilityClass))
	{
		return false;
	}

	ConsecutiveAdvanceCount = 0;
	ClearBossFocus();
	MarkFirstSelectionDoneForPhase(CurrentPhase);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 타겟을 재선정하고 수평 거리가 전진 기준보다 멀면 전진 이동 패턴을 발동한다.
//!        전진은 MaxConsecutiveAdvances까지 연속 허용하며, 이동 패턴은 공격 패턴의 반복 방지 기록
//!        (LastActivatedAbilityClass)에 남기지 않는다.
//! \param BossOwner 판단 기준이 되는 보스 캐릭터.
//! \return 이동 패턴이 발동되면 true.
bool UCPP_BossBrainComponent::TryActivateRepositionPattern(ACPP_BossCharacter* BossOwner)
{
	if (!RepositionAbilityClass || !BossOwner)
	{
		return false;
	}

	UCPP_BossTargetingComponent* TargetingComponent = BossOwner->GetBossTargetingComponent();
	AActor* Target = TargetingComponent ? TargetingComponent->ReevaluateTarget() : nullptr;
	if (!Target)
	{
		return false;
	}

	const UCPP_BossRepositionAbility* RepositionCDO = RepositionAbilityClass->GetDefaultObject<UCPP_BossRepositionAbility>();
	if (!RepositionCDO)
	{
		return false;
	}

	const float DistanceToTarget = FVector::Dist2D(BossOwner->GetActorLocation(), Target->GetActorLocation());
	const bool bAdvanceAllowed = RepositionCDO->ShouldAdvanceAtDistance(DistanceToTarget)
		&& ConsecutiveAdvanceCount < MaxConsecutiveAdvances;
	if (!bAdvanceAllowed)
	{
		return false;
	}

	if (!TryActivateAbilityByClass(RepositionAbilityClass))
	{
		return false;
	}

	++ConsecutiveAdvanceCount;
	ClearBossFocus();
	return true;
}

bool UCPP_BossBrainComponent::CanMakeDecision() const
{
	const ACPP_BossCharacter* BossOwner = GetBossOwner();
	UAbilitySystemComponent* ASC = GetBossAbilitySystemComponent();
	if (!BossOwner || !ASC || !PatternSetData)
	{
		return false;
	}

	const EBossPhase CurrentPhase = BossOwner->GetCurrentPhase();
	if (CurrentPhase == EBossPhase::None || CurrentPhase == EBossPhase::Transition)
	{
		return false;
	}

	if (ASC->HasMatchingGameplayTag(BossGameplayTags::Boss_State_Locked)
		|| ASC->HasMatchingGameplayTag(BossGameplayTags::Boss_State_Dead))
	{
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the pending phase-two transition instead of selecting another normal pattern.
//!        The transition now opens with a staged teleport to the arena center; the brain stops either way
//!        and the existing transition-completion path restarts it.
//! \return true when the pending phase transition is consumed successfully.
bool UCPP_BossBrainComponent::TryBeginPendingPhaseTransition()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	if (!BossOwner || !BossOwner->IsPhaseTransitionPending())
	{
		return false;
	}

	return BossOwner->BeginPhaseTwoTransitionStaged();
}

////////////////////////////
//! \author HanUl
//! \brief Kicks off the staged opening of a pending clear (final judgment) encounter: the boss teleports
//!        to the arena center and starts the encounter on arrival. The caller stops the brain; the boss
//!        character restarts it when the teleport finishes.
//! \return true when the staged opening (or its immediate fallback) was accepted.
bool UCPP_BossBrainComponent::TryBeginPendingClearEncounterStaging()
{
	ACPP_BossCharacter* BossOwner = GetBossOwner();
	return BossOwner && BossOwner->IsClearEncounterPending() && BossOwner->BeginPendingClearEncounterStaged();
}

void UCPP_BossBrainComponent::StopBrainDecisionLoop()
{
	bBrainRunning = false;
	ClearBossFocus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DecisionTimerHandle);
	}
}

AAIController* UCPP_BossBrainComponent::GetBossAIController() const
{
	const ACPP_BossCharacter* BossOwner = GetBossOwner();
	return BossOwner ? Cast<AAIController>(BossOwner->GetController()) : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 패턴 사이 구간의 조준 회전을 시작한다. 타겟을 재선정해 AIController Focus로 지정하면
//!        CMC(bUseControllerDesiredRotation)가 RotationRate 속도로 보스를 타겟 방향으로 돌린다.
//!        패턴이 발동되면 ClearBossFocus로 회전이 멈춘다.
void UCPP_BossBrainComponent::UpdateDecisionGapFocus()
{
	AAIController* BossAIController = GetBossAIController();
	if (!BossAIController)
	{
		return;
	}

	if (!CanMakeDecision())
	{
		ClearBossFocus();
		return;
	}

	const ACPP_BossCharacter* BossOwner = GetBossOwner();
	UCPP_BossTargetingComponent* TargetingComponent = BossOwner ? BossOwner->GetBossTargetingComponent() : nullptr;
	AActor* Target = TargetingComponent ? TargetingComponent->ReevaluateTarget() : nullptr;
	if (Target)
	{
		BossAIController->SetFocus(Target, EAIFocusPriority::Gameplay);
	}
	else
	{
		BossAIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void UCPP_BossBrainComponent::ClearBossFocus()
{
	if (AAIController* BossAIController = GetBossAIController())
	{
		BossAIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

TArray<const FBossPatternEntry*> UCPP_BossBrainComponent::BuildAvailablePatternCandidates(const TArray<FBossPatternEntry>& PhasePatterns, bool bFirstSelection) const
{
	UAbilitySystemComponent* ASC = GetBossAbilitySystemComponent();
	TArray<const FBossPatternEntry*> CandidatePatterns;
	if (!ASC)
	{
		return CandidatePatterns;
	}

	for (const FBossPatternEntry& PatternEntry : PhasePatterns)
	{
		if (!PatternEntry.AbilityClass)
		{
			continue;
		}

		const UMyGameplayAbilityBase* AbilityCDO = PatternEntry.AbilityClass->GetDefaultObject<UMyGameplayAbilityBase>();
		if (!AbilityCDO)
		{
			continue;
		}

		const FGameplayTag CooldownTag = AbilityCDO->GetCooldownTag();
		if (CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag))
		{
			continue;
		}

		if (bFirstSelection && PatternEntry.bIsGimmick)
		{
			continue;
		}

		CandidatePatterns.Add(&PatternEntry);
	}

	if (CandidatePatterns.IsEmpty())
	{
		return CandidatePatterns;
	}

	if (CandidatePatterns.Num() <= 1 || !LastActivatedAbilityClass)
	{
		return CandidatePatterns;
	}

	TArray<const FBossPatternEntry*> NonRepeatedCandidates;
	for (const FBossPatternEntry* CandidatePattern : CandidatePatterns)
	{
		if (CandidatePattern && CandidatePattern->AbilityClass != LastActivatedAbilityClass)
		{
			NonRepeatedCandidates.Add(CandidatePattern);
		}
	}

	return NonRepeatedCandidates.IsEmpty() ? CandidatePatterns : NonRepeatedCandidates;
}

const FBossPatternEntry* UCPP_BossBrainComponent::SelectPatternCandidate(const TArray<const FBossPatternEntry*>& CandidatePatterns, bool bFirstSelection) const
{
	if (CandidatePatterns.IsEmpty())
	{
		return nullptr;
	}

	if (bFirstSelection)
	{
		const int32 SelectedIndex = FMath::RandRange(0, CandidatePatterns.Num() - 1);
		return CandidatePatterns[SelectedIndex];
	}

	float BestCooldownSeconds = -1.0f;
	TArray<const FBossPatternEntry*> BestCandidates;
	for (const FBossPatternEntry* CandidatePattern : CandidatePatterns)
	{
		if (!CandidatePattern || !CandidatePattern->AbilityClass)
		{
			continue;
		}

		const UMyGameplayAbilityBase* PatternAbilityCDO = CandidatePattern->AbilityClass->GetDefaultObject<UMyGameplayAbilityBase>();
		const float CooldownSeconds = PatternAbilityCDO ? PatternAbilityCDO->GetCooldownSeconds() : 0.0f;
		if (CooldownSeconds > BestCooldownSeconds)
		{
			BestCooldownSeconds = CooldownSeconds;
			BestCandidates.Reset();
			BestCandidates.Add(CandidatePattern);
			continue;
		}

		if (FMath::IsNearlyEqual(CooldownSeconds, BestCooldownSeconds))
		{
			BestCandidates.Add(CandidatePattern);
		}
	}

	if (BestCandidates.IsEmpty())
	{
		return nullptr;
	}

	const int32 SelectedIndex = FMath::RandRange(0, BestCandidates.Num() - 1);
	return BestCandidates[SelectedIndex];
}

bool UCPP_BossBrainComponent::TryActivatePatternAbility(TSubclassOf<UMyGameplayAbilityBase> AbilityClass)
{
	if (!TryActivateAbilityByClass(AbilityClass))
	{
		return false;
	}

	// Only attack patterns feed the no-repeat rule; reposition activations go through
	// TryActivateAbilityByClass directly so they never mask an attack repetition.
	LastActivatedAbilityClass = AbilityClass;
	return true;
}

bool UCPP_BossBrainComponent::TryActivateAbilityByClass(TSubclassOf<UMyGameplayAbilityBase> AbilityClass)
{
	UAbilitySystemComponent* ASC = GetBossAbilitySystemComponent();
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (!AbilitySpec.Handle.IsValid() || !AbilitySpec.Ability)
		{
			continue;
		}

		if (AbilitySpec.Ability->GetClass() == AbilityClass)
		{
			return ASC->TryActivateAbility(AbilitySpec.Handle);
		}
	}

	return false;
}

bool UCPP_BossBrainComponent::IsFirstSelectionForPhase(EBossPhase Phase) const
{
	switch (Phase)
	{
	case EBossPhase::Phase1:
		return !bHasSelectedFirstPhase1Pattern;
	case EBossPhase::Phase2:
		return !bHasSelectedFirstPhase2Pattern;
	case EBossPhase::None:
	case EBossPhase::Transition:
	default:
		return false;
	}
}

void UCPP_BossBrainComponent::MarkFirstSelectionDoneForPhase(EBossPhase Phase)
{
	switch (Phase)
	{
	case EBossPhase::Phase1:
		bHasSelectedFirstPhase1Pattern = true;
		break;
	case EBossPhase::Phase2:
		bHasSelectedFirstPhase2Pattern = true;
		break;
	case EBossPhase::None:
	case EBossPhase::Transition:
	default:
		break;
	}
}

void UCPP_BossBrainComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (!bBrainRunning)
	{
		return;
	}

	// While an encounter is pending, sweep gimmick hazards the moment the in-flight pattern ends — a hazard
	// the pattern spawned after the threshold sweep must not linger visibly through the decision delay.
	if (ACPP_BossCharacter* BossOwner = GetBossOwner())
	{
		if (BossOwner->IsPhaseTransitionPending() || BossOwner->IsClearEncounterPending())
		{
			if (UCPP_BossEncounterDirectorComponent* EncounterDirector = BossOwner->GetBossEncounterDirectorComponent())
			{
				EncounterDirector->ClearGimmickHazards();
			}
		}
	}

	// The gap between patterns is when the boss tracks its target: focus now, decide after the delay.
	UpdateDecisionGapFocus();
	ScheduleNextDecision(DecisionDelay);
}

void UCPP_BossBrainComponent::ScheduleNextDecision(float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !bBrainRunning)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(DecisionTimerHandle);
	World->GetTimerManager().SetTimer(
		DecisionTimerHandle,
		this,
		&UCPP_BossBrainComponent::HandleDecisionTimer,
		FMath::Max(Delay, 0.0f),
		false
	);
}

void UCPP_BossBrainComponent::HandleDecisionTimer()
{
	if (!bBrainRunning)
	{
		return;
	}

	if (TrySelectAndActivateNextPattern())
	{
		return;
	}

	ScheduleNextDecision(RetryDelayWhenNoPattern);
}
