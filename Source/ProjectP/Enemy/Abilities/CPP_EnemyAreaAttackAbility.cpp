// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_EnemyAreaAttackAbility.h"

#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Actors/CPP_EnemyAreaBase.h"
#include "Enemy/Actors/CPP_EnemyLobProjectileVisual.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "MyGameplayTags.h"
#include "GAS/MyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanSeul
//! \brief Activates the enemy area attack flow through GAS.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return None
void UCPP_EnemyAreaAttackAbility::ActivateAbility(
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
	bHasTriggeredArea = false;
	EnemyAvatar->SetActiveAreaAttackAbility(this);

	const bool bMontageStarted = EnemyAvatar->PlayPrimaryAttackMontageFromAbility(TargetActor);
	if (!bMontageStarted)
	{
		EnemyAvatar->ClearActiveAreaAttackAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

const FGameplayTagContainer* UCPP_EnemyAreaAttackAbility::GetCooldownTags() const
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
//! \brief Applies the cooldown tag and duration defined by the active area attack pattern.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return None
void UCPP_EnemyAreaAttackAbility::ApplyCooldown(
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
//! \brief Commits the active area ability when the casting montage reaches the area notify.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return true when the notify is accepted and the ability is committed.
bool UCPP_EnemyAreaAttackAbility::TriggerAreaFromNotify(ACPP_EnemyBase* EnemyAvatar)
{
	if (bHasTriggeredArea || !EnemyAvatar)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo))
	{
		return false;
	}

	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	if (!SpawnArea(EnemyAvatar, AttackPattern))
	{
		return false;
	}

	bHasTriggeredArea = true;
	return true;
}

void UCPP_EnemyAreaAttackAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveAreaAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

ACPP_EnemyBase* UCPP_EnemyAreaAttackAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns and initializes the area actor defined by the active attack pattern.
//! \param EnemyAvatar Enemy that owns the active area ability.
//! \param AttackPattern Attack pattern that supplies area and damage settings.
//! \return true when the area actor is spawned and initialized successfully.
bool UCPP_EnemyAreaAttackAbility::SpawnArea(
	ACPP_EnemyBase* EnemyAvatar,
	const UCPP_EnemyAttackPatternData* AttackPattern
)
{
	if (!EnemyAvatar || !EnemyAvatar->HasAuthority() || !AttackPattern || !AttackPattern->HitGameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = EnemyAvatar->GetAbilitySystemComponent();
	UWorld* World = EnemyAvatar->GetWorld();
	if (!ASC || !World)
	{
		return false;
	}

	FVector SpawnLocation = EnemyAvatar->GetActorLocation();
	if (AttackPattern->AreaTargetType == EEnemyAreaTargetType::Player)
	{
		const AActor* TargetActor = EnemyAvatar->GetCurrentTargetActor();
		if (!TargetActor)
		{
			return false;
		}

		SpawnLocation = TargetActor->GetActorLocation();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = EnemyAvatar;
	SpawnParams.Instigator = EnemyAvatar;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACPP_EnemyAreaBase* EnemyArea = World->SpawnActor<ACPP_EnemyAreaBase>(
		ACPP_EnemyAreaBase::StaticClass(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);
	if (!EnemyArea)
	{
		return false;
	}

	EnemyArea->InitializeArea(
		ASC,
		AttackPattern->HitGameplayEffect,
		AttackPattern->AreaDamageType,
		AttackPattern->AreaRadius,
		AttackPattern->AreaHalfHeight,
		AttackPattern->AreaWarningDuration,
		AttackPattern->AreaActiveDuration,
		AttackPattern->AreaDamageInterval,
		AttackPattern->DamageCoefficient,
		AttackPattern->TelegraphActorClass,
		AttackPattern->AreaImpactCueTag
	);
	SpawnLobProjectileVisual(
		EnemyAvatar,
		AttackPattern,
		EnemyArea->GetActorLocation(),
		EnemyArea->GetWarningServerStartTime()
	);
	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief 선택적으로 설정된 곡사 포탄 연출을 발사 소켓에서 Area의 고정 판정 중심까지 생성한다.
//! \param EnemyAvatar 포탄 연출을 발사하는 적.
//! \param AttackPattern 곡사 연출 클래스와 경로 설정을 제공하는 공격 패턴.
//! \param EndLocation 텔레그래프와 실제 피해 판정이 공유하는 도착 위치.
//! \param ServerStartTime 텔레그래프 경고가 시작된 서버 월드 시각.
//! \return None
void UCPP_EnemyAreaAttackAbility::SpawnLobProjectileVisual(
	ACPP_EnemyBase* EnemyAvatar,
	const UCPP_EnemyAttackPatternData* AttackPattern,
	const FVector& EndLocation,
	float ServerStartTime
) const
{
	if (!EnemyAvatar
		|| !EnemyAvatar->HasAuthority()
		|| !AttackPattern
		|| !AttackPattern->LobProjectileVisualClass
		|| AttackPattern->AreaWarningDuration <= 0.0f)
	{
		return;
	}

	FVector StartLocation = EnemyAvatar->GetActorLocation()
		+ EnemyAvatar->GetActorForwardVector() * 100.0f
		+ FVector(0.0f, 0.0f, 60.0f);
	if (USkeletalMeshComponent* Mesh = EnemyAvatar->GetMesh())
	{
		if (!AttackPattern->LobLaunchSocketName.IsNone() && Mesh->DoesSocketExist(AttackPattern->LobLaunchSocketName))
		{
			StartLocation = Mesh->GetSocketLocation(AttackPattern->LobLaunchSocketName);
		}
	}

	UWorld* World = EnemyAvatar->GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = EnemyAvatar;
	SpawnParameters.Instigator = EnemyAvatar;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACPP_EnemyLobProjectileVisual* LobVisual = World->SpawnActor<ACPP_EnemyLobProjectileVisual>(
		AttackPattern->LobProjectileVisualClass,
		StartLocation,
		FRotator::ZeroRotator,
		SpawnParameters
	);
	if (!LobVisual)
	{
		return;
	}

	LobVisual->InitializeLobVisual(
		StartLocation,
		EndLocation,
		AttackPattern->LobPeakHeight,
		AttackPattern->AreaWarningDuration,
		ServerStartTime
	);
}
