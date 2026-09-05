////////////////////////////
//! \page MyStreamingTuningTypes.h
//! \brief 스트리밍이 스스로 말을 거는 시점을 정하는 조절값 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MyStreamingTuningTypes.generated.h"

////////////////////////////
//! \struct FMyStreamingTuningRow
//! \author 장효제
//! \brief 시간 조절값 한 건이다. 초 범위와 선택 가중치를 담는다.
//! \details 잡담 간격은 세 값을 모두 쓰고, 잠수 판정 시간은 MinSeconds만 쓴다.
//!          RowName이 어떤 값인지를 정하므로 이름을 바꾸면 기본값으로 되돌아간다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	//! \brief 최소 초다. 범위가 없는 값은 여기만 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Tuning", meta = (ClampMin = "0.0"))
	float MinSeconds = 0.0f;

	//! \brief 최대 초다. 범위를 쓰지 않으면 MinSeconds와 같게 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Tuning", meta = (ClampMin = "0.0"))
	float MaxSeconds = 0.0f;

	//! \brief 여러 후보 중 하나를 뽑을 때 쓰는 가중치다. 범위가 없는 값은 쓰지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Tuning", meta = (ClampMin = "0"))
	int32 Weight = 0;
};

namespace MyStreamingTuningRowNames
{
	//! \brief 짧은 잡담 간격이다.
	inline const FName SmallTalkShort = TEXT("SmallTalk_Short");
	//! \brief 보통 잡담 간격이다.
	inline const FName SmallTalkNormal = TEXT("SmallTalk_Normal");
	//! \brief 긴 잡담 간격이다.
	inline const FName SmallTalkLong = TEXT("SmallTalk_Long");
	//! \brief 잡담이 뭉칠 때 쓰는 간격이다. Weight를 뭉칠 확률(백분율)로 쓴다.
	inline const FName SmallTalkCluster = TEXT("SmallTalk_Cluster");
}

namespace MyStreamingTuningPolicy
{
	//! \brief 잡담이 뭉칠 확률로 쓸 수 있는 값인지 판정한다.
	//! \details 백분율이라 0~100 밖은 뜻이 없다.
	inline bool IsClusterChanceValid(const int32 Percent)
	{
		return Percent >= 0 && Percent <= 100;
	}

	//! \brief 이번 잡담을 앞 잡담에 뭉쳐 붙일지 판정한다.
	//! \param bJustPlayed 방금 잡담이 재생됐는지다. 아니면 뭉치지 않는다.
	//! \param ClusterChancePercent 뭉칠 확률(백분율)이다.
	//! \param Roll 1~100 사이의 추첨값이다.
	//! \return 뭉쳐야 하면 true다.
	inline bool ShouldClusterSmallTalk(
		const bool bJustPlayed,
		const int32 ClusterChancePercent,
		const int32 Roll)
	{
		return bJustPlayed && ClusterChancePercent > 0 && Roll <= ClusterChancePercent;
	}

	//! \brief 잡담 간격으로 쓸 수 있는 값인지 판정한다.
	inline bool IsIntervalRowValid(const FMyStreamingTuningRow& Row)
	{
		return FMath::IsFinite(Row.MinSeconds)
			&& FMath::IsFinite(Row.MaxSeconds)
			&& Row.MinSeconds >= 0.0f
			&& Row.MaxSeconds >= Row.MinSeconds
			&& Row.Weight >= 0;
	}
}
