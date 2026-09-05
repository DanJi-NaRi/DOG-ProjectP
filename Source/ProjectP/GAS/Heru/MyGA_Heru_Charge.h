////////////////////////////
//! \file MyGA_Heru_Charge.h
//! \brief Heru의 전방 반원 타격 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Heru_Charge.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Heru_Charge
//! \author HanUl
//! \brief 마우스 조준 방향으로 제자리에서 전방 반원 범위의 적에게 피해와 표식을 부여하는 스킬이다.
//! \note 판정(Targeting.Radius/Angle), 피해 계수(Effects.DamageCoefficient), 표식 GE(Effects.StatusGameplayEffect)는 모두 SkillDefinition에서 온다.
UCLASS()
class PROJECTP_API UMyGA_Heru_Charge : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Heru_Charge();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
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

	//! \brief 처치가 발생한 공격 1회당 감소시킬 쿨타임(초). 처치 수와 무관하게 1회만 적용된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Charge", meta = (ClampMin = "0.0"))
	float KillCooldownReductionSeconds = 2.0f;

	//! \brief 공격 범위 디버그 시각화 여부(임시). 배포 전 제거 예정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Charge|Debug")
	bool bDrawDebugArc = true;

private:
	void ResolveChargeArcTargets(const AActor* AvatarActor, const FMySkillDataEntry& SkillData, TArray<AActor*>& OutTargets) const;
	bool ResolveChargeAimDirection(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData, FVector& OutDirection) const;
	void DrawDebugArc(AActor* AvatarActor, const FMySkillDataEntry& SkillData) const;

	//! \brief CanActivate에서 확정해 반원 타격 판정까지 유지하는 조준 방향(수평 정규화).
	FVector CachedAimDirection = FVector::ZeroVector;
};
