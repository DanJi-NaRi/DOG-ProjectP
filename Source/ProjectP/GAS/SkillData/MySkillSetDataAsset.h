////////////////////////////
//! \page MySkillSetDataAsset.h
//! \brief MyGAS 플레이어 스킬 파라미터 DataAsset 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "../../Indicator/MySkillIndicatorTypes.h"
#include "MySkillSetDataAsset.generated.h"

class AMySkillIndicatorActorBase;
class UGameplayEffect;
class UMaterialInterface;
class UTexture2D;

////////////////////////////
//! \enum EMySkillTargetingType
//! \brief 스킬이 사용하는 타겟팅 형태를 정의한다.
UENUM(BlueprintType)
enum class EMySkillTargetingType : uint8
{
	None UMETA(DisplayName = "None"),
	Direction UMETA(DisplayName = "Direction"),
	GroundTarget UMETA(DisplayName = "Ground Target"),
	SelfArea UMETA(DisplayName = "Self Area"),
	Projectile UMETA(DisplayName = "Projectile"),
	Area UMETA(DisplayName = "Area")
};

////////////////////////////
//! \enum EMySkillAimSource
//! \brief 스킬 입력 시점의 조준 컨텍스트를 어떤 기준으로 만들지 정의한다.
UENUM(BlueprintType)
enum class EMySkillAimSource : uint8
{
	None UMETA(DisplayName = "None"),
	MouseCursor UMETA(DisplayName = "Mouse Cursor"),
	ControllerForward UMETA(DisplayName = "Controller Forward"),
	CurrentFacing UMETA(DisplayName = "Current Facing")
};

////////////////////////////
//! \enum EMyDashDirectionPolicy
//! \brief 이동 입력이 없는 대쉬 입력을 어떻게 처리할지 정의한다.
UENUM(BlueprintType)
enum class EMyDashDirectionPolicy : uint8
{
	RequireMoveInput UMETA(DisplayName = "Require Move Input"),
	UseFacingWhenMoveInputMissing UMETA(DisplayName = "Use Facing When Move Input Missing")
};

////////////////////////////
//! \struct FMySkillInputSpec
//! \brief 스킬 입력에서 GA로 전달할 조준 컨텍스트 정책을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillInputSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Input")
	EMySkillAimSource AimSource = EMySkillAimSource::None;
};

////////////////////////////
//! \struct FMySkillTimingSpec
//! \brief 스킬 실행 타이밍 값을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillTimingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Timing", meta = (ClampMin = "0.0"))
	float CastTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Timing", meta = (ClampMin = "0.0", DisplayName = "Impact Delay", ToolTip = "Delay between the Shoot notify and the actual gameplay effect impact. VFX starts at Shoot."))
	float ImpactDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Timing", meta = (ClampMin = "0.0"))
	float ActiveDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Timing", meta = (ClampMin = "0.0"))
	float TickInterval = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Timing", meta = (ClampMin = "0.0"))
	float PostDelay = 0.0f;
};

////////////////////////////
//! \struct FMySkillComboStepSpec
//! \brief 콤보형 스킬의 한 타(Step) 실행 데이터를 정의한다.
//!        타 간 간격은 별도 수치 없이 몽타주 섹션 길이와 EndAttack Notify 위치가 결정한다.
USTRUCT(BlueprintType)
struct FMySkillComboStepSpec
{
	GENERATED_BODY()

	//! \brief 이 타가 재생할 몽타주 섹션 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo")
	FName MontageSectionName;

	//! \brief 이 타의 피해 계수(Source AttackPower에 곱해짐)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.0f;

	//! \brief 타격 시 대상을 밀어낼 거리. 0이면 넉백 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float KnockbackDistance = 0.0f;

	//! \brief true면 타격 시 Effects.StatusGameplayEffect를 함께 부여한다(예: 마지막 타 표식 부여)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo")
	bool bApplyStatusEffect = false;

	//! \brief 이 타에서 캐릭터가 바라보는 방향으로 전진할 거리(cm). 0이면 전진 없음(예: Heru 3타 전진 베기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ForwardMoveDistance = 0.0f;

	//! \brief 전진에 걸리는 시간(초). ForwardMoveDistance > 0일 때만 사용하며 공격 속도에 따라 스케일된다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|Move", meta = (ClampMin = "0.01"))
	float ForwardMoveDuration = 0.2f;

	//! \brief 몽타주가 아직 없을 때만 사용: 스텝 시작부터 Fire(판정/발사)까지의 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|No Montage Fallback", meta = (ClampMin = "0.0"))
	float FallbackFireDelay = 0.1f;

	//! \brief 몽타주가 아직 없을 때만 사용: 스텝 전체 길이(다음 입력이 나갈 수 있을 때까지의 간격)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo|No Montage Fallback", meta = (ClampMin = "0.05"))
	float FallbackStepDuration = 0.5f;
};

////////////////////////////
//! \struct FMySkillComboSpec
//! \brief 연속 입력형 스킬의 콤보 계수와 입력 간격을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillComboSpec
{
	GENERATED_BODY()

	//! \brief 타 순서대로의 콤보 단계 정의. ComboAttackBase 계열 스킬이 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (TitleProperty = "MontageSectionName"))
	TArray<FMySkillComboStepSpec> Steps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Combo", meta = (ClampMin = "0.0"))
	float ResetTime = 0.0f;
};

////////////////////////////
//! \struct FMySkillAimAssistSpec
//! \brief 방향형 스킬의 조준 보정 값을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillAimAssistSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aim Assist", meta = (ClampMin = "0.0"))
	float Range = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aim Assist", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float SearchAngle = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Aim Assist", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxCorrectionAngle = 0.0f;
};

////////////////////////////
//! \struct FMySkillTargetingSpec
//! \brief 스킬 타겟팅과 실제 판정 크기의 원본 값을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillTargetingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Targeting")
	EMySkillTargetingType TargetingType = EMySkillTargetingType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Targeting", meta = (ClampMin = "0.0"))
	float Range = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Targeting", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Targeting", meta = (ClampMin = "0.0"))
	float Width = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Targeting", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float Angle = 0.0f;
};

////////////////////////////
//! \struct FMySkillProjectileSpec
//! \brief 투사체형 스킬의 생성 클래스와 이동/충돌 값을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillProjectileSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile")
	FName SpawnSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile", meta = (ClampMin = "0.0"))
	float MaxDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Projectile", meta = (ClampMin = "0.0"))
	float SpawnUpOffset = 0.0f;
};

////////////////////////////
//! \struct FMySkillAreaSpec
//! \brief 장판형 스킬의 시각 Actor와 실제 판정 반경을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillAreaSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Area")
	TSubclassOf<AActor> AreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Area", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;
};

////////////////////////////
//! \struct FMySkillIndicatorVisualSpec
//! \brief 인디케이터의 표시 방식과 시각 전용 설정을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillIndicatorVisualSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	EMySkillIndicatorType IndicatorType = EMySkillIndicatorType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	TSubclassOf<AMySkillIndicatorActorBase> IndicatorActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bFollowOwner = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bFollowCursor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bSnapToGround = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bCheckValidTarget = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bClampToRange = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	bool bShowRangeVisual = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Indicator")
	TObjectPtr<UMaterialInterface> RangeMaterial;
};

////////////////////////////
//! \struct FMySkillEffectSpec
//! \brief 스킬이 사용할 GameplayEffect 템플릿과 밸런스 계수를 정의한다.
USTRUCT(BlueprintType)
struct FMySkillEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect")
	TSubclassOf<UGameplayEffect> HitGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect")
	TSubclassOf<UGameplayEffect> StatusGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect")
	TSubclassOf<UGameplayEffect> HealGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect")
	TSubclassOf<UGameplayEffect> BuffGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect", meta = (ClampMin = "0.0"))
	float SecondaryDamageCoefficient = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect", meta = (ClampMin = "0.0"))
	float TertiaryDamageCoefficient = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect", meta = (ClampMin = "0.0"))
	float StatusDamageCoefficient = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Effect", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0", DisplayName = "Heal Percent Of Max Health (%)", ToolTip = "4.0 means 4% of MaxHealth."))
	float HealPercentOfMaxHealth = 0.0f;
};

////////////////////////////
//! \struct FMySkillMovementSpec
//! \brief 이동형 스킬의 이동/충전 값을 정의한다.
USTRUCT(BlueprintType)
struct FMySkillMovementSpec
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Movement", meta = (ClampMin = "0.0"))
	float DashStrength = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Movement", meta = (ClampMin = "0.0"))
	float RechargeSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Movement")
	EMyDashDirectionPolicy DashDirectionPolicy = EMyDashDirectionPolicy::RequireMoveInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill|Movement")
	bool bBlockMoveInputDuringAbility = false;
};

////////////////////////////
//! \struct FMySkillDataEntry
//! \brief 하나의 플레이어 스킬에 필요한 조정 가능 데이터를 정의한다.
USTRUCT(BlueprintType)
struct FMySkillDataEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FName SkillId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillTargetingSpec Targeting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillInputSpec Input;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillIndicatorVisualSpec IndicatorVisual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillTimingSpec Timing;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillComboSpec Combo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillAimAssistSpec AimAssist;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillProjectileSpec Projectile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillAreaSpec Area;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillEffectSpec Effects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill")
	FMySkillMovementSpec Movement;
};

////////////////////////////
//! \class UMySkillSetDataAsset
//! \brief 플레이어 스킬별 조정 가능 파라미터를 보관하는 DataAsset이다.
UCLASS(BlueprintType, Const)
class PROJECTP_API UMySkillSetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const FMySkillDataEntry* FindSkillByInputTag(FGameplayTag InputTag) const;
	const FMySkillDataEntry* FindSkillByAbilityTag(FGameplayTag AbilityTag) const;
	const FMySkillDataEntry* FindSkillById(FName SkillId) const;

	bool BuildIndicatorSpecByInputTag(
		FGameplayTag InputTag,
		FMySkillIndicatorSpec& OutIndicatorSpec,
		TSubclassOf<AMySkillIndicatorActorBase>& OutIndicatorActorClass
	) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Set", meta = (AllowPrivateAccess = "true"))
	TArray<FMySkillDataEntry> Skills;
};
