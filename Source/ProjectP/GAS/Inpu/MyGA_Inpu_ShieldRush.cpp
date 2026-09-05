////////////////////////////
//! \file MyGA_Inpu_ShieldRush.cpp
//! \brief Inpu의 방패 돌진 스킬 GameplayAbility를 구현한다.

#include "MyGA_Inpu_ShieldRush.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MySkillDebugShape.h"
#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "MyInpuGameplayEffects.h"
#include "MyGameplayTags.h"
#include "TimerManager.h"

namespace
{
	constexpr float ShieldRushMinValue = 1.0f;
	constexpr float ShieldRushPathSampleInterval = 0.02f;
	constexpr float ShieldRushDebugHalfHeight = 100.0f;
	constexpr float ShieldRushKnockbackDistance = 300.0f;
	constexpr float ShieldRushKnockbackDuration = 0.2f;
	constexpr float ShieldRushDamageTakenIncrease = 0.2f;
	constexpr float ShieldRushDamageTakenIncreaseDuration = 3.0f;
}

////////////////////////////
//! \author HanUl
//! \brief Shield Rush 기본값을 초기화한다. 이동은 로컬 예측하고 판정과 효과는 서버에서만 수행한다.
//! \param 없음
//! \return 없음
UMyGA_Inpu_ShieldRush::UMyGA_Inpu_ShieldRush()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 SkillDefinition 스킬 파이프라인으로 Shield Rush를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \return 없음
void UMyGA_Inpu_ShieldRush::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ResetShieldRushState();
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief 취소·예측 거부·정상 종료 경로에서 타이머, Cue, 충돌과 루트모션 상태를 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Inpu_ShieldRush::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PathCollisionTimerHandle);
	}

	if (ActiveDashTask)
	{
		ActiveDashTask->EndTask();
		ActiveDashTask = nullptr;
	}

	StopDashTrailCue(ActorInfo);
	RestoreMovementState(ActorInfo);
	bShieldRushActive = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief Definition 필수값과 MouseCursor 조준 정책을 검증하고 마우스 방향을 확정한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param SkillData 현재 Shield Rush SkillDefinition 데이터
//! \return 발동 가능한 데이터와 방향이면 true
bool UMyGA_Inpu_ShieldRush::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	if (!SkillDefinition)
	{
		return false;
	}

	if (!SkillDefinition->GetAnimation().Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Shield Rush activation failed - LocalPredicted dash requires a montage. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (SkillData.Targeting.Width <= 0.0f
		|| SkillData.Targeting.Radius <= 0.0f
		|| SkillData.Movement.DashStrength <= 0.0f
		|| SkillData.Timing.ActiveDuration <= 0.0f
		|| !SkillData.Effects.HitGameplayEffect
		|| SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Shield Rush activation failed - Definition tuning invalid. Width: %.1f, Radius: %.1f, Speed: %.1f, Duration: %.2f, HitGE: %s, Coefficient: %.2f"),
			SkillData.Targeting.Width,
			SkillData.Targeting.Radius,
			SkillData.Movement.DashStrength,
			SkillData.Timing.ActiveDuration,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient);
		return false;
	}
	if (SkillData.Input.AimSource != EMySkillAimSource::MouseCursor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Shield Rush activation failed - Input.AimSource must be MouseCursor. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (!ResolveDashDirection(ActorInfo, TriggerEventData, CachedDashDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Shield Rush activation failed - aim direction missing. Avatar: %s"),
			ActorInfo && ActorInfo->AvatarActor.IsValid() ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"));
		return false;
	}

	CachedDashDistance = SkillData.Movement.DashStrength * SkillData.Timing.ActiveDuration;
	CachedPathWidth = SkillData.Targeting.Width;
	CachedImpactRadius = SkillData.Targeting.Radius;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Commit 직후 클라이언트와 서버에서 같은 방향의 예측 돌진을 시작한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param SkillData 현재 Shield Rush SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_ShieldRush::OnStandardSkillCommitted(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;
	BeginShieldRush(ActorInfo, SkillData);
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 EndAttack이 돌진보다 먼저 오면 Ability 종료만 돌진 완료까지 지연한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param SkillData 현재 Shield Rush SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_ShieldRush::OnStandardSkillEndAttack(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	if (bShieldRushActive)
	{
		ApplySkillInputBlock(ActorInfo);
		ApplyMoveInputBlockFromSkillData(SkillData, ActorInfo);
		bEndAttackRequested = true;
		return;
	}

	Super::OnStandardSkillEndAttack(ActorInfo, TriggerEventData, SkillData);
}

////////////////////////////
//! \author HanUl
//! \brief TargetData의 마우스 월드 지점을 사용하고, 지점이 없을 때만 현재 전방으로 폴백한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param OutDirection 확정된 수평 돌진 방향
//! \return 유효한 방향을 얻었으면 true
bool UMyGA_Inpu_ShieldRush::ResolveDashDirection(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	FVector& OutDirection
) const
{
	OutDirection = FVector::ZeroVector;
	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return false;
	}

	if (TriggerEventData)
	{
		for (int32 Index = 0; Index < TriggerEventData->TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(Index);
			const FHitResult* HitResult = TargetData && TargetData->HasHitResult() ? TargetData->GetHitResult() : nullptr;
			if (!HitResult || !HitResult->bBlockingHit)
			{
				continue;
			}

			OutDirection = (HitResult->Location - AvatarActor->GetActorLocation()).GetSafeNormal2D();
			if (!OutDirection.IsNearlyZero())
			{
				return true;
			}
		}

		// CanActivateStandardSkill에서 MouseCursor 정책을 보장하므로 0도 유효한 월드 Yaw다.
		OutDirection = FRotator(0.0f, TriggerEventData->EventMagnitude, 0.0f).Vector().GetSafeNormal2D();
		if (!OutDirection.IsNearlyZero())
		{
			return true;
		}
	}

	OutDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief Pawn 충돌을 잠시 무시하고 고정 거리 RootMotion 돌진과 서버 경로 판정 타이머를 시작한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param SkillData 현재 Shield Rush SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_ShieldRush::BeginShieldRush(const FGameplayAbilityActorInfo* ActorInfo, const FMySkillDataEntry& SkillData)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!AvatarCharacter || CachedDashDirection.IsNearlyZero())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	DashStartLocation = AvatarCharacter->GetActorLocation();
	LastPathSampleLocation = DashStartLocation;
	PathHitActors.Reset();

	if (UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
	{
		CachedPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	AvatarCharacter->SetAnimRootMotionTranslationScale(0.0f);
	bMovementStateModified = true;
	bShieldRushActive = true;

	const float DashSpeed = FMath::Max(SkillData.Movement.DashStrength, ShieldRushMinValue);
	const float DashDuration = SkillData.Timing.ActiveDuration;
	ActiveDashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("InpuShieldRush"),
		CachedDashDirection,
		DashSpeed,
		DashDuration,
		false,
		nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		0.0f,
		true
	);
	if (!ActiveDashTask)
	{
		HandleShieldRushFinished();
		return;
	}

	ActiveDashTask->OnFinish.AddDynamic(this, &UMyGA_Inpu_ShieldRush::HandleShieldRushFinished);
	StartDashTrailCue(ActorInfo);
	ActiveDashTask->ReadyForActivation();

	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UWorld* World = AvatarCharacter->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PathCollisionTimerHandle,
				this,
				&UMyGA_Inpu_ShieldRush::UpdatePathCollision,
				ShieldRushPathSampleInterval,
				true
			);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 직전 위치부터 현재 위치까지 경로 충돌을 연속 Sweep한다.
//! \param 없음
//! \return 없음
void UMyGA_Inpu_ShieldRush::UpdatePathCollision()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	AActor* AvatarActor = CurrentActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	const FVector CurrentLocation = AvatarActor->GetActorLocation();
	SweepPathSegment(LastPathSampleLocation, CurrentLocation);
	LastPathSampleLocation = CurrentLocation;
}

////////////////////////////
//! \author HanUl
//! \brief 지정 구간을 구형 Sweep하여 경로 폭 안의 적대 대상을 한 번씩 처리한다.
//! \param SegmentStart Sweep 시작 위치
//! \param SegmentEnd Sweep 종료 위치
//! \return 없음
void UMyGA_Inpu_ShieldRush::SweepPathSegment(const FVector& SegmentStart, const FVector& SegmentEnd)
{
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !World || CachedPathWidth <= 0.0f)
	{
		return;
	}

	TArray<FHitResult> HitResults;
	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuShieldRushPath), false, AvatarActor);
	World->SweepMultiByObjectType(
		HitResults,
		SegmentStart,
		SegmentEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(CachedPathWidth * 0.5f),
		QueryParams
	);

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		const TWeakObjectPtr<AActor> WeakTarget(TargetActor);
		if (!TargetActor || PathHitActors.Contains(WeakTarget) || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}

		PathHitActors.Add(WeakTarget);
		ApplyPathHit(TargetActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 경로 적에게 계수 피해, 넉백 상태, 물리 넉백과 받는 피해 증가를 적용한다.
//! \param TargetActor 경로에서 처음 적중한 적대 Actor
//! \return 없음
void UMyGA_Inpu_ShieldRush::ApplyPathHit(AActor* TargetActor)
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
	if (!SourceASC || !TargetASC)
	{
		return;
	}
	if (TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
		SourceASC,
		TargetActor,
		CachedSkillDataEntry.Effects.HitGameplayEffect,
		CachedSkillDataEntry.Effects.DamageCoefficient,
		CachedSkillDataEntry.CooldownTag,
		CachedSkillDataEntry.InputTag
	);

	const bool bKnockbackStatusApplied = ApplyTimedStatusEffect(
		TargetASC,
		CachedSkillDataEntry.Effects.StatusGameplayEffect,
		ShieldRushKnockbackDuration,
		MyGameplayTags::Status_CC_Knockback
	);
	if (bKnockbackStatusApplied
		&& TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) > 0.0f)
	{
		ApplyPhysicalKnockback(TargetActor);
		ApplyDamageTakenIncrease(TargetASC);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 도착 지점 충격파 반경 안의 적대 대상을 중복 없이 수집한다.
//! \param ImpactLocation 충격파 중심 위치
//! \param OutTargets 수집된 적대 대상 목록
//! \return 없음
void UMyGA_Inpu_ShieldRush::CollectImpactTargets(const FVector& ImpactLocation, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !World || CachedImpactRadius <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuShieldRushImpact), false, AvatarActor);
	World->OverlapMultiByObjectType(
		OverlapResults,
		ImpactLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(CachedImpactRadius),
		QueryParams
	);

	TSet<AActor*> UniqueTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || UniqueTargets.Contains(TargetActor) || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}

		UniqueTargets.Add(TargetActor);
		OutTargets.Add(TargetActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Character는 지정 거리/시간으로 계산한 Launch 속도로, 그 외 Actor는 스윕 이동으로 밀어낸다.
//! \param TargetActor 밀어낼 대상 Actor
//! \return 없음
void UMyGA_Inpu_ShieldRush::ApplyPhysicalKnockback(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return;
	}

	const FVector KnockbackDirection = CachedDashDirection.GetSafeNormal2D();
	if (ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		const float KnockbackSpeed = ShieldRushKnockbackDistance / ShieldRushKnockbackDuration;
		TargetCharacter->LaunchCharacter(KnockbackDirection * KnockbackSpeed, true, true);
		return;
	}

	TargetActor->AddActorWorldOffset(KnockbackDirection * ShieldRushKnockbackDistance, true);
}

////////////////////////////
//! \author HanUl
//! \brief Duration SetByCaller를 넣은 상태 GameplayEffect를 대상 ASC에 적용한다.
//! \param TargetASC 상태 효과를 받을 대상 ASC
//! \param EffectClass 적용할 GameplayEffect 클래스
//! \param Duration 상태 지속시간(초)
//! \param StatusTag 효과가 유지되는 동안 부여할 상태 태그
//! \return GameplayEffect 적용 요청에 성공하면 true
bool UMyGA_Inpu_ShieldRush::ApplyTimedStatusEffect(
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> EffectClass,
	float Duration,
	FGameplayTag StatusTag
) const
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC || !TargetASC || Duration <= 0.0f)
	{
		return false;
	}
	TSubclassOf<UGameplayEffect> ResolvedEffectClass = EffectClass;
	if (!ResolvedEffectClass)
	{
		ResolvedEffectClass = TSubclassOf<UGameplayEffect>(UMyInpuTimedStatusGameplayEffect::StaticClass());
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceASC->GetAvatarActor());
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ResolvedEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Duration, Duration);
	if (StatusTag.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(StatusTag);
	}
	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return AppliedHandle.IsValid();
}

////////////////////////////
//! \author HanUl
//! \brief Shield Rush 고정 증가량과 지속시간을 SetByCaller로 넣어 받는 피해 증가 GE를 적용한다.
//! \param TargetASC 받는 피해 증가 효과를 받을 대상 ASC
//! \return GameplayEffect 적용 요청에 성공하면 true
bool UMyGA_Inpu_ShieldRush::ApplyDamageTakenIncrease(UAbilitySystemComponent* TargetASC) const
{
	UAbilitySystemComponent* SourceASC = CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC
		|| !TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceASC->GetAvatarActor());
	const TSubclassOf<UGameplayEffect> DamageTakenIncreaseEffectClass(
		UMyInpuDamageTakenIncreaseGameplayEffect::StaticClass());
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageTakenIncreaseEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Duration, ShieldRushDamageTakenIncreaseDuration);
	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_DamageTakenMultiplier, ShieldRushDamageTakenIncrease);
	SpecHandle.Data->DynamicGrantedTags.AddTag(MyGameplayTags::Status_Debuff_Verdict);
	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return AppliedHandle.IsValid();
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 RootMotion 완료 시 마지막 경로 판정, 충격파, Cue와 디버그 도형을 서버에서 처리한다.
//! \param 없음
//! \return 없음
void UMyGA_Inpu_ShieldRush::HandleShieldRushFinished()
{
	if (!bShieldRushActive)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const FVector EndLocation = AvatarActor ? AvatarActor->GetActorLocation() : LastPathSampleLocation;
	TArray<AActor*> ImpactTargets;

	if (ActorInfo && ActorInfo->IsNetAuthority() && AvatarActor)
	{
		CollectImpactTargets(EndLocation, ImpactTargets);
		SweepPathSegment(LastPathSampleLocation, EndLocation);
		ExecuteImpactCue(EndLocation);
		DrawDebugShieldRush(EndLocation, ImpactTargets);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PathCollisionTimerHandle);
	}
	ActiveDashTask = nullptr;
	bShieldRushActive = false;
	StopDashTrailCue(ActorInfo);
	RestoreMovementState(ActorInfo);

	UE_LOG(LogTemp, Log, TEXT("Inpu Shield Rush finished - Avatar: %s, PathHits: %d, ImpactTargets: %d, Start: %s, End: %s"),
		*GetNameSafe(AvatarActor),
		PathHitActors.Num(),
		ImpactTargets.Num(),
		*DashStartLocation.ToCompactString(),
		*EndLocation.ToCompactString());

	if (bEndAttackRequested)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 구간에 공용 Dash Trail GameplayCue를 활성화한다.
//! \param ActorInfo Ability 소유자와 ASC 정보
//! \return 없음
void UMyGA_Inpu_ShieldRush::StartDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (bDashTrailCueActive)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->AddGameplayCue(MyGameplayTags::GameplayCue_Ability_Move_DashTrail, ASC->MakeEffectContext());
		bDashTrailCueActive = true;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 돌진 완료 또는 취소 시 공용 Dash Trail GameplayCue를 제거한다.
//! \param ActorInfo Ability 소유자와 ASC 정보
//! \return 없음
void UMyGA_Inpu_ShieldRush::StopDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bDashTrailCueActive)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveGameplayCue(MyGameplayTags::GameplayCue_Ability_Move_DashTrail);
	}
	bDashTrailCueActive = false;
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 도착 충격파 전용 GameplayCue를 실행한다.
//! \param ImpactLocation 충격파 중심 위치
//! \return 없음
void UMyGA_Inpu_ShieldRush::ExecuteImpactCue(const FVector& ImpactLocation) const
{
	UAbilitySystemComponent* ASC = CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Location = ImpactLocation;
	CueParameters.Instigator = ASC->GetAvatarActor();
	CueParameters.EffectCauser = ASC->GetAvatarActor();
	ASC->ExecuteGameplayCue(MyGameplayTags::GameplayCue_Ability_Inpu_ShieldRush, CueParameters);
}

////////////////////////////
//! \author HanUl
//! \brief 돌진을 위해 변경한 Pawn 충돌과 몽타주 RootMotion 배율을 복원한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGA_Inpu_ShieldRush::RestoreMovementState(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bMovementStateModified)
	{
		return;
	}

	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr);
	if (AvatarCharacter)
	{
		if (UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, CachedPawnCollisionResponse);
		}
		AvatarCharacter->SetAnimRootMotionTranslationScale(1.0f);
	}
	bMovementStateModified = false;
}

////////////////////////////
//! \author HanUl
//! \brief 서버가 판정한 실제 돌진 경로, 충격파 반경과 대상 연결선을 소유자 화면에 표시한다.
//! \param EndLocation 실제 돌진 종료 위치
//! \param ImpactTargets 충격파 반경에서 수집한 대상 목록
//! \return 없음
void UMyGA_Inpu_ShieldRush::DrawDebugShieldRush(const FVector& EndLocation, const TArray<AActor*>& ImpactTargets) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!bDrawDebugShieldRush || !AvatarActor)
	{
		return;
	}

	FVector PathDirection = (EndLocation - DashStartLocation).GetSafeNormal2D();
	if (PathDirection.IsNearlyZero())
	{
		PathDirection = CachedDashDirection;
	}
	const float PathLength = FVector::Dist2D(DashStartLocation, EndLocation);
	const FVector BoxCenter = (DashStartLocation + EndLocation) * 0.5f;
	const FVector BoxExtent(PathLength * 0.5f, CachedPathWidth * 0.5f, ShieldRushDebugHalfHeight);

	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeLine(DashStartLocation, EndLocation, FColor::Yellow, DebugShapeLifeTime, 3.0f));
	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeBox(BoxCenter, BoxExtent, PathDirection, FColor::Red, DebugShapeLifeTime, 2.0f));
	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeSphere(EndLocation, CachedImpactRadius, FColor::Orange, DebugShapeLifeTime, 2.0f));
	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeCircle(EndLocation, CachedImpactRadius, FColor::Orange, DebugShapeLifeTime, 2.0f));

	for (const AActor* TargetActor : ImpactTargets)
	{
		if (TargetActor)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeLine(EndLocation, TargetActor->GetActorLocation(), FColor::Green, DebugShapeLifeTime, 3.0f));
		}
	}
#endif
}

////////////////////////////
//! \author HanUl
//! \brief 새 발동 전에 Shield Rush 인스턴스 상태를 초기화한다.
//! \param 없음
//! \return 없음
void UMyGA_Inpu_ShieldRush::ResetShieldRushState()
{
	CachedDashDirection = FVector::ZeroVector;
	DashStartLocation = FVector::ZeroVector;
	LastPathSampleLocation = FVector::ZeroVector;
	CachedDashDistance = 0.0f;
	CachedPathWidth = 0.0f;
	CachedImpactRadius = 0.0f;
	PathHitActors.Reset();
	bMovementStateModified = false;
	bDashTrailCueActive = false;
	bShieldRushActive = false;
	bEndAttackRequested = false;
}
