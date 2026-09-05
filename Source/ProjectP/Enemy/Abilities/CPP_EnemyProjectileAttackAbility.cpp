// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyProjectileAttackAbility.h"

#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Actors/CPP_EnemyProjectileBase.h"
#include "MyGameplayTags.h"
#include "GAS/MyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanSeul
//! \brief Activates the enemy projectile attack flow through GAS.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return None
void UCPP_EnemyProjectileAttackAbility::ActivateAbility(
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
	if (!TargetActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	bHasFiredProjectile = false;
	EnemyAvatar->SetActiveProjectileAttackAbility(this);

	const bool bMontageStarted = EnemyAvatar->PlayPrimaryAttackMontageFromAbility(TargetActor);
	if (!bMontageStarted)
	{
		EnemyAvatar->ClearActiveProjectileAttackAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

const FGameplayTagContainer* UCPP_EnemyProjectileAttackAbility::GetCooldownTags() const
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
//! \author HanSeul
//! \brief Applies the cooldown tag and duration defined by the active attack pattern.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return None
void UCPP_EnemyProjectileAttackAbility::ApplyCooldown(
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
//! \author HanSeul
//! \brief Commits cooldown and fires the projectile when the attack montage notify reaches the hit frame.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return true when cooldown commit and projectile spawn succeed.
bool UCPP_EnemyProjectileAttackAbility::FireProjectileFromNotify(ACPP_EnemyBase* EnemyAvatar)
{
	if (bHasFiredProjectile || !EnemyAvatar)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo))
	{
		return false;
	}

	AActor* TargetActor = EnemyAvatar->GetCurrentTargetActor();
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	if (!TargetActor || !SpawnProjectile(EnemyAvatar, TargetActor, AttackPattern))
	{
		return false;
	}

	bHasFiredProjectile = true;
	return true;
}

void UCPP_EnemyProjectileAttackAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveProjectileAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

ACPP_EnemyBase* UCPP_EnemyProjectileAttackAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns and initializes the projectile defined by the active attack pattern.
//! \param EnemyAvatar Enemy that owns the active projectile ability.
//! \param TargetActor Actor the projectile should fly toward.
//! \param AttackPattern Attack pattern that supplies projectile and damage settings.
//! \return true when the projectile actor is spawned successfully.
bool UCPP_EnemyProjectileAttackAbility::SpawnProjectile(
	ACPP_EnemyBase* EnemyAvatar,
	AActor* TargetActor,
	const UCPP_EnemyAttackPatternData* AttackPattern
)
{
	if (!EnemyAvatar || !EnemyAvatar->HasAuthority() || !TargetActor || !AttackPattern || !AttackPattern->ProjectileClass)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = EnemyAvatar->GetAbilitySystemComponent();
	UWorld* World = EnemyAvatar->GetWorld();
	if (!ASC || !World)
	{
		return false;
	}

	// 발사 방향은 시전 시점에 고정된 적의 바라보는 방향(GetActorRotation)을 사용한다.
	// 시전 시작 시 PlayAttackPatternMontage가 타겟을 향해 Yaw를 맞추고 SuspendTargetFocus로 회전을 잠그므로,
	// 발사 순간 플레이어 위치를 다시 조준하지 않고 시전 방향 그대로 직선 발사한다.
	const FRotator SpawnRotation = EnemyAvatar->GetActorRotation();
	const FVector SpawnLocation = EnemyAvatar->GetActorLocation()
		+ (EnemyAvatar->GetActorForwardVector() * 100.0f)
		+ FVector(0.0f, 0.0f, 60.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = EnemyAvatar;
	SpawnParams.Instigator = EnemyAvatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		AttackPattern->ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);
	if (!SpawnedActor)
	{
		return false;
	}

	if (ACPP_EnemyProjectileBase* EnemyProjectile = Cast<ACPP_EnemyProjectileBase>(SpawnedActor))
	{
		EnemyProjectile->InitializeProjectile(
			ASC,
			TargetActor,
			AttackPattern->HitGameplayEffect,
			AttackPattern->StatusGameplayEffect,
			AttackPattern->Range,
			AttackPattern->ProjectileSpeed,
			AttackPattern->ProjectileRadius,
			AttackPattern->DamageCoefficient
		);
	}

	return true;
}
