////////////////////////////
//! \page MyStreamingZoneTypes.h
//! \brief 일반 Zone Streaming 사실 Payload와 Rule DataTable 행 계약을 선언한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingCombatRuleTypes.h"
#include "MyStreamingZoneTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingZoneEventPayload
//! \author 장효제
//! \brief 서버가 확정한 Zone Activated 또는 클리어 Zone 재진입 사실이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingZoneEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Streaming|Zone")
	FGameplayTag EventTag;

	UPROPERTY(BlueprintReadOnly, Category = "Streaming|Zone")
	int32 ZoneIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Streaming|Zone")
	int32 InstigatorUserIndex = INDEX_NONE;
};

////////////////////////////
//! \struct FMyStreamingZoneRuleRow
//! \author 장효제
//! \brief 정확한 Zone EventTag와 OrderedZones 인덱스를 Chat Sequence에 연결한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingZoneRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone", meta = (Categories = "Streaming.Event.Zone"))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone", meta = (ClampMin = "0"))
	int32 ZoneIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|Zone")
	EMyStreamingSequenceBusyPolicy BusyPolicy = EMyStreamingSequenceBusyPolicy::Drop;
};
