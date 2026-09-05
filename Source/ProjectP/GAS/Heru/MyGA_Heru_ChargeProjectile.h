////////////////////////////
//! \file MyGA_Heru_ChargeProjectile.h
//! \brief Heru Q 레벨2 반원 관통 투사체 발사 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Heru_ChargeProjectile.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Heru_ChargeProjectile
//! \author HanUl
//! \brief 마우스 조준 방향으로 관통 투사체(AMyPiercingProjectile)를 발사하는 Heru Q 레벨2 스킬이다.
//! \note 투사체 클래스/속도/반경/사거리(Projectile), 피해 계수/데미지 GE(Effects)는 SkillDefinition에서 온다.
//!       발사 개수와 부채꼴 각도는 Definition의 UMyProjectileSpreadFragment에서 오며, 미등록이면 조준 방향으로 1발만 나간다.
//!       스폰과 판정은 서버에서만 수행한다.
UCLASS()
class PROJECTP_API UMyGA_Heru_ChargeProjectile : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Heru_ChargeProjectile();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	virtual bool CanActivateStandardSkill(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

	virtual void OnStandardSkillShoot(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		const FMySkillDataEntry& SkillData
	) override;

private:
	bool SpawnChargeProjectile(AActor* AvatarActor, const FVector& FireDirection, const FMySkillDataEntry& SkillData, float DamageScale) const;
	FVector GetSpawnLocation(AActor* AvatarActor, const FVector& FireDirection, const FMySkillDataEntry& SkillData) const;
	FVector GetFireDirection(AActor* AvatarActor) const;

	//! \brief 발사 방향(수평 정규화). ActivateAbility에서 조준 데이터로 확정해 Shoot까지 유지한다.
	FVector PendingFireDirection = FVector::ForwardVector;

	bool bHasPendingFireDirection = false;

	TWeakObjectPtr<AActor> PendingAvatarActor;

	//! \brief 서버에서 투사체를 한 번만 발사하도록 막는 플래그.
	bool bPendingProjectileFired = false;
};
