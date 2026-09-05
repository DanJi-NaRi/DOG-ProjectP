////////////////////////////
//! \page MySkillSetDataAsset.cpp
//! \brief MyGAS 플레이어 스킬 파라미터 DataAsset 조회 함수를 구현한다.

#include "MySkillSetDataAsset.h"

#include "../../Indicator/MySkillIndicatorActorBase.h"

////////////////////////////
//! \brief InputTag가 일치하는 스킬 데이터를 찾는다.
//! \param InputTag 검색할 입력 GameplayTag
//! \return 일치하는 스킬 데이터. 없으면 nullptr
const FMySkillDataEntry* UMySkillSetDataAsset::FindSkillByInputTag(FGameplayTag InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (const FMySkillDataEntry& Skill : Skills)
	{
		if (Skill.InputTag == InputTag)
		{
			return &Skill;
		}
	}

	return nullptr;
}

////////////////////////////
//! \brief AbilityTag가 일치하는 스킬 데이터를 찾는다.
//! \param AbilityTag 검색할 Ability GameplayTag
//! \return 일치하는 스킬 데이터. 없으면 nullptr
const FMySkillDataEntry* UMySkillSetDataAsset::FindSkillByAbilityTag(FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (const FMySkillDataEntry& Skill : Skills)
	{
		if (Skill.AbilityTag == AbilityTag)
		{
			return &Skill;
		}
	}

	return nullptr;
}

////////////////////////////
//! \brief SkillId가 일치하는 스킬 데이터를 찾는다.
//! \param SkillId 검색할 스킬 식별자
//! \return 일치하는 스킬 데이터. 없으면 nullptr
const FMySkillDataEntry* UMySkillSetDataAsset::FindSkillById(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	for (const FMySkillDataEntry& Skill : Skills)
	{
		if (Skill.SkillId == SkillId)
		{
			return &Skill;
		}
	}

	return nullptr;
}

////////////////////////////
//! \brief InputTag에 해당하는 스킬 데이터에서 인디케이터 표시 Spec을 생성한다.
//! \param InputTag 검색할 입력 GameplayTag
//! \param OutIndicatorSpec 생성된 인디케이터 표시 설정
//! \param OutIndicatorActorClass 생성할 인디케이터 Actor 클래스
//! \return 표시 설정 생성에 성공하면 true
bool UMySkillSetDataAsset::BuildIndicatorSpecByInputTag(
	FGameplayTag InputTag,
	FMySkillIndicatorSpec& OutIndicatorSpec,
	TSubclassOf<AMySkillIndicatorActorBase>& OutIndicatorActorClass
) const
{
	OutIndicatorSpec = FMySkillIndicatorSpec();
	OutIndicatorActorClass = nullptr;

	const FMySkillDataEntry* Skill = FindSkillByInputTag(InputTag);
	if (!Skill)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS SkillSet indicator spec build failed - skill entry not found. SkillSet: %s, InputTag: %s"),
			*GetNameSafe(this),
			*InputTag.ToString());
		return false;
	}

	const FMySkillIndicatorVisualSpec& IndicatorVisual = Skill->IndicatorVisual;
	if (!IndicatorVisual.IndicatorActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS SkillSet indicator spec build failed - IndicatorActorClass is null. SkillSet: %s, SkillId: %s, InputTag: %s"),
			*GetNameSafe(this),
			*Skill->SkillId.ToString(),
			*InputTag.ToString());
		return false;
	}

	OutIndicatorActorClass = IndicatorVisual.IndicatorActorClass;

	OutIndicatorSpec.IndicatorType = IndicatorVisual.IndicatorType;
	OutIndicatorSpec.InputTag = Skill->InputTag;
	OutIndicatorSpec.Range = Skill->Targeting.Range;
	OutIndicatorSpec.Radius = Skill->Area.Radius > 0.0f ? Skill->Area.Radius : Skill->Targeting.Radius;
	OutIndicatorSpec.Width = Skill->Targeting.Width;
	OutIndicatorSpec.Angle = Skill->Targeting.Angle;
	OutIndicatorSpec.ProjectileSpeed = Skill->Projectile.ProjectileSpeed;
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
