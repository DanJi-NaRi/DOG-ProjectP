// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "Enemy/Actors/CPP_EnemyBeamActor.h"
#include "CPP_EnemyBeamAttackAbility.generated.h"

class ACPP_EnemyBase;

////////////////////////////
//! \class UCPP_EnemyBeamAttackAbility
//! \brief 눈빔(MON_PUN_001_PAT_01): 조준(자유 추적) → 정지(발사 직전 연출) → 발사(각속도 캡 추적, 비관통,
//!        대상당 1회 피해)를 WaitDelay 체인으로 구동한다. 실제 회전/트레이스/피해는 ACPP_EnemyBeamActor가 담당.
//! \note  몽타주 없이 타이머로 구동(스펙이 절대 시간 기준). 빔 길이/피해 계수/HitGE는 패턴 데이터(Range/DamageCoefficient/
//!        HitGameplayEffect)를 그대로 쓰고, 타이밍·폭·각속도는 어빌리티 Class Defaults에 둔다.
UCLASS()
class PROJECTP_API UCPP_EnemyBeamAttackAbility : public UMyGameplayAbilityBase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	void FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar);

protected:
	//! \brief 발동 준비(정지·조준·활성 등록)가 끝난 뒤 호출되는 훅. 기본 구현은 즉시 빔 시퀀스를 시작한다.
	//!        선행 동작(예: 순간이동)이 필요한 파생 패턴은 이를 오버라이드해 그 후 BeginBeamSequence를 호출한다.
	virtual void OnBeamActivated();

	//! \brief 빔 액터를 스폰하고 조준→정지→발사 WaitDelay 체인을 시작한다. (파생 클래스가 선행 동작 후 호출)
	void BeginBeamSequence();

	ACPP_EnemyBase* GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const;

	//! \brief 조준선 표시(타겟 자유 추적) 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam", meta = (ClampMin = "0.0"))
	float AimDuration = 0.5f;

	//! \brief 발사 직전 방향을 고정하고 멈추는 연출 시간(초). 0이면 생략.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam", meta = (ClampMin = "0.0"))
	float PreFireLockDuration = 0.1f;

	//! \brief 빔 발사 지속 시간(초). 발사 시점에 방향이 확정되며 이 동안 추적하지 않는다(가디언 레이저 방식).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam", meta = (ClampMin = "0.0"))
	float FireDuration = 0.3f;

	//! \brief 빔 폭 절반(cm). 폭 0.2m면 10.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam", meta = (ClampMin = "0.0"))
	float BeamHalfWidth = 10.0f;

	//! \brief 빔 높이 절반(cm). 히트 판정 세로 범위.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam", meta = (ClampMin = "0.0"))
	float BeamHalfHeight = 50.0f;

	//! \brief 시전자 위치로부터 빔 원점의 Z 오프셋(눈 높이, cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam")
	float BeamOriginHeight = 60.0f;

	//! \brief 빔 액터 클래스. 비우면 기본 ACPP_EnemyBeamActor. 비주얼용 BP 자식을 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam")
	TSubclassOf<ACPP_EnemyBeamActor> BeamActorClass;

	//! \brief 빔 경로 디버그 라인 표시.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Beam")
	bool bDrawDebug = false;

private:
	UFUNCTION()
	void HandleAimFinished();

	UFUNCTION()
	void HandleLockFinished();

	UFUNCTION()
	void HandleFireFinished();

	void FinishBeamAttack();
	void DestroyBeam();

	mutable FGameplayTagContainer PatternCooldownTags;
	FGameplayAbilitySpecHandle ActiveSpecHandle;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	TWeakObjectPtr<ACPP_EnemyBeamActor> ActiveBeam;
};
