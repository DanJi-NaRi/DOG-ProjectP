// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyBlinkNovaAbility.h"

#include "Enemy/Abilities/CPP_AbilityTask_EnemyBlink.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Actors/CPP_EnemyTelegraphActor.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanUl
//! \brief 점멸 자폭 발동: 공용 순간이동 태스크(소멸→후방 재등장→무적)를 시작한다. 재등장 시 HandleBlinkFinished가
//!        자폭 시퀀스(텔레그래프+폭발 타이머)를 이어받는다.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return
void UCPP_EnemyBlinkNovaAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(ActorInfo);
	if (!EnemyAvatar || !EnemyAvatar->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	if (!AttackPattern || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	DestroyExplodeTelegraph();
	EnemyAvatar->SetActiveBlinkNovaAbility(this);

	ActiveBlinkTask = UCPP_AbilityTask_EnemyBlink::EnemyBlink(
		this, EnemyAvatar, VanishDuration, ReappearBehindGap, InvincibleDuration, VanishCueTag, ReappearCueTag);
	if (!ActiveBlinkTask)
	{
		EnemyAvatar->ClearActiveBlinkNovaAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveBlinkTask->OnBlinkFinished.AddDynamic(this, &UCPP_EnemyBlinkNovaAbility::HandleBlinkFinished);
	ActiveBlinkTask->ReadyForActivation();
}

void UCPP_EnemyBlinkNovaAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 소멸 상태 복구·무적 해제는 순간이동 태스크가 자신의 OnDestroy에서 처리한다(어빌리티 종료 시 태스크도 종료됨).
	DestroyExplodeTelegraph();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* UCPP_EnemyBlinkNovaAbility::GetCooldownTags() const
{
	PatternCooldownTags.Reset();

	if (const ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo()))
	{
		if (const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern())
		{
			if (AttackPattern->CooldownTag.IsValid())
			{
				PatternCooldownTags.AddTag(AttackPattern->CooldownTag);
			}
		}
	}

	return &PatternCooldownTags;
}

////////////////////////////
//! \author HanUl
//! \brief 활성 공격 패턴이 정의한 쿨다운 태그/시간을 적용한다. 자폭형은 보통 쿨다운이 무의미하지만
//!        패턴 데이터에 설정돼 있으면 존중한다. (다른 적 공격 어빌리티와 동일 규칙)
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return
void UCPP_EnemyBlinkNovaAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(ActorInfo);
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!CooldownGE || !AttackPattern || !AttackPattern->CooldownTag.IsValid() || AttackPattern->CooldownDuration <= 0.0f || !ASC)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(AttackPattern->CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, AttackPattern->CooldownDuration);
	SpecHandle.Data->SetDuration(AttackPattern->CooldownDuration, true);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

////////////////////////////
//! \author HanUl
//! \brief 외부 강제 종료 경로(사망 태스크의 StopCurrentMontage → CancelActiveAttackAbilities)용 정리.
//!        몽타주는 없지만 다른 적 공격 어빌리티와 같은 계약을 따른다.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemyBlinkNovaAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveBlinkNovaAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 재등장 완료(순간이동 태스크 콜백): 폭발 텔레그래프를 띄우고 ExplodeDelay 후 폭발하도록 타이머를 건다.
//!        무적 창은 태스크가 이 시점부터 InvincibleDuration 동안 유지한다.
//! \param
//! \return
void UCPP_EnemyBlinkNovaAbility::HandleBlinkFinished()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar || EnemyAvatar->IsDead())
	{
		return;
	}

	UAbilityTask_WaitDelay* ExplodeTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(ExplodeDelay, KINDA_SMALL_NUMBER));
	if (!ExplodeTask)
	{
		HandleExplodeTimeReached();
		return;
	}

	ExplodeTask->OnFinish.AddDynamic(this, &UCPP_EnemyBlinkNovaAbility::HandleExplodeTimeReached);
	ExplodeTask->ReadyForActivation();

	SpawnExplodeTelegraph(EnemyAvatar);
}

////////////////////////////
//! \author HanUl
//! \brief 폭발 시점 도달: 살아 있으면 원형 폭발을 수행하고 즉시 자폭(ForceKill)한다.
//!        폭발 전에 죽었으면(무적 종료~폭발 사이 피격) 폭발 없이 정리만 한다 — 그게 카운터플레이.
//! \param
//! \return
void UCPP_EnemyBlinkNovaAbility::HandleExplodeTimeReached()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar || EnemyAvatar->IsDead())
	{
		// 무적 종료~폭발 사이에 죽음 → 폭발 없이 정리.
		if (EnemyAvatar)
		{
			EnemyAvatar->ClearActiveBlinkNovaAbility(this);
		}
		if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
		{
			EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, true);
		}
		return;
	}

	DestroyExplodeTelegraph();
	ExecuteCosmeticCue(ExplodeCueTag);
	ExecuteExplosion(EnemyAvatar);

	EnemyAvatar->ClearActiveBlinkNovaAbility(this);
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}

	EnemyAvatar->ForceKillWithoutRewards();
}

////////////////////////////
//! \author HanUl
//! \brief 자신 중심 반경 ExplodeRadius 구형 판정으로 적대 폰 전원에게 패턴 HitGE × DamageCoefficient와
//!        상태이상을 적용한다.
//! \param EnemyAvatar 폭발 주체 적
//! \return
void UCPP_EnemyBlinkNovaAbility::ExecuteExplosion(ACPP_EnemyBase* EnemyAvatar)
{
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = EnemyAvatar->GetWorld();
	if (!AttackPattern || !AttackPattern->HitGameplayEffect || !SourceASC || !World
		|| ExplodeRadius <= 0.0f || AttackPattern->DamageCoefficient <= 0.0f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBlinkNovaExplosion), false);
	QueryParams.AddIgnoredActor(EnemyAvatar);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		EnemyAvatar->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ExplodeRadius),
		QueryParams
	);

	TSet<AActor*> AppliedTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!IsValid(TargetActor) || AppliedTargets.Contains(TargetActor)
			|| !UMyAbilitySystemLibrary::IsHostile(EnemyAvatar, TargetActor))
		{
			continue;
		}

		AppliedTargets.Add(TargetActor);
		UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			AttackPattern->HitGameplayEffect,
			AttackPattern->DamageCoefficient
		);
		ApplyStatusEffectToTarget(SourceASC, TargetActor, AttackPattern->StatusGameplayEffect);
	}
}

bool UCPP_EnemyBlinkNovaAbility::ApplyStatusEffectToTarget(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> StatusGameplayEffect
) const
{
	if (!SourceASC || !TargetActor || !StatusGameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());

	const FGameplayEffectSpecHandle StatusSpecHandle = SourceASC->MakeOutgoingSpec(StatusGameplayEffect, 1.0f, EffectContext);
	if (!StatusSpecHandle.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpecHandle.Data.Get(), TargetASC);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 폭발 예고용 원형 텔레그래프를 자신 위치에 스폰한다. 채움 시간 = ExplodeDelay.
//! \param EnemyAvatar 폭발 주체 적
//! \return
void UCPP_EnemyBlinkNovaAbility::SpawnExplodeTelegraph(ACPP_EnemyBase* EnemyAvatar)
{
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	UWorld* World = EnemyAvatar->GetWorld();
	if (!AttackPattern || !World || ExplodeRadius <= 0.0f)
	{
		return;
	}

	DestroyExplodeTelegraph();

	TSubclassOf<ACPP_EnemyTelegraphActor> TelegraphActorClass = AttackPattern->TelegraphActorClass;
	if (!TelegraphActorClass)
	{
		TelegraphActorClass = ACPP_EnemyTelegraphActor::StaticClass();
	}

	FBossHitShapeData ExplodeShape;
	ExplodeShape.Shape = EBossAttackShape::Circle;
	ExplodeShape.InnerRadius = 0.0f;
	ExplodeShape.OuterRadius = ExplodeRadius;
	ExplodeShape.HalfHeight = 100.0f;

	ACPP_EnemyTelegraphActor* TelegraphActor = World->SpawnActor<ACPP_EnemyTelegraphActor>(TelegraphActorClass);
	if (TelegraphActor)
	{
		TelegraphActor->Initialize(EnemyAvatar->GetActorTransform(), ExplodeShape, ExplodeDelay);
		ActiveExplodeTelegraph = TelegraphActor;
	}
}

void UCPP_EnemyBlinkNovaAbility::DestroyExplodeTelegraph()
{
	if (ActiveExplodeTelegraph.IsValid())
	{
		ActiveExplodeTelegraph->Destroy();
	}
	ActiveExplodeTelegraph = nullptr;
}

void UCPP_EnemyBlinkNovaAbility::ExecuteCosmeticCue(const FGameplayTag& CueTag) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ExecuteGameplayCue(CueTag, ASC->MakeEffectContext());
	}
}

ACPP_EnemyBase* UCPP_EnemyBlinkNovaAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}
