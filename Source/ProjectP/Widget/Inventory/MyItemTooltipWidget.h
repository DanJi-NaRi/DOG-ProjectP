////////////////////////////
//! \page MyItemTooltipWidget.h
//! \brief 선택한 아이템의 이름/설명/타입/개수를 보여주는 정보 표시 전용 툴팁 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "Item/MyItemTypes.h"
#include "MyItemTooltipWidget.generated.h"

class UImage;
class UTextBlock;

////////////////////////////
//! \class UMyItemTooltipWidget
//! \brief 선택한 아이템의 상세 정보를 표시한다. 사용/키등록 등 조작 버튼은 인벤토리 창(UMyInventoryWidget)이 담당한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyItemTooltipWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! 툴팁에 아이템 정보를 채운다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ShowItem(FName InItemId, const FMyItemData& InItemData, int32 InCount);

	//! 빈 상태(아이템 미선택)로 표시한다. 툴팁 박스 자체는 항상 떠 있다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ShowEmpty();

	//! 상점에서 선택한 아이템의 수량을 반영한 총 가격을 표시한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Shop")
	void SetTotalPrice(int64 InTotalPrice);

	UFUNCTION(BlueprintPure, Category = "UI|Inventory")
	FName GetShownItemId() const { return ShownItemId; }

protected:
	virtual void NativeConstruct() override;

private:
	//! 아이템 타입 enum을 표시용 텍스트로 변환한다.
	static FText GetItemTypeDisplayText(EMyItemType ItemType);

	//! 아이템 미선택 상태에서 이름 자리에 표시할 안내 문구
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory")
	FText EmptyStateText = NSLOCTEXT("MyItem", "Tooltip_Empty", "아이템을 선택하세요");

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Type;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Count;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Icon;

	//! 상점 전용 총 가격 텍스트. 일반 아이템 툴팁에는 없어도 된다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Price;

	FName ShownItemId = NAME_None;
};
