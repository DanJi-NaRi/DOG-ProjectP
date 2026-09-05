#include "CPP_BossDashSectorAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Boss/Abilities/CPP_AbilityTask_BossDash.h"
#include "Boss/Abilities/CPP_AnimNotify_BossAttackWindowEvent.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Core/CPP_BossAttributeSet.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GAS/MyAbilitySystemLibrary.h"

////////////////////////////
//! \brief 부모(공격 윈도우/몽타주/텔레그래프/부채꼴)를 그대로 구동하고, 돌진 이벤트 리스너만 추가한다.
void UCPP_BossDashSectorAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Reset per-activation state (InstancedPerActor: same instance reused every activation).
	bDashDirectionLocked = false;
	DestroyDashTelegraph();

	// If the base ended the ability (e.g. missing AttackData/montage), don't set up the dash phase.
	if (!IsActive())
	{
		return;
	}

	ActiveDashEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_Dash,
		nullptr,
		false,
		true
	);

	if (ActiveDashEventTask)
	{
		ActiveDashEventTask->EventReceived.AddDynamic(this, &UCPP_BossDashSectorAbility::HandleDashEvent);
		ActiveDashEventTask->ReadyForActivation();
	}
}

void UCPP_BossDashSectorAbility::HandleDashEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	const FName WindowId = WindowPayload ? WindowPayload->WindowId : NAME_None;

	if (WindowId == DashAimWindowId)
	{
		LockDashDirectionAndShowTelegraph();
	}
	else if (WindowId == DashGoWindowId)
	{
		StartDash();
	}
}

////////////////////////////
//! \brief 돌진 방향을 현재 보스 정면으로 고정하고, 캡슐 폭/거리에 맞춘 직선 텔레그래프를 스폰한다.
void UCPP_BossDashSectorAbility::LockDashDirectionAndShowTelegraph()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!Avatar || !World)
	{
		return;
	}

	LockedDashDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
	bDashDirectionLocked = !LockedDashDirection.IsNearlyZero();

	if (!DashLineTelegraphActorClass || DashTelegraph)
	{
		return;
	}

	FBossHitShapeData DashShape;
	DashShape.Shape = EBossAttackShape::Rectangle;
	DashShape.ForwardLength = DashDistance;
	DashShape.HalfWidth = DashCapsuleWidth * 0.5f;
	DashShape.HalfHeight = DashCapsuleHalfHeight;

	const FTransform TelegraphTransform(LockedDashDirection.Rotation(), Avatar->GetActorLocation());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Avatar;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DashTelegraph = World->SpawnActor<ACPP_BossTelegraphActor>(DashLineTelegraphActorClass, TelegraphTransform, SpawnParameters);
	if (DashTelegraph)
	{
		// Fill duration is read straight from the montage: remaining real time until the Go window notify.
		const ACharacter* BossCharacter = Cast<ACharacter>(Avatar);
		const UAnimInstance* AnimInstance = (BossCharacter && BossCharacter->GetMesh()) ? BossCharacter->GetMesh()->GetAnimInstance() : nullptr;
		const UAnimMontage* DashMontage = AttackData ? AttackData->GetAttackMontage() : nullptr;
		const float FillDuration = UCPP_AnimNotify_BossAttackWindowEvent::ComputeTimeUntilWindowNotify(AnimInstance, DashMontage, DashGoWindowId);

		DashTelegraph->Initialize(TelegraphTransform, DashShape, FillDuration);
	}
}

////////////////////////////
//! \brief 고정된 방향으로 돌진 태스크를 시작한다(Aim 노티파이가 없었으면 시작 순간의 정면으로 폴백).
void UCPP_BossDashSectorAbility::StartDash()
{
	DestroyDashTelegraph();

	ACharacter* BossCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!BossCharacter)
	{
		return;
	}

	const FVector DashDirection = bDashDirectionLocked
		? LockedDashDirection
		: BossCharacter->GetActorForwardVector().GetSafeNormal2D();

	ActiveDashTask = UCPP_AbilityTask_BossDash::BossDash(
		this,
		BossCharacter,
		DashDirection,
		DashDistance,
		DashSpeed,
		DashCapsuleWidth * 0.5f,
		DashCapsuleHalfHeight
	);

	if (ActiveDashTask)
	{
		ActiveDashTask->OnDashFinished.AddDynamic(this, &UCPP_BossDashSectorAbility::HandleDashFinished);
		ActiveDashTask->ReadyForActivation();

		// Hold the "extended" montage pose (the frame at the Go notify) while the boss slides.
		SetDashMontagePaused(true);
	}
}

////////////////////////////
//! \brief 돌진이 맞춘 폰에 공격력×DashDamageCoefficient 피해를 적용하고, 몽타주를 후려치기 섹션으로 점프한다.
void UCPP_BossDashSectorAbility::HandleDashFinished(const TArray<AActor*>& HitPawns)
{
	const ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (BossAvatar && SourceASC && BossAvatar->GetBossDamageGameplayEffect() && DashDamageCoefficient > 0.0f)
	{
		for (AActor* HitPawn : HitPawns)
		{
			if (IsValid(HitPawn) && UMyAbilitySystemLibrary::IsHostile(BossAvatar, HitPawn))
			{
				UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
					SourceASC,
					HitPawn,
					BossAvatar->GetBossDamageGameplayEffect(),
					DashDamageCoefficient,
					1.0f,
					DashCurseGaugeAmount
				);
			}
		}
	}

	// Resume the montage (was paused to hold the dash pose) and jump to the rear-attack section, so the swing
	// plays exactly when the dash actually ends (incl. early wall/player stop).
	SetDashMontagePaused(false);

	if (!RearAttackSectionName.IsNone() && AttackData && AttackData->GetAttackMontage())
	{
		if (const ACharacter* BossCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (const USkeletalMeshComponent* Mesh = BossCharacter->GetMesh())
			{
				if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(RearAttackSectionName, AttackData->GetAttackMontage());
				}
			}
		}
	}
}

////////////////////////////
//! \brief 돌진 몽타주를 일시정지/재개한다. 돌진 동안 '지른 포즈'를 붙잡아 두기 위해 사용.
void UCPP_BossDashSectorAbility::SetDashMontagePaused(bool bPaused)
{
	if (!AttackData || !AttackData->GetAttackMontage())
	{
		return;
	}

	const ACharacter* BossCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* Mesh = BossCharacter ? BossCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	UAnimMontage* Montage = AttackData->GetAttackMontage();
	if (bPaused)
	{
		AnimInstance->Montage_Pause(Montage);
	}
	else
	{
		AnimInstance->Montage_Resume(Montage);
	}
}

void UCPP_BossDashSectorAbility::DestroyDashTelegraph()
{
	if (DashTelegraph)
	{
		DashTelegraph->Destroy();
		DashTelegraph = nullptr;
	}
}

void UCPP_BossDashSectorAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	DestroyDashTelegraph();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
