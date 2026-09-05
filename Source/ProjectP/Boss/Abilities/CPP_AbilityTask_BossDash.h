#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "CPP_AbilityTask_BossDash.generated.h"

class ACharacter;

//! \brief 돌진 종료 시 브로드캐스트. HitPawns는 비관통 정지 지점에서 캡슐에 맞은 폰들(없으면 벽 충돌/완주).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossDashFinishedSignature, const TArray<AActor*>&, HitPawns);

////////////////////////////
//! \class UCPP_AbilityTask_BossDash
//! \brief 보스를 지정 방향으로 캡슐 sweep하며 전진시키는 AbilityTask. 벽/플레이어 충돌 시 비관통 정지하고 맞은 폰을 보고한다.
//! \note 서버 권한 어빌리티에서만 사용(보스는 클라 예측이 없어 서버 SetActorLocation이 부드럽게 복제됨). 돌진 중 CMC는 MOVE_None으로 두어 위치 충돌을 막는다.
//!       일반 적 돌진(UCPP_AbilityTask_EnemyDash)이 상속해 이동/정리 골격을 재사용한다 — 멤버가 protected인 이유.
UCLASS()
class PROJECTP_API UCPP_AbilityTask_BossDash : public UAbilityTask
{
	GENERATED_BODY()

public:
	UCPP_AbilityTask_BossDash(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FBossDashFinishedSignature OnDashFinished;

	//! \brief 돌진 태스크 생성. 방향은 정규화되며, 거리/속도/캡슐 치수로 매 tick 전진한다.
	static UCPP_AbilityTask_BossDash* BossDash(
		UGameplayAbility* OwningAbility,
		ACharacter* InDashCharacter,
		FVector InDashDirection,
		float InDashDistance,
		float InDashSpeed,
		float InCapsuleRadius,
		float InCapsuleHalfHeight
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

protected:
	virtual void FinishDash(const TArray<AActor*>& HitPawns);

	TWeakObjectPtr<ACharacter> DashCharacter;
	FVector DashDirection = FVector::ForwardVector;
	float RemainingDistance = 0.0f;
	float DashSpeed = 0.0f;
	float CapsuleRadius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	bool bDashFinished = false;
	bool bMovementModeOverridden = false;
};
