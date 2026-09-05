////////////////////////////
//! \file MyGA_ComboAttack_MeleeSweep.h
//! \brief 근접 부채꼴 다단 기본 공격 GameplayAbility 선언 파일이다. (Heru, Inpu)
#pragma once

#include "CoreMinimal.h"
#include "MyGameplayAbility_ComboAttackBase.h"
#include "MyGA_ComboAttack_MeleeSweep.generated.h"

////////////////////////////
//! \class UMyGA_ComboAttack_MeleeSweep
//! \author HanUl
//! \brief Fire 시점에 전방 부채꼴(Targeting.Radius/Angle) 안의 모든 적대 대상을 히트스캔으로 타격하는 콤보 공격이다.
//!        타별 피해 계수·넉백·상태효과는 ComboSpec.Steps에서 읽어 ApplyComboHitToTarget으로 적용한다.
//!        Heru와 Inpu는 이 클래스 하나를 공유하고 SkillDefinition DataAsset만 다르게 쓴다.
UCLASS()
class PROJECTP_API UMyGA_ComboAttack_MeleeSweep : public UMyGameplayAbility_ComboAttackBase
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

	//! \brief true면 서버 판정 시 부채꼴 범위와 적중 대상을 디버그 라인으로 표시한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|Debug")
	bool bDrawDebugSweep = false;

	//! \brief 디버그 라인 표시 유지 시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|Debug", meta = (ClampMin = "0.0"))
	float DebugSweepLifeTime = 0.6f;

private:
	FVector ResolveSweepDirection(AActor* AvatarActor);
	int32 ExecuteMeleeSweep(AActor* AvatarActor, int32 StepIndex, const FVector& SweepDirection, const FMySkillDataEntry& SkillData) const;
	void DrawSweepDebug(AActor* AvatarActor, const FVector& Origin, const FVector& SweepDirection, float SweepRadius, float ConeAngleDegrees, const TArray<AActor*>& HitTargets) const;

	//! \brief 발동 시점 조준 데이터를 첫 판정에만 사용하기 위한 소비 플래그
	UPROPERTY(Transient)
	bool bTriggerAimConsumed = false;
};
