// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemySplitAbility.h"

#include "Enemy/Core/CPP_EnemyAIC.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "MyGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"

////////////////////////////
//! \author HanUl
//! \brief 분열 발동: 원본을 숨기고(피격 불가) 이동을 정지한 뒤, 중앙 마커 좌우에 절반 HP 분신 2기를 스폰한다.
//!        SplitDuration 후 또는 두 분신 사망 시 병합한다.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return
void UCPP_EnemySplitAbility::ActivateAbility(
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
	bMerged = false;
	Clones.Reset();
	EnemyAvatar->SetActiveSplitAbility(this);

	// 분열 예고: 원본은 보인 채 정지하고 예고 큐를 재생한다(피격 가능 — 이 창에 버스트로 저지 가능).
	if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(EnemyAvatar->GetController()))
	{
		EnemyAIC->StopMovementForAttack();
	}
	ExecuteCosmeticCue(SplitCueTag);

	if (UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(SplitWindupDuration, KINDA_SMALL_NUMBER)))
	{
		WindupTask->OnFinish.AddDynamic(this, &UCPP_EnemySplitAbility::HandleSplitWindupElapsed);
		WindupTask->ReadyForActivation();
	}
	else
	{
		HandleSplitWindupElapsed();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 분열 예고 종료: 원본을 숨기고(피격 불가) 중앙 마커 좌우에 절반 HP 분신 2기를 스폰한다.
//!        HP 스냅샷은 이 시점(실제 분열 순간)에 잡는다 — 예고 중 피격으로 HP가 바뀌어도 정확하다.
//! \param
//! \return
void UCPP_EnemySplitAbility::HandleSplitWindupElapsed()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar || EnemyAvatar->IsDead())
	{
		return; // 예고 중 사망 등 → 사망 경로가 정리(분열 미발생)
	}

	OriginalMaxHealth = EnemyAvatar->GetMaxHealth();
	const float SplitHealth = EnemyAvatar->GetHealth() * 0.5f;

	// 원본 숨김(피격 불가) + 이동 정지/동결.
	SetOriginalVanished(EnemyAvatar, true);

	// 중앙 마커 좌우에 분신 2기 스폰.
	FRotator MarkerRotation = FRotator::ZeroRotator;
	const FVector Center = GetCenterLocation(MarkerRotation);
	const FVector RightDir = MarkerRotation.Quaternion().GetRightVector().GetSafeNormal2D();

	if (ACPP_EnemyBase* CloneLeft = SpawnClone(Center - RightDir * CloneSideSpacing, MarkerRotation, SplitHealth))
	{
		Clones.Add(CloneLeft);
	}
	if (ACPP_EnemyBase* CloneRight = SpawnClone(Center + RightDir * CloneSideSpacing, MarkerRotation, SplitHealth))
	{
		Clones.Add(CloneRight);
	}

	// 지속 타이머.
	if (UAbilityTask_WaitDelay* DurationTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(SplitDuration, KINDA_SMALL_NUMBER)))
	{
		DurationTask->OnFinish.AddDynamic(this, &UCPP_EnemySplitAbility::HandleSplitDurationElapsed);
		DurationTask->ReadyForActivation();
	}
	else
	{
		Merge();
	}
}

void UCPP_EnemySplitAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 외부 강제 종료(사망 등)로 병합 전에 끝나면: 남은 분신 정리 + 숨겨진 원본 복구.
	DespawnRemainingClones();

	if (bOriginalVanished)
	{
		if (ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(ActorInfo))
		{
			SetOriginalVanished(EnemyAvatar, false);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* UCPP_EnemySplitAbility::GetCooldownTags() const
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
void UCPP_EnemySplitAbility::ApplyCooldown(
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
//! \brief 외부 강제 종료(사망 → StopCurrentMontage → CancelActiveAttackAbilities)용 정리. 어빌리티를 닫는다.
//!        분신 정리·원본 복구는 EndAbility가 처리한다.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemySplitAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveSplitAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

void UCPP_EnemySplitAbility::HandleSplitDurationElapsed()
{
	Merge();
}

////////////////////////////
//! \author HanUl
//! \brief 분신 사망 콜백. 살아있는 분신이 하나도 없으면 즉시 병합한다(하나만 죽으면 계속).
//! \param
//! \return
void UCPP_EnemySplitAbility::HandleCloneDied()
{
	if (bMerged)
	{
		return;
	}

	for (const TWeakObjectPtr<ACPP_EnemyBase>& Clone : Clones)
	{
		if (Clone.IsValid() && !Clone->IsDead())
		{
			return; // 아직 살아있는 분신 있음 → 계속
		}
	}

	Merge();
}

////////////////////////////
//! \author HanUl
//! \brief 병합 시작: 남은 분신 HP를 합산·기억하고 분신을 소멸시킨 뒤, 등장 연출 시간만큼 뜬다(모으기 연출).
//!        연출 후 HandleMergeWindupElapsed가 원본을 실제로 복귀시킨다.
//! \param
//! \return
void UCPP_EnemySplitAbility::Merge()
{
	if (bMerged)
	{
		return;
	}
	bMerged = true;

	// 살아있는 분신 HP 합산(죽은/무효 분신은 0 기여)해 기억하고 분신 소멸.
	PendingMergeHealth = 0.0f;
	for (const TWeakObjectPtr<ACPP_EnemyBase>& Clone : Clones)
	{
		if (Clone.IsValid() && !Clone->IsDead())
		{
			PendingMergeHealth += Clone->GetHealth();
		}
	}
	DespawnRemainingClones();

	ExecuteCosmeticCue(ReturnCueTag);

	if (UAbilityTask_WaitDelay* WindupTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(MergeWindupDuration, KINDA_SMALL_NUMBER)))
	{
		WindupTask->OnFinish.AddDynamic(this, &UCPP_EnemySplitAbility::HandleMergeWindupElapsed);
		WindupTask->ReadyForActivation();
	}
	else
	{
		HandleMergeWindupElapsed();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 등장 연출 종료: 원본을 마커 위치에 복귀시키고 합산 HP로 설정한다.
//!        합이 0이면 원본 사망, 아니면 ReturnStaggerDuration 동안 무방비 대기 후 종료.
//! \param
//! \return
void UCPP_EnemySplitAbility::HandleMergeWindupElapsed()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar)
	{
		return;
	}

	// 마커 위치로 복귀 + 콜리전 복구.
	FRotator MarkerRotation = FRotator::ZeroRotator;
	const FVector Center = GetCenterLocation(MarkerRotation);
	SetOriginalVanished(EnemyAvatar, false);
	EnemyAvatar->TeleportTo(Center, EnemyAvatar->GetActorRotation());

	EnemyAvatar->SetHealthValues(OriginalMaxHealth, PendingMergeHealth);

	if (PendingMergeHealth <= 0.0f)
	{
		// 두 분신 모두 죽음 → 원본은 0으로 돌아와 사망(보상 지급 대상).
		EnemyAvatar->ClearActiveSplitAbility(this);
		if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
		{
			EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
		}
		EnemyAvatar->ForceKill();
		return;
	}

	// 무방비 대기(피격 가능) 후 패턴 종료.
	if (UAbilityTask_WaitDelay* StaggerTask = UAbilityTask_WaitDelay::WaitDelay(this, FMath::Max(ReturnStaggerDuration, KINDA_SMALL_NUMBER)))
	{
		StaggerTask->OnFinish.AddDynamic(this, &UCPP_EnemySplitAbility::HandleReturnStaggerElapsed);
		StaggerTask->ReadyForActivation();
	}
	else
	{
		FinishSplit();
	}
}

void UCPP_EnemySplitAbility::HandleReturnStaggerElapsed()
{
	FinishSplit();
}

void UCPP_EnemySplitAbility::FinishSplit()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveSplitAbility(this);
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

////////////////////////////
//! \author HanUl
//! \brief 원본과 같은 클래스의 분신을 스폰하고, 분신 플래그·HP를 설정하고 사망 델리게이트를 바인드한다.
//!        SpawnActorDeferred로 BeginPlay(ASC 초기화) 전에 분신 플래그를 세워 보상 제외/분열 금지를 보장한다.
//! \param SpawnLocation 스폰 위치
//! \param SpawnRotation 스폰 회전
//! \param CloneHealth 분신의 최대=현재 체력
//! \return 스폰된 분신(실패 시 nullptr)
ACPP_EnemyBase* UCPP_EnemySplitAbility::SpawnClone(const FVector& SpawnLocation, const FRotator& SpawnRotation, float CloneHealth)
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	UWorld* World = EnemyAvatar ? EnemyAvatar->GetWorld() : nullptr;
	if (!EnemyAvatar || !World)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	ACPP_EnemyBase* Clone = World->SpawnActorDeferred<ACPP_EnemyBase>(
		EnemyAvatar->GetClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Clone)
	{
		return nullptr;
	}

	Clone->SetEnemyLevel(EnemyAvatar->GetEnemyLevel());
	Clone->ConfigureSpawnTarget(
		EnemyAvatar->GetSpawnTargetPolicy(),
		EnemyAvatar->GetAssignedObjectiveTarget());
	Clone->SetAbilitySpawnedMinion(true); // BeginPlay 전에 세팅 → 보상 제외/분열 금지 보장
	UGameplayStatics::FinishSpawningActor(Clone, SpawnTransform);

	// BeginPlay(ASC 초기화)가 끝난 뒤 HP를 절반으로 설정.
	Clone->SetHealthValues(CloneHealth, CloneHealth);
	Clone->OnEnemyDeath.AddDynamic(this, &UCPP_EnemySplitAbility::HandleCloneDied);
	return Clone;
}

////////////////////////////
//! \author HanUl
//! \brief 맵 중앙 마커를 액터 태그로 조회한다(스포너 소환 대응). 미발견 시 자기 위치/회전을 반환.
//! \param OutMarkerRotation 마커(또는 폴백 자기)의 회전
//! \return 중앙 위치(월드)
FVector UCPP_EnemySplitAbility::GetCenterLocation(FRotator& OutMarkerRotation) const
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	UWorld* World = EnemyAvatar ? EnemyAvatar->GetWorld() : nullptr;
	if (!EnemyAvatar || !World)
	{
		OutMarkerRotation = FRotator::ZeroRotator;
		return FVector::ZeroVector;
	}

	if (CenterMarkerTag != NAME_None)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(World, CenterMarkerTag, FoundActors);
		for (AActor* Marker : FoundActors)
		{
			if (IsValid(Marker))
			{
				OutMarkerRotation = Marker->GetActorRotation();
				return Marker->GetActorLocation();
			}
		}
	}

	OutMarkerRotation = EnemyAvatar->GetActorRotation();
	return EnemyAvatar->GetActorLocation();
}

void UCPP_EnemySplitAbility::DespawnRemainingClones()
{
	for (const TWeakObjectPtr<ACPP_EnemyBase>& Clone : Clones)
	{
		if (Clone.IsValid())
		{
			// 살아있는 분신은 조용히 제거(사망 처리·보상 없음). 이미 죽은 분신은 자기 수명으로 정리된다.
			if (!Clone->IsDead())
			{
				Clone->Destroy();
			}
		}
	}
	Clones.Reset();
}

////////////////////////////
//! \author HanUl
//! \brief 원본을 숨김/복구한다. 숨김 시 이동 정지+동결(콜리전 off로 낙하 방지), 복구 시 이동 모드 회복.
//! \param EnemyAvatar 원본
//! \param bVanished true면 숨김, false면 복구
//! \return
void UCPP_EnemySplitAbility::SetOriginalVanished(ACPP_EnemyBase* EnemyAvatar, bool bVanished)
{
	if (!EnemyAvatar)
	{
		return;
	}

	if (bVanished)
	{
		if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(EnemyAvatar->GetController()))
		{
			EnemyAIC->StopMovementForAttack();
		}
		if (UCharacterMovementComponent* MovementComponent = EnemyAvatar->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->SetMovementMode(MOVE_None);
		}
		EnemyAvatar->SetVanishedFromAbility(true);
		bOriginalVanished = true;
	}
	else
	{
		EnemyAvatar->SetVanishedFromAbility(false);
		if (!EnemyAvatar->IsDead())
		{
			if (UCharacterMovementComponent* MovementComponent = EnemyAvatar->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}
		bOriginalVanished = false;
	}
}

void UCPP_EnemySplitAbility::ExecuteCosmeticCue(const FGameplayTag& CueTag) const
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

ACPP_EnemyBase* UCPP_EnemySplitAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}
