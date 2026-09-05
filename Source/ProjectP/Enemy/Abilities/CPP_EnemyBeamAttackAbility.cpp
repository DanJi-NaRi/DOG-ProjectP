// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyBeamAttackAbility.h"

#include "Enemy/Core/CPP_EnemyAIC.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Actors/CPP_EnemyBeamActor.h"
#include "MyGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Kismet/KismetMathLibrary.h"

////////////////////////////
//! \author HanUl
//! \brief 눈빔 발동: 이동을 멈추고 타겟을 향한 뒤 빔 액터를 스폰해 조준 페이즈로 진입한다.
//!        이후 WaitDelay 체인(조준→정지→발사)으로 페이즈를 전환한다.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return
void UCPP_EnemyBeamAttackAbility::ActivateAbility(
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

	AActor* TargetActor = EnemyAvatar->GetCurrentTargetActor();
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	UWorld* World = EnemyAvatar->GetWorld();
	if (!TargetActor || !AttackPattern || !World)
	{
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	EnemyAvatar->SetActiveBeamAttackAbility(this);

	// 공격 중 정지 + 타겟 조준(캐스터 자체 회전은 연출용, 빔 각도는 빔 액터가 독립적으로 추적).
	if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(EnemyAvatar->GetController()))
	{
		EnemyAIC->StopMovementForAttack();
	}
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(EnemyAvatar->GetActorLocation(), TargetActor->GetActorLocation());
	EnemyAvatar->SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));

	// 기본: 즉시 빔 시퀀스. 파생 패턴(순간이동 등)은 OnBeamActivated를 오버라이드해 선행 동작 후 BeginBeamSequence 호출.
	OnBeamActivated();
}

void UCPP_EnemyBeamAttackAbility::OnBeamActivated()
{
	BeginBeamSequence();
}

////////////////////////////
//! \author HanUl
//! \brief 빔 액터를 스폰하고 조준 페이즈로 진입해 WaitDelay 체인(조준→정지→발사)을 시작한다.
//!        스폰 실패 시 공격 종료를 통지하고 어빌리티를 닫는다(행 방지).
//! \param
//! \return
void UCPP_EnemyBeamAttackAbility::BeginBeamSequence()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UWorld* World = EnemyAvatar ? EnemyAvatar->GetWorld() : nullptr;
	if (!EnemyAvatar || !AttackPattern || !World)
	{
		FinishBeamAttack();
		return;
	}

	TSubclassOf<ACPP_EnemyBeamActor> SpawnClass = BeamActorClass;
	if (!SpawnClass)
	{
		SpawnClass = ACPP_EnemyBeamActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = EnemyAvatar;
	SpawnParams.Instigator = EnemyAvatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACPP_EnemyBeamActor* Beam = World->SpawnActor<ACPP_EnemyBeamActor>(
		SpawnClass, EnemyAvatar->GetActorLocation(), EnemyAvatar->GetActorRotation(), SpawnParams);
	if (!Beam)
	{
		FinishBeamAttack();
		return;
	}

	Beam->Initialize(
		EnemyAvatar,
		GetAbilitySystemComponentFromActorInfo(),
		AttackPattern->HitGameplayEffect,
		AttackPattern->StatusGameplayEffect,
		AttackPattern->Range,
		BeamHalfWidth,
		BeamHalfHeight,
		BeamOriginHeight,
		AttackPattern->DamageCoefficient,
		bDrawDebug
	);
	Beam->SetBeamPhase(EEnemyBeamPhase::Aim);
	ActiveBeam = Beam;

	UAbilityTask_WaitDelay* AimTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(AimDuration, KINDA_SMALL_NUMBER));
	if (!AimTask)
	{
		FinishBeamAttack();
		return;
	}

	AimTask->OnFinish.AddDynamic(this, &UCPP_EnemyBeamAttackAbility::HandleAimFinished);
	AimTask->ReadyForActivation();
}

void UCPP_EnemyBeamAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	DestroyBeam();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* UCPP_EnemyBeamAttackAbility::GetCooldownTags() const
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
//! \brief 활성 공격 패턴이 정의한 쿨다운 태그/시간을 적용한다. (다른 적 공격 어빌리티와 동일 규칙)
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return
void UCPP_EnemyBeamAttackAbility::ApplyCooldown(
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
//! \brief 외부 강제 종료(스태거/사망 → StopCurrentMontage → CancelActiveAttackAbilities)용 정리.
//!        빔을 파괴하고 어빌리티를 닫는다. 공격 종료 통지는 호출측(StopCurrentMontage)이 담당한다.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemyBeamAttackAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	DestroyBeam();

	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveBeamAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 조준 종료: 방향을 고정(Lock)하고 발사 직전 정지 연출 시간만큼 대기한다.
//! \param
//! \return
void UCPP_EnemyBeamAttackAbility::HandleAimFinished()
{
	if (ActiveBeam.IsValid())
	{
		ActiveBeam->SetBeamPhase(EEnemyBeamPhase::Lock);
	}

	UAbilityTask_WaitDelay* LockTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(PreFireLockDuration, KINDA_SMALL_NUMBER));
	if (!LockTask)
	{
		HandleLockFinished();
		return;
	}

	LockTask->OnFinish.AddDynamic(this, &UCPP_EnemyBeamAttackAbility::HandleLockFinished);
	LockTask->ReadyForActivation();
}

////////////////////////////
//! \author HanUl
//! \brief 정지 연출 종료: 쿨다운을 커밋하고(실제 발사 시점) 발사 페이즈로 전환한 뒤 지속 시간만큼 대기한다.
//! \param
//! \return
void UCPP_EnemyBeamAttackAbility::HandleLockFinished()
{
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo);
	}

	if (ActiveBeam.IsValid())
	{
		ActiveBeam->SetBeamPhase(EEnemyBeamPhase::Fire);
	}

	UAbilityTask_WaitDelay* FireTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(FireDuration, KINDA_SMALL_NUMBER));
	if (!FireTask)
	{
		HandleFireFinished();
		return;
	}

	FireTask->OnFinish.AddDynamic(this, &UCPP_EnemyBeamAttackAbility::HandleFireFinished);
	FireTask->ReadyForActivation();
}

////////////////////////////
//! \author HanUl
//! \brief 발사 종료: 정상 완료 경로. 빔을 파괴하고 어빌리티를 닫은 뒤 공격 종료를 통지한다.
//! \param
//! \return
void UCPP_EnemyBeamAttackAbility::HandleFireFinished()
{
	FinishBeamAttack();
}

////////////////////////////
//! \author HanUl
//! \brief 빔 공격 정상 종료: 빔 파괴 → 활성 해제 → EndAbility → 공격 종료 통지(AI 복귀). 완료·스폰 실패 공용.
//! \param
//! \return
void UCPP_EnemyBeamAttackAbility::FinishBeamAttack()
{
	DestroyBeam();

	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveBeamAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}

	if (EnemyAvatar)
	{
		EnemyAvatar->FinishPrimaryAttackFromAbility();
	}
}

void UCPP_EnemyBeamAttackAbility::DestroyBeam()
{
	if (ActiveBeam.IsValid())
	{
		ActiveBeam->Destroy();
	}
	ActiveBeam = nullptr;
}

ACPP_EnemyBase* UCPP_EnemyBeamAttackAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}
