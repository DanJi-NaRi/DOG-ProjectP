// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyAIC.h"

#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Core/CPP_EnemyCombatCoordinator.h"
#include "Enemy/Spawning/CPP_EnemyWaveData.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

ACPP_EnemyAIC::ACPP_EnemyAIC(const FObjectInitializer& ObjectInitializer)
	// PathFollowingComponent를 CrowdFollowingComponent로 교체 → DetourCrowd 회피/분리(몰림·빈틈 파고들기).
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));

	BrainComponent = StateTreeAIComponent;
	StateTreeAIComponent->SetStartLogicAutomatically(false);
}

void ACPP_EnemyAIC::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ClearTargetActor();

	if (!HasAuthority())
	{
		return;
	}

	if (ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(InPawn))
	{
		Enemy->UpdateChaseAcceptableRadius();
	}

	// DetourCrowd 분리 설정: 서로 밀어내 뭉침을 풀고 빈 공간으로 흐르게 한다.
	if (UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent()))
	{
		Crowd->SetCrowdSeparation(true);
		Crowd->SetCrowdSeparationWeight(2.0f);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
	}

	if (StateTreeAIComponent)
	{
		StateTreeAIComponent->StartLogic();
	}

	// 지각(Sight) 대신 플레이어를 직접 조회해 타겟을 획득하고, 주기적으로 재평가한다.
	AcquireTarget();
	GetWorldTimerManager().SetTimer(
		TargetEvalTimerHandle, this, &ACPP_EnemyAIC::EvaluateTarget, TargetEvalInterval, true);
}

////////////////////////////
//! \author HanUl
//! \brief 언포제스(사망 등) 시 공격 토큰/타게터 등록을 정리하고 평가 타이머를 정지한다.
//! \param
//! \return
void ACPP_EnemyAIC::OnUnPossess()
{
	if (UEnemyCombatCoordinatorSubsystem* Coordinator = GetCoordinator())
	{
		if (ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(GetPawn()))
		{
			Coordinator->ReleaseAttackToken(Enemy);
			Coordinator->SetEnemyTarget(Enemy, nullptr);
		}
	}

	GetWorldTimerManager().ClearTimer(TargetEvalTimerHandle);

	Super::OnUnPossess();
}

AActor* ACPP_EnemyAIC::GetTargetActor() const
{
	return TargetActor;
}

void ACPP_EnemyAIC::ClearTargetActor()
{
	SetTargetActor(nullptr);
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 플레이어(최대 3명) 중 가장 적게 물린(least-targeted, 동률 시 최근접) 유효 타겟을 획득한다. 타게터 수는 조정자가 O(1)로 제공한다. AIPerception Sight를 대체한다.
//! \param
//! \return
void ACPP_EnemyAIC::AcquireTarget()
{
	if (!HasAuthority())
	{
		return;
	}

	int32 BestCount = 0;
	float BestDistSq = 0.0f;
	AActor* Best = FindBestCandidate(BestCount, BestDistSq);

	if (Best)
	{
		const bool bHadTarget = (TargetActor != nullptr);
		SetTargetActor(Best);

		if (bHadTarget)
		{
			SendTargetChangeEvent();
		}
		else
		{
			SendSeeTargetEvent();
		}

		BP_OnTargetDetected(Best);
	}
	else if (TargetActor)
	{
		AActor* LostTarget = TargetActor;
		ClearTargetActor();
		SendTargetLostEvent();
		BP_OnTargetLost(LostTarget);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 주기 평가. 타겟이 없거나 무효(사망 등)면 재획득, 유효하면 확실히 나은 대안이 있을 때만 히스테리시스로 전환한다.
//! \param
//! \return
void ACPP_EnemyAIC::EvaluateTarget()
{
	if (!HasAuthority())
	{
		return;
	}

	// 타겟 없음/무효(사망 등) → 재획득.
	if (!IsValidTargetActor(TargetActor))
	{
		AcquireTarget();
		return;
	}

	const APawn* Self = GetPawn();
	if (!Self)
	{
		return;
	}

	// 유효 타겟 유지 중: 최적 대안과 히스테리시스 비교로 "확실히 나을 때만" 전환.
	int32 AltCount = 0;
	float AltDistSq = 0.0f;
	AActor* BestAlt = FindBestCandidate(AltCount, AltDistSq);
	if (!BestAlt || BestAlt == TargetActor)
	{
		return;
	}

	const UEnemyCombatCoordinatorSubsystem* Coordinator = GetCoordinator();
	int32 CurCount = Coordinator ? Coordinator->GetTargeterCount(TargetActor) : 0;

	// 현재 타겟의 타게터 수는 나 자신을 포함하므로, 대안(나 미포함)과 대칭 비교하려면 자신을 뺀다.
	CurCount = FMath::Max(0, CurCount - 1);

	const float CurDistSq = FVector::DistSquared(Self->GetActorLocation(), TargetActor->GetActorLocation());

	if (ShouldSwitchTarget(AltCount, AltDistSq, CurCount, CurDistSq))
	{
		SetTargetActor(BestAlt);
		SendTargetChangeEvent();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 유효 플레이어 후보 중 타게터 수 최소→최근접 순으로 최적 후보를 찾는다. (타게터 수는 조정자가 O(1)로 제공)
//! \param OutCount 선택된 후보의 타게터 수
//! \param OutDistSq 선택된 후보까지의 제곱거리
//! \return 최적 후보 플레이어, 없으면 nullptr
AActor* ACPP_EnemyAIC::FindBestCandidate(int32& OutCount, float& OutDistSq) const
{
	const APawn* Self = GetPawn();
	const UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return nullptr;
	}

	const UEnemyCombatCoordinatorSubsystem* Coordinator = GetCoordinator();
	const ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(Self);
	const EEnemySpawnTargetPolicy TargetPolicy = Enemy
		? Enemy->GetSpawnTargetPolicy()
		: EEnemySpawnTargetPolicy::PlayerBalanced;
	const bool bUsesAssignedObjective = TargetPolicy == EEnemySpawnTargetPolicy::DefenseObjective
		|| TargetPolicy == EEnemySpawnTargetPolicy::BreachObjective
		|| TargetPolicy == EEnemySpawnTargetPolicy::RitualObjective;
	if (Enemy && bUsesAssignedObjective)
	{
		AActor* Objective = Enemy->GetAssignedObjectiveTarget();
		if (!IsValidTargetActor(Objective))
		{
			OutCount = 0;
			OutDistSq = 0.0f;
			return nullptr;
		}

		OutCount = Coordinator ? Coordinator->GetTargeterCount(Objective) : 0;
		OutDistSq = FVector::DistSquared(Self->GetActorLocation(), Objective->GetActorLocation());
		return Objective;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	int32 BestCount = TNumericLimits<int32>::Max();
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector SelfLocation = Self->GetActorLocation();

	for (const APlayerState* CandidateState : GameState->PlayerArray)
	{
		APawn* PlayerPawn = CandidateState ? CandidateState->GetPawn() : nullptr;
		if (!IsValidTargetActor(PlayerPawn))
		{
			continue;
		}

		const int32 Count = Coordinator ? Coordinator->GetTargeterCount(PlayerPawn) : 0;
		const float DistSq = FVector::DistSquared(SelfLocation, PlayerPawn->GetActorLocation());

		if (Count < BestCount || (Count == BestCount && DistSq < BestDistSq))
		{
			BestCount = Count;
			BestDistSq = DistSq;
			Best = PlayerPawn;
		}
	}

	OutCount = Best ? BestCount : 0;
	OutDistSq = Best ? BestDistSq : 0.0f;
	return Best;
}

////////////////////////////
//! \author HanUl
//! \brief 대안이 현재 타겟보다 "확실히" 나은지(전환할지) 히스테리시스로 판정한다. 튐 방지.
//! \param AltCount 대안의 타게터 수
//! \param AltDistSq 대안까지의 제곱거리
//! \param CurCount 현재 타겟의 타게터 수(자신 제외)
//! \param CurDistSq 현재 타겟까지의 제곱거리
//! \return 전환해야 하면 true
bool ACPP_EnemyAIC::ShouldSwitchTarget(int32 AltCount, float AltDistSq, int32 CurCount, float CurDistSq) const
{
	// 대안이 확실히 덜 물림 → 부하 재분배 전환.
	if (AltCount + CountSwitchMargin <= CurCount)
	{
		return true;
	}

	// 같거나 덜 물렸고 + 확실히 더 가까움 → 근접 전환. (제곱거리 비교라 비율도 제곱)
	if (AltCount <= CurCount && CurDistSq >= AltDistSq * DistanceSwitchRatio * DistanceSwitchRatio)
	{
		return true;
	}

	return false;
}

void ACPP_EnemyAIC::RefreshTargetFocus()
{
	if (TargetActor)
	{
		SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void ACPP_EnemyAIC::SuspendTargetFocus()
{
	ClearFocus(EAIFocusPriority::Gameplay);
}

void ACPP_EnemyAIC::SendHitEvent()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.Hit"), false));
}

void ACPP_EnemyAIC::SendDeadEvent()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.Dead"), false));
}

////////////////////////////
//! \author HanSeul
//! \brief Sends the StateTree event that immediately redirects combat to a pending special attack.
//! \return None
void ACPP_EnemyAIC::RequestForcedSpecialAttack()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.ForcedSpecialAttack"), false));
}

bool ACPP_EnemyAIC::IsValidTargetActor(AActor* Actor) const
{
	if (!Actor || !UMyAbilitySystemLibrary::IsHostile(GetPawn(), Actor, /*bRequireAlive=*/true))
	{
		return false;
	}

	return Actor->IsA<APawn>();
}

void ACPP_EnemyAIC::SendSeeTargetEvent()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.SeeTarget"), false));
}

void ACPP_EnemyAIC::SendTargetLostEvent()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.TargetLost"), false));
}

void ACPP_EnemyAIC::SendTargetChangeEvent()
{
	SendStateTreeEvent(FGameplayTag::RequestGameplayTag(TEXT("AI.Event.TargetChange"), false));
}

void ACPP_EnemyAIC::SendStateTreeEvent(FGameplayTag EventTag)
{
	if (StateTreeAIComponent && EventTag.IsValid())
	{
		StateTreeAIComponent->SendStateTreeEvent(EventTag);
	}
}

void ACPP_EnemyAIC::SetTargetActor(AActor* NewTarget)
{
	TargetActor = NewTarget;
	RefreshTargetFocus();

	// 타겟 분산 지표(타게터 집계)를 조정자에 반영한다. (AIC는 서버 전용이라 서버에서만 실행됨)
	if (UEnemyCombatCoordinatorSubsystem* Coordinator = GetCoordinator())
	{
		if (ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(GetPawn()))
		{
			Coordinator->SetEnemyTarget(Enemy, NewTarget);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 월드의 적 전투 조정자 서브시스템을 반환한다.
//! \param
//! \return 조정자 서브시스템, 없으면 nullptr
UEnemyCombatCoordinatorSubsystem* ACPP_EnemyAIC::GetCoordinator() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEnemyCombatCoordinatorSubsystem>() : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 타겟으로 접근해 자기 사거리(정지 거리=ChaseAcceptableRadius)에서 멈춘다. DetourCrowd가 접근 중 회피/분리를 처리한다.
//!        목표가 데드존 이상 움직였거나 정지 상태일 때만 재이동해 명령 스팸을 막는다. (StateTree Chasing 태스크가 매 틱 호출)
//! \param
//! \return
void ACPP_EnemyAIC::MoveToTargetInRange()
{
	if (!HasAuthority())
	{
		return;
	}

	const ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(GetPawn());
	if (!Enemy || !IsValid(TargetActor))
	{
		return;
	}

	const FVector GoalLocation = TargetActor->GetActorLocation();
	const float StopDistance = Enemy->GetSpawnTargetPolicy() == EEnemySpawnTargetPolicy::RitualObjective
		? 0.0f
		: Enemy->ChaseAcceptableRadius;

	// 재경로 데드존은 "이동 중"에만 적용(재경로 스팸 방지가 목적).
	// 이동이 끝났는데 이 함수가 불렸다는 건 아직 공격 위치가 아니란 뜻이므로 목표가 그대로여도 재이동한다.
	const bool bMoving = GetMoveStatus() != EPathFollowingStatus::Idle;
	if (bMoving && bLastMoveGoalValid && FVector::DistSquared(LastMoveGoal, GoalLocation) <= RepathDeadzone * RepathDeadzone)
	{
		return;
	}

	MoveToLocation(GoalLocation, StopDistance, /*bStopOnOverlap*/ false, /*bUsePathfinding*/ true,
		/*bProjectDestinationToNavigation*/ true, /*bCanStrafe*/ true);

	LastMoveGoal = GoalLocation;
	bLastMoveGoalValid = true;
}

////////////////////////////
//! \author HanUl
//! \brief 공격 가능 여부. 발동 가능 패턴 + 사거리 안(정지 거리+여유) + 공격 토큰 가용이면 true.
//!        토큰 가용은 여기서 "참고"만 하고, 최종 획득은 공격 커밋 시 ActivatePrimaryEnemyAbility의 TryAcquireAttackToken이 확정한다(경쟁 방지).
//!        토큰이 꽉 찼거나 쿨다운이면 false → Chasing에 머물러 우글대다 자리가 나면 친다. (StateTree Attacking 진입 조건)
//! \param
//! \return 공격 가능하면 true
bool ACPP_EnemyAIC::CanAttackTarget() const
{
	const ACPP_EnemyBase* Enemy = Cast<ACPP_EnemyBase>(GetPawn());
	if (!Enemy || !IsValid(TargetActor))
	{
		return false;
	}

	if (Enemy->GetSpawnTargetPolicy() == EEnemySpawnTargetPolicy::RitualObjective)
	{
		return false;
	}

	// 쿨다운 등으로 발동 가능한 패턴이 없으면 공격 대신 이동.
	const float ReadyAttackRange = Enemy->GetReadyAttackPatternRange();
	if (ReadyAttackRange <= 0.0f)
	{
		return false;
	}

	// 사거리 안? (정지 거리 + 여유. 정지 거리 < 이 값이라 멈추면 반드시 안에 든다 = 경계 깜빡임 방지)
	const float AttackDistance = ReadyAttackRange + AttackRangeTolerance;
	if (FVector::DistSquared2D(Enemy->GetActorLocation(), TargetActor->GetActorLocation()) > AttackDistance * AttackDistance)
	{
		return false;
	}

	// 공격 토큰 가용 여부(참고). 동시 공격자 상한을 넘으면 대기.
	const UEnemyCombatCoordinatorSubsystem* Coordinator = GetCoordinator();
	return !Coordinator || Coordinator->CanAcquireAttackToken(Enemy, TargetActor);
}

////////////////////////////
//! \author HanUl
//! \brief 공격 시작 시 이동을 정지한다. 진행 중 MoveTo를 끊고(공격 중 이동 금지), 데드존 캐시를 비워
//!        공격 후 첫 MoveToTargetInRange가 목표 변화량과 무관하게 반드시 이동을 발행하게 한다.
//! \param
//! \return
void ACPP_EnemyAIC::StopMovementForAttack()
{
	StopMovement();
	bLastMoveGoalValid = false;
}
