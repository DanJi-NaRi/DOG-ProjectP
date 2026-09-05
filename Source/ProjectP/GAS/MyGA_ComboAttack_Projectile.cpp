////////////////////////////
//! \file MyGA_ComboAttack_Projectile.cpp
//! \brief 투사체형 다단 기본 공격 GameplayAbility 구현 파일이다. (Nefer)

#include "MyGA_ComboAttack_Projectile.h"

#include "../Player/Nefer/MyNeferProjectile.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "MyAbilitySystemLibrary.h"
#include "SkillData/MySkillDefinitionDataAsset.h"
#include "SkillData/MySkillDefinitionFragment.h"
#include "SkillData/MySkillSetDataAsset.h"

////////////////////////////
//! \author HanUl
//! \brief 발동 시점 조준 소비 플래그를 초기화하고 콤보 체인을 시작한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_ComboAttack_Projectile::ActivateAbility(
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
//! \brief Fire 시점에 서버에서 현재 조준 방향으로 투사체를 발사한다.
//! \param StepIndex 현재 콤보 스텝 인덱스(0부터)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_ComboAttack_Projectile::OnComboStepFire(int32 StepIndex, const FMySkillDataEntry& SkillData)
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

	const FVector FireDirection = ResolveFireDirection(AvatarActor, SkillData);
	if (!SpawnComboProjectile(AvatarActor, StepIndex, FireDirection, SkillData))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS projectile combo attack spawn failed - Ability: %s, Avatar: %s, StepIndex: %d"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor),
			StepIndex);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 발사 방향을 결정한다. 첫 발사는 발동 시점 조준(EventMagnitude Yaw)을 사용하고,
//!        체인된 타는 현재 컨트롤러 조준 방향을 사용한 뒤 조준 보정을 적용한다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 정규화된 발사 방향
FVector UMyGA_ComboAttack_Projectile::ResolveFireDirection(AActor* AvatarActor, const FMySkillDataEntry& SkillData)
{
	FVector BaseDirection = FVector::ZeroVector;

	const FGameplayEventData* TriggerData = GetComboTriggerEventData();
	if (!bTriggerAimConsumed && TriggerData)
	{
		bTriggerAimConsumed = true;
		const float AimYaw = FRotator::NormalizeAxis(TriggerData->EventMagnitude);
		BaseDirection = FRotator(0.0f, AimYaw, 0.0f).Vector().GetSafeNormal();
	}

	if (BaseDirection.IsNearlyZero())
	{
		BaseDirection = AvatarActor->GetActorForwardVector().GetSafeNormal();
		if (const APawn* AvatarPawn = Cast<APawn>(AvatarActor))
		{
			if (const AController* AvatarController = AvatarPawn->GetController())
			{
				const FRotator ControlYawRotation(0.0f, AvatarController->GetControlRotation().Yaw, 0.0f);
				BaseDirection = ControlYawRotation.Vector().GetSafeNormal();
			}
		}
	}

	return GetAimAssistedDirection(AvatarActor, BaseDirection, SkillData.AimAssist);
}

////////////////////////////
//! \author HanUl
//! \brief 탐색 각도 내 가장 가까운 적대 대상 방향으로 발사 방향을 보정한다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \param BaseDirection 보정 전 발사 방향
//! \param AimAssist SkillDefinition의 조준 보정 설정
//! \return 보정된 발사 방향
FVector UMyGA_ComboAttack_Projectile::GetAimAssistedDirection(AActor* AvatarActor, const FVector& BaseDirection, const FMySkillAimAssistSpec& AimAssist) const
{
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World || AimAssist.Range <= 0.0f || AimAssist.SearchAngle <= 0.0f)
	{
		return BaseDirection;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComboAttackProjectileAimAssist), false, AvatarActor);
	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		AvatarActor->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(AimAssist.Range),
		QueryParams
	);

	if (!bHasOverlap)
	{
		return BaseDirection;
	}

	AActor* BestTarget = nullptr;
	float BestAngle = AimAssist.SearchAngle;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* Candidate = OverlapResult.GetActor();
		if (!Candidate || Candidate == AvatarActor || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, Candidate))
		{
			continue;
		}

		const FVector ToCandidate = (Candidate->GetActorLocation() - AvatarActor->GetActorLocation()).GetSafeNormal();
		const float Dot = FMath::Clamp(FVector::DotProduct(BaseDirection, ToCandidate), -1.0f, 1.0f);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if (Angle <= BestAngle)
		{
			BestAngle = Angle;
			BestTarget = Candidate;
		}
	}

	if (!BestTarget)
	{
		return BaseDirection;
	}

	const FVector TargetDirection = (BestTarget->GetActorLocation() - AvatarActor->GetActorLocation()).GetSafeNormal();
	if (BestAngle <= AimAssist.MaxCorrectionAngle)
	{
		return TargetDirection;
	}

	const float Alpha = AimAssist.MaxCorrectionAngle / BestAngle;
	return FQuat::Slerp(BaseDirection.ToOrientationQuat(), TargetDirection.ToOrientationQuat(), Alpha)
		.GetForwardVector()
		.GetSafeNormal();
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 생성 위치를 구한다. 스폰 소켓이 있으면 소켓 위치, 없으면 오프셋 위치를 사용한다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \param FireDirection 발사 방향
//! \param Projectile SkillDefinition의 투사체 설정
//! \return 투사체 생성 위치
FVector UMyGA_ComboAttack_Projectile::GetProjectileSpawnLocation(AActor* AvatarActor, const FVector& FireDirection, const FMySkillProjectileSpec& Projectile) const
{
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor))
	{
		if (const USkeletalMeshComponent* MeshComponent = AvatarCharacter->GetMesh())
		{
			if (!Projectile.SpawnSocketName.IsNone() && MeshComponent->DoesSocketExist(Projectile.SpawnSocketName))
			{
				return MeshComponent->GetSocketLocation(Projectile.SpawnSocketName);
			}
		}
	}

	return AvatarActor->GetActorLocation()
		+ FireDirection.GetSafeNormal() * Projectile.SpawnForwardOffset
		+ FVector::UpVector * Projectile.SpawnUpOffset;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 스텝의 피해 계수와 넉백 거리를 실은 투사체를 서버에서 생성한다.
//! \param AvatarActor 공격을 수행하는 아바타
//! \param StepIndex 현재 콤보 스텝 인덱스(0부터)
//! \param FireDirection 발사 방향
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 투사체 생성에 성공하면 true
bool UMyGA_ComboAttack_Projectile::SpawnComboProjectile(AActor* AvatarActor, int32 StepIndex, const FVector& FireDirection, const FMySkillDataEntry& SkillData) const
{
	const FMySkillComboStepSpec* Step = GetComboStepSpec(StepIndex);
	const FMySkillProjectileSpec& Projectile = SkillData.Projectile;
	const bool bValidProjectileClass = Projectile.ProjectileClass
		&& Projectile.ProjectileClass->IsChildOf(AMyNeferProjectile::StaticClass());
	if (!AvatarActor || !Step || !bValidProjectileClass || !SkillData.Effects.HitGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS projectile combo attack spawn rejected - missing data. Ability: %s, SkillId: %s, ProjectileClass: %s, HitGE: %s, StepIndex: %d"),
			*GetNameSafe(this),
			*SkillData.SkillId.ToString(),
			*GetNameSafe(Projectile.ProjectileClass),
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			StepIndex);
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = AvatarActor->GetWorld();
	if (!SourceASC || !World)
	{
		return false;
	}

	const FVector SpawnLocation = GetProjectileSpawnLocation(AvatarActor, FireDirection, Projectile);
	const FRotator SpawnRotation = FireDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyNeferProjectile* SpawnedProjectile = World->SpawnActor<AMyNeferProjectile>(
		Projectile.ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (!SpawnedProjectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS projectile combo attack spawn failed - ProjectileClass is not AMyNeferProjectile or spawn rejected. Ability: %s, ProjectileClass: %s"),
			*GetNameSafe(this),
			*GetNameSafe(Projectile.ProjectileClass));
		return false;
	}

	const float ProjectileRange = Projectile.MaxDistance > 0.0f ? Projectile.MaxDistance : SkillData.Targeting.Range;
	SpawnedProjectile->InitializeProjectile(
		AvatarActor,
		SourceASC,
		SkillData.Effects.HitGameplayEffect,
		ProjectileRange,
		Projectile.ProjectileSpeed,
		Projectile.ProjectileRadius,
		Step->DamageCoefficient,
		nullptr,
		0.0f,
		Step->KnockbackDistance
	);
	SpawnedProjectile->ConfigureAttackerHitCameraFeedback(SkillData.InputTag);

	// 폭발 Fragment가 등록된 Definition만 적중 시 원형 범위로 폭발한다(미등록이면 기존 직격 동작 유지).
	if (const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition())
	{
		if (const UMyProjectileExplosionFragment* ExplosionFragment = SkillDefinition->FindFragment<UMyProjectileExplosionFragment>())
		{
			SpawnedProjectile->ConfigureExplosion(
				ExplosionFragment->GetExplosionRadius(),
				ExplosionFragment->GetExplosionDamageCoefficient()
			);
		}
	}

	return true;
}
