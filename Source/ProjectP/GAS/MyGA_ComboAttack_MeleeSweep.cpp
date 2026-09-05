////////////////////////////
//! \file MyGA_ComboAttack_MeleeSweep.cpp
//! \brief 근접 부채꼴 다단 기본 공격 GameplayAbility 구현 파일이다. (Heru, Inpu)

#include "MyGA_ComboAttack_MeleeSweep.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MyAbilitySystemLibrary.h"
#include "MySkillDebugShape.h"
#include "SkillData/MySkillSetDataAsset.h"

////////////////////////////
//! \author HanUl
//! \brief 발동 시점 조준 소비 플래그를 초기화하고 콤보 체인을 시작한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_ComboAttack_MeleeSweep::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	bTriggerAimConsumed = false;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief Fire 시점에 서버에서 전방 부채꼴 히트스캔 판정을 수행한다.
//! \param StepIndex 현재 콤보 스텝 인덱스(0부터)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_ComboAttack_MeleeSweep::OnComboStepFire(int32 StepIndex, const FMySkillDataEntry& SkillData)
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

	const FVector SweepDirection = ResolveSweepDirection(AvatarActor);
	const int32 HitCount = ExecuteMeleeSweep(AvatarActor, StepIndex, SweepDirection, SkillData);

	UE_LOG(LogTemp, Log, TEXT("MyGAS melee sweep combo attack fired - Ability: %s, Avatar: %s, StepIndex: %d, HitCount: %d"),
		*GetNameSafe(this),
		*GetNameSafe(AvatarActor),
		StepIndex,
		HitCount);
}

////////////////////////////
//! \author HanUl
//! \brief 판정 방향을 결정한다. 첫 판정은 발동 시점 조준(EventMagnitude Yaw)을 사용하고,
//!        체인된 타는 애니메이션과 일치하도록 아바타 전방을 사용한다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \return 정규화된 판정 방향
FVector UMyGA_ComboAttack_MeleeSweep::ResolveSweepDirection(AActor* AvatarActor)
{
	const FGameplayEventData* TriggerData = GetComboTriggerEventData();
	if (!bTriggerAimConsumed && TriggerData)
	{
		bTriggerAimConsumed = true;
		const float AimYaw = FRotator::NormalizeAxis(TriggerData->EventMagnitude);
		const FVector AimDirection = FRotator(0.0f, AimYaw, 0.0f).Vector().GetSafeNormal();
		if (!AimDirection.IsNearlyZero())
		{
			return AimDirection;
		}
	}

	return AvatarActor->GetActorForwardVector().GetSafeNormal2D();
}

////////////////////////////
//! \author HanUl
//! \brief 부채꼴(Targeting.Radius/Angle) 안의 모든 적대 대상에게 현재 스텝 타격을 적용한다. 관통 판정이다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \param StepIndex 현재 콤보 스텝 인덱스(0부터)
//! \param SweepDirection 부채꼴 중심 방향
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 타격한 대상 수
int32 UMyGA_ComboAttack_MeleeSweep::ExecuteMeleeSweep(AActor* AvatarActor, int32 StepIndex, const FVector& SweepDirection, const FMySkillDataEntry& SkillData) const
{
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	const float SweepRadius = SkillData.Targeting.Radius;
	if (SweepRadius <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS melee sweep rejected - Targeting.Radius invalid. Ability: %s, SkillId: %s, Radius: %.2f"),
			*GetNameSafe(this),
			*SkillData.SkillId.ToString(),
			SweepRadius);
		return 0;
	}

	// Angle이 0이면 전방위(360도)로 취급한다
	const float ConeAngleDegrees = SkillData.Targeting.Angle > 0.0f ? SkillData.Targeting.Angle : 360.0f;
	const float HalfAngleCos = FMath::Cos(FMath::DegreesToRadians(ConeAngleDegrees * 0.5f));
	const FVector Origin = AvatarActor->GetActorLocation();
	const FVector SweepDirection2D = SweepDirection.GetSafeNormal2D();

	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComboAttackMeleeSweep), false, AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		Origin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SweepRadius),
		QueryParams
	);

	TArray<AActor*> HitTargets;
	TSet<AActor*> ProcessedTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || ProcessedTargets.Contains(TargetActor))
		{
			continue;
		}
		ProcessedTargets.Add(TargetActor);

		if (!UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}

		// 부채꼴 각도 판정: 대상이 아바타와 겹쳐 있으면(방향 산출 불가) 적중으로 취급한다
		const FVector ToTarget2D = (TargetActor->GetActorLocation() - Origin).GetSafeNormal2D();
		if (!ToTarget2D.IsNearlyZero() && FVector::DotProduct(SweepDirection2D, ToTarget2D) < HalfAngleCos)
		{
			continue;
		}

		if (ApplyComboHitToTarget(TargetActor, StepIndex))
		{
			HitTargets.Add(TargetActor);
		}
	}

	if (bDrawDebugSweep)
	{
		DrawSweepDebug(AvatarActor, Origin, SweepDirection2D, SweepRadius, ConeAngleDegrees, HitTargets);
	}

	return HitTargets.Num();
}

////////////////////////////
//! \author HanUl
//! \brief 부채꼴 판정 범위(중심선, 양측 경계선, 호)와 적중 대상 연결선을 스킬 소유자 화면에 표시한다.
//! \param AvatarActor 공격을 수행한 아바타
//! \param Origin 판정 원점
//! \param SweepDirection 부채꼴 중심 방향
//! \param SweepRadius 부채꼴 반경
//! \param ConeAngleDegrees 부채꼴 전체 각도
//! \param HitTargets 이번 판정에서 적중한 대상 목록
//! \return 없음
void UMyGA_ComboAttack_MeleeSweep::DrawSweepDebug(AActor* AvatarActor, const FVector& Origin, const FVector& SweepDirection, float SweepRadius, float ConeAngleDegrees, const TArray<AActor*>& HitTargets) const
{
	if (!AvatarActor)
	{
		return;
	}

	// 중심선 + 부채꼴(양측 경계선/바깥 호)
	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeLine(Origin, Origin + SweepDirection * SweepRadius, FColor::Yellow, DebugSweepLifeTime, 2.0f));
	MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
		FMySkillDebugShape::MakeCone(Origin, SweepDirection, SweepRadius, ConeAngleDegrees, FColor::Red, DebugSweepLifeTime, 2.0f));

	// 적중 대상 연결선
	for (const AActor* HitTarget : HitTargets)
	{
		if (HitTarget)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeLine(Origin, HitTarget->GetActorLocation(), FColor::Green, DebugSweepLifeTime, 3.0f));
		}
	}
}
