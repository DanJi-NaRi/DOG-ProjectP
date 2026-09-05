////////////////////////////
//! \page MyStreamingGimmickTypes.h
//! \brief Gimmick Streaming 사실 Payload와 Rule DataTable 행 계약을 선언한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingGimmickTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingGimmickResetPayload
//! \author 장효제
//! \brief 같은 Dungeon에서 같은 Gimmick의 파티 초기화가 누적된 사실이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingGimmickResetPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Streaming|Gimmick")
	FName GimmickId;

	UPROPERTY(BlueprintReadOnly, Category = "Streaming|Gimmick")
	int32 PartyResetCount = 0;
};

////////////////////////////
//! \struct FMyStreamingGimmickRuleRow
//! \author 장효제
//! \brief GimmickId와 정확한 파티 초기화 횟수를 Chat Sequence에 연결한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingGimmickRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick")
	FName GimmickId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick", meta = (ClampMin = "1"))
	int32 ResetCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Gimmick")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};
