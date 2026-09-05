#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/ActorComponent.h"
#include "Boss/Core/CPP_BossTypes.h"
#include "CPP_BossBrainComponent.generated.h"

class AAIController;
class ACPP_BossCharacter;
class UAbilitySystemComponent;
class UCPP_BossAttackAbility;
class UCPP_BossPatternSetData;
class UCPP_BossRepositionAbility;
class UMyGameplayAbilityBase;
struct FBossPatternEntry;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UCPP_BossBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPP_BossBrainComponent();

	UFUNCTION(BlueprintPure, Category = "Boss|Brain")
	UCPP_BossPatternSetData* GetPatternSetData() const;

	UFUNCTION(BlueprintCallable, Category = "Boss|Brain")
	bool StartBrain();

protected:
	ACPP_BossCharacter* GetBossOwner() const;
	UAbilitySystemComponent* GetBossAbilitySystemComponent() const;

private:
	bool TrySelectAndActivateNextPattern();
	bool CanMakeDecision() const;
	bool TryBeginPendingPhaseTransition();
	bool TryBeginPendingClearEncounterStaging();
	void StopBrainDecisionLoop();
	bool TryActivateRepositionPattern(ACPP_BossCharacter* BossOwner);
	AAIController* GetBossAIController() const;
	void UpdateDecisionGapFocus();
	void ClearBossFocus();
	TArray<const FBossPatternEntry*> BuildAvailablePatternCandidates(const TArray<FBossPatternEntry>& PhasePatterns, bool bFirstSelection) const;
	const FBossPatternEntry* SelectPatternCandidate(const TArray<const FBossPatternEntry*>& CandidatePatterns, bool bFirstSelection) const;
	bool TryActivatePatternAbility(TSubclassOf<UMyGameplayAbilityBase> AbilityClass);
	bool TryActivateAbilityByClass(TSubclassOf<UMyGameplayAbilityBase> AbilityClass);
	bool IsFirstSelectionForPhase(EBossPhase Phase) const;
	void MarkFirstSelectionDoneForPhase(EBossPhase Phase);
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void ScheduleNextDecision(float Delay);
	void HandleDecisionTimer();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Brain", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCPP_BossPatternSetData> PatternSetData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Brain", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DecisionDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Brain", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RetryDelayWhenNoPattern = 0.5f;

	//! \brief 패턴 사이 거리 정리에 쓰는 이동 패턴. 미설정이면 이동 판단을 건너뛴다.
	//!        발동 조건(거리 대역)은 이 클래스 CDO에서 읽는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Brain", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCPP_BossRepositionAbility> RepositionAbilityClass;

	//! \brief 전진 스텝의 최대 연속 횟수(추격용).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Brain", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxConsecutiveAdvances = 3;

	bool bBrainRunning = false;
	bool bAbilityEndedDelegateBound = false;
	//! \brief 연속 전진 카운터. 공격 패턴이 발동되면 0으로 리셋한다.
	int32 ConsecutiveAdvanceCount = 0;
	bool bHasSelectedFirstPhase1Pattern = false;
	bool bHasSelectedFirstPhase2Pattern = false;
	TSubclassOf<UMyGameplayAbilityBase> LastActivatedAbilityClass;
	FTimerHandle DecisionTimerHandle;
};
