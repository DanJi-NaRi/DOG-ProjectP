// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_MessengerMessageBlock.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 메시지 데이터를 받아 "[hh:mm] name(Char) : body" 형태의 한 줄로 표시한다.
// 채널별 폰트 색을 적용한다. (전체=AllChannelColor, 파티=PartyChannelColor)
// InMessage : 표시할 메시지 데이터
void UCPP_MessengerMessageBlock::SetMessage(const FMessengerMessage& InMessage)
{
    if (!TXT_Message)
    {
        return;
    }

    TXT_Message->SetText(FText::FromString(InMessage.ToDisplayString()));
    TXT_Message->SetAutoWrapText(true);

    // 채널별 폰트 색: 전체/파티 모두 WBP 클래스 디폴트에서 지정한 색
    const FLinearColor TextColor = (InMessage.Channel == EMessengerChannel::Party)
        ? PartyChannelColor
        : AllChannelColor;
    TXT_Message->SetColorAndOpacity(FSlateColor(TextColor));
}

////////////////////////////
//! \author 장효제
//! \brief UI 배치 검증용 채팅 블록의 높이만 유지하고 표시 문구는 비운다.
void UCPP_MessengerMessageBlock::SetEmptyTestMessage()
{
    if (!TXT_Message)
    {
        return;
    }

    TXT_Message->SetText(FText::FromString(TEXT(" ")));
    TXT_Message->SetAutoWrapText(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅 메시지 텍스트의 폰트 크기를 변경하는 함수
// InFontSize : 적용할 폰트 크기
void UCPP_MessengerMessageBlock::SetMessageFontSize(int32 InFontSize)
{
    if (!TXT_Message)
    {
        return;
    }

    FSlateFontInfo MessageFont = TXT_Message->GetFont();
    MessageFont.Size = FMath::Max(1, InFontSize);
    TXT_Message->SetFont(MessageFont);
}
