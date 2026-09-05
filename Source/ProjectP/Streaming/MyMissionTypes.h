////////////////////////////
//! \page MyMissionTypes.h
//! \brief 첫 플레이 가능한 Dungeon Mission의 데이터와 서버 상태 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyMissionTypes.generated.h"

UENUM(BlueprintType)
enum class EMyMissionConditionType : uint8
{
	KillCount,
	//! 특정 스킬을 몇 번 썼는지 센다. SkillTag가 셀 스킬을 정한다.
	SkillUseCount,
	//! 파티 사건을 몇 번 겪었는지 센다. EventTag가 셀 사건을 정한다.
	EventCount,
	//! 아이템을 몇 개 쓰거나 샀는지 센다. EventTag와 ItemId가 함께 정한다.
	ItemCount,
};

UENUM(BlueprintType)
enum class EMyMissionProgressPolicy : uint8
{
	SharedTotal,
};

////////////////////////////
//! \enum EMyMissionCountOrigin
//! \author 장효제
//! \brief Mission 진행도를 어느 시점부터 세는지다.
UENUM(BlueprintType)
enum class EMyMissionCountOrigin : uint8
{
	//! Mission이 걸린 순간부터 센다. 제한시간 안에 몇 마리 같은 목표에 쓴다.
	MissionStart,
	//! 게임(던전 한 판) 시작부터의 파티 누계를 그대로 쓴다.
	//! Mission이 걸리기 전에 잡은 몬스터도 진행도에 포함된다.
	GameStart,
};

UENUM(BlueprintType)
enum class EMyMissionAssigneeSelector : uint8
{
	AllParty,
	FixedCharacter,
};

UENUM(BlueprintType)
enum class EMyMissionVisibility : uint8
{
	PublicParty,
};

UENUM(BlueprintType)
enum class EMyMissionState : uint8
{
	None,
	Active,
	Completed,
	Expired,
};

////////////////////////////
//! \struct FMyMissionDefinitionRow
//! \author 장효제
//! \brief 한 번의 Mission과 시작·완료 Sequence를 연결하는 서버 데이터 행이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyMissionDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (Categories = "Mission"))
	FGameplayTag MissionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (Categories = "God"))
	FGameplayTag ProposerGodTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Sequence")
	FName StartSequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective")
	FName ObjectiveRow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective")
	EMyMissionProgressPolicy ProgressPolicy = EMyMissionProgressPolicy::SharedTotal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Assignee")
	EMyMissionAssigneeSelector AssigneeSelector = EMyMissionAssigneeSelector::AllParty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Assignee", meta = (Categories = "Character"))
	TArray<FGameplayTag> FixedAssigneeCharacters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Visibility")
	EMyMissionVisibility Visibility = EMyMissionVisibility::PublicParty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Completion")
	int32 CompletionMesoMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Completion")
	int32 CompletionMesoMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Time", meta = (ClampMin = "0.01"))
	float MissionTimeLimitSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Sequence")
	FName CompletionSequenceId;
};

////////////////////////////
//! \struct FMyMissionCombatObjectiveRow
//! \author 장효제
//! \brief 첫 버전에서 서버가 공동 집계할 KillCount 조건 한 행이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyMissionCombatObjectiveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective")
	EMyMissionConditionType ConditionType = EMyMissionConditionType::KillCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective", meta = (Categories = "Character"))
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective", meta = (ClampMin = "0.0"))
	float RequiredAmount = 0.0f;

	//! \brief 진행도를 어느 시점부터 셀지다. 기본은 Mission이 걸린 순간부터다.
	//! \brief EventCount와 ItemCount 목표가 셀 사건이다. 그 외에는 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective", meta = (Categories = "Streaming.Event"))
	FGameplayTag EventTag;

	//! \brief ItemCount 목표가 셀 DT_ItemTable RowName이다. 그 외에는 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Objective")
	EMyMissionCountOrigin CountOrigin = EMyMissionCountOrigin::MissionStart;
};

////////////////////////////
//! \struct FMyMissionObjectiveView
//! \author 장효제
//! \brief 클라이언트 UI가 반복 표시할 Mission Objective 진행도 한 행이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyMissionObjectiveView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	FName ObjectiveRowName;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	int32 ProgressCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	int32 RequiredCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	bool bCompleted = false;
};

////////////////////////////
//! \struct FMyMissionPublicView
//! \author 장효제
//! \brief 서버 Mission 원본에서 클라이언트 HUD에 공개할 최소 복제 요약이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyMissionPublicView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	FGuid MissionInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	FName DefinitionRowName;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	FGameplayTag MissionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	FGameplayTag ProposerGodTag;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	TArray<FGameplayTag> AssigneeCharacterTags;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	TArray<FMyMissionObjectiveView> Objectives;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	EMyMissionState State = EMyMissionState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	int32 ResolvedMesoDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	float ActivatedAtServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|UI")
	float MissionEndsAtServerTime = 0.0f;
};

//! \brief Mission 서버 원본 생성 요청의 판정 결과다. None만 실제 생성으로 진행한다.
UENUM()
enum class EMyMissionActivationRejection : uint8
{
	None,
	InvalidRequest,
	AlreadyUsed,
	AlreadyActive,
	DefinitionNotFound,
	AssigneeNotSatisfied,
};

namespace MyMissionPolicy
{
	PROJECTP_API bool IsAntiAFKMissionTag(FGameplayTag MissionTag);

	//! \brief StartMission Step의 태그와 재생 SequenceId가 Definition 한 행에 정확히 대응하는지 판정한다.
	//! \param DefinitionMissionTag Definition 행의 MissionTag다.
	//! \param DefinitionStartSequenceId Definition 행의 StartSequenceId다.
	//! \param RequestedMissionTag StartMission Step이 지정한 MissionTag다.
	//! \param SourceSequenceId StartMission Step을 재생 중인 SequenceId다.
	//! \return 태그 완전일치와 시작 Sequence 일치가 동시에 성립하면 true다.
	PROJECTP_API bool DoesDefinitionMatchStartRequest(
		FGameplayTag DefinitionMissionTag,
		FName DefinitionStartSequenceId,
		FGameplayTag RequestedMissionTag,
		FName SourceSequenceId);

	//! \brief Mission 서버 원본을 만들기 전 거부 사유를 계산한다. UObject/RNG 없이 순수하게 테스트 가능하다.
	//! \param MissionTag 시작할 Mission 종류 태그다.
	//! \param SourceSequenceId StartMission Step을 재생 중인 SequenceId다.
	//! \param bIsUsedMissionTag 이 던전 수명에서 이미 소비한 일반 Mission 태그인지 여부다.
	//! \param bHasActiveSameTag 같은 태그의 Active Mission이 이미 있는지 여부다.
	//! \param bDefinitionValid 대응 Definition을 찾고 런타임 계약까지 통과했는지 여부다.
	//! \param bAssigneesResolved 수행자 해석이 성립했는지 여부다.
	//! \return None이면 생성 진행, 그 외는 서버 거부 사유다.
	PROJECTP_API EMyMissionActivationRejection CheckMissionActivation(
		FGameplayTag MissionTag,
		FName SourceSequenceId,
		bool bIsUsedMissionTag,
		bool bHasActiveSameTag,
		bool bDefinitionValid,
		bool bAssigneesResolved);

	//! \brief 잠수 감지가 시작 Sequence를 요청할 AntiAFK Definition 후보인지 판정한다.
	//! \param MissionTag Definition 행의 MissionTag다.
	//! \param StartSequenceId Definition 행의 StartSequenceId다.
	//! \param bDefinitionValid Definition 런타임 계약 통과 여부다.
	//! \param bHasActiveSameTag 같은 태그의 Active Mission이 이미 있는지 여부다.
	//! \return 재사용 가능한 AntiAFK 후보면 true다.
	PROJECTP_API bool IsAntiAFKStartCandidate(
		FGameplayTag MissionTag,
		FName StartSequenceId,
		bool bDefinitionValid,
		bool bHasActiveSameTag);

	PROJECTP_API bool IsKillContribution(
		FGameplayTag EventTag,
		bool bIsKill,
		FGameplayTag InstigatorTag,
		FGameplayTag TargetTag,
		FGameplayTag RequiredTargetTag);

	//! \brief 전투 사실이 이 Mission의 스킬 사용 목표에 기여하는지 판정한다.
	//! \param EventTag 사실 태그다.
	//! \param UsedSkillTag 쓴 스킬 태그다.
	//! \param InstigatorTag 사실을 낸 대상 태그다.
	//! \param RequiredSkillTag 목표가 요구하는 스킬이다. 비면 모든 스킬을 센다.
	//! \return 기여하면 true다.
	PROJECTP_API bool IsSkillUseContribution(
		FGameplayTag EventTag,
		FGameplayTag UsedSkillTag,
		FGameplayTag InstigatorTag,
		FGameplayTag RequiredSkillTag);

	PROJECTP_API bool IsAssigneeContribution(
		EMyMissionAssigneeSelector AssigneeSelector,
		const TArray<FGameplayTag>& AssigneeCharacterTags,
		FGameplayTag InstigatorTag);

	PROJECTP_API EMyMissionState ResolveKillMissionState(
		int32 ProgressCount,
		int32 RequiredCount,
		float ServerTime,
		float EndsAtServerTime);
}
