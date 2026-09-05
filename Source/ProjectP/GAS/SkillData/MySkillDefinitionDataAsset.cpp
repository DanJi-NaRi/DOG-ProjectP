////////////////////////////
//! \file MySkillDefinitionDataAsset.cpp
//! \brief MyGAS 단일 스킬 정의 DataAsset 구현 파일이다.

#include "MySkillDefinitionDataAsset.h"

#include "../../Indicator/MySkillIndicatorActorBase.h"

////////////////////////////
//! \author HanUl
//! \brief 이 스킬을 실행할 GameplayAbility 클래스를 반환한다.
//! \param 없음
//! \return 스킬 Ability 클래스
TSubclassOf<UMyGameplayAbility_SkillBase> UMySkillDefinitionDataAsset::GetAbilityClass() const
{
	return AbilityClass;
}

////////////////////////////
//! \author HanUl
//! \brief 이 스킬을 ASC에 부여할 때 사용할 Ability 레벨을 반환한다.
//! \param 없음
//! \return Ability 레벨
int32 UMySkillDefinitionDataAsset::GetAbilityLevel() const
{
	return FMath::Max(AbilityLevel, 1);
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 식별자를 반환한다.
//! \param 없음
//! \return 스킬 식별자
FName UMySkillDefinitionDataAsset::GetSkillId() const
{
	return SkillId;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 표시 이름을 반환한다.
//! \param 없음
//! \return 스킬 표시 이름
FText UMySkillDefinitionDataAsset::GetDisplayName() const
{
	return DisplayName;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 라우팅에 사용할 GameplayTag를 반환한다.
//! \param 없음
//! \return 입력 GameplayTag
FGameplayTag UMySkillDefinitionDataAsset::GetInputTag() const
{
	return InputTag;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 정체성에 사용할 GameplayTag를 반환한다.
//! \param 없음
//! \return Ability GameplayTag
FGameplayTag UMySkillDefinitionDataAsset::GetAbilityTag() const
{
	return AbilityTag;
}

////////////////////////////
//! \author HanUl
//! \brief 쿨다운 상태에 사용할 GameplayTag를 반환한다.
//! \param 없음
//! \return 쿨다운 GameplayTag
FGameplayTag UMySkillDefinitionDataAsset::GetCooldownTag() const
{
	return CooldownTag;
}

////////////////////////////
//! \author HanUl
//! \brief 쿨다운 지속 시간을 반환한다.
//! \param 없음
//! \return 쿨다운 지속 시간
float UMySkillDefinitionDataAsset::GetCooldownDuration() const
{
	return FMath::Max(CooldownDuration, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 정의가 Ability 부여에 필요한 최소 데이터를 갖췄는지 확인한다.
//! \param 없음
//! \return 유효한 AbilityClass와 InputTag 또는 AbilityTag가 있으면 true
bool UMySkillDefinitionDataAsset::IsValidDefinition() const
{
	return AbilityClass && (InputTag.IsValid() || AbilityTag.IsValid());
}

////////////////////////////
//! \author HanUl
//! \brief 인디케이터 표시 설정이 있는지 확인한다.
//! \param 없음
//! \return 인디케이터 타입과 ActorClass가 유효하면 true
bool UMySkillDefinitionDataAsset::HasIndicator() const
{
	return IndicatorVisual.IndicatorType != EMySkillIndicatorType::None && IndicatorVisual.IndicatorActorClass;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 애니메이션 설정을 반환한다.
//! \param 없음
//! \return 스킬 애니메이션 설정
const FMySkillAnimationSpec& UMySkillDefinitionDataAsset::GetAnimation() const
{
	return Animation;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 타이밍 설정을 반환한다.
//! \param 없음
//! \return 스킬 타이밍 설정
const FMySkillTimingSpec& UMySkillDefinitionDataAsset::GetTiming() const
{
	return Timing;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 타겟팅 설정을 반환한다.
//! \param 없음
//! \return 스킬 타겟팅 설정
const FMySkillTargetingSpec& UMySkillDefinitionDataAsset::GetTargeting() const
{
	return Targeting;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 조준 정책을 반환한다.
//! \param 없음
//! \return 스킬 입력 조준 정책
const FMySkillInputSpec& UMySkillDefinitionDataAsset::GetInput() const
{
	return Input;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 인디케이터 설정을 반환한다.
//! \param 없음
//! \return 스킬 인디케이터 설정
const FMySkillIndicatorVisualSpec& UMySkillDefinitionDataAsset::GetIndicatorVisual() const
{
	return IndicatorVisual;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 콤보 설정을 반환한다.
//! \param 없음
//! \return 스킬 콤보 설정
const FMySkillComboSpec& UMySkillDefinitionDataAsset::GetCombo() const
{
	return Combo;
}

////////////////////////////
//! \author HanUl
//! \brief 조준 보정 설정을 반환한다.
//! \param 없음
//! \return 조준 보정 설정
const FMySkillAimAssistSpec& UMySkillDefinitionDataAsset::GetAimAssist() const
{
	return AimAssist;
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 설정을 반환한다.
//! \param 없음
//! \return 투사체 설정
const FMySkillProjectileSpec& UMySkillDefinitionDataAsset::GetProjectile() const
{
	return Projectile;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 설정을 반환한다.
//! \param 없음
//! \return 장판 설정
const FMySkillAreaSpec& UMySkillDefinitionDataAsset::GetArea() const
{
	return Area;
}

////////////////////////////
//! \author HanUl
//! \brief GameplayEffect와 밸런스 계수 설정을 반환한다.
//! \param 없음
//! \return 스킬 효과 설정
const FMySkillEffectSpec& UMySkillDefinitionDataAsset::GetEffects() const
{
	return Effects;
}

////////////////////////////
//! \author HanUl
//! \brief 이동형 스킬 설정을 반환한다.
//! \param 없음
//! \return 스킬 이동 설정
const FMySkillMovementSpec& UMySkillDefinitionDataAsset::GetMovement() const
{
	return Movement;
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition에 붙은 선택적 Fragment 배열을 반환한다.
//! \param 없음
//! \return Fragment 배열
const TArray<TObjectPtr<UMySkillDefinitionFragment>>& UMySkillDefinitionDataAsset::GetFragments() const
{
	return Fragments;
}

////////////////////////////
//! \author HanUl
//! \brief 단일 SkillDefinition을 기존 SkillDataEntry 형태로 변환한다.
//! \param OutSkillData 변환된 스킬 데이터
//! \return AbilityClass와 식별 태그가 유효하면 true
bool UMySkillDefinitionDataAsset::BuildSkillDataEntry(FMySkillDataEntry& OutSkillData) const
{
	OutSkillData = FMySkillDataEntry();
	OutSkillData.SkillId = SkillId;
	OutSkillData.DisplayName = DisplayName;
	OutSkillData.Icon = Icon;
	OutSkillData.InputTag = InputTag;
	OutSkillData.AbilityTag = AbilityTag;
	OutSkillData.CooldownTag = CooldownTag;
	OutSkillData.CooldownDuration = GetCooldownDuration();
	OutSkillData.Targeting = Targeting;
	OutSkillData.Input = Input;
	OutSkillData.IndicatorVisual = IndicatorVisual;
	OutSkillData.Timing = Timing;
	OutSkillData.Combo = Combo;
	OutSkillData.AimAssist = AimAssist;
	OutSkillData.Projectile = Projectile;
	OutSkillData.Area = Area;
	OutSkillData.Effects = Effects;
	OutSkillData.Movement = Movement;

	return IsValidDefinition();
}

////////////////////////////
//! \author HanUl
//! \brief 이 스킬 정의에서 인디케이터 표시 Spec을 생성한다.
//! \param OutIndicatorSpec 생성된 인디케이터 표시 설정
//! \param OutIndicatorActorClass 생성할 인디케이터 Actor 클래스
//! \return 표시 설정 생성에 성공하면 true
bool UMySkillDefinitionDataAsset::BuildIndicatorSpec(
	FMySkillIndicatorSpec& OutIndicatorSpec,
	TSubclassOf<AMySkillIndicatorActorBase>& OutIndicatorActorClass
) const
{
	OutIndicatorSpec = FMySkillIndicatorSpec();
	OutIndicatorActorClass = nullptr;

	if (!HasIndicator())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS SkillDefinition indicator spec build failed - indicator is invalid. SkillDefinition: %s, SkillId: %s, InputTag: %s"),
			*GetNameSafe(this),
			*SkillId.ToString(),
			*InputTag.ToString());
		return false;
	}

	OutIndicatorActorClass = IndicatorVisual.IndicatorActorClass;

	OutIndicatorSpec.IndicatorType = IndicatorVisual.IndicatorType;
	OutIndicatorSpec.InputTag = InputTag;
	OutIndicatorSpec.Range = Targeting.Range;
	OutIndicatorSpec.Radius = Area.Radius > 0.0f ? Area.Radius : Targeting.Radius;
	OutIndicatorSpec.Width = Targeting.Width;
	OutIndicatorSpec.Angle = Targeting.Angle;
	OutIndicatorSpec.ProjectileSpeed = Projectile.ProjectileSpeed;
	OutIndicatorSpec.bFollowOwner = IndicatorVisual.bFollowOwner;
	OutIndicatorSpec.bFollowCursor = IndicatorVisual.bFollowCursor;
	OutIndicatorSpec.bSnapToGround = IndicatorVisual.bSnapToGround;
	OutIndicatorSpec.bCheckValidTarget = IndicatorVisual.bCheckValidTarget;
	OutIndicatorSpec.bClampToRange = IndicatorVisual.bClampToRange;
	OutIndicatorSpec.bShowRangeVisual = IndicatorVisual.bShowRangeVisual;
	OutIndicatorSpec.GroundTraceChannel = IndicatorVisual.GroundTraceChannel;
	OutIndicatorSpec.ValidMaterial = IndicatorVisual.ValidMaterial;
	OutIndicatorSpec.InvalidMaterial = IndicatorVisual.InvalidMaterial;
	OutIndicatorSpec.RangeMaterial = IndicatorVisual.RangeMaterial;

	return true;
}
