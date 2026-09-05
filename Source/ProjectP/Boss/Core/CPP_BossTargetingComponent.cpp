#include "CPP_BossTargetingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"

UCPP_BossTargetingComponent::UCPP_BossTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AActor* UCPP_BossTargetingComponent::GetCurrentTarget() const
{
	AActor* Target = CurrentTarget.Get();
	return UMyAbilitySystemLibrary::IsLivingPawn(Target) ? Target : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 패턴 경계에서 타겟을 재선정한다. 생존 플레이어 전원에 대해
//!        어그로 점수 = 딜 지분(0~1) × DamageWeight + 근접도(0~1) × ProximityWeight 를 계산하고
//!        최고점을 고른다. 딜 기록이 없으면 근접도만 남아 최근접이 자연히 선정된다.
//!        현재 타겟은 도전자 점수가 교체 마진(TargetSwitchScoreMargin)을 넘을 때만 바뀐다.
//! \return 선정된 타겟, 생존 플레이어가 없으면 nullptr.
AActor* UCPP_BossTargetingComponent::ReevaluateTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return nullptr;
	}

	PruneExpiredRecords();

	TMap<AActor*, float> DamageByInstigator;
	float TotalDamage = 0.0f;
	for (const FBossThreatRecord& ThreatRecord : ThreatRecords)
	{
		AActor* InstigatorPawn = ThreatRecord.InstigatorPawn.Get();
		if (InstigatorPawn && UMyAbilitySystemLibrary::IsLivingPawn(InstigatorPawn))
		{
			DamageByInstigator.FindOrAdd(InstigatorPawn) += ThreatRecord.Damage;
			TotalDamage += ThreatRecord.Damage;
		}
	}

	TArray<AActor*> LivingPlayers;
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(Owner, LivingPlayers);

	AActor* CurrentTargetActor = GetCurrentTarget();
	AActor* BestCandidate = nullptr;
	float BestScore = -1.0f;
	float CurrentTargetScore = -1.0f;

	for (AActor* PlayerPawn : LivingPlayers)
	{
		if (!PlayerPawn)
		{
			continue;
		}

		const float DamageShare = TotalDamage > 0.0f ? DamageByInstigator.FindRef(PlayerPawn) / TotalDamage : 0.0f;
		const float Distance = FVector::Dist2D(Owner->GetActorLocation(), PlayerPawn->GetActorLocation());
		const float Proximity = 1.0f - FMath::Clamp(Distance / FMath::Max(MaxScoringDistance, 1.0f), 0.0f, 1.0f);
		const float AggroScore = DamageShare * DamageWeight + Proximity * ProximityWeight;

		if (PlayerPawn == CurrentTargetActor)
		{
			CurrentTargetScore = AggroScore;
		}

		if (AggroScore > BestScore)
		{
			BestScore = AggroScore;
			BestCandidate = PlayerPawn;
		}
	}

	// Hysteresis: a challenger must clearly beat the current target, or the boss flip-flops
	// whenever two scores hover near each other.
	if (CurrentTargetActor && BestCandidate != CurrentTargetActor && CurrentTargetScore >= 0.0f
		&& BestScore < CurrentTargetScore * TargetSwitchScoreMargin)
	{
		BestCandidate = CurrentTargetActor;
	}

	CurrentTarget = BestCandidate;
	return BestCandidate;
}

////////////////////////////
//! \author HanUl
//! \brief 피해 실행 컨텍스트에서 가해자 폰을 추출해 위협 기록에 추가한다. 적대 진영(플레이어)의
//!        피해만 집계한다(기믹/자해 제외). 서버에서만 동작한다.
//! \param EffectContext 피해 GameplayEffect의 컨텍스트 핸들(가해자 추출용).
//! \param DamageAmount 실드 흡수분을 포함한 총 피해량.
void UCPP_BossTargetingComponent::RecordThreatDamage(const FGameplayEffectContextHandle& EffectContext, float DamageAmount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || DamageAmount <= 0.0f)
	{
		return;
	}

	APawn* InstigatorPawn = ResolveInstigatorPawn(EffectContext);
	if (!InstigatorPawn || !UMyAbilitySystemLibrary::IsHostile(Owner, InstigatorPawn, false))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PruneExpiredRecords();

	FBossThreatRecord ThreatRecord;
	ThreatRecord.InstigatorPawn = InstigatorPawn;
	ThreatRecord.Damage = DamageAmount;
	ThreatRecord.RecordTime = World->GetTimeSeconds();
	ThreatRecords.Add(ThreatRecord);
}

////////////////////////////
//! \author HanUl
//! \brief GameplayEffect 컨텍스트의 Instigator/EffectCauser 후보에서 실제 폰을 찾아낸다.
//!        Controller나 PlayerState가 기록된 경우 소유 폰으로 풀어준다.
//! \param EffectContext 피해 GameplayEffect의 컨텍스트 핸들.
//! \return 가해자 폰, 찾지 못하면 nullptr.
APawn* UCPP_BossTargetingComponent::ResolveInstigatorPawn(const FGameplayEffectContextHandle& EffectContext)
{
	AActor* Candidates[] = {
		EffectContext.GetOriginalInstigator(),
		EffectContext.GetInstigator(),
		EffectContext.GetEffectCauser()
	};

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		if (APawn* CandidatePawn = Cast<APawn>(Candidate))
		{
			return CandidatePawn;
		}

		if (const AController* CandidateController = Cast<AController>(Candidate))
		{
			if (APawn* ControlledPawn = CandidateController->GetPawn())
			{
				return ControlledPawn;
			}
		}

		if (const APlayerState* CandidatePlayerState = Cast<APlayerState>(Candidate))
		{
			if (APawn* StatePawn = CandidatePlayerState->GetPawn())
			{
				return StatePawn;
			}
		}
	}

	return nullptr;
}

void UCPP_BossTargetingComponent::PruneExpiredRecords()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double ExpireTime = World->GetTimeSeconds() - RecentDamageWindowSeconds;
	ThreatRecords.RemoveAll([ExpireTime](const FBossThreatRecord& ThreatRecord)
	{
		return ThreatRecord.RecordTime < ExpireTime || !ThreatRecord.InstigatorPawn.IsValid();
	});
}
