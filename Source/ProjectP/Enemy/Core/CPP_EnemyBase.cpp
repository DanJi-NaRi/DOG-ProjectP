// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyBase.h"

#include "Enemy/Spawning/CPP_EnemyWaveData.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Enemy/Core/CPP_EnemyAIC.h"
#include "Enemy/Core/CPP_EnemyCombatCoordinator.h"
#include "Enemy/Abilities/CPP_EnemyAreaAttackAbility.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Abilities/CPP_EnemyBeamAttackAbility.h"
#include "Enemy/Abilities/CPP_EnemyBlinkNovaAbility.h"
#include "Enemy/Abilities/CPP_EnemyDashAttackAbility.h"
#include "Enemy/Abilities/CPP_EnemyProjectileAttackAbility.h"
#include "Enemy/Abilities/CPP_EnemyShapeAttackAbility.h"
#include "Enemy/Abilities/CPP_EnemySplitAbility.h"
#include "Enemy/Core/CPP_EnemyStatTypes.h"
#include "Enemy/Abilities/CPP_EnemySummonAbility.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MyPlayerState.h"
#include "Item/MyInventoryComponent.h"
#include "MyGameplayTags.h"
#include "Player/PlayerCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

ACPP_EnemyBase::ACPP_EnemyBase()
{
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bUseControllerDesiredRotation = true;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
		MovementComponent->bUseRVOAvoidance = true;
		MovementComponent->AvoidanceConsiderationRadius = 300.0f;
	}

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MyAttributeSet = CreateDefaultSubobject<UMyAttributeSet>(TEXT("MyAttributeSet"));

	TrackedStatusEffectTags.AddTag(MyGameplayTags::Status_Debuff_Decay);
	DefaultCharacterTags.AddTag(MyGameplayTags::Character_Enemy.GetTag());
}

void ACPP_EnemyBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_EnemyBase, bIsDead);
}

void ACPP_EnemyBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateChaseAcceptableRadius();
	InitializeEnemyAbilitySystem();
}

UAbilitySystemComponent* ACPP_EnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

////////////////////////////
//! \author HanSeul
//! \brief 스폰을 요청한 시스템이 지정한 타겟 정책과 공용 목표 Actor 참조를 저장한다.
//! \param NewPolicy 새 타겟 정책
//! \param NewObjectiveTarget 고정 목표 정책에서 사용할 목표 Actor
void ACPP_EnemyBase::ConfigureSpawnTarget(
	EEnemySpawnTargetPolicy NewPolicy,
	AActor* NewObjectiveTarget)
{
	SpawnTargetPolicy = NewPolicy;
	AssignedObjectiveTarget = NewObjectiveTarget;
}

////////////////////////////
//! \author HanSeul
//! \brief Selects the available attack pattern with the longest base cooldown and activates its GAS ability.
//! \return true when the ability activation request succeeds.
bool ACPP_EnemyBase::ActivatePrimaryEnemyAbility()
{
	return ActivateEnemyAbilityBySelectionMode(EEnemyAttackPatternSelectionMode::Any);
}

////////////////////////////
//! \author HanSeul
//! \brief Selects and activates an enemy attack ability from the requested pattern group.
//! \param SelectionMode Pattern group used for selection.
//! \return true when the ability activation request succeeds.
bool ACPP_EnemyBase::ActivateEnemyAbilityBySelectionMode(EEnemyAttackPatternSelectionMode SelectionMode)
{
	if (!HasAuthority() || bIsDead || !AbilitySystemComponent)
	{
		return false;
	}

	if (SelectionMode == EEnemyAttackPatternSelectionMode::SpecialCondition
		&& IsValid(PendingForcedAttackPattern)
		&& DoesAttackPatternMeetConditions(PendingForcedAttackPattern))
	{
		ActiveAttackPattern = PendingForcedAttackPattern;
	}
	else
	{
		ActiveAttackPattern = SelectAvailableAttackPattern(SelectionMode);
	}
	const UCPP_EnemyAttackPatternData* AttackPattern = GetPrimaryAttackPattern();
	if (!AttackPattern)
	{
		return false;
	}
	const bool bIsForcedActivation = ActiveAttackPattern == PendingForcedAttackPattern;

	// 공격 토큰 확정(최종 허가). 상한을 넘어 못 잡으면 이번 공격 취소 — Chasing으로 돌아가 우글대다 자리가 나면 재시도.
	// 조정자/타겟이 없으면 게이트를 열어(fail-open) 데드락을 피한다.
	if (!bIsForcedActivation)
	{
		if (AActor* TargetPlayer = GetCurrentTargetActor())
		{
			if (const UWorld* World = GetWorld())
			{
				if (UEnemyCombatCoordinatorSubsystem* Coordinator = World->GetSubsystem<UEnemyCombatCoordinatorSubsystem>())
				{
					if (!Coordinator->TryAcquireAttackToken(this, TargetPlayer))
					{
						ActiveAttackPattern = nullptr;
						return false;
					}
				}
			}
		}
	}

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = AbilitySystemComponent->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (!AbilitySpec.Handle.IsValid())
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(AttackPattern->AbilityTag))
		{
			continue;
		}

		const FGameplayAbilitySpecHandle SpecHandle = AbilitySpec.Handle;
		ActiveAttackAbilityHandle = SpecHandle;
		bool bActivated = AbilitySystemComponent->TryActivateAbility(SpecHandle);
		if (!bActivated)
		{
			// 워치독: 어떤 경로로든 EndAbility가 누락돼 인스턴스가 활성으로 남으면(InstancedPerActor 재발동 불가)
			// 이 적은 영구히 공격을 못 한다. 감지 시 강제 종료 후 1회 재시도해 자가 복구한다.
			const FGameplayAbilitySpec* StuckSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(SpecHandle);
			if (StuckSpec && StuckSpec->IsActive())
			{
				CancelActiveAttackAbilities();
				AbilitySystemComponent->CancelAbilityHandle(SpecHandle);
				bActivated = AbilitySystemComponent->TryActivateAbility(SpecHandle);
			}
		}

		if (!bActivated)
		{
			BroadcastAttackFinished();
		}
		else
		{
			if (ActiveAttackPattern == PendingForcedAttackPattern)
			{
				PendingForcedAttackPattern = nullptr;
			}
		}
		return bActivated;
	}

	BroadcastAttackFinished();
	return false;
}

////////////////////////////
//! \author HanSeul
//! \brief Returns the current target tracked by this enemy's AI controller.
//! \return Current target actor, or nullptr when no valid AI target exists.
AActor* ACPP_EnemyBase::GetCurrentTargetActor() const
{
	const ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController());
	return EnemyAIC ? EnemyAIC->GetTargetActor() : nullptr;
}

////////////////////////////
//! \author HanSeul
//! \brief Plays the first attack pattern montage for a GAS ability.
//! \param TargetActor Actor to face during montage playback.
//! \return true when a montage asset exists and playback was requested.
bool ACPP_EnemyBase::PlayPrimaryAttackMontageFromAbility(AActor* TargetActor)
{
	const UCPP_EnemyAttackPatternData* AttackPattern = GetPrimaryAttackPattern();
	if (!AttackPattern || !AttackPattern->Montage)
	{
		return false;
	}

	Multicast_PlayAttackPatternMontage(TargetActor, AttackPattern->Montage);
	if (ActiveAttackMontage != AttackPattern->Montage)
	{
		return false;
	}

	// 공격 중 이동 금지: 진행 중이던 이동을 끊는다. 공격이 끝나면 StateTree가 다시 이동시킨다. (AIC는 서버에만 존재)
	if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController()))
	{
		EnemyAIC->StopMovementForAttack();
	}
	return true;
}

void ACPP_EnemyBase::FinishPrimaryAttackFromAbility()
{
	if (HasAuthority())
	{
		ClearActiveProjectileAttackAbility(ActiveProjectileAttackAbility);
		BroadcastAttackFinished();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 공격 몽타주를 전 클라이언트에서 일시정지/재개한다. 돌진 어빌리티가 돌진 중 '찌른 포즈'를 유지하는 데 사용. (서버에서 호출)
//! \param bPaused true면 일시정지, false면 재개
//! \return
void ACPP_EnemyBase::SetAttackMontagePausedFromAbility(bool bPaused)
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_SetAttackMontagePaused(bPaused);
}

void ACPP_EnemyBase::SetActiveProjectileAttackAbility(UCPP_EnemyProjectileAttackAbility* InAbility)
{
	ActiveProjectileAttackAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveProjectileAttackAbility(UCPP_EnemyProjectileAttackAbility* InAbility)
{
	if (!InAbility || ActiveProjectileAttackAbility == InAbility)
	{
		ActiveProjectileAttackAbility = nullptr;
	}
}

bool ACPP_EnemyBase::FirePrimaryProjectileFromAnimNotify()
{
	return ActiveProjectileAttackAbility ? ActiveProjectileAttackAbility->FireProjectileFromNotify(this) : false;
}

void ACPP_EnemyBase::SetActiveAreaAttackAbility(UCPP_EnemyAreaAttackAbility* InAbility)
{
	ActiveAreaAttackAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveAreaAttackAbility(UCPP_EnemyAreaAttackAbility* InAbility)
{
	if (!InAbility || ActiveAreaAttackAbility == InAbility)
	{
		ActiveAreaAttackAbility = nullptr;
	}
}

bool ACPP_EnemyBase::TriggerPrimaryAreaFromAnimNotify()
{
	return ActiveAreaAttackAbility ? ActiveAreaAttackAbility->TriggerAreaFromNotify(this) : false;
}

void ACPP_EnemyBase::SetActiveDashAttackAbility(UCPP_EnemyDashAttackAbility* InAbility)
{
	ActiveDashAttackAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveDashAttackAbility(UCPP_EnemyDashAttackAbility* InAbility)
{
	if (!InAbility || ActiveDashAttackAbility == InAbility)
	{
		ActiveDashAttackAbility = nullptr;
	}
}

bool ACPP_EnemyBase::StartDashAttackFromAnimNotify()
{
	return ActiveDashAttackAbility ? ActiveDashAttackAbility->StartDashFromNotify(this) : false;
}

void ACPP_EnemyBase::SetActiveShapeAttackAbility(UCPP_EnemyShapeAttackAbility* InAbility)
{
	ActiveShapeAttackAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveShapeAttackAbility(UCPP_EnemyShapeAttackAbility* InAbility)
{
	if (!InAbility || ActiveShapeAttackAbility == InAbility)
	{
		ActiveShapeAttackAbility = nullptr;
	}
}

void ACPP_EnemyBase::SetActiveBlinkNovaAbility(UCPP_EnemyBlinkNovaAbility* InAbility)
{
	ActiveBlinkNovaAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveBlinkNovaAbility(UCPP_EnemyBlinkNovaAbility* InAbility)
{
	if (!InAbility || ActiveBlinkNovaAbility == InAbility)
	{
		ActiveBlinkNovaAbility = nullptr;
	}
}

void ACPP_EnemyBase::SetActiveBeamAttackAbility(UCPP_EnemyBeamAttackAbility* InAbility)
{
	ActiveBeamAttackAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveBeamAttackAbility(UCPP_EnemyBeamAttackAbility* InAbility)
{
	if (!InAbility || ActiveBeamAttackAbility == InAbility)
	{
		ActiveBeamAttackAbility = nullptr;
	}
}

void ACPP_EnemyBase::SetActiveSplitAbility(UCPP_EnemySplitAbility* InAbility)
{
	ActiveSplitAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveSplitAbility(UCPP_EnemySplitAbility* InAbility)
{
	if (!InAbility || ActiveSplitAbility == InAbility)
	{
		ActiveSplitAbility = nullptr;
	}
}

void ACPP_EnemyBase::SetActiveSummonAbility(UCPP_EnemySummonAbility* InAbility)
{
	ActiveSummonAbility = InAbility;
}

void ACPP_EnemyBase::ClearActiveSummonAbility(UCPP_EnemySummonAbility* InAbility)
{
	if (!InAbility || ActiveSummonAbility == InAbility)
	{
		ActiveSummonAbility = nullptr;
	}
}

bool ACPP_EnemyBase::TriggerSummonFromAnimNotify()
{
	return ActiveSummonAbility ? ActiveSummonAbility->TriggerSummonFromNotify(this) : false;
}

////////////////////////////
//! \author HanSeul
//! \brief 공격력(AttackPower) 베이스를 현재값 × Scale로 설정한다(서버). 소환몹 공격력 스케일용.
//! \param Scale 배수(예: 0.8)
//! \return
void ACPP_EnemyBase::SetAttackPowerScale(float Scale)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const float BaseAttackPower = AbilitySystemComponent->GetNumericAttributeBase(UMyAttributeSet::GetAttackPowerAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetAttackPowerAttribute(), FMath::Max(BaseAttackPower * Scale, 0.0f));
}

void ACPP_EnemyBase::RegisterSummonedMinion(ACPP_EnemyBase* Minion)
{
	if (Minion && Minion != this)
	{
		SummonedMinions.Add(Minion);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 이 적이 소환한, 현재 살아있는 소환몹 수. 조회 시 무효(파괴)·사망 항목을 제거해 리스트를 정리한다.
//! \param
//! \return 살아있는 소환몹 수
int32 ACPP_EnemyBase::GetLivingSummonedCount() const
{
	int32 LivingCount = 0;
	for (int32 Index = SummonedMinions.Num() - 1; Index >= 0; --Index)
	{
		const ACPP_EnemyBase* Minion = SummonedMinions[Index].Get();
		if (!Minion || Minion->IsDead())
		{
			SummonedMinions.RemoveAtSwap(Index);
			continue;
		}
		++LivingCount;
	}
	return LivingCount;
}

////////////////////////////
//! \author HanSeul
//! \brief 최대/현재 체력을 직접 설정한다(서버). 분신 스폰(절반)·병합 복귀(합산)용. 설정 중에는 데미지 반응
//!        (경직/사망 트리거)을 억제해, HP 하향 설정이 의도치 않은 경직·사망을 일으키지 않게 한다.
//! \param NewMaxHealth 새 최대 체력
//! \param NewCurrentHealth 새 현재 체력(0..Max로 클램프)
//! \return
void ACPP_EnemyBase::SetHealthValues(float NewMaxHealth, float NewCurrentHealth)
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const float ClampedMax = FMath::Max(NewMaxHealth, 1.0f);
	const float ClampedCurrent = FMath::Clamp(NewCurrentHealth, 0.0f, ClampedMax);

	bSuppressDamageReaction = true;
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), ClampedMax);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), ClampedCurrent);
	bSuppressDamageReaction = false;
}

////////////////////////////
//! \author HanSeul
//! \brief 점멸형 어빌리티용: 액터 숨김과 콜리전을 전 클라이언트에서 함께 끄거나 복구한다.
//!        bHidden은 자동 복제되지만 콜리전 상태는 복제되지 않아, 클라이언트가 보이지 않는 벽에
//!        막히지 않도록 멀티캐스트로 동기화한다. (서버 전용 호출)
//! \param bVanished true면 숨김+콜리전 off, false면 복구
//! \return
void ACPP_EnemyBase::SetVanishedFromAbility(bool bVanished)
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_SetActorVanished(bVanished);
}

void ACPP_EnemyBase::Multicast_SetActorVanished_Implementation(bool bVanished)
{
	SetActorHiddenInGame(bVanished);
	SetActorEnableCollision(!bVanished);
}

////////////////////////////
//! \author HanSeul
//! \brief 돌진 등 고속 이동 공격 동안 캡슐의 Pawn 채널 응답을 꺼서 경로상의 폰(특히 공중에 뜬 아군)을
//!        물리로 밀어 날리지 않게 한다. 정지 판정은 돌진 태스크의 sweep이 전담하므로 영향 없다.
//!        클라이언트의 이동 예측도 같은 응답을 보도록 멀티캐스트로 동기화한다. (서버 전용 호출)
//! \param bIgnored true면 Pawn 응답 Ignore, false면 Block 복구
//! \return
void ACPP_EnemyBase::SetPawnPhysicsIgnoredFromAbility(bool bIgnored)
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_SetPawnPhysicsIgnored(bIgnored);
}

void ACPP_EnemyBase::Multicast_SetPawnPhysicsIgnored_Implementation(bool bIgnored)
{
	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, bIgnored ? ECR_Ignore : ECR_Block);
	}
}

////////////////////////////
//! \author HanSeul
//! \editor HanUl - 몽타주 중단 시 진행 중이던 공격 어빌리티까지 강제 종료하도록 추가 (영구 공격 불능 방지)
//! \brief Stops the currently playing montage on the server and all clients.
//! \return None
void ACPP_EnemyBase::StopCurrentMontage()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bHadActiveAttack = ActiveAttackPattern != nullptr || ActiveAttackAbilityHandle.IsValid();
	Multicast_StopCurrentMontage();

	// 외부 중단(스태거 등)은 몽타주 종료 델리게이트 경로를 막아 EndAbility가 누락될 수 있다.
	// 남은 공격 어빌리티를 직접 닫고, 공격 종료를 통지해 ActiveAttackPattern 등 공격 상태를 정리한다.
	CancelActiveAttackAbilities();

	if (AbilitySystemComponent && ActiveAttackAbilityHandle.IsValid())
	{
		const FGameplayAbilitySpec* ActiveSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(ActiveAttackAbilityHandle);
		if (ActiveSpec && ActiveSpec->IsActive())
		{
			AbilitySystemComponent->CancelAbilityHandle(ActiveAttackAbilityHandle);
		}
	}

	ActiveAttackAbilityHandle = FGameplayAbilitySpecHandle();
	if (bHadActiveAttack && ActiveAttackPattern)
	{
		BroadcastAttackFinished();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 공격 종료 통지 없이 현재 몽타주만 전 클라이언트에서 정지한다. 어빌리티는 살아 있고 StateTree 공격
//!        태스크도 대기 상태를 유지하므로, 호출한 어빌리티가 마무리(EndAbility/통지) 시점을 직접 제어한다.
//!        (돌진 벽 기절: 몽타주를 정리하고 기본 자세로 굳는 용도, 서버 전용)
//! \param
//! \return
void ACPP_EnemyBase::StopCurrentMontageWithoutFinish()
{
	if (!HasAuthority())
	{
		return;
	}

	Multicast_StopCurrentMontage();
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the enemy death flow, plays death FX, and detaches the controller.
//! \return None
void ACPP_EnemyBase::Dead()
{
	Multicast_PlayDeadFX();

	if (!HasAuthority())
	{
		return;
	}

	DetachFromControllerPendingDestroy();
}

////////////////////////////
//! \author HanSeul
//! \brief 외부(스포너 등)에서 적을 강제 사망 처리한다. 정상 사망 경로(bIsDead 복제/OnEnemyDeath/BP_OnDeath)를 태운 뒤 사후 처리(Dead)를 수행한다. (서버 전용)
//! \param
//! \return
void ACPP_EnemyBase::ForceKill()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	HandleDeath(EDeathRewardPolicy::GrantRewards);
	Dead();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 적을 처치 보상 없이 정상 사망 경로로 처리한다.
//! \param
//! \return
void ACPP_EnemyBase::ForceKillWithoutRewards()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	HandleDeath(EDeathRewardPolicy::SuppressRewards);
	Dead();
}

////////////////////////////
//! \author HanSeul
//! \brief Plays the montage registered in the selected attack pattern on all clients.
//! \param TargetActor Actor to face during attack montage playback.
//! \param PatternMontage Montage asset read from the attack pattern data.
//! \return None
void ACPP_EnemyBase::Multicast_PlayAttackPatternMontage_Implementation(AActor* TargetActor, UAnimMontage* PatternMontage)
{
	PlayAttackPatternMontage(TargetActor, PatternMontage);
}

////////////////////////////
//! \author HanSeul
//! \brief 재생 중인 공격 몽타주를 모든 머신에서 일시정지/재개한다. (돌진 중 포즈 유지 — 보스 돌진과 동일 방식)
//! \param bPaused true면 일시정지, false면 재개
//! \return
void ACPP_EnemyBase::Multicast_SetAttackMontagePaused_Implementation(bool bPaused)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !ActiveAttackMontage)
	{
		return;
	}

	if (bPaused)
	{
		AnimInstance->Montage_Pause(ActiveAttackMontage);
	}
	else
	{
		AnimInstance->Montage_Resume(ActiveAttackMontage);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 플래그를 몽타주 정지보다 먼저 세우도록 순서 변경. 정지가 유발하는 블렌드아웃 델리게이트가
//!                 FinishAttackMontage를 타지 않게 되어, 외부 정지의 마무리는 항상 호출측이 명시적으로 제어한다.
//! \return None
void ACPP_EnemyBase::Multicast_StopCurrentMontage_Implementation()
{
	bHasAttackFinishedBroadcast = true;
	ActiveAttackMontage = nullptr;
	StopAnimMontage();
}

////////////////////////////
//! \author HanSeul
//! \brief Rotates toward TargetActor, plays PatternMontage, and prepares the finish callback.
//! \param TargetActor Actor to face during attack montage playback.
//! \param PatternMontage Montage asset to play for this attack.
//! \return true when the montage starts successfully.
bool ACPP_EnemyBase::PlayAttackPatternMontage(AActor* TargetActor, UAnimMontage* PatternMontage)
{
	if (HasAuthority())
	{
		if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController()))
		{
			EnemyAIC->SuspendTargetFocus();
		}
	}

	if (TargetActor)
	{
		const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), TargetActor->GetActorLocation());
		SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));
	}

	bHasAttackFinishedBroadcast = false;
	ActiveAttackMontage = PatternMontage;

	if (!PatternMontage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		ActiveAttackMontage = nullptr;
		return false;
	}

	const float MontageLength = AnimInstance->Montage_Play(PatternMontage);
	if (MontageLength <= 0.0f)
	{
		ActiveAttackMontage = nullptr;
		return false;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &ACPP_EnemyBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, PatternMontage);

	FOnMontageBlendingOutStarted MontageBlendingOutDelegate;
	MontageBlendingOutDelegate.BindUObject(this, &ACPP_EnemyBase::HandleAttackMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(MontageBlendingOutDelegate, PatternMontage);

	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Applies replicated death visuals including collision shutdown and ragdoll physics.
//! \return None
void ACPP_EnemyBase::Multicast_PlayDeadFX_Implementation()
{
	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		EnemyCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	if (!EnemyMesh)
	{
		return;
	}

	EnemyMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	EnemyMesh->SetSimulatePhysics(true);
}

////////////////////////////
//! \author HanSeul
//! \brief Finishes the disappearance presentation and destroys the enemy on the server.
//! \return None
void ACPP_EnemyBase::FinishDeathDisappear()
{
	if (!HasAuthority() || !bIsDead || bDeathDisappearFinished)
	{
		return;
	}

	bDeathDisappearFinished = true;
	Destroy();
}

float ACPP_EnemyBase::GetHealth() const
{
	return MyAttributeSet ? MyAttributeSet->GetHealth() : 0.0f;
}

float ACPP_EnemyBase::GetMaxHealth() const
{
	return MyAttributeSet ? MyAttributeSet->GetMaxHealth() : 0.0f;
}

UMyAttributeSet* ACPP_EnemyBase::GetMyAttributeSet() const
{
	return MyAttributeSet;
}

bool ACPP_EnemyBase::IsDead() const
{
	return bIsDead;
}

UCPP_EnemyAttackPatternData* ACPP_EnemyBase::GetPrimaryAttackPattern() const
{
	return ActiveAttackPattern;
}

////////////////////////////
//! \author HanSeul
//! \brief 지금 발동 가능한(쿨다운이 안 도는) 공격 패턴이 있는지 검사한다. 패턴 선택 로직을 그대로 재사용하므로
//!        발동 시점 판정과 항상 일치한다. (AIC CanAttackTarget 게이트 — 쿨다운 중엔 공격 대신 슬롯 이동)
//! \param
//! \return 발동 가능한 패턴이 있으면 true
bool ACPP_EnemyBase::HasReadyAttackPattern() const
{
	return HasReadyAttackPatternBySelectionMode(EEnemyAttackPatternSelectionMode::Any);
}

////////////////////////////
//! \author HanSeul
//! \brief Checks whether the requested pattern group contains an immediately usable attack.
//! \param SelectionMode Pattern group used for selection.
//! \return true when at least one matching pattern is usable.
bool ACPP_EnemyBase::HasReadyAttackPatternBySelectionMode(EEnemyAttackPatternSelectionMode SelectionMode) const
{
	return SelectAvailableAttackPattern(SelectionMode) != nullptr;
}

////////////////////////////
//! \author HanSeul
//! \brief Returns the range of the ready special-condition pattern first, falling back to a normal pattern.
//! \return Selected ready pattern range, or 0 when no pattern is usable.
float ACPP_EnemyBase::GetReadyAttackPatternRange() const
{
	const UCPP_EnemyAttackPatternData* SpecialPattern =
		SelectAvailableAttackPattern(EEnemyAttackPatternSelectionMode::SpecialCondition);
	const UCPP_EnemyAttackPatternData* ReadyPattern = SpecialPattern
		? SpecialPattern
		: SelectAvailableAttackPattern(EEnemyAttackPatternSelectionMode::Normal);
	return ReadyPattern ? ReadyPattern->Range : 0.0f;
}

bool ACPP_EnemyBase::HasEnemyGameplayTag(FGameplayTag Tag) const
{
	if (!AbilitySystemComponent || !Tag.IsValid())
	{
		return false;
	}

	return AbilitySystemComponent->HasMatchingGameplayTag(Tag);
}

////////////////////////////
//! \author HanSeul
//! \brief Broadcasts the current active state of every tracked status effect tag.
//! \return None
void ACPP_EnemyBase::BroadcastCurrentStatusEffectStates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayTag& StatusTag : TrackedStatusEffectTags)
	{
		OnStatusEffectChanged.Broadcast(StatusTag, AbilitySystemComponent->HasMatchingGameplayTag(StatusTag));
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 공격 슬롯 반경을 유효 공격 패턴들의 최소 사거리에서 계산한다. 최소값이어야 어떤 패턴이 선택돼도 슬롯에서 사거리가 닿는다.
//!        사거리보다 버퍼만큼 안쪽에 세워 헛스윙을 막고, 유효 패턴이 없으면 기존 값을 유지한다.
//! \param
//! \return
void ACPP_EnemyBase::UpdateChaseAcceptableRadius()
{
	float MinRange = 0.0f;
	bool bFound = false;
	for (const UCPP_EnemyAttackPatternData* Pattern : AttackPatterns)
	{
		if (!Pattern || Pattern->Range <= 0.0f)
		{
			continue;
		}
		MinRange = bFound ? FMath::Min(MinRange, Pattern->Range) : Pattern->Range;
		bFound = true;
	}

	if (bFound)
	{
		ChaseAcceptableRadius = FMath::Max(MinRange - AttackRangeSlotBuffer, MinSlotRadius);
	}
}

void ACPP_EnemyBase::InitializeEnemyAbilitySystem()
{
	if (!AbilitySystemComponent || !MyAttributeSet)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyDefaultCharacterTagsToAbilitySystem(AbilitySystemComponent);

	if (HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(MyGameplayTags::Faction_Enemy, 1, EGameplayTagReplicationState::TagOnly);
	}

	ApplyDefaultEnemyAttributes();
	GrantEnemyAbilities();
	BindEnemyAttributeDelegates();
	BindEnemyGameplayTagDelegates();
	BroadcastHealthChangedFromGAS();
}

////////////////////////////
//! \author 장효제
//! \brief 서버 권한 ASC에 DefaultCharacterTags를 Loose Tag로 중복 없이 부여한다.
//! \details 장효제: Faction 태그와 분리된 스트리밍용 Character 정체성 태그를 관리한다.
//! \param ASC 태그를 부여할 AbilitySystemComponent
//! \return 없음
void ACPP_EnemyBase::ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	TArray<FGameplayTag> CharacterTags;
	DefaultCharacterTags.GetGameplayTagArray(CharacterTags);

	for (const FGameplayTag& CharacterTag : CharacterTags)
	{
		if (!CharacterTag.IsValid() || ASC->HasMatchingGameplayTag(CharacterTag))
		{
			continue;
		}

		ASC->AddLooseGameplayTag(CharacterTag, 1, EGameplayTagReplicationState::TagOnly);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 기본 능력치에 EnemyLevel과 성장률을 적용한 최종 능력치를 계산한다.
//! \param BaseValue DataTable에 저장된 레벨 1 기본 능력치
//! \param GrowthRate 레벨당 성장률
//! \return EnemyLevel이 적용된 최종 능력치
float ACPP_EnemyBase::CalculateLevelScaledStat(float BaseValue, float GrowthRate) const
{
	const float LevelOffset = static_cast<float>(FMath::Max(EnemyLevel - 1, 0));
	return BaseValue * (1.0f + FMath::Max(GrowthRate, 0.0f) * LevelOffset);
}

////////////////////////////
//! \author HanSeul
//! \brief EnemyBaseStatRow에서 기본 스탯을 읽어 공용 초기화 GameplayEffect의 SetByCaller 값으로 적용한다.
//! \return 없음
void ACPP_EnemyBase::ApplyDefaultEnemyAttributes()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultEnemyAttributeEffect)
	{
		return;
	}

	const FString RowContext = FString::Printf(TEXT("%s::ApplyDefaultEnemyAttributes"), *GetNameSafe(this));
	const FCPP_EnemyBaseStatRow DefaultStatRow;
	const FCPP_EnemyBaseStatRow* StatRow = EnemyBaseStatRow.DataTable
		? EnemyBaseStatRow.DataTable->FindRow<FCPP_EnemyBaseStatRow>(
			EnemyBaseStatRow.RowName,
			RowContext,
			false)
		: nullptr;
	if (!StatRow)
	{
		StatRow = &DefaultStatRow;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultEnemyAttributeEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	const float ScaledMaxHealth = CalculateLevelScaledStat(
		StatRow->MaxHealth,
		StatRow->MaxHealthGrowthRate);
	const float ScaledAttackPower = CalculateLevelScaledStat(
		StatRow->AttackPower,
		StatRow->AttackPowerGrowthRate);
	const float ScaledDefense = CalculateLevelScaledStat(
		StatRow->Defense,
		StatRow->DefenseGrowthRate);
	const float ScaledMoveSpeed = CalculateLevelScaledStat(
		StatRow->MoveSpeed,
		StatRow->MoveSpeedGrowthRate);

	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_MaxHealth,
		FMath::Max(ScaledMaxHealth, 1.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_AttackPower,
		FMath::Max(ScaledAttackPower, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_Defense,
		FMath::Max(ScaledDefense, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_MoveSpeed,
		FMath::Max(ScaledMoveSpeed, 0.0f));

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ACPP_EnemyBase::GrantEnemyAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultEnemyAbilitySet)
	{
		return;
	}

	DefaultEnemyAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles, this);
}

void ACPP_EnemyBase::BindEnemyAttributeDelegates()
{
	if (!AbilitySystemComponent || !MyAttributeSet)
	{
		return;
	}

	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ACPP_EnemyBase::HandleHealthChanged);

	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ACPP_EnemyBase::HandleMaxHealthChanged);

	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &ACPP_EnemyBase::HandleMoveSpeedChanged);

	ApplyMoveSpeedToMovement(MyAttributeSet->GetMoveSpeed());
}

////////////////////////////
//! \author HanSeul
//! \brief GAS MoveSpeed 값을 Enemy CharacterMovement의 실제 최대 보행 속도에 반영한다.
//! \param NewMoveSpeed 적용할 최대 보행 속도
//! \return 없음
void ACPP_EnemyBase::ApplyMoveSpeedToMovement(float NewMoveSpeed)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(NewMoveSpeed, 0.0f);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief GAS MoveSpeed Attribute 변경 시 실제 CharacterMovement 속도를 갱신한다.
//! \param Data MoveSpeed의 이전 값과 새 값을 포함하는 변경 데이터
//! \return 없음
void ACPP_EnemyBase::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	ApplyMoveSpeedToMovement(Data.NewValue);
}

void ACPP_EnemyBase::BindEnemyGameplayTagDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayTag& StatusTag : TrackedStatusEffectTags)
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(StatusTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ACPP_EnemyBase::HandleStatusEffectTagChanged);
	}
}

void ACPP_EnemyBase::HandleStatusEffectTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	OnStatusEffectChanged.Broadcast(Tag, NewCount > 0);
}

void ACPP_EnemyBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bIsDead)
	{
		return;
	}

	BroadcastHealthChangedFromGAS();

	if (HasAuthority() && !bSuppressDamageReaction)
	{
		if (TryRequestForcedSpecialAttack(Data.OldValue, Data.NewValue))
		{
			return;
		}

		HandleDamageReaction(Data.OldValue, Data.NewValue);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Detects a downward health-threshold crossing and queues the matching interruptible special attack.
//! \param OldHealth Health before the attribute change.
//! \param NewHealth Health after the attribute change.
//! \return true when a forced special attack transition was requested.
bool ACPP_EnemyBase::TryRequestForcedSpecialAttack(float OldHealth, float NewHealth)
{
	if (!HasAuthority() || bIsDead || NewHealth <= 0.0f || NewHealth >= OldHealth || IsValid(PendingForcedAttackPattern))
	{
		return false;
	}

	ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController());
	const float MaxHealth = GetMaxHealth();
	if (!EnemyAIC || MaxHealth <= 0.0f || !AbilitySystemComponent)
	{
		return false;
	}

	const float OldHealthPercent = OldHealth / MaxHealth * 100.0f;
	const float NewHealthPercent = NewHealth / MaxHealth * 100.0f;
	UCPP_EnemyAttackPatternData* ForcedPattern = nullptr;

	for (UCPP_EnemyAttackPatternData* AttackPattern : AttackPatterns)
	{
		if (!AttackPattern
			|| !AttackPattern->AbilityTag.IsValid()
			|| !AttackPattern->bUseHealthCondition
			|| !AttackPattern->bInterruptAttackOnHealthCondition)
		{
			continue;
		}

		const float Threshold = AttackPattern->HealthPercentAtOrBelow;
		if (OldHealthPercent <= Threshold || NewHealthPercent > Threshold)
		{
			continue;
		}

		if (!DoesAttackPatternMeetConditions(AttackPattern))
		{
			continue;
		}

		if (bIsAbilitySpawnedMinion && AttackPattern->bExcludeForSpawnedMinion)
		{
			continue;
		}

		if (AttackPattern->CooldownTag.IsValid()
			&& AbilitySystemComponent->HasMatchingGameplayTag(AttackPattern->CooldownTag))
		{
			continue;
		}

		if (!ForcedPattern || AttackPattern->CooldownDuration > ForcedPattern->CooldownDuration)
		{
			ForcedPattern = AttackPattern;
		}
	}

	if (!ForcedPattern)
	{
		return false;
	}

	PendingForcedAttackPattern = ForcedPattern;
	StopCurrentMontage();
	EnemyAIC->RequestForcedSpecialAttack();
	return true;
}

void ACPP_EnemyBase::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	BroadcastHealthChangedFromGAS();
}

void ACPP_EnemyBase::HandleDamageReaction(float OldHealth, float NewHealth)
{
	if (!HasAuthority() || bIsDead || NewHealth >= OldHealth)
	{
		return;
	}

	if (NewHealth <= 0.0f)
	{
		HandleDeath(EDeathRewardPolicy::GrantRewards);
		SendDamageStateTreeEvent(true);
		return;
	}

	if (CanEnterStaggerByGAS())
	{
		MarkStaggered();
		SendDamageStateTreeEvent(false);
	}
}

////////////////////////////
//! \editor 준혁 - 서버에서 처치 보상(메소) 지급 추가
//! \author HanSeul
//! \brief 보상 정책에 따라 적의 공통 사망 상태와 이벤트를 처리한다.
//! \param RewardPolicy 처치 보상 지급 여부를 결정하는 정책
//! \return
void ACPP_EnemyBase::HandleDeath(EDeathRewardPolicy RewardPolicy)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	if (HasAuthority() && RewardPolicy == EDeathRewardPolicy::GrantRewards)
	{
		GrantKillRewards();
	}

	OnEnemyDeath.Broadcast();
	BP_OnDeath();
}

////////////////////////////
//! \author 준혁
//! \brief [서버 전용] 처치 보상 메소와 경험치를 접속 중인 파티 전원에게 지급한다.
//! \note 3인 코옵 기준으로 킬러 판정 없이 전원 동일 지급한다. 지급량은 MesoRewardOnDeath / ExpRewardOnDeath로 조절한다.
void ACPP_EnemyBase::GrantKillRewards()
{
	// 어빌리티가 스폰한 파생 적(분신·소환몹)은 처치해도 보상·카운트에서 제외한다.
	if (!HasAuthority() || bIsAbilitySpawnedMinion || (MesoRewardOnDeath <= 0 && ExpRewardOnDeath <= 0))
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PartyPlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PartyPlayerState);
		if (!MyPlayerState)
		{
			continue;
		}

		if (MesoRewardOnDeath > 0)
		{
			if (UMyInventoryComponent* Inventory = MyPlayerState->GetInventoryComponent())
			{
				Inventory->AddMeso(
					MesoRewardOnDeath,
					MyGameplayTags::Meso_Source_CombatReward);
			}
		}

		if (ExpRewardOnDeath > 0)
		{
			if (APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(MyPlayerState->GetPawn()))
			{
				PlayerCharacter->AddExperience(ExpRewardOnDeath);
			}
			else
			{
				// 폰이 일시적으로 없으면(리스폰/재접속 대기 등) 경험치만 적립하고, 다음 AddExperience 호출 때 레벨업이 정산된다.
				MyPlayerState->SetCharacterExp(MyPlayerState->GetCharacterExp() + ExpRewardOnDeath);
			}
		}
	}
}

bool ACPP_EnemyBase::CanEnterStaggerByGAS() const
{
	return CanBeStaggered() && !IsStaggerImmuneByGAS() && !IsStaggerImmuneDuringCurrentAttack();
}

bool ACPP_EnemyBase::IsStaggerImmuneByGAS() const
{
	return HasEnemyGameplayTag(MyGameplayTags::Status_Buff_SuperArmor);
}

bool ACPP_EnemyBase::IsStaggerImmuneDuringCurrentAttack() const
{
	const UCPP_EnemyAttackPatternData* AttackPattern = GetPrimaryAttackPattern();
	if (!AttackPattern || !AttackPattern->bStaggerImmuneDuringAttack)
	{
		return false;
	}

	const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Enemy.Attacking"), false);
	return AttackingTag.IsValid() && HasEnemyGameplayTag(AttackingTag);
}

void ACPP_EnemyBase::BroadcastHealthChangedFromGAS()
{
	const float CurrentHealth = GetHealth();
	const float MaxHealth = GetMaxHealth();

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	BP_OnHealthChanged(CurrentHealth, MaxHealth);
}

void ACPP_EnemyBase::OnRep_IsDead()
{
	if (!bIsDead)
	{
		return;
	}

	OnEnemyDeath.Broadcast();
	BP_OnDeath();
}

void ACPP_EnemyBase::BroadcastAttackFinished()
{
	ActiveAttackPattern = nullptr;
	ActiveAttackAbilityHandle = FGameplayAbilitySpecHandle();

	if (HasAuthority())
	{
		// 공격 토큰 반납. 이 함수는 정상 종료·활성화 실패·스태거 중단(StopCurrentMontage)의 공통 초크포인트라
		// 모든 공격 종료 경로에서 토큰이 확실히 풀린다(누수=영구 공격자 슬롯 점유 방지).
		if (const UWorld* World = GetWorld())
		{
			if (UEnemyCombatCoordinatorSubsystem* Coordinator = World->GetSubsystem<UEnemyCombatCoordinatorSubsystem>())
			{
				Coordinator->ReleaseAttackToken(this);
			}
		}

		if (ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController()))
		{
			EnemyAIC->RefreshTargetFocus();
		}
	}

	OnAttackFinished.Broadcast();
}

////////////////////////////
//! \author HanSeul
//! \brief Finds the off-cooldown attack pattern with the longest base cooldown.
//! \return Selected attack pattern, or nullptr when no pattern is available.
UCPP_EnemyAttackPatternData* ACPP_EnemyBase::SelectAvailableAttackPattern(EEnemyAttackPatternSelectionMode SelectionMode) const
{
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	UCPP_EnemyAttackPatternData* SelectedPattern = nullptr;
	for (UCPP_EnemyAttackPatternData* AttackPattern : AttackPatterns)
	{
		if (!AttackPattern || !AttackPattern->AbilityTag.IsValid())
		{
			continue;
		}

		const bool bIsSpecialPattern = IsSpecialConditionPattern(AttackPattern);
		if ((SelectionMode == EEnemyAttackPatternSelectionMode::Normal && bIsSpecialPattern)
			|| (SelectionMode == EEnemyAttackPatternSelectionMode::SpecialCondition && !bIsSpecialPattern))
		{
			continue;
		}

		if (!DoesAttackPatternMeetConditions(AttackPattern))
		{
			continue;
		}

		// 어빌리티 스폰 파생 적(분신·소환몹)은 이 패턴을 선택하지 않는다(무한 분열/소환 방지).
		if (bIsAbilitySpawnedMinion && AttackPattern->bExcludeForSpawnedMinion)
		{
			continue;
		}

		if (AttackPattern->CooldownTag.IsValid()
			&& AbilitySystemComponent->HasMatchingGameplayTag(AttackPattern->CooldownTag))
		{
			continue;
		}

		// 소환 게이트: 이미 소환한 생존 소환몹 수가 생존 플레이어 × SummonsPerPlayer 이상이면 선택하지 않는다.
		if (AttackPattern->bIsSummonPattern && AttackPattern->SummonsPerPlayer > 0)
		{
			TArray<AActor*> LivingPlayers;
			UMyAbilitySystemLibrary::GetLivingPlayerPawns(this, LivingPlayers);
			if (GetLivingSummonedCount() >= LivingPlayers.Num() * AttackPattern->SummonsPerPlayer)
			{
				continue;
			}
		}

		if (!SelectedPattern || AttackPattern->CooldownDuration > SelectedPattern->CooldownDuration)
		{
			SelectedPattern = AttackPattern;
		}
	}

	return SelectedPattern;
}

bool ACPP_EnemyBase::IsSpecialConditionPattern(const UCPP_EnemyAttackPatternData* AttackPattern) const
{
	return AttackPattern && (AttackPattern->bUseHealthCondition || AttackPattern->bUseDistanceCondition);
}

bool ACPP_EnemyBase::DoesAttackPatternMeetConditions(const UCPP_EnemyAttackPatternData* AttackPattern) const
{
	if (!AttackPattern)
	{
		return false;
	}

	if (AttackPattern->bUseHealthCondition)
	{
		const float MaxHealth = GetMaxHealth();
		if (MaxHealth <= 0.0f)
		{
			return false;
		}

		const float HealthPercent = GetHealth() / MaxHealth * 100.0f;
		if (HealthPercent > AttackPattern->HealthPercentAtOrBelow)
		{
			return false;
		}
	}

	if (AttackPattern->bUseDistanceCondition)
	{
		const AActor* TargetActor = GetCurrentTargetActor();
		if (!IsValid(TargetActor))
		{
			return false;
		}

		const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), TargetActor->GetActorLocation());
		if (DistanceToTarget < AttackPattern->TargetDistanceAtOrAbove)
		{
			return false;
		}
	}

	return true;
}

void ACPP_EnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	FinishAttackMontage(Montage);
}

void ACPP_EnemyBase::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted)
	{
		FinishAttackMontage(Montage);
	}
}

void ACPP_EnemyBase::SendDamageStateTreeEvent(bool bWasFatal)
{
	ACPP_EnemyAIC* EnemyAIC = Cast<ACPP_EnemyAIC>(GetController());
	if (!EnemyAIC)
	{
		return;
	}

	if (bWasFatal)
	{
		EnemyAIC->SendDeadEvent();
		return;
	}

	EnemyAIC->SendHitEvent();
}

bool ACPP_EnemyBase::CanBeStaggered() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastStaggerTime >= StaggerImmunityDuration;
}

void ACPP_EnemyBase::MarkStaggered()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LastStaggerTime = World->GetTimeSeconds();
}

////////////////////////////
//! \author HanSeul
//! \brief 진행 중인 공격 어빌리티 인스턴스를 강제 종료하고 활성 캐시를 정리한다. 몽타주가 외부(스태거 등)에서 끊기면
//!        몽타주 종료 델리게이트 경로(FinishAttackMontage)가 막혀 EndAbility가 누락될 수 있고, InstancedPerActor
//!        인스턴스가 활성으로 남으면 이후 TryActivateAbility가 영구 실패한다(영구 공격 불능). 그 경로를 여기서 닫는다. (서버 전용)
//! \param
//! \return 강제 종료한 어빌리티가 하나라도 있으면 true
bool ACPP_EnemyBase::CancelActiveAttackAbilities()
{
	if (!HasAuthority())
	{
		return false;
	}

	bool bAnyCanceled = false;

	if (ActiveProjectileAttackAbility)
	{
		ActiveProjectileAttackAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveAreaAttackAbility)
	{
		ActiveAreaAttackAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveDashAttackAbility)
	{
		ActiveDashAttackAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveShapeAttackAbility)
	{
		ActiveShapeAttackAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveBlinkNovaAbility)
	{
		ActiveBlinkNovaAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveBeamAttackAbility)
	{
		ActiveBeamAttackAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveSplitAbility)
	{
		ActiveSplitAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	if (ActiveSummonAbility)
	{
		ActiveSummonAbility->FinishAbilityFromMontage(this);
		bAnyCanceled = true;
	}

	return bAnyCanceled;
}

void ACPP_EnemyBase::FinishAttackMontage(UAnimMontage* Montage)
{
	if (Montage != ActiveAttackMontage || bHasAttackFinishedBroadcast)
	{
		return;
	}

	bHasAttackFinishedBroadcast = true;
	ActiveAttackMontage = nullptr;

	if (HasAuthority())
	{
		if (ActiveProjectileAttackAbility)
		{
			ActiveProjectileAttackAbility->FinishAbilityFromMontage(this);
		}

		if (ActiveAreaAttackAbility)
		{
			ActiveAreaAttackAbility->FinishAbilityFromMontage(this);
		}

		if (ActiveDashAttackAbility)
		{
			ActiveDashAttackAbility->FinishAbilityFromMontage(this);
		}

		if (ActiveShapeAttackAbility)
		{
			ActiveShapeAttackAbility->FinishAbilityFromMontage(this);
		}

		if (ActiveSummonAbility)
		{
			ActiveSummonAbility->FinishAbilityFromMontage(this);
		}

		BroadcastAttackFinished();
	}
}
