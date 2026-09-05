////////////////////////////
//! \page MyStreamingChatTypes.h
//! \brief Streaming Chat 메시지 채널에서 사용하는 표시 데이터 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyStreamingChatTypes.generated.h"

class UTexture2D;

////////////////////////////
//! \enum EMyStreamingGodSource
//! \author 장효제
//! \brief 이 Line이 사용할 God을 어디서 가져오는지 나타낸다.
UENUM(BlueprintType)
enum class EMyStreamingGodSource : uint8
{
	Line,		// CSV의 GodTag를 사용한다.
	Payload		// Mission Instance 등 요청 Payload에 저장된 God을 사용한다.
};

////////////////////////////
//! \enum EMyStreamingPresentationType
//! \author 장효제
//! \brief 선택된 Line이 어떤 연출로 표시되는지 나타낸다.
UENUM(BlueprintType)
enum class EMyStreamingPresentationType : uint8
{
	Chat,
	Donation,
	ItemReward,
	ExpReward,
	MissionStart
};

////////////////////////////
//! \enum EMyStreamingActionType
//! \author 장효제
//! \brief 선택된 Line이 서버에 요구하는 상태 변경 Action을 나타낸다.
UENUM(BlueprintType)
enum class EMyStreamingActionType : uint8
{
	None,
	GrantMeso,
	ApplyMesoDelta,
	GrantItem,
	GrantExp,
	StartMission
};

////////////////////////////
//! \enum EMyStreamingRewardSource
//! \author 장효제
//! \brief 후원 금액을 어디서 정하는지 나타낸다.
UENUM(BlueprintType)
enum class EMyStreamingRewardSource : uint8
{
	None,			// 수량 추첨 없음. Item 보상과 일반 Chat이 쓴다.
	RollFromLine,	// Line의 RewardMin/RewardMax 사이에서 서버가 뽑는다.
	Payload			// Mission Instance에 이미 저장된 수량을 사용한다.
};

////////////////////////////
//! \enum EMyStreamingPresentationTier
//! \author 장효제
//! \brief 후원 연출 등급이다. Auto는 확정 금액으로 자동 산정하고 나머지는 기획자가 강제한다.
UENUM(BlueprintType)
enum class EMyStreamingPresentationTier : uint8
{
	None,
	Auto,
	Small,
	Medium,
	Large
};

////////////////////////////
//! \struct FMyStreamingChatMessageData
//! \author 장효제
//! \brief 스트리밍 채팅 말풍선에 표시할 신 이름, 대사, 아이콘, 태그 정보를 담는 데이터다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingChatMessageData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat")
	FText GodName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat")
	FLinearColor GodNameColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat")
	TObjectPtr<UTexture2D> GodIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat", meta = (Categories = "God"))
	FGameplayTag GodTag;

	//! \author 장효제
	//! \brief 이 채팅 메시지를 만든 원본 스트리밍 EventTag다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat", meta = (Categories = "Streaming.Event,Combat"))
	FGameplayTag SourceEventTag;

	//! \author 장효제
	//! \brief [D-5C] 이 버블의 표시 종류다. UI가 문자열 추론 없이 이 값으로만 배경 Brush를 고른다.
	//! \note 기본값은 Chat이며, 일반 Chat 경로는 이 기본값을 그대로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat")
	EMyStreamingPresentationType PresentationType = EMyStreamingPresentationType::Chat;
};

////////////////////////////
//! \struct FMyStreamingChatLineRow
//! \author 장효제
//! \brief SequenceId로 선택되는 스트리밍 채팅 대사 테이블 행 구조체다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyStreamingChatLineRow : public FTableRowBase
{
	GENERATED_BODY()


	// 어느 시퀀스인가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	FName SequenceId;

	// 시퀀스의 몇 번째 Step인가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (ClampMin = "1"))
	int32 StepOrder = 10;

	// 이전 Step이 재생된 후 몇 초 뒤에 이 Step을 재생하는가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (ClampMin = "0.0"))
	float DelayFromPreviousStepSeconds = 0.0f;

	// 누가 무엇을 말하는가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (Categories = "God"))
	FGameplayTag GodTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (MultiLine = "true"))
	FText MessageText;

	//후보 선택과 활성 상태
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Weight = 1;

	//--- DM-3A: Presentation / Action / Reward 계약 ---

	//! \brief 이 Line이 사용할 God의 출처다. 기존 Chat Line은 Line 기본값을 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	EMyStreamingGodSource GodSource = EMyStreamingGodSource::Line;

	//! \brief 이 Line이 어떤 연출로 표시되는가.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	EMyStreamingPresentationType PresentationType = EMyStreamingPresentationType::Chat;

	//! \brief 이 Line이 서버에 요구하는 상태 변경 Action이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	EMyStreamingActionType ActionType = EMyStreamingActionType::None;

	//! \brief MissionStart 연출이 참조하는 미션 종류 태그다. 일반 Chat/후원은 비운다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (Categories = "Mission"))
	FGameplayTag MissionTag;

	//! \brief 보상 수량의 출처다. Meso와 Exp가 공유한다. Item 보상은 None이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	EMyStreamingRewardSource RewardSource = EMyStreamingRewardSource::None;

	//! \brief RollFromLine일 때 서버가 뽑는 보상 수량의 최솟값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (ClampMin = "0"))
	int32 RewardMin = 0;

	//! \brief RollFromLine일 때 서버가 뽑는 보상 수량의 최댓값이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine", meta = (ClampMin = "0"))
	int32 RewardMax = 0;

	//! \brief ItemReward 연출이 지급할 DT_ItemTable의 RowName이다. 다른 보상은 비운다.
	//! \details 지급 개수는 Meso·Exp와 같은 방식으로 RewardMin~RewardMax에서 뽑는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	FName RewardItemId;

	//! \brief 후원 연출 등급이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|ChatLine")
	EMyStreamingPresentationTier PresentationTier = EMyStreamingPresentationTier::None;

};
