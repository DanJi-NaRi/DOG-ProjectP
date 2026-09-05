////////////////////////////
//! \file MyGameplayAbility_ComboAttackBase.cpp
//! \brief SkillDefinition 콤보 데이터 기반 다단 기본 공격 GameplayAbility 기반 클래스 구현 파일이다.

#include "MyGameplayAbility_ComboAttackBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyAbilitySystemLibrary.h"
#include "MyAttributeSet.h"
#include "SkillData/MySkillDefinitionDataAsset.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"
#include "TimerManager.h"

const FName UMyGameplayAbility_ComboAttackBase::ComboSaveInputNotifyName = TEXT("SaveCombo");

namespace
{
	constexpr float MinComboMontagePlayRate = 0.01f;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 공격 Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGameplayAbility_ComboAttackBase::UMyGameplayAbility_ComboAttackBase()
{
	// 콤보 단계와 리셋 판정을 서버 시계 기준으로 일원화한다
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 스윙 체인을 시작한다. 발동 시점 콤보 기억으로 시작 타를 결정한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ResetComboSwingState();

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	const FMySkillDataEntry* SkillData = FindSkillDataEntryFromActorInfo(ActorInfo);
	if (!SkillData || !ValidateComboData(*SkillData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ShouldBlockStandardSkillWhileFalling())
	{
		const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor);
		const UCharacterMovementComponent* MovementComponent = AvatarCharacter ? AvatarCharacter->GetCharacterMovement() : nullptr;
		if (MovementComponent && MovementComponent->IsFalling())
		{
			UE_LOG(LogTemp, Log, TEXT("MyGAS combo attack blocked while jumping/falling - Ability: %s, Avatar: %s"),
				*GetNameSafe(this),
				*GetNameSafe(AvatarActor));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (TriggerEventData)
	{
		ComboTriggerEventData = *TriggerEventData;
		bHasComboTriggerEventData = true;
	}

	ApplySkillInputBlock(ActorInfo);
	ApplyMoveInputBlockFromSkillData(CachedSkillDataEntry, ActorInfo);

	if (ActorInfo->IsNetAuthority())
	{
		const FGameplayTag StreamingSkillTag = CachedSkillDataEntry.AbilityTag.IsValid()
			? CachedSkillDataEntry.AbilityTag
			: AbilityTag;
		UMyStreamingCombatMessageLibrary::BroadcastSkillUsed(AvatarActor, AvatarActor, StreamingSkillTag);
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	const int32 StartStepIndex = ResolveActivationStepIndex(CurrentTime);

	bComboChainActive = true;
	ArmComboInputTask();
	StartComboStep(StartStepIndex);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 콤보 스윙 상태를 정리한다. 체인 정상 종료를 거치지 않은 종료는 콤보 기억을 초기화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// FinishComboChain을 거치지 않은 종료(외부 캔슬, 발동 실패 등)는 다음 발동을 1타로 되돌린다
	if (bComboChainActive)
	{
		LastCompletedStepIndex = INDEX_NONE;
	}

	ClearComboStepTimers();
	UnbindComboMontageNotify();
	ResetComboSwingState();
	ComboInputTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 공격은 몽타주 간격으로 페이스를 제어하므로 GAS 쿨다운 검사를 통과시킨다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param OptionalRelevantTags 실패 관련 태그
//! \return 항상 true
bool UMyGameplayAbility_ComboAttackBase::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 공격은 GAS 쿨다운 GameplayEffect를 적용하지 않는다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
}

////////////////////////////
//! \author HanUl
//! \brief Fire 시점에 파생 Ability의 실제 공격(투사체/근접 판정)을 실행한다.
//! \param StepIndex 현재 콤보 스텝 인덱스(0부터)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnComboStepFire(int32 StepIndex, const FMySkillDataEntry& SkillData)
{
	(void)StepIndex;
	(void)SkillData;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 스텝 섹션 재생 직전에 파생 Ability가 상태를 준비할 수 있도록 호출된다.
//! \param StepIndex 시작하는 콤보 스텝 인덱스(0부터)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnComboStepStarted(int32 StepIndex, const FMySkillDataEntry& SkillData)
{
	(void)StepIndex;
	(void)SkillData;
}

////////////////////////////
//! \author HanUl
//! \brief 스텝 계수 피해와 (플래그 시) 상태 GameplayEffect, 넉백을 대상에게 적용한다. 서버 전용.
//! \param TargetActor 타격 대상 Actor
//! \param StepIndex 적용할 콤보 스텝 인덱스(0부터)
//! \return 피해 적용 요청에 성공하면 true
bool UMyGameplayAbility_ComboAttackBase::ApplyComboHitToTarget(AActor* TargetActor, int32 StepIndex) const
{
	if (!TargetActor || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}

	const FMySkillComboStepSpec* Step = GetComboStepSpec(StepIndex);
	if (!Step)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return false;
	}

	const bool bDamageApplied = UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
		SourceASC,
		TargetActor,
		CachedSkillDataEntry.Effects.HitGameplayEffect,
		Step->DamageCoefficient,
		CachedSkillDataEntry.CooldownTag,
		CachedSkillDataEntry.InputTag
	);

	// 상태 GE(예: 마지막 타 표식)는 살아있는 대상에게만 부여한다. 스택/지속 규칙은 상태 GE 자체가 관리한다.
	if (Step->bApplyStatusEffect && CachedSkillDataEntry.Effects.StatusGameplayEffect)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		if (TargetASC && TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) > 0.0f)
		{
			FGameplayEffectContextHandle StatusContext = SourceASC->MakeEffectContext();
			StatusContext.AddSourceObject(CurrentActorInfo->AvatarActor.Get());
			FGameplayEffectSpecHandle StatusSpec = SourceASC->MakeOutgoingSpec(CachedSkillDataEntry.Effects.StatusGameplayEffect, 1.0f, StatusContext);
			if (StatusSpec.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpec.Data.Get(), TargetASC);
			}
		}
	}

	if (Step->KnockbackDistance > 0.0f)
	{
		ApplyComboKnockback(TargetActor, Step->KnockbackDistance);
	}

	return bDamageApplied;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 진행 중인 콤보 스텝 인덱스를 반환한다.
//! \param 없음
//! \return 진행 중 스텝 인덱스, 스윙 중이 아니면 INDEX_NONE
int32 UMyGameplayAbility_ComboAttackBase::GetActiveComboStepIndex() const
{
	return ActiveStepIndex;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 스텝 개수를 반환한다.
//! \param 없음
//! \return SkillDefinition에 정의된 콤보 스텝 수
int32 UMyGameplayAbility_ComboAttackBase::GetComboStepCount() const
{
	return CachedSkillDataEntry.Combo.Steps.Num();
}

////////////////////////////
//! \author HanUl
//! \brief 지정 인덱스의 콤보 스텝 정의를 반환한다.
//! \param StepIndex 조회할 스텝 인덱스(0부터)
//! \return 스텝 정의, 범위를 벗어나면 nullptr
const FMySkillComboStepSpec* UMyGameplayAbility_ComboAttackBase::GetComboStepSpec(int32 StepIndex) const
{
	return CachedSkillDataEntry.Combo.Steps.IsValidIndex(StepIndex)
		? &CachedSkillDataEntry.Combo.Steps[StepIndex]
		: nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 이번 체인의 발동 시점 GameplayEvent 데이터를 반환한다.
//! \param 없음
//! \return 발동 이벤트 데이터, 없으면 nullptr
const FGameplayEventData* UMyGameplayAbility_ComboAttackBase::GetComboTriggerEventData() const
{
	return bHasComboTriggerEventData ? &ComboTriggerEventData : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 기억과 ResetTime으로 이번 발동의 시작 스텝을 결정한다.
//! \param CurrentTime 현재 월드 시간
//! \return 시작할 콤보 스텝 인덱스(0부터)
int32 UMyGameplayAbility_ComboAttackBase::ResolveActivationStepIndex(float CurrentTime) const
{
	const FMySkillComboSpec& Combo = CachedSkillDataEntry.Combo;
	if (LastCompletedStepIndex == INDEX_NONE || Combo.ResetTime <= 0.0f)
	{
		return 0;
	}

	if (CurrentTime - LastSwingEndTime > Combo.ResetTime)
	{
		return 0;
	}

	const int32 NextStepIndex = LastCompletedStepIndex + 1;
	return Combo.Steps.IsValidIndex(NextStepIndex) ? NextStepIndex : 0;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 실행에 필요한 몽타주와 스텝 섹션 구성을 검증한다.
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 콤보 실행 조건을 만족하면 true
bool UMyGameplayAbility_ComboAttackBase::ValidateComboData(const FMySkillDataEntry& SkillData) const
{
	if (SkillData.Combo.Steps.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS combo attack activation failed - Combo.Steps is empty. Ability: %s, SkillId: %s"),
			*GetNameSafe(this),
			*SkillData.SkillId.ToString());
		return false;
	}

	// 몽타주가 아직 없으면(애니메이션 미적용 단계) 스텝 데이터 타이머 폴백으로 진행하므로 섹션 검증을 건너뛴다
	UAnimMontage* Montage = GetComboMontage();
	if (!Montage)
	{
		UE_LOG(LogTemp, Log, TEXT("MyGAS combo attack running without montage - using fallback step timers. Ability: %s, SkillId: %s"),
			*GetNameSafe(this),
			*SkillData.SkillId.ToString());
		return true;
	}

	for (int32 StepIndex = 0; StepIndex < SkillData.Combo.Steps.Num(); ++StepIndex)
	{
		const FName SectionName = SkillData.Combo.Steps[StepIndex].MontageSectionName;
		if (SectionName.IsNone() || Montage->GetSectionIndex(SectionName) == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyGAS combo attack activation failed - montage section missing. Ability: %s, SkillId: %s, StepIndex: %d, Section: %s, Montage: %s"),
				*GetNameSafe(this),
				*SkillData.SkillId.ToString(),
				StepIndex,
				*SectionName.ToString(),
				*GetNameSafe(Montage));
			return false;
		}
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 지정 스텝의 몽타주 섹션을 재생하고 Notify 대체 타이머를 등록한다.
//! \param StepIndex 시작할 콤보 스텝 인덱스(0부터)
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::StartComboStep(int32 StepIndex)
{
	const FMySkillComboStepSpec* Step = GetComboStepSpec(StepIndex);
	if (!Step)
	{
		FinishComboChain(true);
		return;
	}

	ActiveStepIndex = StepIndex;
	++ActiveStepSerial;
	bFireHandledThisStep = false;
	bSaveHandledThisStep = false;
	bEndHandledThisStep = false;
	bBufferedNextInput = false;
	bUsingComboMontage = GetComboMontage() != nullptr;
	ClearComboStepTimers();

	// SaveCombo Notify가 섹션에 없거나 몽타주 자체가 없으면 스윙 전체를 입력 버퍼 구간으로 취급한다
	float IgnoredRelativeTime = 0.0f;
	bSaveInputWindowOpen = !bUsingComboMontage
		|| !TryGetComboNotifyTimeInSection(ComboSaveInputNotifyName, StepIndex, IgnoredRelativeTime);

	OnComboStepStarted(StepIndex, CachedSkillDataEntry);

	// 스텝 시작마다 재계산해 체인 중 공격 속도 변화를 반영한다
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const float BasePlayRate = SkillDefinition ? SkillDefinition->GetAnimation().PlayRate : 1.0f;
	CurrentComboPlayRate = FMath::Max(MinComboMontagePlayRate, BasePlayRate) * GetAttackSpeedMultiplier(GetCurrentActorInfo());

	// 데이터에 전진 이동이 설정된 스텝이면 RootMotion으로 전진시킨다(몽타주/폴백 모드 공통)
	StartComboStepForwardMove(*Step);

	// 몽타주가 없으면(애니메이션 미적용 단계) 스텝 데이터 타이머로 Fire/종료를 진행한다
	if (!bUsingComboMontage)
	{
		ScheduleFallbackStepTimers(*Step);
		return;
	}

	BindComboMontageNotify();
	PlayComboSection(Step->MontageSectionName);

	if (bComboChainActive)
	{
		ScheduleComboStepNotifyFallbacks();
	}
}

////////////////////////////
//! \author HanUl
//! \brief Fire 처리를 스텝당 한 번만 실행한다. 몽타주가 외부 요인으로 중단되었으면 체인을 취소 종료한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::HandleComboStepFire()
{
	if (!bComboChainActive || bFireHandledThisStep)
	{
		return;
	}

	if (bUsingComboMontage && !IsComboMontagePlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("MyGAS combo attack cancelled - montage no longer playing at fire. Ability: %s, StepIndex: %d"),
			*GetNameSafe(this),
			ActiveStepIndex);
		FinishComboChain(true);
		return;
	}

	bFireHandledThisStep = true;
	OnComboStepFire(ActiveStepIndex, CachedSkillDataEntry);
}

////////////////////////////
//! \author HanUl
//! \brief SaveCombo 시점부터 다음 콤보 입력 저장을 허용한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::HandleComboStepSaveWindowOpen()
{
	if (bComboChainActive && !bSaveHandledThisStep)
	{
		bSaveHandledThisStep = true;
		bSaveInputWindowOpen = true;
	}
}

////////////////////////////
//! \author HanUl
//! \brief EndAttack 시점에 버퍼된 입력이 있으면 다음 스텝으로 체인하고, 없으면 체인을 종료한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::HandleComboStepEnd()
{
	if (!bComboChainActive || bEndHandledThisStep)
	{
		return;
	}
	bEndHandledThisStep = true;

	if (!bFireHandledThisStep)
	{
		HandleComboStepFire();
	}

	// Fire 처리 중 취소되었을 수 있다
	if (!bComboChainActive)
	{
		return;
	}

	const int32 NextStepIndex = ActiveStepIndex + 1;
	if (bBufferedNextInput && CachedSkillDataEntry.Combo.Steps.IsValidIndex(NextStepIndex))
	{
		StartComboStep(NextStepIndex);
		return;
	}

	FinishComboChain(false);
}

////////////////////////////
//! \author HanUl
//! \brief 체인을 종료하고 콤보 기억을 갱신한 뒤 Ability를 종료한다.
//! \param bWasCancelled 취소 종료 여부. 취소면 콤보 기억을 초기화한다
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::FinishComboChain(bool bWasCancelled)
{
	if (bWasCancelled)
	{
		LastCompletedStepIndex = INDEX_NONE;
	}
	else
	{
		LastCompletedStepIndex = ActiveStepIndex;
		if (const UWorld* World = GetWorld())
		{
			LastSwingEndTime = World->GetTimeSeconds();
		}
	}

	bComboChainActive = false;

	// 섹션이 이어져 있어도 다음 타 모션이 새어 나가지 않도록 블렌드 아웃한다
	if (!bWasCancelled && IsComboMontagePlaying())
	{
		if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
		{
			SourceASC->CurrentMontageStop(ComboEndMontageBlendOutTime);
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bWasCancelled, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 스윙 체인 진행 상태를 초기화한다. 콤보 기억(마지막 타/종료 시각)은 유지한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ResetComboSwingState()
{
	// 이전 Activation에서 지연되어 들어오는 fallback을 무효화한다
	++ActiveStepSerial;
	ActiveStepIndex = INDEX_NONE;
	bComboChainActive = false;
	bFireHandledThisStep = false;
	bSaveHandledThisStep = false;
	bEndHandledThisStep = false;
	bSaveInputWindowOpen = false;
	bBufferedNextInput = false;
	bUsingComboMontage = false;
	CurrentComboPlayRate = 1.0f;
	ComboTriggerEventData = FGameplayEventData();
	bHasComboTriggerEventData = false;
}

////////////////////////////
//! \author HanUl
//! \brief 대상을 타격 방향으로 밀어낸다. Character는 LaunchCharacter, 그 외는 스윕 이동으로 처리한다.
//! \param TargetActor 밀어낼 대상 Actor
//! \param KnockbackDistance 밀어낼 거리(cm)
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ApplyComboKnockback(AActor* TargetActor, float KnockbackDistance) const
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor || !TargetActor)
	{
		return;
	}

	// 이 타의 피해로 죽은 대상은 넉백에서 제외한다.
	// 사망 시 래그돌이 켜지면 Launch 속도가 물리 초기 속도로 승계돼 시체가 날아간다.
	if (!UMyAbilitySystemLibrary::IsLivingPawn(TargetActor))
	{
		return;
	}

	FVector KnockbackDirection = (TargetActor->GetActorLocation() - AvatarActor->GetActorLocation()).GetSafeNormal2D();
	if (KnockbackDirection.IsNearlyZero())
	{
		KnockbackDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	}

	if (ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		TargetCharacter->LaunchCharacter(KnockbackDirection * KnockbackDistance * KnockbackVelocityPerDistance, true, false);
		return;
	}

	TargetActor->AddActorWorldOffset(KnockbackDirection * KnockbackDistance, true);
}

////////////////////////////
//! \author HanUl
//! \brief 스텝 데이터에 전진 이동이 설정되어 있으면 아바타 전방으로 RootMotion 전진을 시작한다(예: Heru 3타 전진 베기).
//!        RootMotionConstantForce는 CharacterMovement가 전 클라이언트로 복제하므로 시뮬 프록시도 같은 전진을 본다.
//!        지속 시간은 공격 속도(재생 속도)에 맞춰 스케일해 스윙 모션과 어긋나지 않게 한다.
//! \param Step 현재 콤보 스텝 정의
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::StartComboStepForwardMove(const FMySkillComboStepSpec& Step)
{
	if (Step.ForwardMoveDistance <= 0.0f)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return;
	}

	const FVector MoveDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	const float SafePlayRate = FMath::Max(MinComboMontagePlayRate, CurrentComboPlayRate);
	const float MoveDuration = FMath::Max(Step.ForwardMoveDuration, 0.01f) / SafePlayRate;
	const float MoveSpeed = Step.ForwardMoveDistance / MoveDuration;

	UAbilityTask_ApplyRootMotionConstantForce* ForwardMoveTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("ComboStepForwardMove"),
			MoveDirection,
			MoveSpeed,
			MoveDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.0f,
			true
		);
	if (ForwardMoveTask)
	{
		ForwardMoveTask->ReadyForActivation();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 입력 대기 태스크를 무장한다. WaitInputPress는 1회성이라 입력마다 다시 무장해야 한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ArmComboInputTask()
{
	ComboInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (ComboInputTask)
	{
		ComboInputTask->OnPress.AddDynamic(this, &UMyGameplayAbility_ComboAttackBase::OnComboInputPressed);
		ComboInputTask->ReadyForActivation();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 스윙 중 추가 입력을 수신해 버퍼 구간이면 다음 타 입력으로 저장한다.
//! \param TimeWaited 태스크 대기 시간
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnComboInputPressed(float TimeWaited)
{
	(void)TimeWaited;

	ArmComboInputTask();

	if (!bComboChainActive || ActiveStepIndex == INDEX_NONE)
	{
		return;
	}

	// 마지막 타 이후의 입력은 다음 Activation이 ResetTime 기준으로 처리한다
	if (!CachedSkillDataEntry.Combo.Steps.IsValidIndex(ActiveStepIndex + 1))
	{
		return;
	}

	if (bSaveInputWindowOpen)
	{
		bBufferedNextInput = true;
	}
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition의 콤보 몽타주를 반환한다.
//! \param 없음
//! \return 콤보 몽타주, 없으면 nullptr
UAnimMontage* UMyGameplayAbility_ComboAttackBase::GetComboMontage() const
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	return SkillDefinition ? SkillDefinition->GetAnimation().Montage.Get() : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 몽타주가 현재 ASC에서 재생 중인지 확인한다.
//! \param 없음
//! \return 콤보 몽타주가 재생 중이면 true
bool UMyGameplayAbility_ComboAttackBase::IsComboMontagePlaying() const
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAnimMontage* Montage = GetComboMontage();
	return SourceASC && Montage && SourceASC->GetCurrentMontage() == Montage;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 몽타주를 지정 섹션에서 재생한다.
//! \param SectionName 재생할 몽타주 섹션 이름
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::PlayComboSection(FName SectionName)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAnimMontage* Montage = GetComboMontage();
	if (!SourceASC || !Montage)
	{
		FinishComboChain(true);
		return;
	}

	SourceASC->PlayMontage(this, CurrentActivationInfo, Montage, CurrentComboPlayRate, SectionName);
}

////////////////////////////
//! \author HanUl
//! \brief Avatar Mesh의 AnimInstance에 콤보 Montage Notify 수신 함수를 바인딩한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::BindComboMontageNotify()
{
	UnbindComboMontageNotify();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ACharacter* AvatarCharacter = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? Cast<ACharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	USkeletalMeshComponent* MeshComponent = AvatarCharacter ? AvatarCharacter->GetMesh() : nullptr;
	ComboBoundAnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (ComboBoundAnimInstance)
	{
		ComboBoundAnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UMyGameplayAbility_ComboAttackBase::OnComboMontageNotifyBegin);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 Montage Notify 바인딩을 해제한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::UnbindComboMontageNotify()
{
	if (ComboBoundAnimInstance)
	{
		ComboBoundAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UMyGameplayAbility_ComboAttackBase::OnComboMontageNotifyBegin);
		ComboBoundAnimInstance = nullptr;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 Shoot/SaveCombo/EndAttack Montage Notify를 수신해 스텝 처리를 실행한다.
//! \param NotifyName 발생한 Notify 이름
//! \param BranchingPointPayload Notify Payload
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnComboMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (!bComboChainActive || !IsNotifyFromActiveComboStep(BranchingPointPayload))
	{
		return;
	}

	UWorld* World = GetWorld();

	if (NotifyName == StandardShootNotifyName)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(FireNotifyTimerHandle);
		}
		HandleComboStepFire();
		return;
	}

	if (NotifyName == ComboSaveInputNotifyName)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(SaveInputNotifyTimerHandle);
		}
		HandleComboStepSaveWindowOpen();
		return;
	}

	if (NotifyName == StandardEndAttackNotifyName)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(EndAttackNotifyTimerHandle);
		}
		HandleComboStepEnd();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 수신한 Montage Notify가 현재 콤보 스텝 섹션에서 발생한 이벤트인지 확인한다.
//! \param BranchingPointPayload Montage Notify payload
//! \return 현재 스텝의 Notify이면 true
bool UMyGameplayAbility_ComboAttackBase::IsNotifyFromActiveComboStep(const FBranchingPointNotifyPayload& BranchingPointPayload) const
{
	if (!BranchingPointPayload.NotifyEvent)
	{
		return true;
	}

	float SectionStartTime = 0.0f;
	float SectionEndTime = 0.0f;
	if (!TryGetComboSectionTimeRange(ActiveStepIndex, SectionStartTime, SectionEndTime))
	{
		return false;
	}

	const float TriggerTime = BranchingPointPayload.NotifyEvent->GetTriggerTime();
	return TriggerTime > SectionStartTime && TriggerTime < SectionEndTime;
}

////////////////////////////
//! \author HanUl
//! \brief Notify 누락에 대비해 현재 스텝의 Shoot/SaveCombo/EndAttack 대체 타이머를 등록한다.
//!        EndAttack Notify가 섹션에 없으면 섹션 끝 시각으로 종료를 보장한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ScheduleComboStepNotifyFallbacks()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SafePlayRate = FMath::Max(MinComboMontagePlayRate, CurrentComboPlayRate);
	FTimerManager& TimerManager = World->GetTimerManager();
	const uint32 ExpectedStepSerial = ActiveStepSerial;

	float RelativeTime = 0.0f;
	if (TryGetComboNotifyTimeInSection(StandardShootNotifyName, ActiveStepIndex, RelativeTime))
	{
		FTimerDelegate FireFallbackDelegate;
		FireFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnFireNotifyFallback, ExpectedStepSerial);
		TimerManager.SetTimer(FireNotifyTimerHandle, FireFallbackDelegate, FMath::Max(RelativeTime / SafePlayRate, 0.01f), false);
	}

	if (TryGetComboNotifyTimeInSection(ComboSaveInputNotifyName, ActiveStepIndex, RelativeTime))
	{
		FTimerDelegate SaveFallbackDelegate;
		SaveFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnSaveInputNotifyFallback, ExpectedStepSerial);
		TimerManager.SetTimer(SaveInputNotifyTimerHandle, SaveFallbackDelegate, FMath::Max(RelativeTime / SafePlayRate, 0.01f), false);
	}

	if (TryGetComboNotifyTimeInSection(StandardEndAttackNotifyName, ActiveStepIndex, RelativeTime))
	{
		FTimerDelegate EndFallbackDelegate;
		EndFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnEndAttackNotifyFallback, ExpectedStepSerial);
		TimerManager.SetTimer(EndAttackNotifyTimerHandle, EndFallbackDelegate, FMath::Max(RelativeTime / SafePlayRate, 0.01f), false);
	}
	else
	{
		float SectionStartTime = 0.0f;
		float SectionEndTime = 0.0f;
		if (TryGetComboSectionTimeRange(ActiveStepIndex, SectionStartTime, SectionEndTime))
		{
			const float SectionDuration = FMath::Max(SectionEndTime - SectionStartTime, 0.01f);
			FTimerDelegate EndFallbackDelegate;
			EndFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnEndAttackNotifyFallback, ExpectedStepSerial);
			TimerManager.SetTimer(EndAttackNotifyTimerHandle, EndFallbackDelegate, SectionDuration / SafePlayRate, false);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주가 없을 때 스텝 데이터(FallbackFireDelay/FallbackStepDuration)로 Fire와 종료 타이머를 등록한다.
//!        공격 속도 배율이 타이머 길이에 반영된다.
//! \param Step 현재 콤보 스텝 정의
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ScheduleFallbackStepTimers(const FMySkillComboStepSpec& Step)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FinishComboChain(true);
		return;
	}

	const float SafePlayRate = FMath::Max(MinComboMontagePlayRate, CurrentComboPlayRate);
	FTimerManager& TimerManager = World->GetTimerManager();
	const uint32 ExpectedStepSerial = ActiveStepSerial;

	const float FireDelay = FMath::Max(Step.FallbackFireDelay, 0.0f) / SafePlayRate;
	if (FireDelay > 0.0f)
	{
		FTimerDelegate FireFallbackDelegate;
		FireFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnFireNotifyFallback, ExpectedStepSerial);
		TimerManager.SetTimer(FireNotifyTimerHandle, FireFallbackDelegate, FireDelay, false);
	}
	else
	{
		HandleComboStepFire();
		if (!bComboChainActive)
		{
			return;
		}
	}

	// 종료는 항상 Fire 이후가 되도록 보정한다
	const float StepDuration = FMath::Max(Step.FallbackStepDuration, Step.FallbackFireDelay + 0.05f) / SafePlayRate;
	FTimerDelegate EndFallbackDelegate;
	EndFallbackDelegate.BindUObject(this, &UMyGameplayAbility_ComboAttackBase::OnEndAttackNotifyFallback, ExpectedStepSerial);
	TimerManager.SetTimer(EndAttackNotifyTimerHandle, EndFallbackDelegate, StepDuration, false);
}

////////////////////////////
//! \author HanUl
//! \brief 콤보 Notify 대체 타이머들을 제거한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::ClearComboStepTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(FireNotifyTimerHandle);
		TimerManager.ClearTimer(SaveInputNotifyTimerHandle);
		TimerManager.ClearTimer(EndAttackNotifyTimerHandle);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Shoot Notify 대체 타이머에서 Fire 처리를 실행한다.
//! \param ExpectedStepSerial 타이머 예약 시점의 콤보 스텝 식별자
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnFireNotifyFallback(uint32 ExpectedStepSerial)
{
	if (!bComboChainActive || ExpectedStepSerial != ActiveStepSerial)
	{
		return;
	}

	HandleComboStepFire();
}

////////////////////////////
//! \author HanUl
//! \brief SaveCombo Notify 대체 타이머에서 입력 버퍼 구간을 연다.
//! \param ExpectedStepSerial 타이머 예약 시점의 콤보 스텝 식별자
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnSaveInputNotifyFallback(uint32 ExpectedStepSerial)
{
	if (!bComboChainActive || ExpectedStepSerial != ActiveStepSerial)
	{
		return;
	}

	HandleComboStepSaveWindowOpen();
}

////////////////////////////
//! \author HanUl
//! \brief EndAttack Notify 대체 타이머에서 스텝 종료 처리를 실행한다.
//! \param ExpectedStepSerial 타이머 예약 시점의 콤보 스텝 식별자
//! \return 없음
void UMyGameplayAbility_ComboAttackBase::OnEndAttackNotifyFallback(uint32 ExpectedStepSerial)
{
	if (!bComboChainActive || ExpectedStepSerial != ActiveStepSerial)
	{
		return;
	}

	HandleComboStepEnd();
}

////////////////////////////
//! \author HanUl
//! \brief 지정 스텝 섹션의 시작/끝 시간을 구한다.
//! \param StepIndex 조회할 콤보 스텝 인덱스(0부터)
//! \param OutStartTime 섹션 시작 시간
//! \param OutEndTime 섹션 끝 시간
//! \return 유효한 범위를 구했으면 true
bool UMyGameplayAbility_ComboAttackBase::TryGetComboSectionTimeRange(int32 StepIndex, float& OutStartTime, float& OutEndTime) const
{
	OutStartTime = 0.0f;
	OutEndTime = 0.0f;

	const FMySkillComboStepSpec* Step = GetComboStepSpec(StepIndex);
	UAnimMontage* Montage = GetComboMontage();
	if (!Step || !Montage)
	{
		return false;
	}

	const int32 SectionIndex = Montage->GetSectionIndex(Step->MontageSectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return false;
	}

	Montage->GetSectionStartAndEndTime(SectionIndex, OutStartTime, OutEndTime);
	return OutEndTime > OutStartTime;
}

////////////////////////////
//! \author HanUl
//! \brief 지정 스텝 섹션 내부에서 이름 기반 Notify의 섹션 시작 기준 상대 시간을 찾는다.
//! \param NotifyName 찾을 Notify 이름
//! \param StepIndex 검색할 콤보 스텝 인덱스(0부터)
//! \param OutRelativeTime 섹션 시작 기준 Notify 시간
//! \return Notify를 찾았으면 true
bool UMyGameplayAbility_ComboAttackBase::TryGetComboNotifyTimeInSection(FName NotifyName, int32 StepIndex, float& OutRelativeTime) const
{
	OutRelativeTime = 0.0f;

	UAnimMontage* Montage = GetComboMontage();
	if (!Montage || NotifyName.IsNone())
	{
		return false;
	}

	float SectionStartTime = 0.0f;
	float SectionEndTime = 0.0f;
	if (!TryGetComboSectionTimeRange(StepIndex, SectionStartTime, SectionEndTime))
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const float TriggerTime = NotifyEvent.GetTriggerTime();
		// 섹션 경계 Notify가 인접한 두 스텝에 동시에 귀속되지 않도록 양쪽 경계를 제외한다
		if (TriggerTime <= SectionStartTime || TriggerTime >= SectionEndTime)
		{
			continue;
		}

		if (NotifyEvent.NotifyName == NotifyName
			|| (NotifyEvent.Notify && NotifyEvent.Notify->GetNotifyName() == NotifyName.ToString()))
		{
			OutRelativeTime = FMath::Max(0.0f, TriggerTime - SectionStartTime);
			return true;
		}
	}

	return false;
}
