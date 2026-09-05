////////////////////////////
//! \file MyGA_Inpu_BulwarkFissure.h
//! \brief Inpu의 방벽 균열 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Inpu_BulwarkFissure.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Inpu_BulwarkFissure
//! \author HanUl
//! \brief 방패를 지면에 내리쳐 마우스 방향 전방으로 뻗는 사다리꼴 지면 균열을 전파시키는 스킬이다.
//!        균열은 지정 시간 동안 전방으로 진행하며 경로에 닿은 적을 한 번씩 타격한다.
//! \note 길이(Targeting.Range)/시작 폭(Targeting.Width)/전파 시간(Timing.ActiveDuration)/피해(Effects)는 SkillDefinition에서,
//!       끝 폭·전방 오프셋·판정 높이·서브틱 간격은 UMyBulwarkFissureFragment에서 온다.
//!       판정과 효과는 서버에서만 수행하고, 판정 러너는 어빌리티 수명과 분리되어 몽타주 조기 종료와 무관하게 전체 전파를 완주한다.
UCLASS()
class PROJECTP_API UMyGA_Inpu_BulwarkFissure : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Inpu_BulwarkFissure();

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

	//! \brief true면 서버가 판정한 균열 전파 구간을 소유자 화면에 표시한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Bulwark Fissure|Debug")
	bool bDrawDebugBulwarkFissure = true;

	//! \brief 디버그 도형 표시 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inpu|Bulwark Fissure|Debug", meta = (ClampMin = "0.0"))
	float DebugShapeLifeTime = 0.5f;

private:
	bool ResolveFissureDirection(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData,
		FVector& OutDirection
	) const;

	FVector ResolveGroundOrigin(const FGameplayAbilityActorInfo* ActorInfo) const;

	//! \brief CanActivate에서 확정해 OnStandardSkillShoot이 사용하는 수평 균열 방향.
	FVector CachedFissureDirection = FVector::ZeroVector;
};
