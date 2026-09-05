////////////////////////////
//! \page MyGodDetailPanelWidget.h
//! \brief 선택된 신의 상세 정보와 호감도 상태를 표시하는 패널 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "God/MyGodPresentationTypes.h"
#include "MyGodDetailPanelWidget.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UVerticalBox;

////////////////////////////
//! \class UMyGodDetailPanelWidget
//! \author 장효제
//! \brief 신 초상화, 소개, 호감도, 가호 및 선호 행동을 표시하는 내부 부품 위젯이다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyGodDetailPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! \author 장효제
	//! \brief 선택된 신의 정적 표현 데이터와 초상화 단계를 패널에 적용한다.
	void SetGodPresentation(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage);

	//! \author 장효제
	//! \brief 호감도 시스템이 제공한 레벨과 현재/목표 수치를 표시한다.
	void SetAffinityDisplay(int32 Level, int32 CurrentValue, int32 RequiredValue);

private:
	void RebuildBlessingIds(const TArray<FName>& BlessingIds);
	void RebuildFavoriteActions(const TArray<FText>& FavoriteActions);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_GodPortrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_GodSymbol;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_GodName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_Introduction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> PB_Affinity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_AffinityLevel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_AffinityValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> HB_BlessingSlots;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> VB_FavoriteActions;
};
