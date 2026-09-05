// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "CPP_AbilityTask_EnemyBlink.generated.h"

class ACPP_EnemyBase;
struct FActiveGameplayEffectsContainer;
struct FGameplayEffectSpec;

//! \brief 재등장 성공 시 브로드캐스트. 무적 창은 태스크가 내부적으로 유지하다 스스로 종료한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyBlinkFinishedSignature);

////////////////////////////
//! \class UCPP_AbilityTask_EnemyBlink
//! \brief 적 순간이동 부품. 소멸(숨김+콜리전 off, 이동 정지) → VanishDuration 후 타겟 후방으로 재등장(캡슐 간격 +
//!        내비 투영 + TeleportTo 비겹침 보정) → InvincibleDuration 무적(ASC ApplicationQuery로 외부 GE 거부).
//!        재등장 시 OnBlinkFinished를 쏘아 소유 어빌리티가 후속 동작(자폭/눈빔 등)을 이어가게 한다.
//! \note  점멸 자폭·점멸 눈빔 등이 공유하는 단일 소스. 무적/타이머 수명은 태스크가 관리하며, 어빌리티가 먼저 끝나면
//!        OnDestroy에서 무적 해제·소멸 상태 복구까지 처리한다.
UCLASS()
class PROJECTP_API UCPP_AbilityTask_EnemyBlink : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEnemyBlinkFinishedSignature OnBlinkFinished;

	//! \brief 순간이동 태스크 생성. 수치·연출 큐를 받아 소멸→재등장→무적 시퀀스를 구동한다.
	static UCPP_AbilityTask_EnemyBlink* EnemyBlink(
		UGameplayAbility* OwningAbility,
		ACPP_EnemyBase* InEnemy,
		float InVanishDuration,
		float InReappearBehindGap,
		float InInvincibleDuration,
		FGameplayTag InVanishCueTag,
		FGameplayTag InReappearCueTag
	);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleVanishElapsed();
	void HandleInvincibleElapsed();
	FVector ComputeReappearLocation(FRotator& OutFaceRotation) const;
	bool ShouldAllowGameplayEffectApplication(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& SpecToApply) const;
	void RegisterInvincibilityQuery();
	void UnregisterInvincibilityQuery();
	void ExecuteCosmeticCue(const FGameplayTag& CueTag) const;
	void RestoreMovementMode() const;
	void ClearTimers();

	TWeakObjectPtr<ACPP_EnemyBase> Enemy;
	float VanishDuration = 0.8f;
	float ReappearBehindGap = 50.0f;
	float InvincibleDuration = 0.15f;
	FGameplayTag VanishCueTag;
	FGameplayTag ReappearCueTag;

	FTimerHandle VanishTimerHandle;
	FTimerHandle InvincibleTimerHandle;
	bool bInvincibleWindowActive = false;
	bool bVanishStateApplied = false;
};
