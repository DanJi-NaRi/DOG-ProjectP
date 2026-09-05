////////////////////////////
//! \page MyDamageExecutionCalculation.h
//! \brief Re Duat 전투의 최종 피해량 계산(치명타, 방어력 감쇄) ExecutionCalculation 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "MyDamageExecutionCalculation.generated.h"

////////////////////////////
//! \class UMyDamageExecutionCalculation
//! \author HanUl
//! \brief 기본 피해량에 Source의 치명타와 Target의 방어력·받는 피해 배율을 적용해
//!        최종 피해량을 IncomingDamage 메타 어트리뷰트로 출력하는 ExecutionCalculation이다.
//! \note 기본 피해량은 두 경로 중 하나로 결정된다.
//!       1) SetByCaller(Data.Coefficient) 설정 시: 캡처된 Source AttackPower x 계수 (권장, 스킬 계수 전달)
//!       2) SetByCaller(Data.Damage) 설정 시: 완성값 그대로 (체력 비례 등 공격력과 무관한 피해용)
//!       공식: FinalDamage = BaseDamage x CritMultiplier x DefenseMitigation x TargetDamageTakenMultiplier
//! \note AttackPower/CritChance/CritDamage는 Spec 생성 시점 스냅샷, Target Defense/DamageTakenMultiplier는 실행 시점 값이다.
//! \note 치명타 판정은 GE가 실행되는 곳(서버)에서만 굴려지므로 3인 멀티 환경에서 결과가 항상 서버 기준으로 일치한다.
UCLASS()
class PROJECTP_API UMyDamageExecutionCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UMyDamageExecutionCalculation();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput
	) const override;

protected:
	//! \brief 치명타 적용 여부. DoT 틱처럼 치명타가 없어야 하는 GE는 이 값을 끈 BP 서브클래스를 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "ReDuat|Damage")
	bool bAllowCriticalHit = true;

	//! \brief 방어력 감쇄 공식 DefenseConstant / (DefenseConstant + Defense)의 상수 K이다.
	//!        Defense == K일 때 피해가 50% 감소한다.
	UPROPERTY(EditDefaultsOnly, Category = "ReDuat|Damage", meta = (ClampMin = "1.0"))
	float DefenseConstant = 100.0f;
};
