#include "CPP_BossBlackHoleAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Boss/Actors/CPP_BossBlackHoleActor.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Encounter/CPP_BossEncounterDirectorComponent.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MyGameplayTags.h"

UCPP_BossBlackHoleAbility::UCPP_BossBlackHoleAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

////////////////////////////
//! \brief 소환 몽타주를 재생하고, 몽타주 노티파이 이벤트에서 검은 구를 소환한다. 몽타주 종료 시 어빌리티가 끝난다.
void UCPP_BossBlackHoleAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Reset per-activation state: this ability is InstancedPerActor, so the same instance is reused every activation.
	bBlackHoleSpawned = false;

	if (!HasAuthority(&ActivationInfo) || !SpawnMontage || !BlackHoleActorClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Spawn the black hole when the montage notify fires its gameplay event.
	ActiveSpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_AttackWindow,
		nullptr,
		false,
		true
	);

	if (!ActiveSpawnEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpawnEventTask->EventReceived.AddDynamic(this, &UCPP_BossBlackHoleAbility::HandleSpawnEvent);
	ActiveSpawnEventTask->ReadyForActivation();

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		SpawnMontage
	);

	if (!ActiveMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMontageTask->OnCompleted.AddDynamic(this, &UCPP_BossBlackHoleAbility::HandleMontageCompleted);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &UCPP_BossBlackHoleAbility::HandleMontageInterrupted);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &UCPP_BossBlackHoleAbility::HandleMontageCancelled);
	ActiveMontageTask->OnBlendOut.AddDynamic(this, &UCPP_BossBlackHoleAbility::HandleMontageBlendOut);
	ActiveMontageTask->ReadyForActivation();
}

void UCPP_BossBlackHoleAbility::HandleSpawnEvent(FGameplayEventData Payload)
{
	SpawnBlackHole();
}

////////////////////////////
//! \brief 전장 중앙에 검은 구를 1회 소환하고 Director에 기믹 해저드로 등록한다.
void UCPP_BossBlackHoleAbility::SpawnBlackHole()
{
	if (bBlackHoleSpawned)
	{
		return;
	}

	ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = BossAvatar ? BossAvatar->GetWorld() : nullptr;
	if (!BossAvatar || !SourceASC || !World)
	{
		return;
	}

	const FVector SpawnLocation = BossAvatar->GetArenaCenterLocation();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = BossAvatar;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACPP_BossBlackHoleActor* BlackHole = World->SpawnActor<ACPP_BossBlackHoleActor>(
		BlackHoleActorClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters
	);

	if (!BlackHole)
	{
		return;
	}

	BlackHole->Initialize(SourceASC, BossAvatar->GetBossDamageGameplayEffect(), KillDamage);

	if (UCPP_BossEncounterDirectorComponent* Director = BossAvatar->GetBossEncounterDirectorComponent())
	{
		Director->RegisterGimmickHazard(BlackHole);
	}

	bBlackHoleSpawned = true;
}

const FGameplayTagContainer* UCPP_BossBlackHoleAbility::GetCooldownTags() const
{
	BossCooldownTags.Reset();

	if (CooldownTag.IsValid())
	{
		BossCooldownTags.AddTag(CooldownTag);
	}

	return &BossCooldownTags;
}

////////////////////////////
//! \brief 이 어빌리티의 GAS 쿨다운 효과를 동적 쿨다운 태그와 지속시간으로 적용한다.
void UCPP_BossBlackHoleAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !BossCooldownGameplayEffectClass || !CooldownTag.IsValid() || CooldownSeconds <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		BossCooldownGameplayEffectClass,
		GetAbilityLevel(Handle, ActorInfo)
	);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, CooldownSeconds);
	SpecHandle.Data->SetDuration(CooldownSeconds, true);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

float UCPP_BossBlackHoleAbility::GetCooldownSeconds() const
{
	return CooldownSeconds;
}

void UCPP_BossBlackHoleAbility::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCPP_BossBlackHoleAbility::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCPP_BossBlackHoleAbility::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCPP_BossBlackHoleAbility::HandleMontageBlendOut()
{
}

void UCPP_BossBlackHoleAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
