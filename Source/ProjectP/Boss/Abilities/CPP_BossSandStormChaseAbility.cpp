#include "CPP_BossSandStormChaseAbility.h"

#include "AbilitySystemComponent.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Encounter/CPP_BossEncounterDirectorComponent.h"
#include "Boss/Actors/CPP_BossSandStormChaseActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"

UCPP_BossSandStormChaseAbility::UCPP_BossSandStormChaseAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

////////////////////////////
//! \brief 무작위 생존 아군을 표적으로 마킹하고 MarkLeadTime 뒤 폭풍 소환을 예약한다.
void UCPP_BossSandStormChaseAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Reset per-activation state: this ability is InstancedPerActor, so the same instance is reused every activation.
	bHandedOffToStorm = false;
	PendingMarkHandle.Invalidate();
	PendingTarget = nullptr;

	if (!HasAuthority(&ActivationInfo) || !SandStormActorClass || !TargetMarkGameplayEffect)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = BossAvatar ? BossAvatar->GetWorld() : nullptr;
	if (!BossAvatar || !SourceASC || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Pick the initial target at random and mark it now, so the gold foot mark appears before the storm spawns.
	AActor* Target = UMyAbilitySystemLibrary::GetRandomLivingPlayer(BossAvatar);
	if (!Target)
	{
		// No living players to target; nothing to do (cooldown already committed).
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(Target))
	{
		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(BossAvatar);

		const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(TargetMarkGameplayEffect, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			PendingMarkHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
	}
	PendingTarget = Target;

	if (MarkLeadTime > 0.0f)
	{
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UCPP_BossSandStormChaseAbility::HandleSpawnStorm, MarkLeadTime, false);
	}
	else
	{
		HandleSpawnStorm();
	}
}

////////////////////////////
//! \brief MarkLeadTime 경과 후 보스 앞에 추격 폭풍을 소환하고 표적 마크 소유권을 폭풍으로 이관한다.
void UCPP_BossSandStormChaseAbility::HandleSpawnStorm()
{
	ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = BossAvatar ? BossAvatar->GetWorld() : nullptr;
	if (!BossAvatar || !SourceASC || !World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// The initial target may have died during the lead time: move the mark to the nearest survivor.
	AActor* Target = PendingTarget.Get();
	if (!UMyAbilitySystemLibrary::IsLivingPawn(Target))
	{
		Target = UMyAbilitySystemLibrary::GetNearestLivingPlayer(BossAvatar, BossAvatar->GetActorLocation());

		if (PendingMarkHandle.IsValid())
		{
			if (UAbilitySystemComponent* MarkedASC = PendingMarkHandle.GetOwningAbilitySystemComponent())
			{
				MarkedASC->RemoveActiveGameplayEffect(PendingMarkHandle);
			}
			PendingMarkHandle.Invalidate();
		}

		if (Target)
		{
			if (UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(Target))
			{
				FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
				EffectContext.AddSourceObject(BossAvatar);

				const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(TargetMarkGameplayEffect, 1.0f, EffectContext);
				if (SpecHandle.IsValid())
				{
					PendingMarkHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
				}
			}
		}
	}

	if (!Target)
	{
		// Everyone died during the lead time; the encounter is ending anyway.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FVector SpawnLocation = BossAvatar->GetActorLocation() + BossAvatar->GetActorForwardVector() * SpawnForwardDistance;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = BossAvatar;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACPP_BossSandStormChaseActor* SandStorm = World->SpawnActor<ACPP_BossSandStormChaseActor>(
		SandStormActorClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters
	);

	if (!SandStorm)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	SandStorm->Initialize(
		SourceASC,
		Target,
		BossAvatar->GetBossDamageGameplayEffect(),
		TargetMarkGameplayEffect,
		PendingMarkHandle
	);

	// The storm now owns the target mark's lifetime; EndAbility must not remove it.
	bHandedOffToStorm = true;

	if (UCPP_BossEncounterDirectorComponent* Director = BossAvatar->GetBossEncounterDirectorComponent())
	{
		Director->RegisterGimmickHazard(SandStorm);
	}

	// Keep the ability (and boss decision loop) occupied until the storm starts moving, then hand off to the next pattern.
	if (SpawnToEndTime > 0.0f)
	{
		World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UCPP_BossSandStormChaseAbility::HandleFinish, SpawnToEndTime, false);
	}
	else
	{
		HandleFinish();
	}
}

void UCPP_BossSandStormChaseAbility::HandleFinish()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

float UCPP_BossSandStormChaseAbility::GetCooldownSeconds() const
{
	return CooldownSeconds;
}

const FGameplayTagContainer* UCPP_BossSandStormChaseAbility::GetCooldownTags() const
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
void UCPP_BossSandStormChaseAbility::ApplyCooldown(
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

void UCPP_BossSandStormChaseAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ClearActiveTimers();

	// If the ability ends before the storm spawned (e.g. cancelled), clean up the orphaned target mark ourselves.
	if (!bHandedOffToStorm && PendingMarkHandle.IsValid())
	{
		if (UAbilitySystemComponent* MarkedASC = PendingMarkHandle.GetOwningAbilitySystemComponent())
		{
			MarkedASC->RemoveActiveGameplayEffect(PendingMarkHandle);
		}
		PendingMarkHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCPP_BossSandStormChaseAbility::ClearActiveTimers()
{
	if (const UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SpawnTimerHandle);
		TimerManager.ClearTimer(FinishTimerHandle);
	}
}
