////////////////////////////
//! \file MyGA_ComboAttack_Projectile.h
//! \brief 투사체형 다단 기본 공격 GameplayAbility 선언 파일이다. (Nefer)
#pragma once

#include "CoreMinimal.h"
#include "MyGameplayAbility_ComboAttackBase.h"
#include "MyGA_ComboAttack_Projectile.generated.h"

struct FMySkillAimAssistSpec;
struct FMySkillProjectileSpec;

////////////////////////////
//! \class UMyGA_ComboAttack_Projectile
//! \author HanUl
//! \brief Fire 시점에 SkillDefinition의 Projectile/AimAssist 스펙으로 투사체를 발사하는 콤보 공격이다.
//!        타별 피해 계수와 넉백 거리는 ComboSpec.Steps에서 읽어 투사체에 싣는다.
//!        콤보 흐름(입력 버퍼/체인/리셋)은 전부 ComboAttackBase가 처리한다.
UCLASS()
class PROJECTP_API UMyGA_ComboAttack_Projectile : public UMyGameplayAbility_ComboAttackBase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

protected:
	virtual void OnComboStepFire(int32 StepIndex, const FMySkillDataEntry& SkillData) override;

private:
	FVector ResolveFireDirection(AActor* AvatarActor, const FMySkillDataEntry& SkillData);
	FVector GetAimAssistedDirection(AActor* AvatarActor, const FVector& BaseDirection, const FMySkillAimAssistSpec& AimAssist) const;
	FVector GetProjectileSpawnLocation(AActor* AvatarActor, const FVector& FireDirection, const FMySkillProjectileSpec& Projectile) const;
	bool SpawnComboProjectile(AActor* AvatarActor, int32 StepIndex, const FVector& FireDirection, const FMySkillDataEntry& SkillData) const;

	//! \brief 발동 시점 조준 데이터를 첫 발사에만 사용하기 위한 소비 플래그
	UPROPERTY(Transient)
	bool bTriggerAimConsumed = false;
};
