////////////////////////////
//! \page MyDamageExecutionCalculation.cpp
//! \brief Re Duat 전투의 최종 피해량 계산 ExecutionCalculation을 구현한다.
#include "MyDamageExecutionCalculation.h"

#include "MyAttributeSet.h"
#include "../MyGameplayTags.h"

namespace
{
	//! \brief 캡처할 어트리뷰트 정의를 한 번만 생성해 보관하는 정적 구조체이다.
	struct FMyDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DamageTakenMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamage);
		DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);

		FMyDamageStatics()
		{
			// Target 방어력은 GE 실행 시점 값을 사용한다(Snapshot = false).
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMyAttributeSet, Defense, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMyAttributeSet, DamageTakenMultiplier, Target, false);
			// Source 치명타 확률/배율은 Spec 생성 시점 값을 스냅샷한다(투사체가 날아가는 동안 버프가 바뀌어도 발사 시점 기준).
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMyAttributeSet, CritChance, Source, true);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMyAttributeSet, CritDamage, Source, true);
			// Source 공격력도 Spec 생성 시점 값을 스냅샷한다. Data.Coefficient 경로에서 BaseDamage 계산에 사용된다.
			DEFINE_ATTRIBUTE_CAPTUREDEF(UMyAttributeSet, AttackPower, Source, true);
		}
	};

	const FMyDamageStatics& DamageStatics()
	{
		static FMyDamageStatics Statics;
		return Statics;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 계산에 필요한 어트리뷰트 캡처 목록을 등록한다.
//! \param 없음
//! \return 없음
UMyDamageExecutionCalculation::UMyDamageExecutionCalculation()
{
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageTakenMultiplierDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
}

////////////////////////////
//! \author HanUl
//! \brief 기본 피해량에 치명타와 방어력 감쇄를 적용해 IncomingDamage로 출력한다.
//!        기본 피해량은 SetByCaller(Data.Coefficient)가 있으면 캡처된 Source AttackPower x 계수로 계산하고,
//!        없으면 SetByCaller(Data.Damage)의 완성값을 그대로 사용한다(체력 비례 등 공격력 무관 피해용).
//! \param ExecutionParams GE Spec, 캡처된 어트리뷰트, Source/Target 태그를 담은 실행 파라미터
//! \param OutExecutionOutput 최종 피해량 Modifier를 담아 반환할 실행 결과
//! \return 없음
void UMyDamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput
) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 계수 경로: BaseDamage = 캡처된 Source AttackPower x Data.Coefficient
	// 완성값 경로: BaseDamage = Data.Damage (공격력과 무관한 피해를 위해 유지)
	float BaseDamage = 0.0f;
	const float Coefficient = Spec.GetSetByCallerMagnitude(MyGameplayTags::Data_Coefficient, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.0f);
	if (Coefficient > 0.0f)
	{
		float AttackPower = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvaluateParameters, AttackPower);
		BaseDamage = FMath::Max(AttackPower, 0.0f) * Coefficient;
	}
	else
	{
		BaseDamage = FMath::Max(
			Spec.GetSetByCallerMagnitude(MyGameplayTags::Data_Damage, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.0f),
			0.0f
		);
	}

	if (BaseDamage <= 0.0f)
	{
		return;
	}

	float Damage = BaseDamage;
	bool bCriticalHit = false;
	float CritDamageMultiplier = 1.0f;

	if (bAllowCriticalHit)
	{
		float CritChance = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, EvaluateParameters, CritChance);
		CritChance = FMath::Clamp(CritChance, 0.0f, 1.0f);

		if (CritChance > 0.0f && FMath::FRand() < CritChance)
		{
			// 크리티컬 배율은 Source의 CritDamage 어트리뷰트(캐릭터별 CurveTable 주도)에서 가져온다.
			ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritDamageDef, EvaluateParameters, CritDamageMultiplier);
			CritDamageMultiplier = FMath::Max(CritDamageMultiplier, 1.0f);

			bCriticalHit = true;
			Damage *= CritDamageMultiplier;
		}
	}

	float Defense = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefenseDef, EvaluateParameters, Defense);
	Defense = FMath::Max(Defense, 0.0f);

	const float SafeDefenseConstant = FMath::Max(DefenseConstant, 1.0f);
	const float DefenseMitigation = SafeDefenseConstant / (SafeDefenseConstant + Defense);
	float DamageTakenMultiplier = 1.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		DamageStatics().DamageTakenMultiplierDef,
		EvaluateParameters,
		DamageTakenMultiplier
	);
	DamageTakenMultiplier = FMath::Max(DamageTakenMultiplier, 0.0f);

	const float FinalDamage = Damage * DefenseMitigation * DamageTakenMultiplier;

	UE_LOG(LogTemp, Log, TEXT("MyGAS Damage Calc - Base: %.2f, Crit: %s(x%.2f), Defense: %.2f, Mitigation: %.2f, TakenMultiplier: %.2f, Final: %.2f"),
		BaseDamage,
		bCriticalHit ? TEXT("true") : TEXT("false"),
		CritDamageMultiplier,
		Defense,
		DefenseMitigation,
		DamageTakenMultiplier,
		FinalDamage);

	if (FinalDamage > 0.0f)
	{
		//! \author 장효제
		//! \brief AttributeSet이 치명타 여부를 Payload에 반영하도록 메타 Attribute로 전달한다.
		if (bCriticalHit)
		{
			OutExecutionOutput.AddOutputModifier(
				FGameplayModifierEvaluatedData(
					UMyAttributeSet::GetIncomingCriticalHitAttribute(),
					EGameplayModOp::Additive,
					1.0f
				)
			);
		}

		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				UMyAttributeSet::GetIncomingDamageAttribute(),
				EGameplayModOp::Additive,
				FinalDamage
			)
		);
	}

	const float CurseGaugeAmount = FMath::Max(
		Spec.GetSetByCallerMagnitude(MyGameplayTags::Data_CurseGauge, /*WarnIfNotFound=*/false, /*DefaultIfNotFound=*/0.0f),
		0.0f
	);
	if (CurseGaugeAmount > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				UMyAttributeSet::GetIncomingCurseGaugeAttribute(),
				EGameplayModOp::Additive,
				CurseGaugeAmount
			)
		);
	}
}
