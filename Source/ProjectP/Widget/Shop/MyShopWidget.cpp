////////////////////////////
//! \page MyShopWidget.cpp
//! \brief 상점 창 위젯 구현 파일이다.
#include "Widget/Shop/MyShopWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Item/MyInventoryComponent.h"
#include "MyGameplayTags.h"
#include "Player/Components/PlayerInteractionComponent.h"
#include "Shop/MyShopActor.h"
#include "TimerManager.h"
#include "Widget/Inventory/MyItemTooltipWidget.h"
#include "Widget/MyUIManagerSubsystem.h"
#include "Widget/Shop/MyShopSlotWidget.h"

////////////////////////////
//! \author 준혁
//! \brief 키 입력(F/ESC 닫기)을 받을 수 있도록 위젯을 포커스 가능하게 설정한다.
UMyShopWidget::UMyShopWidget()
{
	SetIsFocusable(true);
}

////////////////////////////
//! \author 준혁
//! \brief 구매 요청 대상 상점 액터를 지정한다.
//! \param InShopActor 이 창을 연 상점 액터
void UMyShopWidget::InitShop(AMyShopActor* InShopActor)
{
	ShopActor = InShopActor;
}

////////////////////////////
//! \author 준혁
//! \brief 상호작용 종료 통지에 의한 닫기. 종료 재요청 없이 창만 닫는다.
void UMyShopWidget::CloseFromInteraction()
{
	// 창 닫기(ESC 등)로 시작된 상호작용 종료가 다시 이 함수를 부르는 경우가 있다.
	// 이미 닫힌 위젯에 플래그를 남기면 풀링 재사용 시 다음 닫기의 종료 요청이 생략되므로 무시한다.
	if (!IsActivated())
	{
		return;
	}

	bClosedFromInteraction = true;
	DeactivateWidget();
}

////////////////////////////
//! \author 준혁
//! \brief 활성화 시 버튼/인벤토리 델리게이트를 바인딩하고 판매 목록을 그린다.
//! \editor 준혁 - 풀링 재사용 대비 bClosedFromInteraction 리셋 추가
void UMyShopWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

    // CommonUI 스택은 위젯 인스턴스를 풀링 재사용하므로 이전 세션의 닫힘 사유를 초기화한다.
    bClosedFromInteraction = false;
    HideNotice();

	if (BTN_Purchase)
	{
		BTN_Purchase->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleBuyClicked);
	}

	if (BTN_QuantityMinusOne)
	{
		BTN_QuantityMinusOne->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleQuantityMinusOneClicked);
	}

	if (BTN_QuantityPlusOne)
	{
		BTN_QuantityPlusOne->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleQuantityPlusOneClicked);
	}

	if (BTN_QuantityMinusTen)
	{
		BTN_QuantityMinusTen->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleQuantityMinusTenClicked);
	}

	if (BTN_QuantityPlusTen)
	{
		BTN_QuantityPlusTen->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleQuantityPlusTenClicked);
	}

	if (BTN_Close)
	{
		BTN_Close->OnClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleCloseClicked);
	}

	if (!BindToInventoryComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop UI] InventoryComponent not found on owning PlayerState."));
	}

	RefreshShopGrid();
	RefreshMesoText();
	ClearSelection();

	// InputMode=Menu에서 게임 입력이 차단되므로, 닫기 키(F/ESC)를 받으려면 이 위젯이 포커스를 가져야 한다.
	SetFocus();
}

////////////////////////////
//! \author 준혁
//! \brief CommonUI가 활성화 시 포커스를 줄 대상으로 이 위젯 자신을 반환한다. (닫기 키 수신용)
//! \return 포커스 대상 위젯
UWidget* UMyShopWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMyShopWidget*>(this);
}

////////////////////////////
//! \author 준혁
//! \brief 닫기 키(CloseKeys, 기본 F/ESC) 입력 시 상점을 닫는다.
//!        버튼 등 자식 위젯이 포커스를 가져도 키 이벤트가 버블링으로 올라와 여기서 처리된다.
//! \param InGeometry 위젯 지오메트리
//! \param InKeyEvent 키 이벤트
//! \return 닫기 키면 Handled
FReply UMyShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && CloseKeys.Contains(InKeyEvent.GetKey()))
	{
		// 비활성화 처리(NativeOnDeactivated)에서 서버에 상호작용 종료를 요청한다.
		DeactivateWidget();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 비활성화 시 델리게이트를 해제하고, 상호작용 종료 통지에 의한 닫기가 아니면 서버에 종료를 요청한다.
//!        (닫기 버튼/ESC로 닫아도 서버의 상점 상호작용 상태가 정리되게 한다)
void UMyShopWidget::NativeOnDeactivated()
{
	UnbindFromInventoryComponent();

    HideNotice();

	if (BTN_Purchase)
	{
		BTN_Purchase->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleBuyClicked);
	}

	if (BTN_QuantityMinusOne)
	{
		BTN_QuantityMinusOne->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleQuantityMinusOneClicked);
	}

	if (BTN_QuantityPlusOne)
	{
		BTN_QuantityPlusOne->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleQuantityPlusOneClicked);
	}

	if (BTN_QuantityMinusTen)
	{
		BTN_QuantityMinusTen->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleQuantityMinusTenClicked);
	}

	if (BTN_QuantityPlusTen)
	{
		BTN_QuantityPlusTen->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleQuantityPlusTenClicked);
	}

	if (BTN_Close)
	{
		BTN_Close->OnClicked.RemoveDynamic(this, &UMyShopWidget::HandleCloseClicked);
	}

	RestoreHUDLayer();

	if (!bClosedFromInteraction)
	{
		RequestEndInteraction();
	}

	Super::NativeOnDeactivated();
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 변경 알림 처리. 선택 아이템의 보유 수량 표시와 수량 클램프를 갱신한다.
//!        (구매 성공으로 보유량이 늘면 스택 상한이 가까워져 수량 상한이 줄 수 있다)
void UMyShopWidget::HandleInventoryUpdated()
{
	if (SelectedItemId.IsNone() || !BoundInventoryComponent)
	{
		return;
	}

	if (WBP_Tooltip)
	{
		WBP_Tooltip->ShowItem(SelectedItemId, SelectedItemData, BoundInventoryComponent->GetItemCount(SelectedItemId));
	}

	SetQuantity(Quantity);
}

////////////////////////////
//! \author 준혁
//! \brief 메소 변경 알림 처리. 메소 텍스트와 구매 버튼 활성 상태를 갱신한다.
//! \param NewMeso 변경된 메소량
void UMyShopWidget::HandleMesoChanged(int32 NewMeso)
{
	if (TXT_Meso)
	{
		TXT_Meso->SetText(FText::AsNumber(NewMeso));
	}

	UpdatePurchasePanel();
}

////////////////////////////
//! \author 준혁
//! \brief 구매 요청의 서버 처리 결과 알림 처리. 안내 문구와 결과 몽타주를 로컬에서 재생한다.
//! \param Result 서버 처리 결과
//! \param ItemId 구매 요청했던 아이템 ID
//! \param Count 구매 요청했던 개수
void UMyShopWidget::HandlePurchaseResult(EMyShopPurchaseResult Result, FName ItemId, int32 Count)
{
	ShowNotice(GetResultNoticeText(Result));

	if (AMyShopActor* ActiveShopActor = ShopActor.Get())
	{
		if (Result == EMyShopPurchaseResult::Success)
		{
			ActiveShopActor->PlayPurchaseMontage();
		}
		else
		{
			ActiveShopActor->PlayPurchaseFailedMontage();
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 판매 아이템 슬롯 클릭 처리. 해당 아이템을 선택한다.
//! \param ItemId 클릭된 아이템 ID
void UMyShopWidget::HandleSlotClicked(FName ItemId)
{
	SelectItem(ItemId);
}

////////////////////////////
//! \author 준혁
//! \brief 구매 버튼 처리. 선택 아이템의 구매를 서버에 요청한다. 결과는 OnPurchaseResult로 통지된다.
void UMyShopWidget::HandleBuyClicked()
{
	if (BoundInventoryComponent && !SelectedItemId.IsNone())
	{
		BoundInventoryComponent->RequestPurchase(ShopActor.Get(), SelectedItemId, Quantity);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 수량 1 감소 버튼 처리.
void UMyShopWidget::HandleQuantityMinusOneClicked()
{
	SetQuantity(Quantity - 1);
}

////////////////////////////
//! \author 준혁
//! \brief 수량 1 증가 버튼 처리.
void UMyShopWidget::HandleQuantityPlusOneClicked()
{
	SetQuantity(Quantity + 1);
}

////////////////////////////
//! \author 준혁
//! \brief 수량 10 감소 버튼 처리. 범위를 벗어나면 클램프된다.
void UMyShopWidget::HandleQuantityMinusTenClicked()
{
	SetQuantity(Quantity - 10);
}

////////////////////////////
//! \author 준혁
//! \brief 수량 10 증가 버튼 처리. 범위를 벗어나면 클램프된다.
void UMyShopWidget::HandleQuantityPlusTenClicked()
{
	SetQuantity(Quantity + 10);
}

////////////////////////////
//! \author 준혁
//! \brief 닫기 버튼 처리. 위젯을 비활성화하고, 비활성화 처리에서 상호작용 종료가 요청된다.
void UMyShopWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

////////////////////////////
//! \author 준혁
//! \brief 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 변경/구매 결과 델리게이트를 구독한다.
//! \return 바인딩 성공 여부
bool UMyShopWidget::BindToInventoryComponent()
{
	if (BoundInventoryComponent)
	{
		return true;
	}

	const APlayerController* OwningPC = GetOwningPlayer();
	const APlayerState* OwningPS = OwningPC ? OwningPC->PlayerState : nullptr;
	UMyInventoryComponent* InventoryComponent = OwningPS ? OwningPS->FindComponentByClass<UMyInventoryComponent>() : nullptr;
	if (!InventoryComponent)
	{
		return false;
	}

	BoundInventoryComponent = InventoryComponent;
	BoundInventoryComponent->OnInventoryUpdated.AddUniqueDynamic(this, &UMyShopWidget::HandleInventoryUpdated);
	BoundInventoryComponent->OnMesoChanged.AddUniqueDynamic(this, &UMyShopWidget::HandleMesoChanged);
	BoundInventoryComponent->OnPurchaseResult.AddUniqueDynamic(this, &UMyShopWidget::HandlePurchaseResult);
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 컴포넌트 델리게이트 구독을 해제한다.
void UMyShopWidget::UnbindFromInventoryComponent()
{
	if (!BoundInventoryComponent)
	{
		return;
	}

	BoundInventoryComponent->OnInventoryUpdated.RemoveDynamic(this, &UMyShopWidget::HandleInventoryUpdated);
	BoundInventoryComponent->OnMesoChanged.RemoveDynamic(this, &UMyShopWidget::HandleMesoChanged);
	BoundInventoryComponent->OnPurchaseResult.RemoveDynamic(this, &UMyShopWidget::HandlePurchaseResult);
	BoundInventoryComponent = nullptr;
}

////////////////////////////
//! \author 준혁
//! \brief 데이터테이블에서 판매 아이템(BuyPrice > 0) ID 목록을 만든다. 테이블 Row 순서를 유지한다.
//! \param OutItemIds 판매 아이템 ID 목록(출력)
void UMyShopWidget::BuildShopItemIds(TArray<FName>& OutItemIds) const
{
	OutItemIds.Reset();

	if (!BoundInventoryComponent)
	{
		return;
	}

	TArray<FName> AllItemIds;
	BoundInventoryComponent->GetAllItemIds(AllItemIds);

	for (const FName& ItemId : AllItemIds)
	{
		FMyItemData ItemData;
		if (BoundInventoryComponent->FindItemData(ItemId, ItemData) && ItemData.BuyPrice > 0)
		{
			OutItemIds.Add(ItemId);
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 판매 아이템 슬롯 그리드를 다시 그린다.
//!        그리드는 NumGridColumns 열 고정이고, 최소 MinGridRows 행이 되도록 빈 슬롯으로 채운다.
void UMyShopWidget::RefreshShopGrid()
{
	if (!GRD_ItemBox)
	{
		return;
	}

	GRD_ItemBox->ClearChildren();
	SlotWidgets.Reset();
    GRD_ItemBox->SetSlotPadding(GridSlotPadding);

	if (!BoundInventoryComponent || !SlotWidgetClass)
	{
		return;
	}

	TArray<FName> ShopItemIds;
	BuildShopItemIds(ShopItemIds);

	// 최소 행 수를 보장하고, 아이템이 그보다 많으면 행 단위로 올림해 빈 칸 없이 채운다 (인벤토리 창과 동일)
	const int32 Columns = FMath::Max(1, NumGridColumns);
	const int32 MinCells = Columns * FMath::Max(1, MinGridRows);
	const int32 NumCells = FMath::Max(FMath::DivideAndRoundUp(ShopItemIds.Num(), Columns) * Columns, MinCells);

	for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
	{
		UMyShopSlotWidget* SlotWidget = CreateWidget<UMyShopSlotWidget>(this, SlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		FMyItemData ItemData;
		if (ShopItemIds.IsValidIndex(CellIndex)
			&& BoundInventoryComponent->FindItemData(ShopItemIds[CellIndex], ItemData))
		{
			SlotWidget->SetItem(ShopItemIds[CellIndex], ItemData);
		}
		else
		{
			SlotWidget->SetEmpty();
		}

		SlotWidget->OnSlotClicked.AddUniqueDynamic(this, &UMyShopWidget::HandleSlotClicked);
		SlotWidgets.Add(SlotWidget);

		// 셀을 가득 채워 칸 사이 틈을 없앤다. 간격은 그리드의 Slot Padding으로만 제어한다.
        // 위 설명은 정렬값이 Fill일 때의 동작이며, 실제 정렬과 패딩은 WBP_Shop의 BP 디폴트 설정을 사용한다.
		if (UUniformGridSlot* GridSlot = GRD_ItemBox->AddChildToUniformGrid(SlotWidget, CellIndex / Columns, CellIndex % Columns))
		{
            GridSlot->SetHorizontalAlignment(GridSlotHorizontalAlignment);
            GridSlot->SetVerticalAlignment(GridSlotVerticalAlignment);
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 메소 텍스트를 현재 보유량으로 갱신한다.
void UMyShopWidget::RefreshMesoText()
{
	if (TXT_Meso)
	{
		const int32 CurrentMeso = BoundInventoryComponent ? BoundInventoryComponent->GetMeso() : 0;
		TXT_Meso->SetText(FText::AsNumber(CurrentMeso));
	}
}

////////////////////////////
//! \author 준혁
//! \brief 아이템을 선택 상태로 만든다. 툴팁에 보유 수량과 함께 표시하고 수량을 1로 초기화한다.
//! \param InItemId 선택할 아이템 ID
void UMyShopWidget::SelectItem(FName InItemId)
{
	if (!BoundInventoryComponent)
	{
		return;
	}

	FMyItemData ItemData;
	if (!BoundInventoryComponent->FindItemData(InItemId, ItemData))
	{
		ClearSelection();
		return;
	}

	SelectedItemId = InItemId;
	SelectedItemData = ItemData;

	if (WBP_Tooltip)
	{
		WBP_Tooltip->ShowItem(InItemId, ItemData, BoundInventoryComponent->GetItemCount(InItemId));
		WBP_Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

    for (UMyShopSlotWidget* SlotWidget : SlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(SlotWidget->GetItemId() == SelectedItemId);
		}
	}

	SetQuantity(1);
}

////////////////////////////
//! \author 준혁
//! \brief 선택을 해제하고 툴팁을 빈 상태로 되돌리며 구매 패널을 비활성화한다.
void UMyShopWidget::ClearSelection()
{
	SelectedItemId = NAME_None;
	SelectedItemData = FMyItemData();
	Quantity = 1;

	if (WBP_Tooltip)
	{
		WBP_Tooltip->ShowEmpty();
		WBP_Tooltip->SetVisibility(ESlateVisibility::Hidden);
	}

    for (UMyShopSlotWidget* SlotWidget : SlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSelected(false);
		}
	}

	UpdatePurchasePanel();
}

////////////////////////////
//! \author 준혁
//! \brief 수량을 구매 가능 범위(1 ~ 스택 여유분)로 클램프해 설정하고 표시를 갱신한다.
//! \param NewQuantity 설정할 수량
void UMyShopWidget::SetQuantity(int32 NewQuantity)
{
	// 받을 수 있는 개수가 0이어도 표시는 1을 유지한다. 구매 요청 후 서버가 실패 결과를 판정한다.
	const int32 MaxQuantity = FMath::Max(1, GetMaxPurchasableCount());
	Quantity = FMath::Clamp(NewQuantity, 1, MaxQuantity);

	UpdatePurchasePanel();
}

////////////////////////////
//! \author 준혁
//! \brief 선택 아이템을 지금 최대 몇 개까지 받을 수 있는지 반환한다.
//!        보유 중이면 스택 상한까지의 여유분, 새 종류면 빈 칸이 있을 때 스택 상한만큼이다. (서버 검증과 동일 기준)
//! \return 받을 수 있는 최대 개수 (선택이 없으면 0)
int32 UMyShopWidget::GetMaxPurchasableCount() const
{
	if (SelectedItemId.IsNone() || !BoundInventoryComponent)
	{
		return 0;
	}

	const int32 OwnedCount = BoundInventoryComponent->GetItemCount(SelectedItemId);
	if (OwnedCount > 0)
	{
		return FMath::Max(0, SelectedItemData.MaxStackCount - OwnedCount);
	}

	return BoundInventoryComponent->IsInventoryFull() ? 0 : SelectedItemData.MaxStackCount;
}

////////////////////////////
//! \author 준혁
//! \brief 수량/합계 표시와 구매 버튼 활성 상태를 갱신한다.
//!        구매 버튼은 아이템이 선택되어 구매 영역이 표시되는 동안 항상 활성화한다.
void UMyShopWidget::UpdatePurchasePanel()
{
	const bool bHasSelection = !SelectedItemId.IsNone();
	const int32 MaxPurchasable = GetMaxPurchasableCount();

	// int32 곱 오버플로 방지를 위해 합계는 64비트로 계산한다
	const int64 TotalPrice = bHasSelection ? static_cast<int64>(SelectedItemData.BuyPrice) * Quantity : 0;

	if (TXT_Quantity)
	{
		TXT_Quantity->SetText(FText::AsNumber(Quantity));
	}

	if (TXT_TotalPrice)
	{
		TXT_TotalPrice->SetText(FText::AsNumber(TotalPrice));
	}

	if (WBP_Tooltip)
	{
		WBP_Tooltip->SetTotalPrice(TotalPrice);
	}

	if (SB_Purchase)
	{
		SB_Purchase->SetVisibility(bHasSelection
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Hidden);
	}

	if (BTN_Purchase)
	{
		BTN_Purchase->SetIsEnabled(bHasSelection);
	}

	const bool bCanDecrease = bHasSelection && Quantity > 1;
	const bool bCanIncrease = bHasSelection && Quantity < MaxPurchasable;

	if (BTN_QuantityMinusOne)
	{
		BTN_QuantityMinusOne->SetIsEnabled(bCanDecrease);
	}

	if (BTN_QuantityPlusOne)
	{
		BTN_QuantityPlusOne->SetIsEnabled(bCanIncrease);
	}

	if (BTN_QuantityMinusTen)
	{
		BTN_QuantityMinusTen->SetIsEnabled(bCanDecrease);
	}

	if (BTN_QuantityPlusTen)
	{
		BTN_QuantityPlusTen->SetIsEnabled(bCanIncrease);
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 상점 시작 시 숨겼던 HUD 레이어의 표시와 CommonUI 활성 상태를 복원하는 함수
void UMyShopWidget::RestoreHUDLayer()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (UIManager)
	{
		UIManager->SetLayerVisible(MyGameplayTags::UI_Layer_HUD, true);
		UIManager->SetLayerActive(MyGameplayTags::UI_Layer_HUD, true);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 서버에 상호작용 종료를 요청한다. 창이 상호작용 종료 통지 없이(닫기 버튼, ESC 등) 닫힐 때 호출된다.
//!        서버가 종료를 확정하면 상점 액터의 상호작용자 목록에서도 제거된다.
void UMyShopWidget::RequestEndInteraction()
{
	const APawn* OwningPawn = GetOwningPlayerPawn();
	UPlayerInteractionComponent* InteractionComponent = OwningPawn ? OwningPawn->FindComponentByClass<UPlayerInteractionComponent>() : nullptr;

	// TryInteract는 진행 중인 상호작용이 있으면 종료를 요청하는 토글이다. 진행 중일 때만 호출해 새 시작을 막는다.
	if (InteractionComponent && InteractionComponent->IsInteracting())
	{
		InteractionComponent->TryInteract();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 구매 결과 토스트를 표시하고 NoticeDuration 후 자동으로 숨긴다.
//!        토스트는 클릭을 막지 않도록 HitTestInvisible로 띄운다.
//! \param NoticeText 표시할 안내 문구
void UMyShopWidget::ShowNotice(const FText& NoticeText)
{
	if (TXT_Notice)
	{
		TXT_Notice->SetText(NoticeText);
	}

	// 배경 박스가 있는 구성(BDR_Toast)이면 컨테이너 단위로, 없으면 텍스트 단위로 표시한다.
	if (UWidget* NoticeWidget = BDR_Toast ? BDR_Toast.Get() : Cast<UWidget>(TXT_Notice))
	{
		NoticeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(NoticeTimerHandle, this, &UMyShopWidget::HideNotice, FMath::Max(NoticeDuration, 0.1f), false);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 구매 결과 토스트를 즉시 숨기고 자동 숨김 타이머를 정리한다.
void UMyShopWidget::HideNotice()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoticeTimerHandle);
	}

	if (UWidget* NoticeWidget = BDR_Toast ? BDR_Toast.Get() : Cast<UWidget>(TXT_Notice))
	{
		NoticeWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 구매 결과를 안내 문구로 변환한다.
//! \param Result 서버 처리 결과
//! \return 표시할 안내 문구
FText UMyShopWidget::GetResultNoticeText(EMyShopPurchaseResult Result) const
{
	switch (Result)
	{
	case EMyShopPurchaseResult::Success:
		return NSLOCTEXT("MyShop", "Purchase_Success", "고맙누비.");
	case EMyShopPurchaseResult::NotEnoughMeso:
		return NotEnoughMesoNoticeText;
	case EMyShopPurchaseResult::InventoryFull:
		return InventoryFullNoticeText;
	case EMyShopPurchaseResult::NotSold:
		return NotSoldNoticeText;
	default:
		return PurchaseFailedNoticeText;
	}
}
