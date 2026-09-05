////////////////////////////
//! \page MyStreamingChatBubbleWidget.cpp
//! \brief Streaming Chat 말풍선 HUD 부품 위젯 구현 파일이다.
#include "MyStreamingChatBubbleWidget.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Engine/Texture2D.h"
#include "MyPlayerController.h"
#include "Widget/RichText/MyRichTextDecorators.h"

void UMyStreamingChatBubbleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BTN_GodName)
	{
		BTN_GodName->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGodNameClicked);
	}
	MyRichText::ConfigureDecorators(TXT_Message);
}

////////////////////////////
//! \author 장효제
//! \brief 스트리밍 채팅 메시지 데이터를 말풍선 위젯에 적용한다.
//! \param InMessageData 표시할 스트리밍 채팅 메시지 데이터
void UMyStreamingChatBubbleWidget::SetMessage(const FMyStreamingChatMessageData& InMessageData)
{
	CachedMessageData = InMessageData;
	ApplyMessage();
}

void UMyStreamingChatBubbleWidget::ApplyMessage()
{
	if (TXT_GodName)
	{
		TXT_GodName->SetText(CachedMessageData.GodName);
		TXT_GodName->SetColorAndOpacity(FSlateColor(CachedMessageData.GodNameColor));
	}

	if (TXT_Message)
	{
		TXT_Message->SetText(CachedMessageData.Message);
	}

	if (BTN_GodName)
	{
		BTN_GodName->SetIsEnabled(CachedMessageData.GodTag.IsValid());
	}

	ApplyBackgroundForPresentation();
}

////////////////////////////
//! \author 장효제
//! \brief [D-5C] PresentationType으로만 배경 이미지를 고른다. 문자열/GodTag/금액 추론을 하지 않는다.
//! \note Brush의 Draw As/Margin/9-slice 설정은 WBP 값을 유지하고 Resource Object(텍스처)만 교체한다.
void UMyStreamingChatBubbleWidget::ApplyBackgroundForPresentation()
{
	UTexture2D* SelectedTexture = DefaultBubbleTexture;

	switch (CachedMessageData.PresentationType)
	{
	case EMyStreamingPresentationType::MissionStart:
		if (MissionBubbleTexture)
		{
			SelectedTexture = MissionBubbleTexture;
		}
		break;

	case EMyStreamingPresentationType::Donation:
		if (DonationBubbleTexture)
		{
			SelectedTexture = DonationBubbleTexture;
		}
		break;

	default:
		break;
	}

	if (!IMG_Background)
	{
		// 배경 Image가 바인딩되지 않았으면 WBP 기본 배경을 그대로 둔다(회귀 방지).
		return;
	}

	if (!SelectedTexture)
	{
		// 두 텍스처가 모두 없으면 WBP 기본 Brush 유지
		return;
	}

	// 기존 Brush(9-slice/Margin/Draw As 포함)를 복사해 Resource Object만 바꾼다.
	FSlateBrush Brush = IMG_Background->GetBrush();
	Brush.SetResourceObject(SelectedTexture);
	IMG_Background->SetBrush(Brush);
}

void UMyStreamingChatBubbleWidget::HandleGodNameClicked()
{
	if (!CachedMessageData.GodTag.IsValid())
	{
		return;
	}

	if (AMyPlayerController* PlayerController = GetOwningPlayer<AMyPlayerController>())
	{
		PlayerController->OpenGodPage(CachedMessageData.GodTag);
	}
}
