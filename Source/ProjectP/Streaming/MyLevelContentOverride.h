////////////////////////////
//! \page MyLevelContentOverride.h
//! \brief 레벨마다 다른 Mission/Streaming DataTable 세트를 쓰기 위한 레벨 배치용 Override 선언 파일이다.
//! \note 아 레벨2에는 다른 테이블 쓰기 위해서 존재함. 아직 작업 안함. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyLevelContentOverride.generated.h"

class UDataTable;

////////////////////////////
//! \class AMyLevelContentTableOverride
//! \author 장효제
//! \brief 레벨에 하나 배치해 Mission/Streaming Manager가 쓰는 DataTable을 레벨별로 교체한다. 비워두면 기존 기본 테이블을 그대로 쓴다.
UCLASS()
class PROJECTP_API AMyLevelContentTableOverride : public AActor
{
	GENERATED_BODY()

public:
	AMyLevelContentTableOverride();

	//! \brief 인자로 받은 World의 PersistentLevel에서 첫 번째 Override 인스턴스를 찾는다. 없으면 nullptr이다.
	static AMyLevelContentTableOverride* FindForWorld(const UWorld* World);

	UPROPERTY(EditAnywhere, Category = "Mission", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionDefinitionRow"))
	TObjectPtr<UDataTable> MissionDefinitionTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Mission", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionCombatObjectiveRow"))
	TObjectPtr<UDataTable> MissionObjectiveTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingCombatRuleRow"))
	TObjectPtr<UDataTable> CombatRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingMesoRuleRow"))
	TObjectPtr<UDataTable> MesoRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingKillCountRuleRow"))
	TObjectPtr<UDataTable> KillCountRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingSkillUseRuleRow"))
	TObjectPtr<UDataTable> SkillUseRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingTuningRow"))
	TObjectPtr<UDataTable> TuningTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingAntiAFKRuleRow"))
	TObjectPtr<UDataTable> AntiAFKRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingCountRuleRow"))
	TObjectPtr<UDataTable> CountRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingItemRuleRow"))
	TObjectPtr<UDataTable> ItemRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingStateRuleRow"))
	TObjectPtr<UDataTable> StateRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingChatLineRow"))
	TObjectPtr<UDataTable> ChatLineTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingGimmickRuleRow"))
	TObjectPtr<UDataTable> GimmickRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingZoneDonationRuleRow"))
	TObjectPtr<UDataTable> ZoneDonationRuleTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyStreamingZoneRuleRow"))
	TObjectPtr<UDataTable> ZoneRuleTable = nullptr;
};
