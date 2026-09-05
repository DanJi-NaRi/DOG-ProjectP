////////////////////////////
//! \page MyStreamingMesoRuleTypes.h
//! \brief Dungeon 내 플레이어별 Meso 누계와 Streaming Sequence를 연결하는 Rule 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingMesoRuleTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingMesoRuleRow
//! \author 장효제
//! \brief 플레이어별 Dungeon Meso 획득·사용 누계가 문턱을 넘을 때 실행할 Sequence를 정의한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingMesoRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule", meta = (Categories = "Streaming.Event.Meso"))
	FGameplayTag EventTag;

	//! 비어 있으면 모든 Meso 출처와 일치한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule", meta = (Categories = "Meso.Source"))
	FGameplayTag SourceTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule", meta = (ClampMin = "1"))
	int32 RequiredMeso = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|MesoRule")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};

namespace MyStreamingMesoRulePolicy
{
	//! \brief 증가하는 Dungeon 누계가 Rule 문턱을 이번 변화에서 처음 통과했는지 반환한다.
	inline bool DidCrossThreshold(const int64 PreviousTotal, const int64 NewTotal, const int32 RequiredMeso)
	{
		return RequiredMeso > 0 && PreviousTotal < RequiredMeso && NewTotal >= RequiredMeso;
	}
}
