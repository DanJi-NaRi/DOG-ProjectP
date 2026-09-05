#pragma once

#include "CoreMinimal.h"
#include "MyNoticeTypes.generated.h"

//! Notice의 시각 표현 종류다. 메시지 내용으로 Donation 여부를 추론하지 않는다.
UENUM(BlueprintType)
enum class EMyNoticePresentationType : uint8
{
    Default UMETA(DisplayName = "Default"),
    Donation UMETA(DisplayName = "Donation"),
};

//! Notice의 단일 RichText 문구와 표시 연출을 전달하는 데이터다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyNoticeData
{
    GENERATED_BODY()

    //! 채팅 버블과 공유할 수 있는 단일 RichText 문구다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Notice")
    FText Message;

    //! 0 이하이면 WBP_Notice에 설정된 기본 표시 시간을 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Notice")
    float DurationSeconds = 0.0f;

    //! BDR_Halo와 전용 애니메이션을 선택할 시각 표현 종류다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Notice")
    EMyNoticePresentationType PresentationType = EMyNoticePresentationType::Default;
};
