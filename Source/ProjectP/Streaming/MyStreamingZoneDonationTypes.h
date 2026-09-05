////////////////////////////
//! \page MyStreamingZoneDonationTypes.h
//! \brief Zone Clear Donation 이벤트와 전용 DataTable 행 계약을 선언한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MyStreamingZoneDonationTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingZoneClearedPayload
//! \brief ZoneManager가 서버에서 발행하는 OrderedZones 기반 Clear 사실이다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingZoneClearedPayload
{
	GENERATED_BODY()

	//! OrderedZones의 0-based 배열 인덱스다.
	UPROPERTY(BlueprintReadOnly, Category = "Streaming|ZoneDonation")
	int32 ZoneIndex = INDEX_NONE;
};

////////////////////////////
//! \struct FMyStreamingZoneDonationRuleRow
//! \brief 한 Zone 배열 인덱스를 반드시 실행할 Donation Sequence에 연결한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingZoneDonationRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	//! AZoneManager::OrderedZones의 0-based 배열 인덱스다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|ZoneDonation")
	int32 ZoneIndex = INDEX_NONE;

	//! Donation 전용 Line 후보 한 Step으로 구성된 SequenceId다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming|ZoneDonation")
	FName SequenceId;
};
