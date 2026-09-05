////////////////////////////
//! \page MyGodListEntryWidget.h
//! \brief God Page 우측 목록에서 사용하는 신 항목 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "God/MyGodPresentationTypes.h"
#include "GameplayTagContainer.h"
#include "MyGodListEntryWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMyGodEntrySelectedSignature, FGameplayTag, GodTag);

////////////////////////////
//! \class UMyGodListEntryWidget
//! \author 장효제
//! \brief 신의 작은 초상화와 이름을 표시하고 선택된 GodTag를 상위 페이지에 전달한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyGodListEntryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! \author 장효제
	//! \brief 목록 항목에 신 표현 데이터를 적용한다.
	void InitializeEntry(const FMyGodPresentationRow& Presentation, EMyGodPortraitStage PortraitStage);

	//! \author 장효제
	//! \brief 현재 선택 항목 여부를 버튼과 강조 표시에 반영한다.
	void SetSelected(bool bSelected);

	FGameplayTag GetGodTag() const { return GodTag; }

	UPROPERTY(BlueprintAssignable, Category = "UI|GodPage")
	FMyGodEntrySelectedSignature OnGodEntrySelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSelectClicked();

	void ApplyPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_Select;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_GodPortrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> TXT_GodName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> IMG_SelectedFrame;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GodPage|Selection", meta = (AllowPrivateAccess = "true"))
	FLinearColor UnselectedTextColor = FLinearColor::White;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|GodPage", meta = (AllowPrivateAccess = "true", Categories = "God"))
	FGameplayTag GodTag;

	UPROPERTY(Transient)
	FMyGodPresentationRow PresentationData;

	EMyGodPortraitStage CurrentPortraitStage = EMyGodPortraitStage::Full;
};
