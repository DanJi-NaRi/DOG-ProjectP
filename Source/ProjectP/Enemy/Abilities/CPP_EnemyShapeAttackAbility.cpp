// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_EnemyShapeAttackAbility.h"

#include "Enemy/Abilities/CPP_EnemyAttackShapeLibrary.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "Enemy/Actors/CPP_EnemyTelegraphActor.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"

////////////////////////////
//! \author HanUl
//! \brief 모양 커스텀 공격 발동. 윈도우/텔레그래프 이벤트 리스너를 준비하고 패턴 몽타주를 재생한다.
//!        판정/쿨다운 커밋은 몽타주의 EnemyAttackWindow 노티파이 도달 시점(HandleAttackWindowEvent)에 일어난다.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \param TriggerEventData Optional trigger payload.
//! \return
void UCPP_EnemyShapeAttackAbility::ActivateAbility(
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
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	if (!TargetActor || !AttackPattern || AttackPattern->AttackWindows.IsEmpty())
	{
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpecHandle = Handle;
	ActiveActivationInfo = ActivationInfo;
	bHasCommittedAttack = false;
	ClearActiveTelegraphs();
	EnemyAvatar->SetActiveShapeAttackAbility(this);

	// 리스너는 몽타주 시작 전에 준비한다 — 첫 프레임에 배치된 텔레그래프/윈도우 노티파이 대비.
	if (!SetupWindowEventListeners())
	{
		EnemyAvatar->ClearActiveShapeAttackAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bMontageStarted = EnemyAvatar->PlayPrimaryAttackMontageFromAbility(TargetActor);
	if (!bMontageStarted)
	{
		EnemyAvatar->ClearActiveShapeAttackAbility(this);
		EnemyAvatar->FinishPrimaryAttackFromAbility();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UCPP_EnemyShapeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	// 스태거 등 어떤 경로로 끝나도 남은 텔레그래프를 정리한다.
	ClearActiveTelegraphs();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* UCPP_EnemyShapeAttackAbility::GetCooldownTags() const
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
//! \brief 활성 공격 패턴이 정의한 쿨다운 태그/시간을 적용한다. (다른 적 공격 어빌리티와 동일)
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
//! \return
void UCPP_EnemyShapeAttackAbility::ApplyCooldown(
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
//! \brief 공격 몽타주 종료 시 어빌리티를 닫는다(투사체/장판/대시와 동일한 공통 종료 경로).
//! \param EnemyAvatar Enemy that owns this active ability.
//! \return
void UCPP_EnemyShapeAttackAbility::FinishAbilityFromMontage(ACPP_EnemyBase* EnemyAvatar)
{
	if (EnemyAvatar)
	{
		EnemyAvatar->ClearActiveShapeAttackAbility(this);
	}

	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		EndAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo, true, false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 윈도우 노티파이 도달: 첫 윈도우에서 쿨다운을 커밋하고, 해당 윈도우의 모양들로 타겟을 수집해 피해를 적용한다.
//! \param Payload WindowId를 담은 이벤트 페이로드
//! \return
void UCPP_EnemyShapeAttackAbility::HandleAttackWindowEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	if (!WindowPayload || !EnemyAvatar || !EnemyAvatar->HasAuthority())
	{
		return;
	}

	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar->GetPrimaryAttackPattern();
	const FBossAttackWindowData* AttackWindow = FindAttackWindow(AttackPattern, WindowPayload->WindowId);
	if (!AttackWindow)
	{
		return;
	}

	// 노티파이 도달 시점 커밋(적 어빌리티 공통 규칙). 다단히트 몽타주에서도 쿨다운은 첫 윈도우에서 한 번만.
	if (!bHasCommittedAttack)
	{
		const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
		if (!ActorInfo || !CommitAbility(ActiveSpecHandle, ActorInfo, ActiveActivationInfo))
		{
			return;
		}
		bHasCommittedAttack = true;
	}

	TSet<AActor*> HitTargets;
	for (const FBossHitShapeData& HitShape : AttackWindow->HitShapes)
	{
		CollectTargetsFromHitShape(HitShape, HitTargets);
	}

	ApplyDamageToTargets(EnemyAvatar, *AttackWindow, HitTargets);
}

void UCPP_EnemyShapeAttackAbility::HandleTelegraphBeginEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	SpawnTelegraphsForWindow(WindowPayload->WindowId, WindowPayload->TelegraphDuration);
}

void UCPP_EnemyShapeAttackAbility::HandleTelegraphEndEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	RemoveTelegraphsForWindow(WindowPayload->WindowId);
}

////////////////////////////
//! \author HanUl
//! \brief 윈도우/텔레그래프 GameplayEvent 리스너 3종을 생성·바인드한다. (보스와 같은 이벤트 태그를 리슨)
//! \param
//! \return 세 리스너 모두 생성에 성공하면 true
bool UCPP_EnemyShapeAttackAbility::SetupWindowEventListeners()
{
	ActiveAttackWindowEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, BossGameplayTags::Event_Boss_AttackWindow, nullptr, false, true);
	ActiveTelegraphBeginEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, BossGameplayTags::Event_Boss_Telegraph_Begin, nullptr, false, true);
	ActiveTelegraphEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, BossGameplayTags::Event_Boss_Telegraph_End, nullptr, false, true);

	if (!ActiveAttackWindowEventTask || !ActiveTelegraphBeginEventTask || !ActiveTelegraphEndEventTask)
	{
		return false;
	}

	ActiveAttackWindowEventTask->EventReceived.AddDynamic(this, &UCPP_EnemyShapeAttackAbility::HandleAttackWindowEvent);
	ActiveAttackWindowEventTask->ReadyForActivation();

	ActiveTelegraphBeginEventTask->EventReceived.AddDynamic(this, &UCPP_EnemyShapeAttackAbility::HandleTelegraphBeginEvent);
	ActiveTelegraphBeginEventTask->ReadyForActivation();

	ActiveTelegraphEndEventTask->EventReceived.AddDynamic(this, &UCPP_EnemyShapeAttackAbility::HandleTelegraphEndEvent);
	ActiveTelegraphEndEventTask->ReadyForActivation();
	return true;
}

const FBossAttackWindowData* UCPP_EnemyShapeAttackAbility::FindAttackWindow(const UCPP_EnemyAttackPatternData* AttackPattern, FName WindowId) const
{
	if (!AttackPattern || WindowId.IsNone())
	{
		return nullptr;
	}

	for (const FBossAttackWindowData& AttackWindow : AttackPattern->AttackWindows)
	{
		if (AttackWindow.WindowId == WindowId)
		{
			return &AttackWindow;
		}
	}

	return nullptr;
}

void UCPP_EnemyShapeAttackAbility::CollectTargetsFromHitShape(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return;
	}

	UCPP_EnemyAttackShapeLibrary::CollectTargetsFromHitShape(
		this,
		AvatarActor,
		AvatarActor->GetActorTransform(),
		HitShape,
		OutTargets
	);
}

////////////////////////////
//! \author HanUl
//! \brief 원(도넛) 판정. 수평 거리 + 높이로 검사한다. (보스 CollectTargetsFromCircle 이식 — 스케일 미적용 규칙 동일)
void UCPP_EnemyShapeAttackAbility::CollectTargetsFromCircle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.OuterRadius <= 0.0f)
	{
		return;
	}

	const FVector ShapeOrigin = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackCircle), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const float HorizontalDistanceSquared = FVector(ToTarget.X, ToTarget.Y, 0.0f).SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;

		if (bInRadius && bInHeight)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 부채꼴 판정. 수평 거리 + 높이 + 각도로 검사한다. (보스 CollectTargetsFromSector 이식)
void UCPP_EnemyShapeAttackAbility::CollectTargetsFromSector(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.OuterRadius <= 0.0f || HitShape.SectorAngleDegrees <= 0.0f)
	{
		return;
	}

	const FVector ShapeOrigin = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);
	const FRotator ShapeRotation(0.0f, AvatarActor->GetActorRotation().Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackSector), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);
	const float HalfAngleDegrees = HitShape.SectorAngleDegrees * 0.5f;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	const bool bFullCircle = HitShape.SectorAngleDegrees >= 360.0f;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const FVector HorizontalToTarget(ToTarget.X, ToTarget.Y, 0.0f);
		const float HorizontalDistanceSquared = HorizontalToTarget.SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;
		const bool bInAngle = bFullCircle || (HorizontalDistanceSquared > KINDA_SMALL_NUMBER && FVector::DotProduct(ShapeForward, HorizontalToTarget.GetSafeNormal()) >= MinDot);

		if (bInRadius && bInHeight && bInAngle)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 사각형 판정. 회전 박스 오버랩으로 검사한다. (보스 CollectTargetsFromRectangle 이식)
void UCPP_EnemyShapeAttackAbility::CollectTargetsFromRectangle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.ForwardLength <= 0.0f || HitShape.HalfWidth <= 0.0f || HitShape.HalfHeight <= 0.0f)
	{
		return;
	}

	const FRotator ShapeRotation(0.0f, AvatarActor->GetActorRotation().Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();
	const FVector ShapeStart = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);
	const FVector ShapeCenter = ShapeStart + ShapeForward * (HitShape.ForwardLength * 0.5f);
	const FVector BoxExtent(HitShape.ForwardLength * 0.5f, HitShape.HalfWidth, HitShape.HalfHeight);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyAttackRectangle), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeCenter,
		ShapeRotation.Quaternion(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(BoxExtent),
		QueryParams
	);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (AActor* TargetActor = OverlapResult.GetActor())
		{
			OutTargets.Add(TargetActor);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 수집된 타겟 중 적대 폰에만 패턴 HitGameplayEffect(윈도우별 피해 계수) + 상태이상을 적용한다.
//!        MaxShapeTargets가 1 이상이면 최근접 순으로 그 수만큼만 타격한다(단일 대상 스킬용).
//! \param EnemyAvatar 공격 주체 적
//! \param AttackWindow 피해 계수를 담은 윈도우 데이터
//! \param Targets 모양 판정으로 수집된 타겟들
//! \return
void UCPP_EnemyShapeAttackAbility::ApplyDamageToTargets(ACPP_EnemyBase* EnemyAvatar, const FBossAttackWindowData& AttackWindow, const TSet<AActor*>& Targets) const
{
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!AttackPattern || !AttackPattern->HitGameplayEffect || !SourceASC || AttackWindow.DamageCoefficient <= 0.0f)
	{
		return;
	}

	TArray<AActor*> HostileTargets;
	HostileTargets.Reserve(Targets.Num());
	for (AActor* TargetActor : Targets)
	{
		if (IsValid(TargetActor) && UMyAbilitySystemLibrary::IsHostile(EnemyAvatar, TargetActor))
		{
			HostileTargets.Add(TargetActor);
		}
	}

	if (AttackPattern->MaxShapeTargets > 0 && HostileTargets.Num() > AttackPattern->MaxShapeTargets)
	{
		const FVector Origin = EnemyAvatar->GetActorLocation();
		const AActor* AssignedObjective = EnemyAvatar->GetAssignedObjectiveTarget();
		HostileTargets.Sort([&Origin, AssignedObjective](const AActor& A, const AActor& B)
		{
			const bool bAIsAssignedObjective = &A == AssignedObjective;
			const bool bBIsAssignedObjective = &B == AssignedObjective;
			if (bAIsAssignedObjective != bBIsAssignedObjective)
			{
				return bAIsAssignedObjective;
			}

			return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
		});
		HostileTargets.SetNum(AttackPattern->MaxShapeTargets);
	}

	for (AActor* TargetActor : HostileTargets)
	{
		UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			AttackPattern->HitGameplayEffect,
			AttackWindow.DamageCoefficient
		);
		ApplyStatusEffectToTarget(SourceASC, TargetActor, AttackPattern->StatusGameplayEffect);
	}
}

bool UCPP_EnemyShapeAttackAbility::ApplyStatusEffectToTarget(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> StatusGameplayEffect
) const
{
	if (!SourceASC || !TargetActor || !StatusGameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());

	const FGameplayEffectSpecHandle StatusSpecHandle = SourceASC->MakeOutgoingSpec(StatusGameplayEffect, 1.0f, EffectContext);
	if (!StatusSpecHandle.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*StatusSpecHandle.Data.Get(), TargetASC);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 윈도우의 모양마다 텔레그래프 액터를 스폰한다. 서버에서만 스폰되며 텔레그래프의 복제 spawn-data가
//!        클라이언트 비주얼을 재구성한다. (보스 SpawnTelegraphsForWindow 이식)
//! \param WindowId 텔레그래프를 띄울 윈도우
//! \param TelegraphDuration 채움 시간(노티파이 구간 길이)
//! \return
void UCPP_EnemyShapeAttackAbility::SpawnTelegraphsForWindow(FName WindowId, float TelegraphDuration)
{
	ACPP_EnemyBase* EnemyAvatar = GetEnemyAvatar(GetCurrentActorInfo());
	const UCPP_EnemyAttackPatternData* AttackPattern = EnemyAvatar ? EnemyAvatar->GetPrimaryAttackPattern() : nullptr;
	UWorld* World = EnemyAvatar ? EnemyAvatar->GetWorld() : nullptr;
	const FBossAttackWindowData* AttackWindow = FindAttackWindow(AttackPattern, WindowId);
	if (!AttackWindow || !World || !EnemyAvatar->HasAuthority())
	{
		return;
	}

	RemoveTelegraphsForWindow(WindowId);

	TSubclassOf<ACPP_EnemyTelegraphActor> TelegraphActorClass = AttackPattern->TelegraphActorClass;
	if (!TelegraphActorClass)
	{
		TelegraphActorClass = ACPP_EnemyTelegraphActor::StaticClass();
	}

	TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>& TelegraphActors = ActiveTelegraphActorsByWindow.FindOrAdd(WindowId);
	for (const FBossHitShapeData& HitShape : AttackWindow->HitShapes)
	{
		ACPP_EnemyTelegraphActor* TelegraphActor = World->SpawnActor<ACPP_EnemyTelegraphActor>(TelegraphActorClass);
		if (!TelegraphActor)
		{
			continue;
		}

		TelegraphActor->Initialize(EnemyAvatar->GetActorTransform(), HitShape, TelegraphDuration);
		TelegraphActors.Add(TelegraphActor);
	}
}

void UCPP_EnemyShapeAttackAbility::RemoveTelegraphsForWindow(FName WindowId)
{
	TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>> TelegraphActors;
	if (!ActiveTelegraphActorsByWindow.RemoveAndCopyValue(WindowId, TelegraphActors))
	{
		return;
	}

	for (const TWeakObjectPtr<ACPP_BossTelegraphActor>& TelegraphActor : TelegraphActors)
	{
		if (TelegraphActor.IsValid())
		{
			TelegraphActor->Destroy();
		}
	}
}

void UCPP_EnemyShapeAttackAbility::ClearActiveTelegraphs()
{
	for (const TPair<FName, TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>>& TelegraphPair : ActiveTelegraphActorsByWindow)
	{
		for (const TWeakObjectPtr<ACPP_BossTelegraphActor>& TelegraphActor : TelegraphPair.Value)
		{
			if (TelegraphActor.IsValid())
			{
				TelegraphActor->Destroy();
			}
		}
	}

	ActiveTelegraphActorsByWindow.Empty();
}

ACPP_EnemyBase* UCPP_EnemyShapeAttackAbility::GetEnemyAvatar(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	return Cast<ACPP_EnemyBase>(ActorInfo->AvatarActor.Get());
}
