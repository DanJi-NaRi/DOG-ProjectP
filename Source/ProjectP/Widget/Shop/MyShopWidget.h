////////////////////////////
//! \page MyShopWidget.h
//! \brief 판매 아이템 목록 표시와 수량 선택 구매를 제공하는 상점 창 위젯 선언 파일이다.
#pragma once

#include "Widget/MyActivatableWidget.h"
#include "Engine/TimerHandle.h"
#include "InputCoreTypes.h"
#include "Item/MyItemTypes.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "MyShopWidget.generated.h"

class AMyShopActor;
class UButton;
class UMyInventoryComponent;
class UMyItemTooltipWidget;
class UMyShopSlotWidget;
class UTextBlock;
class UUniformGridPanel;
class UWidget;

////////////////////////////
//! \class UMyShopWidget
//! \brief Menu 레이어에 푸시되는 상점 창. 아이템 데이터테이블의 BuyPrice > 0인 모든 아이템을 그리드로 표시하고,
//!        슬롯 선택 → 수량 조절 → 구매 버튼으로 서버에 구매를 요청한다.
//! \note 열고 닫기는 상호작용 시스템과 묶여 있다: 상점 액터가 상호작용 시작 통지로 이 창을 푸시하고,
//!       창이 닫히면(닫기 버튼, F/ESC 등) 서버에 상호작용 종료를 요청해 상태를 정리한다.
//!       BP 디폴트에서 InputMode=Menu(이동/스킬 차단)와 SlotWidgetClass를 지정해야 한다.
//!       Menu 모드는 게임 입력을 차단하므로 F/ESC 닫기는 이 위젯이 키보드 포커스를 잡고 직접 처리한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyShopWidget : public UMyActivatableWidget
{
	GENERATED_BODY()

public:
	UMyShopWidget();

	//! 구매 요청 대상 상점 액터를 지정한다. 상점 액터가 이 위젯을 푸시한 직후 호출한다.
	void InitShop(AMyShopActor* InShopActor);

	//! 상호작용 종료 통지에 의한 닫기. 서버에 종료를 재요청하지 않고 창만 닫는다.
	void CloseFromInteraction();

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	//! 활성화 시 키 입력(F/ESC 닫기)을 받을 수 있도록 이 위젯 자신을 포커스 대상으로 지정한다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	//! 닫기 키(CloseKeys) 입력 시 상점을 닫는다.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//! 상점을 닫는 키 목록. InputMode=Menu로 게임 입력이 차단되므로 위젯이 직접 처리한다. (BP 디폴트에서 수정 가능)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop")
	TArray<FKey> CloseKeys = { EKeys::F, EKeys::Escape };

	//! 판매 아이템 슬롯으로 생성할 위젯 클래스 (BP 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop")
	TSubclassOf<UMyShopSlotWidget> SlotWidgetClass;

	//! 아이템 그리드 열 개수 (고정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop", meta = (ClampMin = "1"))
	int32 NumGridColumns = 5;

	//! 항상 표시할 최소 행 개수. 판매 아이템이 이보다 적어도 빈 슬롯으로 채운다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop", meta = (ClampMin = "1"))
	int32 MinGridRows = 2;

    //! 그리드 셀 안에서 WBP_ShopSlot을 배치할 가로 정렬 기준
    UPROPERTY(EditDefaultsOnly, Category = "UI|Shop|Grid")
    TEnumAsByte<EHorizontalAlignment> GridSlotHorizontalAlignment = HAlign_Center;

    //! 그리드 셀 안에서 WBP_ShopSlot을 배치할 세로 정렬 기준
    UPROPERTY(EditDefaultsOnly, Category = "UI|Shop|Grid")
    TEnumAsByte<EVerticalAlignment> GridSlotVerticalAlignment = VAlign_Center;

    //! 각 WBP_ShopSlot의 UniformGridSlot에 적용할 여백
    //! UUniformGridPanel 전체 Slot Padding을 통해 모든 슬롯에 동일하게 적용한다.
    UPROPERTY(EditDefaultsOnly, Category = "UI|Shop|Grid")
    FMargin GridSlotPadding = FMargin(0.0f);

	//! 구매 결과 토스트가 자동으로 사라지기까지의 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Shop", meta = (ClampMin = "0.1"))
	float NoticeDuration = 1.5f;

	//! 메소 부족으로 구매에 실패했을 때 표시할 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shop|Notice")
	FText NotEnoughMesoNoticeText = NSLOCTEXT("MyShop", "Purchase_NotEnoughMeso", "메소가 부족하누비.");

	//! 인벤토리 공간 부족으로 구매에 실패했을 때 표시할 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shop|Notice")
	FText InventoryFullNoticeText = NSLOCTEXT("MyShop", "Purchase_InventoryFull", "인벤토리에 공간이 부족하누비.");

	//! 판매할 수 없는 아이템이라 구매에 실패했을 때 표시할 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shop|Notice")
	FText NotSoldNoticeText = NSLOCTEXT("MyShop", "Purchase_NotSold", "구매할 수 없는 아이템누비.");

	//! 그 밖의 사유로 구매에 실패했을 때 표시할 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shop|Notice")
	FText PurchaseFailedNoticeText = NSLOCTEXT("MyShop", "Purchase_Failed", "구매에 실패했누비.");

private:
	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleMesoChanged(int32 NewMeso);

	UFUNCTION()
	void HandlePurchaseResult(EMyShopPurchaseResult Result, FName ItemId, int32 Count);

	UFUNCTION()
	void HandleSlotClicked(FName ItemId);

	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleQuantityMinusOneClicked();

	UFUNCTION()
	void HandleQuantityPlusOneClicked();

	UFUNCTION()
	void HandleQuantityMinusTenClicked();

	UFUNCTION()
	void HandleQuantityPlusTenClicked();

	UFUNCTION()
	void HandleCloseClicked();

	//! 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 델리게이트를 구독한다.
	bool BindToInventoryComponent();
	void UnbindFromInventoryComponent();

	//! 데이터테이블에서 판매 아이템(BuyPrice > 0) ID 목록을 만든다. 테이블 Row 순서를 유지한다.
	void BuildShopItemIds(TArray<FName>& OutItemIds) const;

	//! 판매 아이템 슬롯 그리드를 다시 그린다. 판매 목록은 정적이므로 활성화 시 한 번만 호출한다.
	void RefreshShopGrid();

	void RefreshMesoText();

	//! 아이템을 선택 상태로 만들고 툴팁/수량/구매 버튼을 갱신한다. 수량은 1로 초기화한다.
	void SelectItem(FName InItemId);
	void ClearSelection();

	//! 수량을 구매 가능 범위로 클램프해 설정하고 표시를 갱신한다.
	void SetQuantity(int32 NewQuantity);

	//! 선택 아이템을 지금 최대 몇 개까지 받을 수 있는지 반환한다. (스택 상한/빈 칸 기준, 메소는 미고려)
	int32 GetMaxPurchasableCount() const;

	//! 수량/합계 표시와 구매 버튼 활성 상태를 갱신한다.
	void UpdatePurchasePanel();

	//! 창이 닫힐 때 서버에 상호작용 종료를 요청해 상점 상호작용 상태를 정리한다.
	void RequestEndInteraction();

	//! 상점 시작 시 숨겼던 HUD 레이어를 복원한다.
	void RestoreHUDLayer();

	//! 구매 결과 토스트를 표시하고 NoticeDuration 후 자동으로 숨긴다.
	void ShowNotice(const FText& NoticeText);

	//! 구매 결과 토스트를 즉시 숨기고 자동 숨김 타이머를 정리한다.
	void HideNotice();

	//! 구매 결과를 안내 문구로 변환한다.
	FText GetResultNoticeText(EMyShopPurchaseResult Result) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UUniformGridPanel> GRD_ItemBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Meso;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Quantity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_QuantityMinusOne;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_QuantityPlusOne;

	//! ±10 수량 버튼 (WBP에 없으면 생략된다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_QuantityMinusTen;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_QuantityPlusTen;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Purchase;

	//! 수량 조절과 구매 버튼을 포함하는 영역. 아이템 선택 전에는 자리만 유지한 채 숨긴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UWidget> SB_Purchase;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UMyItemTooltipWidget> WBP_Tooltip;

	//! 선택 수량 x 가격 합계 표시 (WBP에 없으면 생략된다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_TotalPrice;

	//! 구매 결과 안내 문구 표시 (WBP에 없으면 생략된다)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Notice;

	//! 결과 토스트의 컨테이너(배경 박스 포함). 있으면 표시/숨김을 이 단위로 처리한다. (WBP에 없으면 TXT_Notice 단위로 처리)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UWidget> BDR_Toast;

	//! 구매 요청 대상 상점 액터
	TWeakObjectPtr<AMyShopActor> ShopActor;

	UPROPERTY(Transient)
	TObjectPtr<UMyInventoryComponent> BoundInventoryComponent;

	//! 생성한 슬롯 위젯 목록. 선택 하이라이트 갱신에 사용한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMyShopSlotWidget>> SlotWidgets;

	//! 슬롯 클릭으로 선택된 아이템. 구매 버튼의 대상이다.
	FName SelectedItemId = NAME_None;

	//! 선택 아이템의 정적 데이터 사본. 수량 클램프와 합계 계산에 사용한다.
	FMyItemData SelectedItemData;

	//! 구매 수량 (1 이상, 스택 상한까지)
	int32 Quantity = 1;

	//! 상호작용 종료 통지로 닫히는 중인지 여부. true면 닫힐 때 종료를 재요청하지 않는다.
	bool bClosedFromInteraction = false;

	//! 결과 토스트 자동 숨김 타이머
	FTimerHandle NoticeTimerHandle;
};
