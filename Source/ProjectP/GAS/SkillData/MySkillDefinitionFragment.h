////////////////////////////
//! \file MySkillDefinitionFragment.h
//! \brief SkillDefinition의 선택적 확장 Fragment 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "MySkillDefinitionFragment.generated.h"

////////////////////////////
//! \class UMySkillDefinitionFragment
//! \author HanUl
//! \brief 특정 스킬만 필요한 추가 규칙 데이터를 SkillDefinition에 인스턴스 형태로 붙이기 위한 기반 클래스다.
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMySkillDefinitionFragment : public UObject
{
	GENERATED_BODY()
};

////////////////////////////
//! \class UMySkillChainFragment
//! \author HanUl
//! \brief 첫 적중 후 주변 대상에게 연쇄되는 투사체형 스킬의 추가 규칙을 정의한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMySkillChainFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	int32 GetMaxAdditionalTargets() const;
	float GetSearchRadius() const;
	bool ShouldHitEachTargetOnce() const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Chain", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 MaxAdditionalTargets = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Chain", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float SearchRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Chain", meta = (AllowPrivateAccess = "true"))
	bool bHitEachTargetOnce = true;
};

////////////////////////////
//! \class UMySkillExistingStatusBonusFragment
//! \author HanUl
//! \brief 대상이 특정 상태 태그를 이미 보유한 경우 즉시 추가 피해를 주는 규칙을 정의한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMySkillExistingStatusBonusFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	const FGameplayTagContainer& GetStatusTags() const;
	float GetBonusDamageCoefficient() const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Status Interaction", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer StatusTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Status Interaction", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float BonusDamageCoefficient = 0.0f;
};

////////////////////////////
//! \class UMyBulwarkFissureFragment
//! \author HanUl
//! \brief Inpu Bulwark Fissure(방벽 균열)의 사다리꼴 형상과 전파 판정 규칙을 정의한다.
//! \note 시작 폭은 Definition Targeting.Width, 길이는 Targeting.Range, 전파 시간은 Timing.ActiveDuration에서 온다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMyBulwarkFissureFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	float GetEndWidth() const;
	float GetStartForwardOffset() const;
	float GetTraceHeight() const;
	float GetSubTickInterval() const;

private:
	//! \brief 균열 끝 지점의 판정 폭(cm). 시작 폭(Targeting.Width)에서 이 값까지 선형으로 넓어진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Fissure", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float EndWidth = 420.0f;

	//! \brief 균열이 시작되는 캐릭터 전방 오프셋(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Fissure", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float StartForwardOffset = 50.0f;

	//! \brief 지면 균열 판정 박스의 절반 높이(cm). 대상 캡슐을 포착하기 위한 수직 여유다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Fissure", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float TraceHeight = 150.0f;

	//! \brief 균열 전파 판정 서브틱 간격(초). 구간 검사라 값이 커도 누락은 없고 타격 시점 정밀도만 낮아진다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Fissure", meta = (ClampMin = "0.01", AllowPrivateAccess = "true"))
	float SubTickInterval = 0.05f;
};

////////////////////////////
//! \enum EMyAegisVortexMoveMode
//! \author HanUl
//! \brief Aegis Vortex 파동이 대상을 어느 방향으로 이동시킬지 정의한다.
UENUM(BlueprintType)
enum class EMyAegisVortexMoveMode : uint8
{
	//! \brief 중심 방향으로 당긴다. 이동량은 중심까지 남은 거리로 클램프되어 중심을 넘지 않는다.
	Pull UMETA(DisplayName = "Pull (중심으로 당김)"),

	//! \brief 중심 바깥 방향으로 밀어낸다. 파동마다 MoveDistance만큼 일정하게 밀린다.
	Push UMETA(DisplayName = "Push (바깥으로 밀어냄)")
};

////////////////////////////
//! \class UMyAegisVortexFragment
//! \author HanUl
//! \brief Inpu Aegis Vortex(이지스 소용돌이)의 파동 횟수·이동 방향/거리·피니셔 반경을 정의한다.
//! \note 같은 어빌리티를 쓰는 스킬들이 MoveMode로 당김(E)과 밀어냄(R)을 각자 선택한다.
//!       파동 반경은 Definition Targeting.Radius, 지속은 Timing.ActiveDuration, 피해 계수는 Effects에서 온다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMyAegisVortexFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	int32 GetPulseCount() const;
	EMyAegisVortexMoveMode GetMoveMode() const;
	float GetMoveDistance() const;
	float GetFinisherRadius() const;
	float GetFinisherDelay() const;

private:
	//! \brief 파동 횟수. 총 지속시간(Timing.ActiveDuration)을 이 횟수로 나눈 간격마다 파동이 발생한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aegis Vortex", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	int32 PulseCount = 3;

	//! \brief 파동이 대상을 당길지 밀어낼지 결정한다(예: E는 Pull, R은 Push).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aegis Vortex", meta = (AllowPrivateAccess = "true"))
	EMyAegisVortexMoveMode MoveMode = EMyAegisVortexMoveMode::Pull;

	//! \brief 파동당 이동 거리(cm). Pull이면 중심까지 남은 거리로 클램프되고, Push면 이 거리만큼 일정하게 밀어낸다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aegis Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float MoveDistance = 90.0f;

	//! \brief 마지막 지면 강타(피니셔)의 원형 판정 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aegis Vortex", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float FinisherRadius = 450.0f;

	//! \brief 마지막 파동과 피니셔 강타 사이의 지연(초). 0이면 마지막 파동과 동시. 몽타주 강타 타이밍에 맞춰 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aegis Vortex", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float FinisherDelay = 0.3f;
};

////////////////////////////
//! \class UMyBulwarkOfJudgementFragment
//! \author HanUl
//! \brief Inpu 궁극기 Bulwark of Judgement의 보호막 비율과 보호막 수 비례 추가 피해 계수를 정의한다.
//! \note 돔/강타 반경은 Definition Targeting.Radius, 돔 유지 시간은 Timing.ActiveDuration, 강타 기본 계수는 Effects.DamageCoefficient에서 온다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMyBulwarkOfJudgementFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	float GetShieldPercentOfMaxHealth() const;
	float GetPerShieldedCoefficient() const;

private:
	//! \brief 보호막량 비율(%). 부여량 = 시전자 MaxHP * (이 값 / 100).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Of Judgement", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float ShieldPercentOfMaxHealth = 15.0f;

	//! \brief 보호막을 받은 캐릭터 1명당 강타 피해 계수에 더해지는 값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Bulwark Of Judgement", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float PerShieldedCoefficient = 2.0f;
};

////////////////////////////
//! \class UMyProjectileExplosionFragment
//! \author HanUl
//! \brief 투사체가 적에 적중했을 때 적중 지점에서 원형 범위로 폭발해 주변 적까지 타격하는 규칙을 정의한다.
//! \note 이 Fragment를 등록한 SkillDefinition의 투사체만 폭발한다. 미등록 스킬은 기존 동작(직격만)을 그대로 유지한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMyProjectileExplosionFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	float GetExplosionRadius() const;
	float GetExplosionDamageCoefficient() const;

private:
	//! \brief 적중 지점 기준 폭발 판정 반경(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Explosion", meta = (ClampMin = "0.0", ForceUnits = "cm", AllowPrivateAccess = "true"))
	float ExplosionRadius = 300.0f;

	//! \brief 폭발 피해 계수(공격력에 곱해짐). 직격 피해 계수와 별개로 적용된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Explosion", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float ExplosionDamageCoefficient = 1.0f;
};

////////////////////////////
//! \enum EMyProjectileSpreadMode
//! \author HanUl
//! \brief 다발 투사체의 확산 각도를 어떤 기준으로 해석할지 정의한다.
UENUM(BlueprintType)
enum class EMyProjectileSpreadMode : uint8
{
	//! \brief 확산 각도를 인접한 두 투사체 사이의 간격으로 사용한다. 개수가 늘어도 간격은 유지되고 전체 폭만 넓어진다.
	AngleStep UMETA(DisplayName = "Angle Step (인접 간격)"),

	//! \brief 확산 각도를 양 끝 투사체 사이의 전체 부채꼴 폭으로 사용한다. 개수가 늘면 간격이 좁아진다.
	TotalAngle UMETA(DisplayName = "Total Angle (전체 폭)")
};

////////////////////////////
//! \class UMyProjectileSpreadFragment
//! \author HanUl
//! \brief 투사체형 스킬이 한 번의 발사에서 여러 발을 좌우 대칭 부채꼴로 퍼뜨려 쏘는 규칙을 정의한다.
//! \note 이 Fragment를 등록한 SkillDefinition의 투사체만 다발로 나간다. 미등록 스킬은 기존 동작(조준 방향 1발)을 그대로 유지한다.
//!       개수가 1이면 확산 각도와 무관하게 조준 방향 직선 1발이다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class PROJECTP_API UMyProjectileSpreadFragment : public UMySkillDefinitionFragment
{
	GENERATED_BODY()

public:
	int32 GetProjectileCount() const;
	EMyProjectileSpreadMode GetSpreadMode() const;
	float GetSpreadAngleDegrees() const;
	float GetPerProjectileDamageScale() const;
	float GetYawOffsetDegrees(int32 ProjectileIndex) const;

private:
	//! \brief 한 번의 발사에서 나가는 투사체 개수. 1이면 직선 1발, 3이면 3갈래로 나간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Spread", meta = (ClampMin = "1", ClampMax = "9", AllowPrivateAccess = "true"))
	int32 ProjectileCount = 1;

	//! \brief 확산 각도를 인접 간격으로 볼지 전체 폭으로 볼지 결정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Spread", meta = (AllowPrivateAccess = "true"))
	EMyProjectileSpreadMode SpreadMode = EMyProjectileSpreadMode::AngleStep;

	//! \brief 확산 각도(도). 모드에 따라 인접 투사체 간격 또는 양 끝 투사체 사이 전체 폭으로 해석된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Spread", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg", AllowPrivateAccess = "true"))
	float SpreadAngleDegrees = 15.0f;

	//! \brief 투사체 1발당 피해 계수 배율. 1.0이면 Effects.DamageCoefficient를 발마다 그대로 사용하므로 총 피해가 개수배가 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile Spread", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float PerProjectileDamageScale = 1.0f;
};
