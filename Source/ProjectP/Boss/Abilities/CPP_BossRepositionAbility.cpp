#include "CPP_BossRepositionAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Boss/Abilities/CPP_AbilityTask_BossDash.h"
#include "Boss/Abilities/CPP_AnimNotify_BossAttackWindowEvent.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Core/CPP_BossTargetingComponent.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "Engine/World.h"
#include "GAS/MyAbilitySystemLibrary.h"

UCPP_BossRepositionAbility::UCPP_BossRepositionAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UCPP_BossRepositionAbility::ShouldAdvanceAtDistance(float DistanceToTarget) const
{
	return DistanceToTarget > TooFarDistance;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 타겟과의 수평 거리가 전진 기준보다 멀면 몽타주 기반 전진을 시작한다.
//!        Aim/Go 노티파이로 텔레그래프, 이동, 명중 피해를 처리한다.
//!        전진 몽타주 미지정 시 무모션 즉시 직진으로 강하한다.
void UCPP_BossRepositionAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Reset per-activation state (InstancedPerActor: same instance reused every activation).
	ActiveStepTask = nullptr;
	ActiveAdvanceMontageTask = nullptr;
	ActiveAdvanceEventTask = nullptr;
	AdvanceTelegraph = nullptr;
	PendingAdvanceTarget = nullptr;
	LockedAdvanceDirection = FVector::ForwardVector;
	LockedAdvanceDistance = 0.0f;
	bAdvanceDirectionLocked = false;
	bAdvanceStepStarted = false;

	if (!HasAuthority(&ActivationInfo))
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
	if (!BossAvatar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Target = nullptr;
	if (UCPP_BossTargetingComponent* TargetingComponent = BossAvatar->GetBossTargetingComponent())
	{
		Target = TargetingComponent->GetCurrentTarget();
	}
	if (!Target)
	{
		Target = UMyAbilitySystemLibrary::GetNearestLivingPlayer(BossAvatar, BossAvatar->GetActorLocation());
	}
	if (!Target)
	{
		// No living players; nothing to reposition against.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const float DistanceToTarget = FVector::Dist2D(BossAvatar->GetActorLocation(), Target->GetActorLocation());
	FVector ToTargetDirection = (Target->GetActorLocation() - BossAvatar->GetActorLocation()).GetSafeNormal2D();
	if (ToTargetDirection.IsNearlyZero())
	{
		// Overlapping the target; fall back to the boss facing so the direction stays deterministic.
		ToTargetDirection = BossAvatar->GetActorForwardVector().GetSafeNormal2D();
	}

	if (ShouldAdvanceAtDistance(DistanceToTarget))
	{
		PendingAdvanceTarget = Target;
		// Fallback lock in case the target dies before the Aim notify recomputes it.
		LockedAdvanceDirection = ToTargetDirection;
		LockedAdvanceDistance = ComputeAdvanceStepDistance(DistanceToTarget);

		if (AdvanceMontage)
		{
			StartMontageAdvance(BossAvatar);
		}
		else
		{
			// No montage wired yet: keep the boss functional with an immediate, telegraph-less step.
			bAdvanceDirectionLocked = true;
			BossAvatar->SetActorRotation(LockedAdvanceDirection.Rotation());
			StartAdvanceStep();
		}
		return;
	}

	// Already inside the comfort band (target moved during the decision delay); skip quietly.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 기반 전진을 시작한다: 몽타주 재생 + Event.Boss.Dash 윈도우 이벤트 대기.
//!        Aim 노티파이가 방향 고정/텔레그래프, Go 노티파이가 실제 이동을 트리거한다(돌진과 동일 구조).
//! \param BossAvatar 전진할 보스.
void UCPP_BossRepositionAbility::StartMontageAdvance(ACPP_BossCharacter* BossAvatar)
{
	ActiveAdvanceEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_Dash,
		nullptr,
		false,
		true
	);
	if (ActiveAdvanceEventTask)
	{
		ActiveAdvanceEventTask->EventReceived.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceWindowEvent);
		ActiveAdvanceEventTask->ReadyForActivation();
	}

	ActiveAdvanceMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AdvanceMontage,
		AdvanceMontagePlayRate
	);
	if (!ActiveAdvanceMontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	ActiveAdvanceMontageTask->OnCompleted.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceMontageFinished);
	ActiveAdvanceMontageTask->OnBlendOut.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceMontageFinished);
	ActiveAdvanceMontageTask->OnInterrupted.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceMontageAborted);
	ActiveAdvanceMontageTask->OnCancelled.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceMontageAborted);
	ActiveAdvanceMontageTask->ReadyForActivation();
}

////////////////////////////
//! \brief Aim/Go 윈도우 이벤트 분기(돌진과 동일한 Event.Boss.Dash + WindowId 페이로드).
void UCPP_BossRepositionAbility::HandleAdvanceWindowEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	const FName WindowId = WindowPayload ? WindowPayload->WindowId : NAME_None;

	if (WindowId == AdvanceAimWindowId)
	{
		LockAdvanceDirectionAndShowTelegraph();
	}
	else if (WindowId == AdvanceGoWindowId)
	{
		StartAdvanceStep();
	}
}

////////////////////////////
//! \author HanUl
//! \brief Aim 시점의 타겟 위치로 방향/거리를 다시 고정하고, 캡슐 폭 × 스텝 거리의 직선 텔레그래프를 스폰한다.
//!        타겟이 죽었으면 활성화 시점의 방향/거리를 그대로 쓴다.
void UCPP_BossRepositionAbility::LockAdvanceDirectionAndShowTelegraph()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!Avatar || !World)
	{
		return;
	}

	if (AActor* Target = PendingAdvanceTarget.Get())
	{
		const FVector ToTarget = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			LockedAdvanceDirection = ToTarget;
			LockedAdvanceDistance = ComputeAdvanceStepDistance(
				FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation()));
		}
	}
	bAdvanceDirectionLocked = true;

	// Snap the body to the step direction so the pose, telegraph, and movement all line up — focus rotation
	// is gated off during patterns, so the boss may still be mid-turn here. The wind-up masks the snap.
	Avatar->SetActorRotation(LockedAdvanceDirection.Rotation());

	if (!AdvanceLineTelegraphActorClass || AdvanceTelegraph || LockedAdvanceDistance <= 0.0f)
	{
		return;
	}

	const UCapsuleComponent* BossCapsule = Cast<ACharacter>(Avatar) ? Cast<ACharacter>(Avatar)->GetCapsuleComponent() : nullptr;
	const float CapsuleRadius = BossCapsule ? BossCapsule->GetScaledCapsuleRadius() : 100.0f;
	const float CapsuleHalfHeight = BossCapsule ? BossCapsule->GetScaledCapsuleHalfHeight() : 100.0f;

	FBossHitShapeData StepShape;
	StepShape.Shape = EBossAttackShape::Rectangle;
	StepShape.ForwardLength = LockedAdvanceDistance;
	StepShape.HalfWidth = CapsuleRadius;
	StepShape.HalfHeight = CapsuleHalfHeight;

	const FTransform TelegraphTransform(LockedAdvanceDirection.Rotation(), Avatar->GetActorLocation());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Avatar;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AdvanceTelegraph = World->SpawnActor<ACPP_BossTelegraphActor>(AdvanceLineTelegraphActorClass, TelegraphTransform, SpawnParameters);
	if (AdvanceTelegraph)
	{
		// Fill duration is read straight from the montage: remaining real time until the Go window notify
		// (play-rate aware, so the AdvanceMontagePlayRate speed-up is handled automatically).
		const ACharacter* BossCharacter = Cast<ACharacter>(Avatar);
		const UAnimInstance* AnimInstance = (BossCharacter && BossCharacter->GetMesh()) ? BossCharacter->GetMesh()->GetAnimInstance() : nullptr;
		const float FillDuration = UCPP_AnimNotify_BossAttackWindowEvent::ComputeTimeUntilWindowNotify(AnimInstance, AdvanceMontage, AdvanceGoWindowId);

		AdvanceTelegraph->Initialize(TelegraphTransform, StepShape, FillDuration);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 고정된 방향으로 전진 이동을 시작한다. 텔레그래프를 걷고, 몽타주가 있으면 이동 동안 일시정지해
//!        '내딛는 포즈'를 유지한다(돌진과 동일 기법).
void UCPP_BossRepositionAbility::StartAdvanceStep()
{
	if (bAdvanceStepStarted)
	{
		return;
	}
	bAdvanceStepStarted = true;

	DestroyAdvanceTelegraph();

	ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	if (!BossAvatar || !bAdvanceDirectionLocked || LockedAdvanceDistance <= 0.0f)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	ActiveStepTask = StartStepTask(BossAvatar, LockedAdvanceDirection, LockedAdvanceDistance, AdvanceStepSpeed);
	if (!ActiveStepTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	ActiveStepTask->OnDashFinished.AddDynamic(this, &UCPP_BossRepositionAbility::HandleAdvanceStepFinished);
	ActiveStepTask->ReadyForActivation();

	PauseAdvanceMontage();
}

////////////////////////////
//! \author HanUl
//! \brief 전진 종료: 정지 지점에서 맞은 적대 폰에 공격력×AdvanceDamageCoefficient 피해와 넉백을 적용하고
//!        어빌리티를 끝낸다(몽타주는 태스크 정리가 블렌드 아웃).
//! \param HitPawns 스텝 정지 지점에서 캡슐에 맞은 폰들.
void UCPP_BossRepositionAbility::HandleAdvanceStepFinished(const TArray<AActor*>& HitPawns)
{
	const ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (BossAvatar && SourceASC)
	{
		for (AActor* HitPawn : HitPawns)
		{
			if (!IsValid(HitPawn) || !UMyAbilitySystemLibrary::IsHostile(BossAvatar, HitPawn))
			{
				continue;
			}

			if (AdvanceDamageCoefficient > 0.0f && BossAvatar->GetBossDamageGameplayEffect())
			{
				UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
					SourceASC,
					HitPawn,
					BossAvatar->GetBossDamageGameplayEffect(),
					AdvanceDamageCoefficient,
					1.0f,
					AdvanceCurseGaugeAmount
				);
			}

			if (AdvanceKnockbackStrength > 0.0f)
			{
				if (ACharacter* HitCharacter = Cast<ACharacter>(HitPawn))
				{
					HitCharacter->LaunchCharacter(LockedAdvanceDirection * AdvanceKnockbackStrength, true, false);
				}
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

////////////////////////////
//! \brief 몽타주가 이동 시작 전에 끝났다(Go 노티파이 누락 등). 이동 없이 조용히 종료한다.
void UCPP_BossRepositionAbility::HandleAdvanceMontageFinished()
{
	if (!bAdvanceStepStarted)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UCPP_BossRepositionAbility::HandleAdvanceMontageAborted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCPP_BossRepositionAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	DestroyAdvanceTelegraph();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCPP_BossRepositionAbility::DestroyAdvanceTelegraph()
{
	if (AdvanceTelegraph)
	{
		AdvanceTelegraph->Destroy();
		AdvanceTelegraph = nullptr;
	}
}

////////////////////////////
//! \brief 전진 몽타주를 일시정지한다(이동 동안 포즈 유지). 어빌리티 종료 시 태스크 정리가 블렌드 아웃한다.
void UCPP_BossRepositionAbility::PauseAdvanceMontage()
{
	if (!AdvanceMontage)
	{
		return;
	}

	const ACharacter* BossCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* Mesh = BossCharacter ? BossCharacter->GetMesh() : nullptr;
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Pause(AdvanceMontage);
	}
}

//! \brief 과접근 방지: 전진은 AdvanceStopDistance를 넘지 않는다. 마지막 스텝만 짧아질 수 있다.
float UCPP_BossRepositionAbility::ComputeAdvanceStepDistance(float DistanceToTarget) const
{
	return FMath::Max(FMath::Min(AdvanceStepDistance, DistanceToTarget - AdvanceStopDistance), 0.0f);
}

UCPP_AbilityTask_BossDash* UCPP_BossRepositionAbility::StartStepTask(
	ACPP_BossCharacter* BossAvatar,
	const FVector& StepDirection,
	float StepDistance,
	float StepSpeed
)
{
	const UCapsuleComponent* BossCapsule = BossAvatar->GetCapsuleComponent();
	const float CapsuleRadius = BossCapsule ? BossCapsule->GetScaledCapsuleRadius() : 100.0f;
	const float CapsuleHalfHeight = BossCapsule ? BossCapsule->GetScaledCapsuleHalfHeight() : 100.0f;

	return UCPP_AbilityTask_BossDash::BossDash(
		this,
		BossAvatar,
		StepDirection,
		StepDistance,
		StepSpeed,
		CapsuleRadius,
		CapsuleHalfHeight
	);
}
