////////////////////////////
//! \file MyGameplayAbility_ComboAttackBase.h
//! \brief SkillDefinition 콤보 데이터 기반 다단 기본 공격 GameplayAbility 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "MyGameplayAbility_SkillBase.h"
#include "MyGameplayAbility_ComboAttackBase.generated.h"

class UAbilityTask_WaitInputPress;
class UAnimInstance;
class UAnimMontage;

////////////////////////////
//! \class UMyGameplayAbility_ComboAttackBase
//! \author HanUl
//! \brief SkillDefinition의 ComboSpec.Steps를 따라 다단 공격의 입력 버퍼/체인/리셋을 처리하는 기반 클래스다.
//!        한 번의 Activation이 하나의 스윙 체인을 담당한다. 스윙 중 SaveCombo Notify 이후 들어온 입력은
//!        버퍼되어 EndAttack Notify 시점에 같은 Activation 안에서 다음 타 섹션으로 체인되고,
//!        버퍼가 없으면 즉시 종료한다. 마지막 타 인덱스와 스윙 종료 시각은 인스턴스에 보존되어
//!        (InstancedPerActor) 다음 Activation에서 ResetTime 기준 콤보 계속/1타 복귀를 판정한다.
//!        파생 클래스는 OnComboStepFire에서 실제 공격(투사체/근접 판정)만 구현한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyGameplayAbility_ComboAttackBase : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGameplayAbility_ComboAttackBase();

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

	virtual bool CheckCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const override;

protected:
	//! \brief 다음 콤보 입력 저장을 허용하는 시점의 Notify 이름. 섹션에 없으면 스윙 전체가 버퍼 구간이 된다.
	static const FName ComboSaveInputNotifyName;

	//! \brief 파생 훅: Fire(Shoot) 시점의 실제 공격 수행. 서버/클라 모두 호출되므로 판정·스폰은 IsNetAuthority 가드 필요.
	virtual void OnComboStepFire(int32 StepIndex, const FMySkillDataEntry& SkillData);

	//! \brief 파생 훅: 콤보 스텝 섹션 재생 직전에 호출된다(조준 방향 캐시 등).
	virtual void OnComboStepStarted(int32 StepIndex, const FMySkillDataEntry& SkillData);

	bool ApplyComboHitToTarget(AActor* TargetActor, int32 StepIndex) const;

	int32 GetActiveComboStepIndex() const;
	int32 GetComboStepCount() const;
	const FMySkillComboStepSpec* GetComboStepSpec(int32 StepIndex) const;
	const FGameplayEventData* GetComboTriggerEventData() const;

	//! \brief 넉백 거리(cm)를 LaunchCharacter 속도(cm/s)로 바꾸는 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (ClampMin = "0.0"))
	float KnockbackVelocityPerDistance = 6.0f;

	//! \brief 체인 없이 스윙이 끝날 때 몽타주 블렌드 아웃 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (ClampMin = "0.0"))
	float ComboEndMontageBlendOutTime = 0.15f;

private:
	// 흐름
	int32 ResolveActivationStepIndex(float CurrentTime) const;
	bool ValidateComboData(const FMySkillDataEntry& SkillData) const;
	void StartComboStep(int32 StepIndex);
	void HandleComboStepFire();
	void HandleComboStepSaveWindowOpen();
	void HandleComboStepEnd();
	void FinishComboChain(bool bWasCancelled);
	void ResetComboSwingState();
	void ApplyComboKnockback(AActor* TargetActor, float KnockbackDistance) const;
	void StartComboStepForwardMove(const FMySkillComboStepSpec& Step);

	// 입력
	void ArmComboInputTask();
	UFUNCTION()
	void OnComboInputPressed(float TimeWaited);

	// 몽타주/Notify
	UAnimMontage* GetComboMontage() const;
	bool IsComboMontagePlaying() const;
	void PlayComboSection(FName SectionName);
	void BindComboMontageNotify();
	void UnbindComboMontageNotify();
	UFUNCTION()
	void OnComboMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
	bool IsNotifyFromActiveComboStep(const FBranchingPointNotifyPayload& BranchingPointPayload) const;
	void ScheduleComboStepNotifyFallbacks();
	void ScheduleFallbackStepTimers(const FMySkillComboStepSpec& Step);
	void ClearComboStepTimers();
	void OnFireNotifyFallback(uint32 ExpectedStepSerial);
	void OnSaveInputNotifyFallback(uint32 ExpectedStepSerial);
	void OnEndAttackNotifyFallback(uint32 ExpectedStepSerial);
	bool TryGetComboSectionTimeRange(int32 StepIndex, float& OutStartTime, float& OutEndTime) const;
	bool TryGetComboNotifyTimeInSection(FName NotifyName, int32 StepIndex, float& OutRelativeTime) const;

	// 스윙 체인 진행 상태(Activation 단위)
	UPROPERTY(Transient)
	int32 ActiveStepIndex = INDEX_NONE;

	UPROPERTY(Transient)
	bool bComboChainActive = false;

	UPROPERTY(Transient)
	bool bFireHandledThisStep = false;

	UPROPERTY(Transient)
	bool bSaveHandledThisStep = false;

	UPROPERTY(Transient)
	bool bEndHandledThisStep = false;

	UPROPERTY(Transient)
	bool bSaveInputWindowOpen = false;

	UPROPERTY(Transient)
	bool bBufferedNextInput = false;

	UPROPERTY(Transient)
	float CurrentComboPlayRate = 1.0f;

	//! \brief 이번 체인이 몽타주 기반으로 진행 중인지. false면 스텝 데이터 타이머로 진행한다(애니메이션 미적용 단계)
	UPROPERTY(Transient)
	bool bUsingComboMontage = false;

	//! \brief 지연된 Notify fallback이 다른 스텝 상태를 변경하지 못하게 하는 스텝 식별자
	uint32 ActiveStepSerial = 0;

	// 콤보 기억(Activation 간 유지 - InstancedPerActor)
	UPROPERTY(Transient)
	int32 LastCompletedStepIndex = INDEX_NONE;

	UPROPERTY(Transient)
	float LastSwingEndTime = -1000.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputPress> ComboInputTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> ComboBoundAnimInstance;

	FGameplayEventData ComboTriggerEventData;
	bool bHasComboTriggerEventData = false;

	FTimerHandle FireNotifyTimerHandle;
	FTimerHandle SaveInputNotifyTimerHandle;
	FTimerHandle EndAttackNotifyTimerHandle;
};
