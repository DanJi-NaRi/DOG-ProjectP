////////////////////////////
//! \file MyGA_Heru_Thrust.h
//! \brief Heru의 전방 찌르기(다단 히트 + 표식 소비) 스킬 GameplayAbility 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GAS/MyGameplayAbility_SkillBase.h"
#include "MyGA_Heru_Thrust.generated.h"

struct FGameplayEventData;

////////////////////////////
//! \class UMyGA_Heru_Thrust
//! \author HanUl
//! \brief 마우스 방향 직사각형 범위의 적을 1타 시점에 고정하고, 일정 간격으로 다단 피해를 준다.
//!        1타 시점에 대상의 표식 스택을 전량 소비해 스택당 피해를 증폭하고,
//!        3스택을 소비한 대상에게는 마지막 타 이후 추가타를 1회 더 가한다.
//! \note 사거리/폭(Targeting.Range/Width), 틱당 계수(Effects.DamageCoefficient),
//!       추가타 계수(Effects.SecondaryDamageCoefficient), 스택당 증폭(Effects.StatusDamageCoefficient),
//!       히트 간격(Timing.TickInterval), 쿨타임은 모두 SkillDefinition에서 온다.
UCLASS()
class PROJECTP_API UMyGA_Heru_Thrust : public UMyGameplayAbility_SkillBase
{
	GENERATED_BODY()

public:
	UMyGA_Heru_Thrust();

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

	//! \brief 일반 타격 횟수. 스킬 데이터에 대응 필드가 없어 어빌리티에서 관리한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Thrust", meta = (ClampMin = "1"))
	int32 ThrustHitCount = 3;

	//! \brief 추가타가 발동하는 소비 스택 수 기준. 표식 최대 1스택 기획에 맞춰 1로 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Thrust", meta = (ClampMin = "1"))
	int32 BonusHitRequiredStacks = 1;

	//! \brief 소비할 표식 상태 태그. 표식 GE가 부여하는 태그와 일치해야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Thrust")
	FGameplayTag ConsumeStatusTag;

	//! \brief 판정 박스의 수직 반높이(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Thrust", meta = (ClampMin = "0.0"))
	float HitBoxHalfHeight = 100.0f;

	//! \brief 공격 범위 디버그 시각화 여부(임시). 배포 전 제거 예정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heru|Thrust|Debug")
	bool bDrawDebugBox = true;

private:
	//! \brief 조준(마우스) 지점 방향을 확정한다. 없으면 바라보는 방향으로 폴백한다.
	bool ResolveThrustDirection(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData, FVector& OutDirection) const;

	//! \brief 시전자 전방 직사각형(Range x Width) 범위의 적대 대상을 수집한다.
	void CollectThrustTargets(const AActor* AvatarActor, const FVector& Direction, const FMySkillDataEntry& SkillData, TArray<AActor*>& OutTargets) const;

	//! \brief 판정 박스를 스킬 소유자 화면에 디버그로 시각화한다(임시).
	void DrawDebugThrustBox(AActor* AvatarActor, const FVector& Direction, const FMySkillDataEntry& SkillData) const;

	//! \brief CanActivate에서 확정해 Shoot 판정까지 유지하는 찌르기 방향(수평 정규화).
	FVector CachedThrustDirection = FVector::ZeroVector;
};
