// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGA_Nefer_JudgementOfIsis.h"

#include "../MyAttributeSet.h"
#include "../../MyGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

namespace
{
	FGameplayTagContainer MakeDecayTags()
	{
		FGameplayTagContainer DecayTags;
		DecayTags.AddTag(MyGameplayTags::Status_Decay);
		DecayTags.AddTag(MyGameplayTags::Status_Debuff_Decay);
		return DecayTags;
	}
}

////////////////////////////
//! \author HanUl
//! \brief Judgement Of Isis Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGA_Nefer_JudgementOfIsis::UMyGA_Nefer_JudgementOfIsis()
{
	AbilityTag = MyGameplayTags::Skill_Nefer_JudgementOfIsis;
	InputTag = MyGameplayTags::Input_Skill_C;
	CooldownTag = MyGameplayTags::Cooldown_Skill_NFR_SK_C;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 시전 취소 요청을 처리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateCancelAbility 취소 복제 여부
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::CancelAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility
)
{
	UE_LOG(LogTemp, Log, TEXT("Nefer judgement cast cancelled - Avatar: %s"),
		ActorInfo && ActorInfo->AvatarActor.IsValid() ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"));

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 Judgement 전용 감시와 슈퍼아머를 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	StopCastCancelWatches();
	RemoveSuperArmor(ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() ? ActorInfo->AbilitySystemComponent.Get() : CastingSourceASC.Get());
	CastingAvatarActor = nullptr;
	CastingSourceASC = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief Definition에 Judgement 장판에 필요한 효과와 수치가 있는지 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 발동 가능하면 true
bool UMyGA_Nefer_JudgementOfIsis::CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	(void)ActorInfo;

	const FMySkillEffectSpec& Effects = Context.SkillData.Effects;
	if (!Effects.HitGameplayEffect || !Effects.StatusGameplayEffect)
	{
		return false;
	}

	if (Context.AreaSpec.Duration <= 0.0f || Context.AreaSpec.TickInterval <= 0.0f)
	{
		return false;
	}

	if (Effects.DamageCoefficient <= 0.0f || Effects.SecondaryDamageCoefficient <= 0.0f || Effects.TertiaryDamageCoefficient <= 0.0f || Effects.StatusDamageCoefficient <= 0.0f)
	{
		
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Commit 직후 Judgement 전용 슈퍼아머와 취소 감시를 시작한다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::OnAreaSkillCommitted(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	CastingAvatarActor = Context.AvatarActor;
	CastingSourceASC = Context.SourceASC;

	ApplySuperArmor(Context);
	BeginCastCancelWatches(Context.AvatarActor, Context.SourceASC);

	UE_LOG(LogTemp, Log, TEXT("Nefer judgement cast started - Avatar: %s, Authority: %s, TargetLocation: %s, Radius: %.2f"),
		*GetNameSafe(Context.AvatarActor),
		ActorInfo && ActorInfo->IsNetAuthority() ? TEXT("true") : TEXT("false"),
		*Context.AreaSpec.TargetLocation.ToCompactString(),
		Context.AreaSpec.Radius);
}

////////////////////////////
//! \author HanUl
//! \brief Judgement는 지속 피해 장판이므로 Tick을 예약한다.
//! \param Context 장판 런타임 컨텍스트
//! \return true
bool UMyGA_Nefer_JudgementOfIsis::ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const
{
	(void)Context;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 생성 즉시 Decay 상태에 따른 폭발 피해를 적용한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context)
{
	TArray<AActor*> Targets;
	CollectValidAreaTargets(Context, Targets);

	int32 DamageAppliedCount = 0;
	for (AActor* TargetActor : Targets)
	{
		const bool bHasDecay = IsDecayTarget(TargetActor);
		const int32 RemainingDecayTicks = bHasDecay ? GetRemainingDecayTickCount(TargetActor, Context.AreaSpec.TickInterval) : 0;
		// 계수만 전달하고 공격력 곱셈은 ExecCalc가 담당한다. Decay 대상은 남은 틱 수에 비례한 계수를 사용한다.
		const float DamageCoefficient = bHasDecay
			? Context.SkillData.Effects.SecondaryDamageCoefficient * static_cast<float>(RemainingDecayTicks)
			: Context.SkillData.Effects.DamageCoefficient;

		bool bKilled = false;
		if (ApplyDamageGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.HitGameplayEffect, DamageCoefficient, &bKilled))
		{
			++DamageAppliedCount;
			ApplyKillHealIfNeeded(Context, bKilled);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Nefer judgement initial explosion - InstanceId: %d, Location: %s, Radius: %.2f, Targets: %d, DamageApplied: %d"),
		Context.AreaInstanceId,
		*Context.AreaSpec.TargetLocation.ToCompactString(),
		Context.AreaSpec.Radius,
		Targets.Num(),
		DamageAppliedCount);
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 지속 Tick마다 피해를 주고 Decay가 없는 대상에게 Decay를 부여한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context)
{
	TArray<AActor*> Targets;
	CollectValidAreaTargets(Context, Targets);

	// 틱 피해는 계수만 전달하고 공격력 곱셈은 ExecCalc가 담당한다. Decay 틱은 상태 GE라 완성값(Data.Damage)을 유지한다.
	const float TickDamageCoefficient = Context.SkillData.Effects.TertiaryDamageCoefficient;
	const float DecayTickDamage = GetSourceAttackPower(Context) * Context.SkillData.Effects.StatusDamageCoefficient;

	int32 DamageAppliedCount = 0;
	int32 DecayAppliedCount = 0;
	for (AActor* TargetActor : Targets)
	{
		bool bKilled = false;
		if (ApplyDamageGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.HitGameplayEffect, TickDamageCoefficient, &bKilled))
		{
			++DamageAppliedCount;
			ApplyKillHealIfNeeded(Context, bKilled);
		}

		if (!IsDecayTarget(TargetActor)
			&& ApplyStatusGameplayEffectToTarget(Context, TargetActor, Context.SkillData.Effects.StatusGameplayEffect, DecayTickDamage))
		{
			++DecayAppliedCount;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("Nefer judgement tick - InstanceId: %d, Targets: %d, DamageApplied: %d, DecayApplied: %d"),
		Context.AreaInstanceId,
		Targets.Num(),
		DamageAppliedCount,
		DecayAppliedCount);
}

////////////////////////////
//! \author HanUl
//! \brief Avatar가 파괴되면 시전을 취소한다.
//! \param DestroyedActor 파괴된 Actor
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::HandleAvatarDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	CancelCastFromCondition(TEXT("AvatarDestroyed"));
}

////////////////////////////
//! \author HanUl
//! \brief Source Health 변경을 감시하여 사망 시 시전을 취소한다.
//! \param Data Attribute 변경 데이터
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::HandleSourceHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.0f && Data.NewValue < Data.OldValue)
	{
		CancelCastFromCondition(TEXT("Death"));
	}
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 시전 취소 조건 감시를 시작한다.
//! \param AvatarActor 시전자 Avatar
//! \param SourceASC 시전자 ASC
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::BeginCastCancelWatches(AActor* AvatarActor, UAbilitySystemComponent* SourceASC)
{
	if (AvatarActor)
	{
		AvatarActor->OnDestroyed.AddDynamic(this, &UMyGA_Nefer_JudgementOfIsis::HandleAvatarDestroyed);
	}

	if (SourceASC)
	{
		HealthChangedDelegateHandle = SourceASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
			.AddUObject(this, &UMyGA_Nefer_JudgementOfIsis::HandleSourceHealthChanged);
	}

	if (AvatarActor)
	{
		if (UWorld* World = AvatarActor->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MovementCancelTimerHandle,
				this,
				&UMyGA_Nefer_JudgementOfIsis::CheckMovementCancel,
				0.05f,
				true
			);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 시전 취소 조건 감시를 정리한다.
//! \param 없음
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::StopCastCancelWatches()
{
	if (CastingAvatarActor)
	{
		CastingAvatarActor->OnDestroyed.RemoveDynamic(this, &UMyGA_Nefer_JudgementOfIsis::HandleAvatarDestroyed);
		if (UWorld* World = CastingAvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(MovementCancelTimerHandle);
		}
	}

	if (CastingSourceASC && HealthChangedDelegateHandle.IsValid())
	{
		CastingSourceASC->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute()).Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 이동 입력 차단이 없는 상태에서 실제 이동이 발생하면 Judgement 시전을 취소한다.
//! \param 없음
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::CheckMovementCancel()
{
	if (IsMoveInputBlockApplied())
	{
		return;
	}

	const ACharacter* AvatarCharacter = Cast<ACharacter>(CastingAvatarActor.Get());
	const UCharacterMovementComponent* MovementComponent = AvatarCharacter ? AvatarCharacter->GetCharacterMovement() : nullptr;
	if (MovementComponent && MovementComponent->Velocity.SizeSquared2D() > 25.0f)
	{
		CancelCastFromCondition(TEXT("Movement"));
	}
}

////////////////////////////
//! \author HanUl
//! \brief 특정 조건으로 Judgement 시전 취소를 요청한다.
//! \param Reason 취소 사유
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::CancelCastFromCondition(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Log, TEXT("Nefer judgement cast cancel requested - Reason: %s, Avatar: %s"),
		Reason ? Reason : TEXT("Unknown"),
		*GetNameSafe(CastingAvatarActor.Get()));

	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

////////////////////////////
//! \author HanUl
//! \brief Definition의 BuffGameplayEffect를 슈퍼아머로 시전자에게 적용한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::ApplySuperArmor(const FMyAreaSkillRuntimeContext& Context)
{
	if (!Context.SourceASC || !Context.SkillData.Effects.BuffGameplayEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = Context.SourceASC->MakeOutgoingSpec(Context.SkillData.Effects.BuffGameplayEffect, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		SuperArmorEffectHandle = Context.SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

////////////////////////////
//! \author HanUl
//! \brief 적용된 슈퍼아머 GameplayEffect를 제거한다.
//! \param SourceASC 시전자 ASC
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::RemoveSuperArmor(UAbilitySystemComponent* SourceASC)
{
	if (SourceASC && SuperArmorEffectHandle.IsValid())
	{
		SourceASC->RemoveActiveGameplayEffect(SuperArmorEffectHandle);
		SuperArmorEffectHandle.Invalidate();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 대상이 Decay 상태인지 확인한다.
//! \param TargetActor 대상 Actor
//! \return Decay 상태이면 true
bool UMyGA_Nefer_JudgementOfIsis::IsDecayTarget(AActor* TargetActor) const
{
	return TargetHasAnyGameplayTag(TargetActor, MakeDecayTags());
}

////////////////////////////
//! \author HanUl
//! \brief 대상에게 남은 Decay Tick 수를 계산한다.
//! \param TargetActor 대상 Actor
//! \param TickInterval Decay Tick 간격
//! \return 남은 Tick 수
int32 UMyGA_Nefer_JudgementOfIsis::GetRemainingDecayTickCount(AActor* TargetActor, float TickInterval) const
{
	return GetRemainingEffectTickCountByTags(TargetActor, MakeDecayTags(), TickInterval);
}

////////////////////////////
//! \author HanUl
//! \brief Judgement 피해로 처치가 발생하면 Definition의 Heal 효과를 시전자에게 적용한다.
//! \param Context 장판 런타임 컨텍스트
//! \param bKilled 처치 발생 여부
//! \return 없음
void UMyGA_Nefer_JudgementOfIsis::ApplyKillHealIfNeeded(const FMyAreaSkillRuntimeContext& Context, bool bKilled) const
{
	if (!bKilled || !Context.AvatarActor || !Context.SkillData.Effects.HealGameplayEffect || Context.SkillData.Effects.HealPercentOfMaxHealth <= 0.0f)
	{
		return;
	}

	const float SourceMaxHealth = GetTargetMaxHealth(Context.AvatarActor);
	const float HealAmount = SourceMaxHealth * PercentValueToRatio(Context.SkillData.Effects.HealPercentOfMaxHealth);
	ApplyHealGameplayEffectToTarget(Context, Context.AvatarActor, Context.SkillData.Effects.HealGameplayEffect, HealAmount);
}
