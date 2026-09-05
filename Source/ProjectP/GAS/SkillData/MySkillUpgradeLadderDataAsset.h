////////////////////////////
//! \file MySkillUpgradeLadderDataAsset.h
//! \brief 스킬 강화(레벨) 단계별 SkillDefinition 사다리를 SkillId로 매핑해 보관하는 DataAsset 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MySkillUpgradeLadderDataAsset.generated.h"

class UMySkillDefinitionDataAsset;

////////////////////////////
//! \struct FMySkillUpgradeTrack
//! \author HanUl
//! \brief 한 스킬의 기본(1단계)→2단계→3단계 SkillDefinition을 순서대로 보관하는 강화 트랙이다.
USTRUCT(BlueprintType)
struct FMySkillUpgradeTrack
{
	GENERATED_BODY()

	//! \brief 강화 트랙을 식별하는 스킬 ID. SkillDefinition.SkillId와 동일해야 하며 레벨이 올라가도 유지된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Upgrade")
	FName SkillId;

	//! \brief 레벨 순서대로의 SkillDefinition. [0]=기본(Lv1), [1]=2단계, [2]=3단계. 최대 3개까지만 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Upgrade")
	TArray<TObjectPtr<UMySkillDefinitionDataAsset>> Levels;
};

////////////////////////////
//! \class UMySkillUpgradeLadderDataAsset
//! \author HanUl
//! \brief SkillId별 강화 단계(최대 3단계) SkillDefinition 사다리를 보관하고 조회하는 DataAsset이다.
UCLASS(BlueprintType, Const)
class PROJECTP_API UMySkillUpgradeLadderDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	//! \brief 강화 단계 상한(기본 포함 최대 3단계).
	static constexpr int32 MaxSkillLevel = 3;

	const FMySkillUpgradeTrack* FindTrack(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Upgrade")
	int32 GetMaxLevel(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Skill Upgrade")
	bool CanUpgrade(FName SkillId, int32 CurrentLevel) const;

	const UMySkillDefinitionDataAsset* GetDefinitionForLevel(FName SkillId, int32 Level) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Skill Upgrade", meta = (TitleProperty = "SkillId", AllowPrivateAccess = "true"))
	TArray<FMySkillUpgradeTrack> Tracks;
};
