// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MessengerTypes.h"
#include "CPP_MessengerMessageBlock.generated.h"

class UTextBlock;

/**
 * 메신저 메시지 한 줄을 표시하는 블록 위젯.
 * WBP_MessengerMessageBlock 의 부모 클래스로 사용한다.
 */
UCLASS()
class PROJECTP_API UCPP_MessengerMessageBlock : public UUserWidget
{
    GENERATED_BODY()

public:
    // 메시지 데이터를 받아 한 줄 텍스트로 표시한다.
    void SetMessage(const FMessengerMessage& InMessage);

    void SetEmptyTestMessage();

    void SetMessageFontSize(int32 InFontSize);

protected:
    // 전체 채널 메시지의 폰트 색. (기획이 WBP_MessengerMessageBlock 디폴트에서 지정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Messenger")
    FLinearColor AllChannelColor = FLinearColor::White;

    // 파티 채널 메시지의 폰트 색. (기획이 WBP_MessengerMessageBlock 디폴트에서 지정)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Messenger")
    FLinearColor PartyChannelColor = FLinearColor(0.35f, 0.65f, 1.0f, 1.0f);

    // WBP에서 동일 이름(TXT_Message)의 TextBlock을 배치해야 한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Message;
};
