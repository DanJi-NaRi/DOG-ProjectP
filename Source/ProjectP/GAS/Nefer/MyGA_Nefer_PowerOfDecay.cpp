// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGA_Nefer_PowerOfDecay.h"

#include "../../Player/Nefer/MyNeferProjectile.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../SkillData/MySkillDefinitionFragment.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

////////////////////////////
//! \author HanUl
//! \brief Definition 기반 Power Of Decay Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGA_Nefer_PowerOfDecay::UMyGA_Nefer_PowerOfDecay()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition 데이터를 검증하고 몽타주 또는 CastTime 이후 투사체를 발사한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \return 없음
void UMyGA_Nefer_PowerOfDecay::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ActiveSkillDefinition = nullptr;
	PendingAvatarActor = nullptr;
	PendingAimYaw = 0.0f;
	PendingFireDirection = FVector::ForwardVector;
	bHasPendingFireDirection = false;
	bPendingProjectileFired = false;

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	const bool bHasTriggerEventData = TriggerEventData != nullptr;
	if (bHasTriggerEventData)
	{
		PendingAimYaw = FRotator::NormalizeAxis(TriggerEventData->EventMagnitude);
		PendingFireDirection = FRotator(0.0f, PendingAimYaw, 0.0f).Vector().GetSafeNormal();
		bHasPendingFireDirection = !PendingFireDirection.IsNearlyZero();
	}

	if (!bHasPendingFireDirection)
	{
		PendingFireDirection = GetFireDirection(AvatarActor).GetSafeNormal();
		PendingAimYaw = FRotator::NormalizeAxis(PendingFireDirection.Rotation().Yaw);
		bHasPendingFireDirection = !PendingFireDirection.IsNearlyZero();
	}

	PendingAvatarActor = AvatarActor;
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 입력 차단과 몽타주 Notify 바인딩을 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Nefer_PowerOfDecay::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ActiveSkillDefinition = nullptr;
	PendingAvatarActor = nullptr;
	PendingAimYaw = 0.0f;
	PendingFireDirection = FVector::ForwardVector;
	bHasPendingFireDirection = false;
	bPendingProjectileFired = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief Power Of Decay에 필요한 Definition 데이터를 검증한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 실행 가능한 데이터이면 true
bool UMyGA_Nefer_PowerOfDecay::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;
	(void)SkillData;

	AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	return CacheAndValidateSkillDefinition(ActorInfo, AvatarActor);
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot 시점에 Power Of Decay 투사체를 발사한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Nefer_PowerOfDecay::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;
	FirePendingProjectile();
}

////////////////////////////
//! \author HanUl
//! \brief SourceObject의 SkillDefinition이 Power Of Decay 실행에 필요한 데이터를 갖췄는지 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param AvatarActor Ability Avatar Actor
//! \return 실행 가능한 SkillDefinition이면 true
bool UMyGA_Nefer_PowerOfDecay::CacheAndValidateSkillDefinition(const FGameplayAbilityActorInfo* ActorInfo, AActor* AvatarActor)
{
	ActiveSkillDefinition = GetSkillDefinitionDataAssetFromActorInfo(ActorInfo);
	if (!ActiveSkillDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay activation failed - SkillDefinition is missing. Avatar: %s"),
			*GetNameSafe(AvatarActor));
		return false;
	}

	const FMySkillProjectileSpec& ProjectileSpec = ActiveSkillDefinition->GetProjectile();
	const FMySkillTargetingSpec& TargetingSpec = ActiveSkillDefinition->GetTargeting();
	const FMySkillEffectSpec& EffectSpec = ActiveSkillDefinition->GetEffects();
	const float ProjectileRange = ProjectileSpec.MaxDistance > 0.0f ? ProjectileSpec.MaxDistance : TargetingSpec.Range;

	if (!ProjectileSpec.ProjectileClass || !ProjectileSpec.ProjectileClass->IsChildOf(AMyNeferProjectile::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay activation failed - projectile class is invalid. Avatar: %s, Definition: %s, ProjectileClass: %s"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(ActiveSkillDefinition),
			*GetNameSafe(ProjectileSpec.ProjectileClass));
		return false;
	}

	if (!EffectSpec.HitGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay activation failed - hit GameplayEffect is null. Avatar: %s, Definition: %s"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(ActiveSkillDefinition));
		return false;
	}

	if (ProjectileRange <= 0.0f || ProjectileSpec.ProjectileSpeed <= 0.0f || ProjectileSpec.ProjectileRadius <= 0.0f || EffectSpec.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay activation failed - projectile tuning is invalid. Avatar: %s, Definition: %s, Range: %.2f, Speed: %.2f, Radius: %.2f, DamageCoefficient: %.2f"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(ActiveSkillDefinition),
			ProjectileRange,
			ProjectileSpec.ProjectileSpeed,
			ProjectileSpec.ProjectileRadius,
			EffectSpec.DamageCoefficient);
		return false;
	}

	if (!EffectSpec.StatusGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay activation warning - status GameplayEffect is null, decay DoT will not be applied. Avatar: %s, Definition: %s"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(ActiveSkillDefinition));
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 서버 권한에서 대기 중인 투사체를 한 번만 발사한다.
//! \param 없음
//! \return 없음
void UMyGA_Nefer_PowerOfDecay::FirePendingProjectile()
{
	if (bPendingProjectileFired)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	bPendingProjectileFired = true;

	AActor* AvatarActor = PendingAvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	const FVector FireDirection = bHasPendingFireDirection ? PendingFireDirection.GetSafeNormal() : FVector::ForwardVector;
	if (!SpawnPowerOfDecayProjectile(AvatarActor, FireDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nefer power of decay projectile spawn failed - Avatar: %s, Definition: %s"),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(GetActiveSkillDefinition()));
	}
}

////////////////////////////
//! \author HanUl
//! \brief Definition의 소켓 또는 오프셋 값을 사용해 투사체 생성 위치를 계산한다.
//! \param AvatarActor Ability Avatar Actor
//! \param FireDirection 발사 방향
//! \return 투사체 생성 위치
FVector UMyGA_Nefer_PowerOfDecay::GetSpawnLocation(AActor* AvatarActor, const FVector& FireDirection) const
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	if (!AvatarActor || !SkillDefinition)
	{
		return FVector::ZeroVector;
	}

	const FMySkillProjectileSpec& ProjectileSpec = SkillDefinition->GetProjectile();
	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor))
	{
		if (const USkeletalMeshComponent* MeshComponent = AvatarCharacter->GetMesh())
		{
			if (!ProjectileSpec.SpawnSocketName.IsNone() && MeshComponent->DoesSocketExist(ProjectileSpec.SpawnSocketName))
			{
				return MeshComponent->GetSocketLocation(ProjectileSpec.SpawnSocketName);
			}
		}
	}

	return AvatarActor->GetActorLocation()
		+ FireDirection.GetSafeNormal() * ProjectileSpec.SpawnForwardOffset
		+ FVector::UpVector * ProjectileSpec.SpawnUpOffset;
}

////////////////////////////
//! \author HanUl
//! \brief 컨트롤러 방향을 우선으로 발사 방향을 계산한다.
//! \param AvatarActor Ability Avatar Actor
//! \return 발사 방향
FVector UMyGA_Nefer_PowerOfDecay::GetFireDirection(AActor* AvatarActor) const
{
	if (!AvatarActor)
	{
		return FVector::ForwardVector;
	}

	if (const APawn* AvatarPawn = Cast<APawn>(AvatarActor))
	{
		if (const AController* AvatarController = AvatarPawn->GetController())
		{
			const FRotator ControlYawRotation(0.0f, AvatarController->GetControlRotation().Yaw, 0.0f);
			return ControlYawRotation.Vector().GetSafeNormal();
		}
	}

	return AvatarActor->GetActorForwardVector().GetSafeNormal();
}

////////////////////////////
//! \author HanUl
//! \brief Definition과 Fragment 데이터를 적용한 Power Of Decay 투사체를 생성한다.
//! \param AvatarActor Ability Avatar Actor
//! \param FireDirection 발사 방향
//! \return 투사체 생성과 초기화에 성공하면 true
bool UMyGA_Nefer_PowerOfDecay::SpawnPowerOfDecayProjectile(AActor* AvatarActor, const FVector& FireDirection) const
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	if (!AvatarActor || !SkillDefinition)
	{
		return false;
	}

	const FMySkillProjectileSpec& ProjectileSpec = SkillDefinition->GetProjectile();
	const FMySkillTargetingSpec& TargetingSpec = SkillDefinition->GetTargeting();
	const FMySkillEffectSpec& EffectSpec = SkillDefinition->GetEffects();
	if (!ProjectileSpec.ProjectileClass || !ProjectileSpec.ProjectileClass->IsChildOf(AMyNeferProjectile::StaticClass()) || !EffectSpec.HitGameplayEffect)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = AvatarActor->GetWorld();
	if (!SourceASC || !World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyNeferProjectile* Projectile = World->SpawnActor<AMyNeferProjectile>(
		TSubclassOf<AMyNeferProjectile>(ProjectileSpec.ProjectileClass.Get()),
		GetSpawnLocation(AvatarActor, FireDirection),
		FireDirection.Rotation(),
		SpawnParams
	);

	if (!Projectile)
	{
		return false;
	}

	const float ProjectileRange = ProjectileSpec.MaxDistance > 0.0f ? ProjectileSpec.MaxDistance : TargetingSpec.Range;
	Projectile->InitializeProjectile(
		AvatarActor,
		SourceASC,
		EffectSpec.HitGameplayEffect,
		ProjectileRange,
		ProjectileSpec.ProjectileSpeed,
		ProjectileSpec.ProjectileRadius,
		EffectSpec.DamageCoefficient,
		EffectSpec.StatusGameplayEffect,
		EffectSpec.StatusDamageCoefficient
	);
	Projectile->ConfigureAttackerHitCameraFeedback(SkillDefinition->GetInputTag());

	// 폭발 Fragment가 등록된 Definition만 적중 시 원형 범위로 폭발한다(미등록이면 기존 직격 동작 유지).
	if (const UMyProjectileExplosionFragment* ExplosionFragment = SkillDefinition->FindFragment<UMyProjectileExplosionFragment>())
	{
		Projectile->ConfigureExplosion(
			ExplosionFragment->GetExplosionRadius(),
			ExplosionFragment->GetExplosionDamageCoefficient()
		);
	}

	if (const UMySkillChainFragment* ChainFragment = SkillDefinition->FindFragment<UMySkillChainFragment>())
	{
		Projectile->ConfigureChain(
			ChainFragment->GetMaxAdditionalTargets(),
			ChainFragment->GetSearchRadius(),
			ChainFragment->ShouldHitEachTargetOnce()
		);
	}

	if (const UMySkillExistingStatusBonusFragment* ExistingStatusBonusFragment = SkillDefinition->FindFragment<UMySkillExistingStatusBonusFragment>())
	{
		Projectile->ConfigureExistingStatusBonus(
			ExistingStatusBonusFragment->GetStatusTags(),
			ExistingStatusBonusFragment->GetBonusDamageCoefficient()
		);
	}

	UE_LOG(LogTemp, Log, TEXT("Nefer power of decay projectile spawned - Avatar: %s, Definition: %s, Range: %.2f, Speed: %.2f, Radius: %.2f, DamageCoefficient: %.2f, StatusCoefficient: %.2f"),
		*GetNameSafe(AvatarActor),
		*GetNameSafe(SkillDefinition),
		ProjectileRange,
		ProjectileSpec.ProjectileSpeed,
		ProjectileSpec.ProjectileRadius,
		EffectSpec.DamageCoefficient,
		EffectSpec.StatusDamageCoefficient);

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 활성화에 사용 중인 SkillDefinition을 반환한다.
//! \param 없음
//! \return 활성 SkillDefinition, 없으면 nullptr
const UMySkillDefinitionDataAsset* UMyGA_Nefer_PowerOfDecay::GetActiveSkillDefinition() const
{
	return ActiveSkillDefinition;
}
