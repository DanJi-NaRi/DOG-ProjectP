// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyDashAttackAbility.h"

#include "Enemy/Abilities/CPP_AbilityTask_EnemyDash.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Actors/CPP_EnemyTelegraphActor.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"
#include "MyGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanSeul
//! \editor HanUl - 보스 돌진 방식(몽타주 1개 + 일시정지/재개 + sweep 태스크)으로 재작성. 발동마다 인스턴스 상태 리셋.
//! \brief Activates the enemy dash attack flow through GAS.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return None
void UCPP_EnemyDashAttackAbility::ActivateAbility(
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
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	ActiveDashTask = nullptr;
	bHasStartedDash = false;
	DestroyDashTelegraph();
	EnemyAvatar->SetActiveDashAttackAbility(this);

	// 텔레그래프는 코스메틱 — 리스너 준비에 실패해도 공격은 진행한다. (몽타주에 EnemyTelegraph 노티파이가 없으면 그냥 안 뜸)
	SetupTelegraphEventListeners();

	const bool bMontageStarted = EnemyAvatar->PlayPrimaryAttackMontageFromAbility(TargetActor);
	if (!bMontageStarted)
	{
		EnemyAvatar->ClearActiveDashAttackAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

const FGameplayTagContainer* UCPP_EnemyDashAttackAbility::GetCooldownTags() const
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
void UCPP_EnemyDashAttackAbility::ApplyCooldown(
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

void UCPP_EnemyDashAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 스태거 등 어떤 경로로 끝나도 남은 텔레그래프를 정리한다.
	DestroyDashTelegraph();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주가 돌진 노티파이에 도달하면 쿨다운을 커밋하고, 몽타주를 일시정지한 채(찌른 포즈 유지)
//!        캡슐 sweep 돌진 태스크를 시작한다. 벽/폰에 닿거나 거리를 완주하면 HandleDashFinished가 이어받는다.
//!        태스크 생성 실패 시에도 몽타주를 재개해 공통 종료 경로가 어빌리티를 닫는다(행 없음).
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return 쿨다운 커밋과 돌진 태스크 시작에 성공하면 true
bool UCPP_EnemyDashAttackAbility::StartDashFromNotify(ACPP_EnemyBase* EnemyAvatar)
{
	if (bHasStartedDash || !EnemyAvatar)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo))
	{
		return false;
	}

	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	if (!AttackPattern)
	{
		return false;
	}

	bHasStartedDash = true;

	// 돌진이 실제로 나가는 순간 텔레그래프 제거. (보스 돌진과 동일)
	DestroyDashTelegraph();

	EnemyAvatar->SetAttackMontagePausedFromAbility(true);

	// 주의: DashCollisionRadius/HalfHeight가 캐릭터 캡슐보다 크면 시작 지점부터 지형과 겹쳐 즉시 벽 정지한다.
	// 데이터에서 캐릭터 캡슐 이하로 설정할 것.
	ActiveDashTask = UCPP_AbilityTask_EnemyDash::EnemyDash(
		this,
		EnemyAvatar,
		EnemyAvatar->GetActorForwardVector().GetSafeNormal2D(),
		AttackPattern->Range,
		AttackPattern->DashSpeed,
		AttackPattern->DashCollisionRadius,
		AttackPattern->DashCollisionHalfHeight
	);
	if (!ActiveDashTask)
	{
		EnemyAvatar->SetAttackMontagePausedFromAbility(false);
		return false;
	}

	ActiveDashTask->OnEnemyDashFinished.AddDynamic(this, &UCPP_EnemyDashAttackAbility::HandleDashFinished);
	ActiveDashTask->ReadyForActivation();
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 종료 콜백. 정지 지점에서 맞은 적대 폰들에 패턴 피해/상태이상을 적용하고 몽타주를 재개한다.
//!        벽에 막혀 정지했으면 몽타주를 정리하고 DashWallStunDuration 동안 기본 자세로 굳는다(기절).
//!        재개된 몽타주의 남은 구간(후딜)이 끝나면 공통 경로(FinishAttackMontage)가 어빌리티를 닫는다.
//! \param HitPawns 돌진 정지 지점에서 캡슐에 맞은 폰들(벽 충돌/완주 시 비어 있음)
//! \param bHitWall 폰이 아닌 지오메트리(벽)에 막혀 조기 정지했으면 true
//! \return
void UCPP_EnemyDashAttackAbility::HandleDashFinished(const TArray<AActor*>& HitPawns, bool bHitWall)
{
	ActiveDashTask = nullptr;

	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!EnemyAvatar || !EnemyAvatar->HasAuthority())
	{
		return;
	}

	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	bool bHitAnyHostile = false;
	for (AActor* HitPawn : HitPawns)
	{
		if (IsValid(HitPawn) && HitPawn != EnemyAvatar && UMyAbilitySystemLibrary::IsHostile(EnemyAvatar, HitPawn))
		{
			ApplyDashHitToActor(EnemyAvatar, HitPawn, AttackPattern);
			bHitAnyHostile = true;
		}
	}

	// 반동(이 패턴 한정, DashSelfKnockback>0): 플레이어를 맞히면 돌진 반대 방향으로 살짝 밀려난다.
	// sweep 이동이라 벽에 박히지 않는다. (플레이어 히트 시엔 벽 정지가 아니므로 아래 벽 기절과 겹치지 않음)
	if (bHitAnyHostile && AttackPattern && AttackPattern->DashSelfKnockback > 0.0f)
	{
		const FVector RecoilOffset = -EnemyAvatar->GetActorForwardVector().GetSafeNormal2D() * AttackPattern->DashSelfKnockback;
		EnemyAvatar->AddActorWorldOffset(RecoilOffset, /*bSweep=*/true);
	}

	// 벽 기절: 몽타주를 정리하고(종료 통지 없이) 기본 자세로 기절 시간만큼 가만히 서 있는다.
	// 어빌리티와 StateTree 공격 태스크는 살아 있으므로 이동/재공격이 없고, 대기가 끝나면 직접 마무리한다.
	// WaitDelay 태스크는 EndAbility 시 자동 파괴되므로 스태거/사망으로 강제 종료돼도 잔여 타이머가 남지 않는다.
	const float WallStunDuration = (bHitWall && AttackPattern) ? AttackPattern->DashWallStunDuration : 0.0f;
	if (WallStunDuration > 0.0f)
	{
		if (UAbilityTask_WaitDelay* StunTask = UAbilityTask_WaitDelay::WaitDelay(this, WallStunDuration))
		{
			EnemyAvatar->StopCurrentMontageWithoutFinish();
			StunTask->OnFinish.AddDynamic(this, &UCPP_EnemyDashAttackAbility::HandleDashWallStunFinished);
			StunTask->ReadyForActivation();
			return;
		}
	}

	EnemyAvatar->SetAttackMontagePausedFromAbility(false);
}

////////////////////////////
//! \author HanUl
//! \brief 벽 기절 대기 종료. 몽타주는 이미 정리됐으므로(종료 통지 억제) 어빌리티를 닫고 공격 종료를 직접 통지한다.
//! \param
//! \return
void UCPP_EnemyDashAttackAbility::HandleDashWallStunFinished()
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());

	FinishAbilityFromMontage(EnemyAvatar);

	if (EnemyAvatar)
	{
		EnemyAvatar->FinishPrimaryAttackFromAbility();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 공격 몽타주 종료 시 어빌리티를 닫는다(투사체/장판과 동일한 공통 종료 경로).
//!        돌진 태스크가 아직 살아 있으면 EndAbility가 태스크를 파괴하며 이동 모드도 복구된다.
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemyDashAttackAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	ActiveDashTask = nullptr;

	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveDashAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

void UCPP_EnemyDashAttackAbility::HandleTelegraphBeginEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	SpawnDashTelegraph(WindowPayload->TelegraphDuration);
}

void UCPP_EnemyDashAttackAbility::HandleTelegraphEndEvent(FGameplayEventData Payload)
{
	DestroyDashTelegraph();
}

////////////////////////////
//! \author HanUl
//! \brief 텔레그래프 Begin/End GameplayEvent 리스너를 준비한다. 몽타주에 EnemyTelegraph 노티파이 구간이
//!        있을 때만 이벤트가 오므로, 노티파이가 없는 돌진 몽타주에서는 아무 일도 일어나지 않는다.
//! \param
//! \return
void UCPP_EnemyDashAttackAbility::SetupTelegraphEventListeners()
{
	ActiveTelegraphBeginEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, BossGameplayTags::Event_Boss_Telegraph_Begin, nullptr, false, true);
	if (ActiveTelegraphBeginEventTask)
	{
		ActiveTelegraphBeginEventTask->EventReceived.AddDynamic(this, &UCPP_EnemyDashAttackAbility::HandleTelegraphBeginEvent);
		ActiveTelegraphBeginEventTask->ReadyForActivation();
	}

	ActiveTelegraphEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, BossGameplayTags::Event_Boss_Telegraph_End, nullptr, false, true);
	if (ActiveTelegraphEndEventTask)
	{
		ActiveTelegraphEndEventTask->EventReceived.AddDynamic(this, &UCPP_EnemyDashAttackAbility::HandleTelegraphEndEvent);
		ActiveTelegraphEndEventTask->ReadyForActivation();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 경로를 직선(사각) 텔레그래프로 표시한다. 치수는 돌진 파라미터(Range/캡슐)에서 자동 계산되고,
//!        방향은 현재 정면 — 몽타주 시작 시 타겟을 향해 고정된 상태라 실제 돌진 방향과 일치한다. (서버 스폰, 복제)
//! \param TelegraphDuration 채움 시간(노티파이 구간 길이)
//! \return
void UCPP_EnemyDashAttackAbility::SpawnDashTelegraph(float TelegraphDuration)
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UWorld* World = EnemyAvatar ? EnemyAvatar->GetWorld() : nullptr;
	if (!AttackPattern || !World || !EnemyAvatar->HasAuthority() || bHasStartedDash)
	{
		return;
	}

	DestroyDashTelegraph();

	TSubclassOf<ACPP_EnemyTelegraphActor> TelegraphActorClass = AttackPattern->TelegraphActorClass;
	if (!TelegraphActorClass)
	{
		TelegraphActorClass = ACPP_EnemyTelegraphActor::StaticClass();
	}

	FBossHitShapeData DashShape;
	DashShape.Shape = EBossAttackShape::Rectangle;
	DashShape.ForwardLength = AttackPattern->Range;
	DashShape.HalfWidth = AttackPattern->DashCollisionRadius;
	DashShape.HalfHeight = AttackPattern->DashCollisionHalfHeight;

	ACPP_EnemyTelegraphActor* TelegraphActor = World->SpawnActor<ACPP_EnemyTelegraphActor>(TelegraphActorClass);
	if (TelegraphActor)
	{
		TelegraphActor->Initialize(EnemyAvatar->GetActorTransform(), DashShape, TelegraphDuration);
		ActiveDashTelegraph = TelegraphActor;
	}
}

void UCPP_EnemyDashAttackAbility::DestroyDashTelegraph()
{
	if (ActiveDashTelegraph.IsValid())
	{
		ActiveDashTelegraph->Destroy();
	}
	ActiveDashTelegraph = nullptr;
}

ACPP_EnemyBase* UCPP_EnemyDashAttackAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}

////////////////////////////
//! \author HanSeul
//! \brief Builds and applies the dash hit Gameplay Effect using the active pattern.
//! \param EnemyAvatar Enemy that owns the active dash ability.
//! \param HitActor Target actor receiving dash damage.
//! \param AttackPattern Attack pattern that supplies damage and status settings.
//! \return true when the hit Gameplay Effect is applied successfully.
bool UCPP_EnemyDashAttackAbility::ApplyDashHitToActor(
	ACPP_EnemyBase* EnemyAvatar,
	AActor* HitActor,
	const UCPP_EnemyAttackPatternData* AttackPattern
)
{
	UAbilitySystemComponent* SourceASC = EnemyAvatar ? EnemyAvatar->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC || !HitActor || !AttackPattern || !AttackPattern->HitGameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(HitActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(EnemyAvatar);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		AttackPattern->HitGameplayEffect,
		1.0f,
		EffectContext
	);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	UMyAbilitySystemLibrary::AssignSetByCallerCoefficient(SpecHandle, AttackPattern->DamageCoefficient);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	ApplyStatusEffectToTarget(SourceASC, TargetASC, AttackPattern->StatusGameplayEffect, EffectContext);
	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Applies the optional status Gameplay Effect associated with the dash pattern.
//! \param SourceASC Ability System Component that creates and applies the effect.
//! \param TargetASC Ability System Component receiving the status effect.
//! \param StatusGameplayEffect Optional status Gameplay Effect class.
//! \param EffectContext Context shared with the dash hit effect.
//! \return true when the status Gameplay Effect is applied successfully.
bool UCPP_EnemyDashAttackAbility::ApplyStatusEffectToTarget(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> StatusGameplayEffect,
	const FGameplayEffectContextHandle& EffectContext
)
{
	if (!SourceASC || !TargetASC || !StatusGameplayEffect)
	{
		return false;
	}

	const FGameplayEffectSpecHandle StatusSpecHandle = SourceASC->MakeOutgoingSpec(
		StatusGameplayEffect,
		1.0f,
		EffectContext
	);
	if (!StatusSpecHandle.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpecHandle.Data.Get(), TargetASC);
	return true;
}
