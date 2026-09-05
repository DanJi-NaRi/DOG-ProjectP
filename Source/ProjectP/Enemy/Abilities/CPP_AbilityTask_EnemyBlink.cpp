// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_AbilityTask_EnemyBlink.h"

#include "Enemy/Core/CPP_EnemyAIC.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UCPP_AbilityTask_EnemyBlink* UCPP_AbilityTask_EnemyBlink::EnemyBlink(
	UGameplayAbility* OwningAbility,
	ACPP_EnemyBase* InEnemy,
	float InVanishDuration,
	float InReappearBehindGap,
	float InInvincibleDuration,
	FGameplayTag InVanishCueTag,
	FGameplayTag InReappearCueTag
)
{
	UCPP_AbilityTask_EnemyBlink* Task = NewAbilityTask<UCPP_AbilityTask_EnemyBlink>(OwningAbility);
	Task->Enemy = InEnemy;
	Task->VanishDuration = FMath::Max(InVanishDuration, 0.0f);
	Task->ReappearBehindGap = FMath::Max(InReappearBehindGap, 0.0f);
	Task->InvincibleDuration = FMath::Max(InInvincibleDuration, 0.0f);
	Task->VanishCueTag = InVanishCueTag;
	Task->ReappearCueTag = InReappearCueTag;
	return Task;
}

////////////////////////////
//! \author HanUl
//! \brief 소멸 시작: 이동을 정지·동결(MOVE_None)하고 숨김+콜리전 off, 무적 쿼리 등록(재등장 전까진 비활성),
//!        소멸 큐 재생. VanishDuration 후 HandleVanishElapsed가 재등장을 이어받는다.
//! \param
//! \return
void UCPP_AbilityTask_EnemyBlink::Activate()
{
	ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (!IsValid(EnemyActor) || !EnemyActor->HasAuthority())
	{
		EndTask();
		return;
	}

	if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(EnemyActor->GetController()))
	{
		EnemyAIC->StopMovementForAttack();
	}
	if (UCharacterMovementComponent* MovementComponent = EnemyActor->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->SetMovementMode(MOVE_None);
	}

	RegisterInvincibilityQuery();

	ExecuteCosmeticCue(VanishCueTag);
	EnemyActor->SetVanishedFromAbility(true);
	bVanishStateApplied = true;

	EnemyActor->GetWorldTimerManager().SetTimer(
		VanishTimerHandle, this, &UCPP_AbilityTask_EnemyBlink::HandleVanishElapsed,
		FMath::Max(VanishDuration, KINDA_SMALL_NUMBER), false);
}

////////////////////////////
//! \author HanUl
//! \brief 소멸 종료: 콜리전을 먼저 켜고 타겟 후방으로 텔레포트(비겹침 보정), 이동 모드 복구, 재등장 큐 재생,
//!        무적 창을 열고, OnBlinkFinished로 소유 어빌리티의 후속 동작을 시작시킨다.
//! \param
//! \return
void UCPP_AbilityTask_EnemyBlink::HandleVanishElapsed()
{
	ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (!IsValid(EnemyActor) || EnemyActor->IsDead())
	{
		// 소멸 중 소실/사망 → 재등장·통지 없이 종료(어빌리티는 사망 경로로 정리됨).
		EndTask();
		return;
	}

	// 콜리전을 켠 상태여야 TeleportTo(FindTeleportSpot)가 폰/벽 관통을 보정한다.
	EnemyActor->SetVanishedFromAbility(false);
	bVanishStateApplied = false;

	FRotator FaceRotation = EnemyActor->GetActorRotation();
	const FVector ReappearLocation = ComputeReappearLocation(FaceRotation);
	EnemyActor->TeleportTo(ReappearLocation, FaceRotation);

	RestoreMovementMode();
	ExecuteCosmeticCue(ReappearCueTag);

	if (InvincibleDuration > 0.0f)
	{
		bInvincibleWindowActive = true;
		EnemyActor->GetWorldTimerManager().SetTimer(
			InvincibleTimerHandle, this, &UCPP_AbilityTask_EnemyBlink::HandleInvincibleElapsed,
			InvincibleDuration, false);
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlinkFinished.Broadcast();
	}
}

void UCPP_AbilityTask_EnemyBlink::HandleInvincibleElapsed()
{
	bInvincibleWindowActive = false;
	EndTask();
}

void UCPP_AbilityTask_EnemyBlink::OnDestroy(bool bInOwnerFinished)
{
	ClearTimers();
	bInvincibleWindowActive = false;
	UnregisterInvincibilityQuery();

	// 소멸 상태로 어빌리티가 먼저 끝나면(취소/사망 등) 살아있는 유령이 되지 않도록 표시/콜리전/이동을 복구한다.
	if (bVanishStateApplied)
	{
		if (ACPP_EnemyBase* EnemyActor = Enemy.Get())
		{
			EnemyActor->SetVanishedFromAbility(false);
			RestoreMovementMode();
		}
	}
	bVanishStateApplied = false;

	Super::OnDestroy(bInOwnerFinished);
}

////////////////////////////
//! \author HanUl
//! \brief 재등장 위치 계산: 타겟 후방(정면 반대), 중심 거리 = ReappearBehindGap + 양쪽 캡슐 반경. 내비 투영으로
//!        벽 너머/절벽을 막고, 타겟이 무효면 제자리를 반환한다.
//! \param OutFaceRotation 재등장 시 바라볼 방향(타겟 방향 요)
//! \return 재등장 위치(월드)
FVector UCPP_AbilityTask_EnemyBlink::ComputeReappearLocation(FRotator& OutFaceRotation) const
{
	ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (!IsValid(EnemyActor))
	{
		return FVector::ZeroVector;
	}

	const float MyCapsuleRadius = EnemyActor->GetCapsuleComponent() ? EnemyActor->GetCapsuleComponent()->GetScaledCapsuleRadius() : 35.0f;
	const float MyCapsuleHalfHeight = EnemyActor->GetCapsuleComponent() ? EnemyActor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;

	AActor* TargetActor = EnemyActor->GetCurrentTargetActor();
	if (!IsValid(TargetActor))
	{
		OutFaceRotation = EnemyActor->GetActorRotation();
		return EnemyActor->GetActorLocation();
	}

	float TargetCapsuleRadius = 35.0f;
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		if (const UCapsuleComponent* TargetCapsule = TargetCharacter->GetCapsuleComponent())
		{
			TargetCapsuleRadius = TargetCapsule->GetScaledCapsuleRadius();
		}
	}

	FVector BehindDirection = -TargetActor->GetActorForwardVector().GetSafeNormal2D();
	if (BehindDirection.IsNearlyZero())
	{
		BehindDirection = -EnemyActor->GetActorForwardVector().GetSafeNormal2D();
	}

	const float CenterDistance = ReappearBehindGap + MyCapsuleRadius + TargetCapsuleRadius;
	FVector Candidate = TargetActor->GetActorLocation() + BehindDirection * CenterDistance;

	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EnemyActor->GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(600.0f, 600.0f, 250.0f)))
		{
			Candidate = Projected.Location;
			Candidate.Z += MyCapsuleHalfHeight;
		}
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - Candidate;
	OutFaceRotation = FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	return Candidate;
}

////////////////////////////
//! \author HanUl
//! \brief 무적 창 중 외부(다른 액터 발) GameplayEffect 적용을 거부한다. 자가 적용(쿨다운 등)은 허용.
//! \param ActiveGEContainer 대상 ASC의 활성 GE 컨테이너
//! \param SpecToApply 적용하려는 GE 스펙
//! \return 적용을 허용하면 true
bool UCPP_AbilityTask_EnemyBlink::ShouldAllowGameplayEffectApplication(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& SpecToApply) const
{
	if (!bInvincibleWindowActive)
	{
		return true;
	}

	const AActor* Instigator = SpecToApply.GetEffectContext().GetInstigator();
	return !Instigator || Instigator == Enemy.Get();
}

void UCPP_AbilityTask_EnemyBlink::RegisterInvincibilityQuery()
{
	const ACPP_EnemyBase* EnemyActor = Enemy.Get();
	UAbilitySystemComponent* ASC = EnemyActor ? EnemyActor->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	UnregisterInvincibilityQuery();

	FGameplayEffectApplicationQuery& NewQuery = ASC->GameplayEffectApplicationQueries.AddDefaulted_GetRef();
	NewQuery.BindUObject(this, &UCPP_AbilityTask_EnemyBlink::ShouldAllowGameplayEffectApplication);
}

void UCPP_AbilityTask_EnemyBlink::UnregisterInvincibilityQuery()
{
	const ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (UAbilitySystemComponent* ASC = EnemyActor ? EnemyActor->GetAbilitySystemComponent() : nullptr)
	{
		ASC->GameplayEffectApplicationQueries.RemoveAll([this](const FGameplayEffectApplicationQuery& Query)
		{
			return Query.IsBoundToObject(this);
		});
	}
}

void UCPP_AbilityTask_EnemyBlink::ExecuteCosmeticCue(const FGameplayTag& CueTag) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	const ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (UAbilitySystemComponent* ASC = EnemyActor ? EnemyActor->GetAbilitySystemComponent() : nullptr)
	{
		ASC->ExecuteGameplayCue(CueTag, ASC->MakeEffectContext());
	}
}

void UCPP_AbilityTask_EnemyBlink::RestoreMovementMode() const
{
	ACPP_EnemyBase* EnemyActor = Enemy.Get();
	if (!IsValid(EnemyActor) || EnemyActor->IsDead())
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = EnemyActor->GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
}

void UCPP_AbilityTask_EnemyBlink::ClearTimers()
{
	if (ACPP_EnemyBase* EnemyActor = Enemy.Get())
	{
		FTimerManager& TimerManager = EnemyActor->GetWorldTimerManager();
		TimerManager.ClearTimer(VanishTimerHandle);
		TimerManager.ClearTimer(InvincibleTimerHandle);
	}
}
