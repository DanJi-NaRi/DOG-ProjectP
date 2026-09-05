////////////////////////////
//! \file MySkillUpgradeLadderDataAsset.cpp
//! \brief 스킬 강화 사다리 DataAsset 구현 파일이다.

#include "MySkillUpgradeLadderDataAsset.h"

#include "MySkillDefinitionDataAsset.h"

////////////////////////////
//! \author HanUl
//! \brief SkillId에 해당하는 강화 트랙을 찾는다.
//! \param SkillId 찾을 스킬 ID
//! \return 일치하는 트랙 포인터, 없으면 nullptr
const FMySkillUpgradeTrack* UMySkillUpgradeLadderDataAsset::FindTrack(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	for (const FMySkillUpgradeTrack& Track : Tracks)
	{
		if (Track.SkillId == SkillId)
		{
			return &Track;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief SkillId의 최대 강화 레벨을 반환한다(선두부터 연속으로 채워진 유효 Definition 개수, 상한 3).
//! \param SkillId 조회할 스킬 ID
//! \return 최대 레벨(1~3), 트랙이 없으면 0
int32 UMySkillUpgradeLadderDataAsset::GetMaxLevel(FName SkillId) const
{
	const FMySkillUpgradeTrack* Track = FindTrack(SkillId);
	if (!Track)
	{
		return 0;
	}

	int32 ValidCount = 0;
	for (const TObjectPtr<UMySkillDefinitionDataAsset>& Definition : Track->Levels)
	{
		if (!Definition)
		{
			break; // 중간에 빈 칸이 있으면 그 앞까지만 유효한 사다리로 본다.
		}

		++ValidCount;
		if (ValidCount >= MaxSkillLevel)
		{
			break;
		}
	}

	return ValidCount;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 레벨에서 다음 단계로 강화할 수 있는지 확인한다.
//! \param SkillId 조회할 스킬 ID
//! \param CurrentLevel 현재 스킬 레벨(1 기준)
//! \return 다음 단계 Definition이 존재하면 true
bool UMySkillUpgradeLadderDataAsset::CanUpgrade(FName SkillId, int32 CurrentLevel) const
{
	return GetDefinitionForLevel(SkillId, CurrentLevel + 1) != nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief SkillId의 지정 레벨에 해당하는 SkillDefinition을 반환한다.
//! \param SkillId 조회할 스킬 ID
//! \param Level 조회할 레벨(1=기본, 최대 3)
//! \return 해당 레벨 Definition, 범위를 벗어나거나 비어 있으면 nullptr
const UMySkillDefinitionDataAsset* UMySkillUpgradeLadderDataAsset::GetDefinitionForLevel(FName SkillId, int32 Level) const
{
	if (Level < 1 || Level > MaxSkillLevel)
	{
		return nullptr;
	}

	const FMySkillUpgradeTrack* Track = FindTrack(SkillId);
	if (!Track || !Track->Levels.IsValidIndex(Level - 1))
	{
		return nullptr;
	}

	return Track->Levels[Level - 1];
}
