////////////////////////////
//! \page MyShopSlotWidget.h
//! \brief 상점 창의 판매 아이템 한 칸(아이콘 + 이름 + 가격)을 표시하는 슬롯 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "Item/MyItemTypes.h"
#include "Styling/SlateBrush.h"
#include "MyShopSlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopSlotClickedSignature, FName, ItemId);

////////////////////////////
//! \class UMyShopSlotWidget
//! \brief 판매 아이템의 아이콘/이름/가격을 표시하고, 클릭 시 아이템 ID를 브로드캐스트한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyShopSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! 슬롯에 표시할 판매 아이템을 설정한다. 가격은 ItemData의 BuyPrice를 사용한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void SetItem(FName InItemId, const FMyItemData& InItemData);

	//! 빈 슬롯 상태로 만든다. (표시 숨김, 클릭해도 반응 없음)
	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void SetEmpty();

	//! 선택 상태를 갱신한다. 하이라이트 연출은 BP_OnSelectionChanged로 WBP에서 구현한다.
	//! 기본 버튼 이미지는 C++에서 선택 중 Normal/Hovered를 Pressed(Clicked) 브러시로 유지하고, 추가 연출만 BP 이벤트에 맡긴다.
	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "UI|Shop")
	FName GetItemId() const { return ItemId; }

	//! 슬롯 클릭 시 알림 (상점 창이 선택/툴팁 표시에 사용)
	UPROPERTY(BlueprintAssignable, Category = "UI|Shop")
	FShopSlotClickedSignature OnSlotClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//! 선택 상태가 바뀔 때 호출된다. 테두리 하이라이트 등 연출을 WBP에서 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shop")
	void BP_OnSelectionChanged(bool bNewSelected);

private:
	void ApplySelectionStyle();

	UFUNCTION()
	void HandleSlotButtonClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Slot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Price;

	//! 가격 옆 메소(코인) 아이콘. 빈 슬롯에서 함께 숨기기 위해 바인딩한다. (WBP에 없으면 생략된다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Price;

	FName ItemId = NAME_None;

	//! 선택 해제 시 복원할 BTN_Slot의 원래 Normal/Hovered 브러시
	FSlateBrush DefaultNormalBrush;

	FSlateBrush DefaultHoveredBrush;

	bool bHasCachedDefaultBrushes = false;

	bool bSelected = false;
};
