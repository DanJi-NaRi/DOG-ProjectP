// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "CPP_EnemyAIC.generated.h"

class UStateTreeAIComponent;
class UEnemyCombatCoordinatorSubsystem;
class ACPP_EnemyBase;

UCLASS()
class PROJECTP_API ACPP_EnemyAIC : public AAIController
{
	GENERATED_BODY()

public:
	ACPP_EnemyAIC(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	AActor* GetTargetActor() const;

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void ClearTargetActor();

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void RefreshTargetFocus();

	// StateTree Chasing 태스크가 매 틱 호출. 타겟으로 접근해 자기 사거리(정지 거리)에서 멈춘다. DetourCrowd가 접근 중 회피/분리.
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI|Combat")
	void MoveToTargetInRange();

	// StateTree Attacking 진입 조건. 사거리 안 + 발동 가능 패턴 + 공격 토큰 가용이면 true.
	// 토큰이 꽉 찼거나 쿨다운이면 false → Chasing에 머물러 우글대다 자리가 나면 친다.
	UFUNCTION(BlueprintPure, Category = "Enemy|AI|Combat")
	bool CanAttackTarget() const;

	// 공격 시작 시 호출. 진행 중 이동을 끊어 공격 중 정지를 보장하고, 재경로 캐시를 비워 공격 후 첫 이동이 반드시 발행되게 한다.
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI|Combat")
	void StopMovementForAttack();

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void SuspendTargetFocus();

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI|Event")
	void SendHitEvent();

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI|Event")
	void SendDeadEvent();

	UFUNCTION(BlueprintCallable, Category = "Enemy|AI|Event")
	void RequestForcedSpecialAttack();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<AActor> TargetActor;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	// 타겟 재획득/스위칭 평가 주기(초).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI|Target", meta = (ClampMin = "0.05"))
	float TargetEvalInterval = 0.75f;

	// 스위칭 히스테리시스: 대안이 현재보다 타게터 수가 이만큼 이상 적어야 부하 재분배 전환.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI|Target", meta = (ClampMin = "1"))
	int32 CountSwitchMargin = 2;

	// 스위칭 히스테리시스: 같거나 덜 물린 대안이 이 배율만큼 더 가까워야 근접 전환.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI|Target", meta = (ClampMin = "1.0"))
	float DistanceSwitchRatio = 1.3f;

	// 공격 판정 여유. 공격은 정지 거리(사거리−버퍼)에서 이만큼 바깥까지 허용해 경계 깜빡임을 막는다.
	// 정지 거리 < 공격 거리여야 멈춘 뒤 반드시 공격 범위 안. 버퍼(60) 이하로 둬야 실사거리를 안 넘어 헛방이 없다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "0.0"))
	float AttackRangeTolerance = 50.0f;

	// 목표(타겟)가 이 거리 이상 움직였을 때만 재경로(미세 이동엔 재경로 안 함, 이동 명령 스팸 방지).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|AI|Combat", meta = (ClampMin = "0.0"))
	float RepathDeadzone = 100.0f;

	FTimerHandle TargetEvalTimerHandle;

	FVector LastMoveGoal = FVector::ZeroVector;
	bool bLastMoveGoalValid = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|AI")
	void BP_OnTargetDetected(AActor* NewTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|AI")
	void BP_OnTargetLost(AActor* LostTarget);

private:
	bool IsValidTargetActor(AActor* Actor) const;
	void AcquireTarget();
	void EvaluateTarget();
	// 유효 플레이어 후보 중 타게터 수 최소→거리순으로 최적 후보를 찾는다. 타게터 수/제곱거리를 out으로 반환.
	AActor* FindBestCandidate(int32& OutCount, float& OutDistSq) const;
	// 히스테리시스: 대안이 현재 타겟보다 "확실히" 나을 때만 true.
	bool ShouldSwitchTarget(int32 AltCount, float AltDistSq, int32 CurCount, float CurDistSq) const;
	UEnemyCombatCoordinatorSubsystem* GetCoordinator() const;
	void SendSeeTargetEvent();
	void SendTargetLostEvent();
	void SendTargetChangeEvent();
	void SendStateTreeEvent(FGameplayTag EventTag);
	void SetTargetActor(AActor* NewTarget);
};
