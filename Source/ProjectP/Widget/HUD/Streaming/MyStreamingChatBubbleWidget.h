////////////////////////////
//! \page MyStreamingChatBubbleWidget.h
//! \brief Streaming Chat 말풍선 HUD 부품 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "Streaming/MyStreamingChatTypes.h"
#include "MyStreamingChatBubbleWidget.generated.h"

class UCommonTextBlock;
class UButton;
class UImage;
class URichTextBlock;
class UTexture2D;

////////////////////////////
//! \class UMyStreamingChatBubbleWidget
//! \author 장효제
//! \brief 클릭 가능한 신 이름과 대사 본문을 표시하는 스트리밍 채팅 말풍선 위젯이다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyStreamingChatBubbleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	////////////////////////////
	//! \author 장효제
	//! \brief 스트리밍 채팅 메시지 데이터를 말풍선 위젯에 적용한다.
	//! \param InMessageData 표시할 스트리밍 채팅 메시지 데이터
	UFUNCTION(BlueprintCallable, Category = "UI|StreamingChat")
	void SetMessage(const FMyStreamingChatMessageData& InMessageData);

	UFUNCTION(BlueprintPure, Category = "UI|StreamingChat")
	FMyStreamingChatMessageData GetMessageData() const { return CachedMessageData; }

protected:
	virtual void NativeOnInitialized() override;

private:
	void ApplyMessage();
	//! \brief [D-5C] CachedMessageData.PresentationType에 따라 배경 Brush의 이미지를 고른다(9-slice 설정은 보존).
	void ApplyBackgroundForPresentation();

	UFUNCTION()
	void HandleGodNameClicked();

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_GodName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_GodName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<URichTextBlock> TXT_Message;

	//! \brief [D-5C] 말풍선 배경 Image. WBP에서 이 이름으로 바인딩한다(선택적). 없으면 배경 교체를 건너뛴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_Background;

	//! \brief [D-5C] 일반 Chat 배경 텍스처. 미지정 시 WBP 기본 Brush를 그대로 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DefaultBubbleTexture;

	//! \brief [D-5C] Donation 배경 텍스처. Draw As/Margin 등은 배경 Image의 Brush 설정을 그대로 재사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|StreamingChat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DonationBubbleTexture;

	//! \brief MissionStart 버블에 사용할 미션 제안 전용 배경 텍스처다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|StreamingChat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> MissionBubbleTexture;

	UPROPERTY(Transient)
	FMyStreamingChatMessageData CachedMessageData;
};
