#include "CPP_BossSpiralLightningAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Actors/CPP_BossRockWarningActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "MyGameplayTags.h"

UCPP_BossSpiralLightningAbility::UCPP_BossSpiralLightningAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

////////////////////////////
//! \brief 좌/우를 무작위로 골라 나선 점을 계산하고, 시작 몽타주 재생 + 주기적 번개 생성 타이머를 건다.
void UCPP_BossSpiralLightningAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Reset per-activation state (InstancedPerActor: same instance reused every activation).
	SpiralPoints.Reset();
	NextLightningIndex = 0;
	bSpiralStarted = false;

	if (!HasAuthority(&ActivationInfo) || !LightningActorClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = BossAvatar ? BossAvatar->GetWorld() : nullptr;
	if (!BossAvatar || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bLeftHand = FMath::RandBool();

	ComputeSpiralPoints(bLeftHand, BossAvatar->GetActorLocation(), BossAvatar->GetActorForwardVector());
	if (SpiralPoints.Num() == 0)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Start-of-pattern cast montage for the chosen hand. Spiral spawning begins at a montage notify
	// (Event.Boss.AttackWindow) so the lightning starts on the authored cast frame; montage end is a fallback.
	if (UAnimMontage* Montage = bLeftHand ? LeftHandMontage : RightHandMontage)
	{
		ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage);
		if (ActiveMontageTask)
		{
			ActiveMontageTask->OnCompleted.AddDynamic(this, &UCPP_BossSpiralLightningAbility::HandleMontageEnded);
			ActiveMontageTask->OnBlendOut.AddDynamic(this, &UCPP_BossSpiralLightningAbility::HandleMontageEnded);
			ActiveMontageTask->OnInterrupted.AddDynamic(this, &UCPP_BossSpiralLightningAbility::HandleMontageEnded);
			ActiveMontageTask->OnCancelled.AddDynamic(this, &UCPP_BossSpiralLightningAbility::HandleMontageEnded);
			ActiveMontageTask->ReadyForActivation();
		}
	}

	ActiveSpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_AttackWindow,
		nullptr,
		false,
		true
	);
	if (ActiveSpawnEventTask)
	{
		ActiveSpawnEventTask->EventReceived.AddDynamic(this, &UCPP_BossSpiralLightningAbility::HandleSpawnStartEvent);
		ActiveSpawnEventTask->ReadyForActivation();
	}
}

void UCPP_BossSpiralLightningAbility::HandleSpawnStartEvent(FGameplayEventData Payload)
{
	StartSpiralSpawning();
}

void UCPP_BossSpiralLightningAbility::HandleMontageEnded()
{
	// Fallback: if the cast montage ended without the spawn-start notify, begin the spiral now.
	StartSpiralSpawning();
}

////////////////////////////
//! \brief 나선 번개 생성을 시작한다(노티파이 또는 몽타주 종료 폴백으로 1회만). SpawnInterval마다 SpawnNextLightning.
void UCPP_BossSpiralLightningAbility::StartSpiralSpawning()
{
	if (bSpiralStarted)
	{
		return;
	}
	bSpiralStarted = true;

	UWorld* World = GetWorld();
	if (!World || SpiralPoints.Num() == 0)
	{
		HandleFinished();
		return;
	}

	// Spawn one lightning every SpawnInterval (first after one interval).
	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&UCPP_BossSpiralLightningAbility::SpawnNextLightning,
		SpawnInterval,
		true,
		SpawnInterval
	);
}

////////////////////////////
//! \brief 보스 위치를 중심으로 좌/우에 따른 나선 점 목록을 계산한다.
//!        왼손: 시계방향, 바깥→안쪽. 오른손: 반시계방향, 안쪽→바깥.
void UCPP_BossSpiralLightningAbility::ComputeSpiralPoints(bool bLeftHand, const FVector& Center, const FVector& ForwardDirection)
{
	SpiralPoints.Reset();

	const int32 Count = FMath::Max(LightningCount, 1);
	const float TotalAngleDegrees = Revolutions * 360.0f;
	const float StartYawDegrees = ForwardDirection.Rotation().Yaw;

	const float StartRadius = bLeftHand ? OuterRadius : InnerRadius;
	const float EndRadius = bLeftHand ? InnerRadius : OuterRadius;
	const float DirectionSign = bLeftHand ? -1.0f : 1.0f; // 왼손=시계(-), 오른손=반시계(+)

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Fraction = (Count > 1) ? static_cast<float>(Index) / static_cast<float>(Count - 1) : 0.0f;
		const float Radius = FMath::Lerp(StartRadius, EndRadius, Fraction);
		const float YawRadians = FMath::DegreesToRadians(StartYawDegrees + DirectionSign * Fraction * TotalAngleDegrees);

		const FVector Point(
			Center.X + Radius * FMath::Cos(YawRadians),
			Center.Y + Radius * FMath::Sin(YawRadians),
			Center.Z
		);
		SpiralPoints.Add(Point);
	}
}

////////////////////////////
//! \brief 다음 나선 점에 번개(경고→낙하) 액터를 스폰한다. 마지막을 스폰하면 낙하 대기 후 종료를 예약한다.
void UCPP_BossSpiralLightningAbility::SpawnNextLightning()
{
	if (!HasAuthority(&CurrentActivationInfo) || !SpiralPoints.IsValidIndex(NextLightningIndex))
	{
		return;
	}

	const ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = BossAvatar ? BossAvatar->GetWorld() : nullptr;
	if (BossAvatar && SourceASC && World && LightningActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = const_cast<ACPP_BossCharacter*>(BossAvatar);
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACPP_BossRockWarningActor* Lightning = World->SpawnActor<ACPP_BossRockWarningActor>(
			LightningActorClass,
			SpiralPoints[NextLightningIndex],
			FRotator::ZeroRotator,
			SpawnParameters
		);

		if (Lightning)
		{
			Lightning->Initialize(SourceASC, BossAvatar->GetBossDamageGameplayEffect());
		}
	}

	++NextLightningIndex;

	if (NextLightningIndex >= SpiralPoints.Num())
	{
		// Last lightning spawned: stop the loop and end the ability once its warning resolves (the strike lands).
		if (UWorld* TimerWorld = GetWorld())
		{
			TimerWorld->GetTimerManager().ClearTimer(SpawnTimerHandle);
			TimerWorld->GetTimerManager().SetTimer(
				FinishTimerHandle,
				this,
				&UCPP_BossSpiralLightningAbility::HandleFinished,
				FMath::Max(LightningWarningDuration, 0.01f),
				false
			);
		}
	}
}

void UCPP_BossSpiralLightningAbility::HandleFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

const FGameplayTagContainer* UCPP_BossSpiralLightningAbility::GetCooldownTags() const
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
void UCPP_BossSpiralLightningAbility::ApplyCooldown(
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

float UCPP_BossSpiralLightningAbility::GetCooldownSeconds() const
{
	return CooldownSeconds;
}

void UCPP_BossSpiralLightningAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	if (const UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SpawnTimerHandle);
		TimerManager.ClearTimer(FinishTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
