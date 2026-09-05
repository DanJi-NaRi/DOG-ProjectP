////////////////////////////
//! \file MyGA_Heru_Charge.cpp
//! \brief Heru 전방 반원 타격 스킬 GameplayAbility를 구현한다.
#include "MyGA_Heru_Charge.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "../MyAbilitySystemLibrary.h"
#include "../MyAttributeSet.h"
#include "../MySkillDebugShape.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../../MyGameplayTags.h"

////////////////////////////
//! \author HanUl
//! \brief Heru Charge 기본값을 초기화한다. 몽타주는 예측하되 피해/표식/쿨감은 서버에서만 적용한다.
//! \param 없음
//! \return 없음
UMyGA_Heru_Charge::UMyGA_Heru_Charge()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인으로 Charge를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_Heru_Charge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief 조준(마우스) 방향과 필수 스킬 데이터를 확정한다. 방향을 못 구하거나 데이터가 없으면 발동을 막는다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGA_Heru_Charge::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	if (SkillData.Targeting.Radius <= 0.0f || !SkillData.Effects.HitGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru charge activation failed - targeting radius or hit GE missing. Radius: %.2f, HitGE: %s"),
			SkillData.Targeting.Radius,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect));
		return false;
	}

	if (!ResolveChargeAimDirection(ActorInfo, TriggerEventData, CachedAimDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru charge activation failed - aim direction is missing. Ability: %s"), *GetNameSafe(this));
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 Shoot 시점에 제자리에서 조준 방향 전방 반원 타격을 수행한다. 클라/서버 모두 호출되며 피해는 서버에서만 적용한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Charge::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return;
	}

	// 판정 범위 디버그는 로컬 예측 인스턴스에서 스킬 소유자 화면에만 표시한다.
	if (bDrawDebugArc)
	{
		DrawDebugArc(AvatarActor, SkillData);
	}

	// 피해/표식/쿨감은 서버 권위에서만 적용한다(클라는 연출만).
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return;
	}

	TArray<AActor*> Targets;
	ResolveChargeArcTargets(AvatarActor, SkillData, Targets);

	const float DamageCoefficient = SkillData.Effects.DamageCoefficient;
	int32 KillCount = 0;

	for (AActor* TargetActor : Targets)
	{
		UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		const float HealthBefore = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());

		// 피해: 계수만 전달하고 공격력 곱셈/치명타/방어 감쇄는 ExecCalc가 처리한다. 쿨다운 태그는 처치 스킬 식별용 꼬리표.
		UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			SkillData.Effects.HitGameplayEffect,
			DamageCoefficient,
			SkillData.CooldownTag,
			SkillData.InputTag
		);

		const float HealthAfter = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
		const bool bKilled = HealthBefore > 0.0f && HealthAfter <= 0.0f;

		if (bKilled)
		{
			++KillCount;
		}
		else if (SkillData.Effects.StatusGameplayEffect)
		{
			// 표식 1스택 부여(살아있는 대상만). 스택 상한/지속/갱신은 표식 GE의 스태킹 설정이 관리한다.
			FGameplayEffectContextHandle MarkContext = SourceASC->MakeEffectContext();
			MarkContext.AddSourceObject(AvatarActor);
			FGameplayEffectSpecHandle MarkSpec = SourceASC->MakeOutgoingSpec(SkillData.Effects.StatusGameplayEffect, 1.0f, MarkContext);
			if (MarkSpec.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*MarkSpec.Data.Get(), TargetASC);
			}
		}
	}

	// 처치가 1명 이상 발생하면 처치 수와 무관하게 1회 감소한다.
	if (KillCount > 0 && KillCooldownReductionSeconds > 0.0f && SkillData.CooldownTag.IsValid())
	{
		UMyAbilitySystemLibrary::ReduceCooldownByTag(SourceASC, SkillData.CooldownTag, KillCooldownReductionSeconds);
	}

	UE_LOG(LogTemp, Log, TEXT("Heru charge impact - Avatar: %s, Targets: %d, Kills: %d, Coefficient: %.2f"),
		*GetNameSafe(AvatarActor),
		Targets.Num(),
		KillCount,
		DamageCoefficient);
}

////////////////////////////
//! \author HanUl
//! \brief 시전자 위치 전방 반원(Targeting.Angle) 반경(Targeting.Radius) 내 적대 대상을 수집한다.
//! \param AvatarActor 판정 기준 Avatar
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \param OutTargets 수집된 대상 목록(출력)
//! \return 없음
void UMyGA_Heru_Charge::ResolveChargeArcTargets(const AActor* AvatarActor, const FMySkillDataEntry& SkillData, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	const UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World || SkillData.Targeting.Radius <= 0.0f)
	{
		return;
	}

	const FVector Origin = AvatarActor->GetActorLocation();
	FVector Forward = CachedAimDirection;
	if (Forward.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		Forward = AvatarActor->GetActorForwardVector();
	}
	Forward = Forward.GetSafeNormal2D();

	// 반원(전방 180°) = halfAngle 90° → minDot 0. Angle이 360 이상이면 전방향으로 처리한다.
	const float HalfAngleDegrees = FMath::Clamp(SkillData.Targeting.Angle, 0.0f, 360.0f) * 0.5f;
	const bool bFullCircle = SkillData.Targeting.Angle >= 360.0f;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));

	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeruChargeArc), false, AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(
			OverlapResults,
			Origin,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(SkillData.Targeting.Radius),
			QueryParams))
	{
		return;
	}

	TSet<AActor*> UniqueTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || UniqueTargets.Contains(TargetActor))
		{
			continue;
		}

		if (!UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}

		if (!bFullCircle && !Forward.IsNearlyZero())
		{
			const FVector ToTarget = (TargetActor->GetActorLocation() - Origin).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero() && FVector::DotProduct(Forward, ToTarget) < MinDot)
			{
				continue;
			}
		}

		UniqueTargets.Add(TargetActor);
		OutTargets.Add(TargetActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 입력 EventData의 조준(마우스) 지점을 향하는 방향을 확정한다. 조준 지점이 없으면 바라보는 방향으로 폴백한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param OutDirection 확정된 조준 방향(수평 정규화)
//! \return 방향을 확정했으면 true
bool UMyGA_Heru_Charge::ResolveChargeAimDirection(
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

	// 조준(마우스) 지점 = TargetData 중 bBlockingHit==true 항목(이동 방향 항목은 false라 제외된다).
	if (TriggerEventData)
	{
		const FVector Origin = AvatarActor->GetActorLocation();
		for (int32 Index = 0; Index < TriggerEventData->TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(Index);
			if (!TargetData || !TargetData->HasHitResult())
			{
				continue;
			}

			const FHitResult* HitResult = TargetData->GetHitResult();
			if (!HitResult || !HitResult->bBlockingHit)
			{
				continue;
			}

			FVector ToAim = HitResult->Location - Origin;
			ToAim.Z = 0.0f;
			const FVector Candidate = ToAim.GetSafeNormal2D();
			if (!Candidate.IsNearlyZero())
			{
				OutDirection = Candidate;
				return true;
			}
		}
	}

	// 폴백: 조준 지점이 없으면 바라보는 방향을 사용한다.
	OutDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief 공격 반원 범위를 디버그로 시각화한다(임시). 시전자 위치 기준 반경 원과 전방 반원 경계를 그린다.
//! \param AvatarActor 판정 기준 Avatar
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Charge::DrawDebugArc(AActor* AvatarActor, const FMySkillDataEntry& SkillData) const
{
	if (!AvatarActor || SkillData.Targeting.Radius <= 0.0f)
	{
		return;
	}

	const FVector Origin = AvatarActor->GetActorLocation();
	FVector Forward = CachedAimDirection.IsNearlyZero() ? AvatarActor->GetActorForwardVector() : CachedAimDirection;
	Forward = Forward.GetSafeNormal2D();

	const float Radius = SkillData.Targeting.Radius;
	const float AngleDegrees = FMath::Clamp(SkillData.Targeting.Angle, 0.0f, 360.0f);
	constexpr float DebugDuration = 2.0f;

	// 전체 판정 반경 원 + 전방 반원 경계(부채꼴) + 중앙 방향.
	MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
		FMySkillDebugShape::MakeCircle(Origin, Radius, FColor::Red, DebugDuration, 2.0f));
	MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
		FMySkillDebugShape::MakeCone(Origin, Forward, Radius, AngleDegrees, FColor::Red, DebugDuration, 2.0f));
	MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
		FMySkillDebugShape::MakeLine(Origin, Origin + Forward * Radius, FColor::Yellow, DebugDuration, 3.0f));
}
