////////////////////////////
//! \file MyGA_Heru_ChargeProjectile.cpp
//! \brief Heru Q 레벨2 반원 관통 투사체 발사 GameplayAbility를 구현한다.
#include "MyGA_Heru_ChargeProjectile.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "../../Player/Projectile/MyPiercingProjectile.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../SkillData/MySkillDefinitionFragment.h"

////////////////////////////
//! \author HanUl
//! \brief 기본값을 초기화한다. 투사체 스킬이라 서버 개시로 동작한다.
//! \param 없음
//! \return 없음
UMyGA_Heru_ChargeProjectile::UMyGA_Heru_ChargeProjectile()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

////////////////////////////
//! \author HanUl
//! \brief 조준 방향을 확정하고 표준 스킬 파이프라인으로 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \return 없음
void UMyGA_Heru_ChargeProjectile::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	PendingFireDirection = FVector::ForwardVector;
	bHasPendingFireDirection = false;
	PendingAvatarActor = nullptr;
	bPendingProjectileFired = false;

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	if (TriggerEventData)
	{
		const float AimYaw = FRotator::NormalizeAxis(TriggerEventData->EventMagnitude);
		PendingFireDirection = FRotator(0.0f, AimYaw, 0.0f).Vector().GetSafeNormal();
		bHasPendingFireDirection = !PendingFireDirection.IsNearlyZero();
	}

	if (!bHasPendingFireDirection)
	{
		PendingFireDirection = GetFireDirection(AvatarActor).GetSafeNormal();
		bHasPendingFireDirection = !PendingFireDirection.IsNearlyZero();
	}

	PendingAvatarActor = AvatarActor;
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief 종료 시 대기 상태를 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Heru_ChargeProjectile::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	PendingAvatarActor = nullptr;
	bHasPendingFireDirection = false;
	bPendingProjectileFired = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 클래스와 필수 발사 데이터를 검증한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGA_Heru_ChargeProjectile::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;

	const FMySkillProjectileSpec& ProjectileSpec = SkillData.Projectile;
	const float ProjectileRange = ProjectileSpec.MaxDistance > 0.0f ? ProjectileSpec.MaxDistance : SkillData.Targeting.Range;

	if (!ProjectileSpec.ProjectileClass || !ProjectileSpec.ProjectileClass->IsChildOf(AMyPiercingProjectile::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru charge projectile activation failed - ProjectileClass must derive from AMyPiercingProjectile. ProjectileClass: %s"),
			*GetNameSafe(ProjectileSpec.ProjectileClass));
		return false;
	}

	if (!SkillData.Effects.HitGameplayEffect
		|| SkillData.Effects.DamageCoefficient <= 0.0f
		|| ProjectileRange <= 0.0f
		|| ProjectileSpec.ProjectileSpeed <= 0.0f
		|| ProjectileSpec.ProjectileRadius <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru charge projectile activation failed - tuning invalid. HitGE: %s, Coefficient: %.2f, Range: %.2f, Speed: %.2f, Radius: %.2f"),
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient,
			ProjectileRange,
			ProjectileSpec.ProjectileSpeed,
			ProjectileSpec.ProjectileRadius);
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 Shoot 시점에 서버 권한에서만 관통 투사체를 Spread Fragment 개수만큼 한 번에 발사한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 방향 이벤트 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_ChargeProjectile::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	if (!ActorInfo || !ActorInfo->IsNetAuthority() || bPendingProjectileFired)
	{
		return;
	}

	// 노티파이와 폴백 타이머가 겹쳐도 한 시전당 아래 발사 루프가 한 번만 돌도록 루프 밖에서 소비한다.
	bPendingProjectileFired = true;

	AActor* AvatarActor = PendingAvatarActor.IsValid() ? PendingAvatarActor.Get() : ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	const FVector FireDirection = bHasPendingFireDirection ? PendingFireDirection.GetSafeNormal() : GetFireDirection(AvatarActor);

	// Spread Fragment가 등록된 Definition만 다발로 나간다(미등록이면 조준 방향 1발 직선을 그대로 유지).
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const UMyProjectileSpreadFragment* SpreadFragment = SkillDefinition ? SkillDefinition->FindFragment<UMyProjectileSpreadFragment>() : nullptr;
	const int32 ProjectileCount = SpreadFragment ? SpreadFragment->GetProjectileCount() : 1;
	const float DamageScale = SpreadFragment ? SpreadFragment->GetPerProjectileDamageScale() : 1.0f;

	int32 SpawnedCount = 0;
	for (int32 ProjectileIndex = 0; ProjectileIndex < ProjectileCount; ++ProjectileIndex)
	{
		// 개수가 1이면 오프셋이 0이라 조준 방향 그대로, 3이면 중앙 기준 좌우 대칭 3갈래로 퍼진다.
		const float YawOffsetDegrees = SpreadFragment ? SpreadFragment->GetYawOffsetDegrees(ProjectileIndex) : 0.0f;
		const FVector SpreadDirection = FMath::IsNearlyZero(YawOffsetDegrees)
			? FireDirection
			: FireDirection.RotateAngleAxis(YawOffsetDegrees, FVector::UpVector);

		if (SpawnChargeProjectile(AvatarActor, SpreadDirection, SkillData, DamageScale))
		{
			++SpawnedCount;
		}
	}

	if (SpawnedCount < ProjectileCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru charge projectile spawn failed - Avatar: %s, Spawned: %d / %d"),
			*GetNameSafe(AvatarActor),
			SpawnedCount,
			ProjectileCount);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Definition 데이터로 관통 투사체를 생성하고 초기화한다.
//! \param AvatarActor Ability Avatar Actor
//! \param FireDirection 발사 방향(수평 정규화). 다발 발사면 갈래마다 다른 방향이 들어온다.
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \param DamageScale 발당 피해 계수 배율. Spread Fragment가 없으면 1.0이다.
//! \return 생성·초기화에 성공하면 true
bool UMyGA_Heru_ChargeProjectile::SpawnChargeProjectile(AActor* AvatarActor, const FVector& FireDirection, const FMySkillDataEntry& SkillData, float DamageScale) const
{
	const FMySkillProjectileSpec& ProjectileSpec = SkillData.Projectile;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!AvatarActor || !World || !SourceASC || !ProjectileSpec.ProjectileClass
		|| !ProjectileSpec.ProjectileClass->IsChildOf(AMyPiercingProjectile::StaticClass()) || !SkillData.Effects.HitGameplayEffect)
	{
		return false;
	}

	const FVector SafeDirection = FireDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMyPiercingProjectile* Projectile = World->SpawnActor<AMyPiercingProjectile>(
		TSubclassOf<AMyPiercingProjectile>(ProjectileSpec.ProjectileClass.Get()),
		GetSpawnLocation(AvatarActor, SafeDirection, SkillData),
		SafeDirection.Rotation(),
		SpawnParams
	);
	if (!Projectile)
	{
		return false;
	}

	const float ProjectileRange = ProjectileSpec.MaxDistance > 0.0f ? ProjectileSpec.MaxDistance : SkillData.Targeting.Range;
	const float ProjectileDamageCoefficient = SkillData.Effects.DamageCoefficient * FMath::Max(DamageScale, 0.0f);
	Projectile->InitializeProjectile(
		AvatarActor,
		SourceASC,
		SkillData.Effects.HitGameplayEffect,
		ProjectileRange,
		ProjectileSpec.ProjectileSpeed,
		ProjectileSpec.ProjectileRadius,
		ProjectileDamageCoefficient,
		SkillData.CooldownTag,
		SkillData.InputTag
	);

	UE_LOG(LogTemp, Log, TEXT("Heru charge projectile spawned - Avatar: %s, Range: %.1f, Speed: %.1f, Radius: %.1f, Coefficient: %.2f, Yaw: %.1f"),
		*GetNameSafe(AvatarActor),
		ProjectileRange,
		ProjectileSpec.ProjectileSpeed,
		ProjectileSpec.ProjectileRadius,
		ProjectileDamageCoefficient,
		SafeDirection.Rotation().Yaw);

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Definition 소켓 또는 오프셋으로 투사체 생성 위치를 계산한다.
//! \param AvatarActor Ability Avatar Actor
//! \param FireDirection 발사 방향
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 투사체 생성 위치
FVector UMyGA_Heru_ChargeProjectile::GetSpawnLocation(AActor* AvatarActor, const FVector& FireDirection, const FMySkillDataEntry& SkillData) const
{
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	const FMySkillProjectileSpec& ProjectileSpec = SkillData.Projectile;
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
//! \brief 컨트롤러 Yaw를 우선으로 발사 방향을 계산한다.
//! \param AvatarActor Ability Avatar Actor
//! \return 발사 방향(수평 정규화)
FVector UMyGA_Heru_ChargeProjectile::GetFireDirection(AActor* AvatarActor) const
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
