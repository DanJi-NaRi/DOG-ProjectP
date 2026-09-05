////////////////////////////
//! \page MyInventoryWidget.cpp
//! \brief 인벤토리 창 위젯 구현 파일이다.
#include "Widget/Inventory/MyInventoryWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/WidgetAnimation.h"
#include "MyPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Item/MyInventoryComponent.h"
#include "Widget/Inventory/MyInventorySlotWidget.h"
#include "Widget/Inventory/MyItemTooltipWidget.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리가 ESC 입력을 직접 받을 수 있도록 포커스 가능한 위젯으로 설정하는 생성자
UMyInventoryWidget::UMyInventoryWidget()
{
	SetIsFocusable(true);
}

////////////////////////////
//! \author 준혁
//! \brief 활성화 시 버튼/인벤토리 델리게이트를 바인딩하고 진입 슬라이드와 전체 표시를 갱신한다.
void UMyInventoryWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bIsClosing = false;
	HideLegacyActionButtons();
	SetSortOptionsVisible(false);

	if (BTN_Sort)
	{
		BTN_Sort->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSortClicked);
	}

	if (BTN_SortByCount)
	{
		BTN_SortByCount->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSortByCountClicked);
	}

	if (BTN_SortByAcquisition)
	{
		BTN_SortByAcquisition->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSortByAcquisitionClicked);
	}

	if (BTN_SortByName)
	{
		BTN_SortByName->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSortByNameClicked);
	}

	if (BTN_SortByType)
	{
		BTN_SortByType->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSortByTypeClicked);
	}

	if (BTN_Close)
	{
		BTN_Close->OnClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleCloseClicked);
	}

	if (!BindToInventoryComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory UI] InventoryComponent not found on owning PlayerState."));
	}

	HideTooltip();
	RefreshInventory();

	if (InventorySlideAnimation)
	{
		FWidgetAnimationDynamicEvent AnimationFinishedEvent;
		AnimationFinishedEvent.BindDynamic(this, &UMyInventoryWidget::HandleSlideAnimationFinished);
		UnbindFromAnimationFinished(InventorySlideAnimation, AnimationFinishedEvent);
		BindToAnimationFinished(InventorySlideAnimation, AnimationFinishedEvent);
		PlayAnimationForward(InventorySlideAnimation, CalculateSlideAnimationPlaybackSpeed());
	}

	SetFocus();
}

////////////////////////////
//! \author 준혁
//! \brief 비활성화 시 델리게이트를 해제한다.
void UMyInventoryWidget::NativeOnDeactivated()
{
	UnbindFromInventoryComponent();

	if (InventorySlideAnimation)
	{
		FWidgetAnimationDynamicEvent AnimationFinishedEvent;
		AnimationFinishedEvent.BindDynamic(this, &UMyInventoryWidget::HandleSlideAnimationFinished);
		UnbindFromAnimationFinished(InventorySlideAnimation, AnimationFinishedEvent);
	}

	if (BTN_Sort)
	{
		BTN_Sort->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleSortClicked);
	}

	if (BTN_SortByCount)
	{
		BTN_SortByCount->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleSortByCountClicked);
	}

	if (BTN_SortByAcquisition)
	{
		BTN_SortByAcquisition->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleSortByAcquisitionClicked);
	}

	if (BTN_SortByName)
	{
		BTN_SortByName->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleSortByNameClicked);
	}

	if (BTN_SortByType)
	{
		BTN_SortByType->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleSortByTypeClicked);
	}

	if (BTN_Close)
	{
		BTN_Close->OnClicked.RemoveDynamic(this, &UMyInventoryWidget::HandleCloseClicked);
	}

	bIsClosing = false;
	HideTooltip();
	Super::NativeOnDeactivated();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ESC 입력을 받을 포커스 대상으로 인벤토리 자신을 반환하는 함수
// Return Value : 포커스를 받을 인벤토리 위젯
UWidget* UMyInventoryWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMyInventoryWidget*>(this);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리가 열린 상태에서 ESC 입력 시 닫기 애니메이션을 요청하는 함수
// InGeometry : 인벤토리의 현재 지오메트리
// InKeyEvent : 입력된 키 이벤트
// Return Value : ESC 입력을 처리했으면 Handled
FReply UMyInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestCloseInventory();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 저장된 표시 순서에 맞춰 아이템 슬롯 그리드와 메소 표시를 다시 그린다.
//!        그리드는 NumGridColumns 열 고정이고, 최소 MinGridRows 행이 되도록 빈 슬롯으로 채운다.
void UMyInventoryWidget::RefreshInventory()
{
	HideTooltip();
	RefreshMesoText();
	UnbindCooldownTags();

	for (UMyInventorySlotWidget* SlotWidget : ActiveItemSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->ClearCooldown();
		}
	}
	ActiveItemSlotWidgets.Reset();

	if (!GRD_ItemBox)
	{
		return;
	}

	GRD_ItemBox->ClearChildren();

	if (!BoundInventoryComponent || !SlotWidgetClass)
	{
		return;
	}

	SynchronizeDisplayOrder();

	TArray<FMyInventoryEntry> DisplayEntries;
	BuildDisplayEntries(DisplayEntries);

	// 최소 행 수를 보장하고, 아이템이 그보다 많으면 행 단위로 올림해 빈 칸 없이 채운다
	const int32 Columns = FMath::Max(1, NumGridColumns);
	const int32 MinCells = Columns * FMath::Max(1, MinGridRows);
	const int32 NumCells = FMath::Max(FMath::DivideAndRoundUp(DisplayEntries.Num(), Columns) * Columns, MinCells);

	for (int32 CellIndex = 0; CellIndex < NumCells; ++CellIndex)
	{
		UMyInventorySlotWidget* SlotWidget = CreateWidget<UMyInventorySlotWidget>(this, SlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		FMyItemData ItemData;
		if (DisplayEntries.IsValidIndex(CellIndex)
			&& BoundInventoryComponent->FindItemData(DisplayEntries[CellIndex].ItemId, ItemData))
		{
			SlotWidget->SetItem(DisplayEntries[CellIndex].ItemId, ItemData, DisplayEntries[CellIndex].Count);

			const float RemainingCooldown = BoundInventoryComponent->GetItemCooldownRemaining(DisplayEntries[CellIndex].ItemId);
			if (RemainingCooldown > 0.0f)
			{
				SlotWidget->StartCooldownRemaining(RemainingCooldown, ItemData.CooldownSeconds);
			}

			ActiveItemSlotWidgets.Add(SlotWidget);
		}
		else
		{
			SlotWidget->SetEmpty();
		}

		SlotWidget->OnSlotHovered.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSlotHovered);
		SlotWidget->OnSlotUnhovered.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSlotUnhovered);
		SlotWidget->OnSlotDoubleClicked.AddUniqueDynamic(this, &UMyInventoryWidget::HandleSlotDoubleClicked);

		// 셀을 가득 채워 칸 사이 틈을 없앤다. 간격은 그리드의 Slot Padding으로만 제어한다.
		if (UUniformGridSlot* GridSlot = GRD_ItemBox->AddChildToUniformGrid(SlotWidget, CellIndex / Columns, CellIndex % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	RebindCooldownTags();
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 변경 알림 처리. 툴팁을 숨기고 목록을 다시 그린다.
void UMyInventoryWidget::HandleInventoryUpdated()
{
	RefreshInventory();
}

////////////////////////////
//! \author 준혁
//! \brief 메소 변경 알림 처리. 메소 텍스트만 갱신한다.
//! \param NewMeso 변경된 메소량
void UMyInventoryWidget::HandleMesoChanged(int32 NewMeso)
{
	if (TXT_Meso)
	{
		TXT_Meso->SetText(FText::AsNumber(NewMeso));
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 슬롯 Hover 시작을 처리해 고정 위치 툴팁을 표시하는 함수
// ItemId : Hover가 시작된 아이템 ID
void UMyInventoryWidget::HandleSlotHovered(FName ItemId)
{
	ShowHoveredItem(ItemId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 슬롯 Hover 종료를 처리해 현재 툴팁을 숨기는 함수
// ItemId : Hover가 끝난 아이템 ID
void UMyInventoryWidget::HandleSlotUnhovered(FName ItemId)
{
	if (HoveredItemId == ItemId)
	{
		HideTooltip();
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Hover 중인 사용 가능 아이템의 더블클릭을 처리해 서버 사용 요청을 보내는 함수
// ItemId : 더블클릭한 아이템 ID
void UMyInventoryWidget::HandleSlotDoubleClicked(FName ItemId)
{
	if (!BoundInventoryComponent || HoveredItemId != ItemId || !IsItemUsable(ItemId))
	{
		return;
	}

	BoundInventoryComponent->UseItem(ItemId);
}

////////////////////////////
//! \author 준혁
//! \brief 정렬 버튼을 누를 때 정렬 방식 선택 패널의 표시 상태를 전환한다.
void UMyInventoryWidget::HandleSortClicked()
{
	if (!PNL_SortOptions)
	{
		return;
	}

	SetSortOptionsVisible(PNL_SortOptions->GetVisibility() == ESlateVisibility::Collapsed);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 개수 정렬 버튼을 눌렀을 때 현재 인벤토리를 개수가 많은 순서로 한 번 정렬하는 함수
void UMyInventoryWidget::HandleSortByCountClicked()
{
	ApplySortOnce(EMyInventorySortMode::ByCount);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 획득순 정렬 버튼을 눌렀을 때 현재 인벤토리를 원래 획득 순서로 한 번 정렬하는 함수
void UMyInventoryWidget::HandleSortByAcquisitionClicked()
{
	ApplySortOnce(EMyInventorySortMode::ByAcquisition);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 이름순 정렬 버튼을 눌렀을 때 현재 인벤토리를 아이템 이름 가나다순으로 한 번 정렬하는 함수
void UMyInventoryWidget::HandleSortByNameClicked()
{
	ApplySortOnce(EMyInventorySortMode::ByName);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 종류 정렬 버튼을 눌렀을 때 현재 인벤토리를 아이템 종류 순서로 한 번 정렬하는 함수
void UMyInventoryWidget::HandleSortByTypeClicked()
{
	ApplySortOnce(EMyInventorySortMode::ByType);
}

////////////////////////////
//! \author 준혁
//! \brief 닫기 버튼 처리. 퇴장 슬라이드가 끝난 뒤 레이어 스택에서 제거되게 한다.
void UMyInventoryWidget::HandleCloseClicked()
{
	RequestCloseInventory();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 닫기 슬라이드를 재생하고 완료 후 비활성화를 예약하는 함수
void UMyInventoryWidget::RequestCloseInventory()
{
	if (bIsClosing || !IsActivated())
	{
		return;
	}

	if (!InventorySlideAnimation
		|| InventorySlideAnimation->GetEndTime() <= InventorySlideAnimation->GetStartTime())
	{
		DeactivateWidget();
		return;
	}

	bIsClosing = true;
	PlayAnimationReverse(InventorySlideAnimation, CalculateSlideAnimationPlaybackSpeed());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퇴장 슬라이드가 완료됐을 때 인벤토리를 비활성화하는 함수
void UMyInventoryWidget::HandleSlideAnimationFinished()
{
	if (!bIsClosing)
	{
		return;
	}

	bIsClosing = false;
	DeactivateWidget();
}

////////////////////////////
//! \author 준혁
//! \brief 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 변경 델리게이트를 구독한다.
//! \return 바인딩 성공 여부
bool UMyInventoryWidget::BindToInventoryComponent()
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
	const IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(OwningPS);
	BoundAbilitySystemComponent = ASCOwner ? ASCOwner->GetAbilitySystemComponent() : nullptr;
	BoundInventoryComponent->OnInventoryUpdated.AddUniqueDynamic(this, &UMyInventoryWidget::HandleInventoryUpdated);
	BoundInventoryComponent->OnMesoChanged.AddUniqueDynamic(this, &UMyInventoryWidget::HandleMesoChanged);
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 컴포넌트 델리게이트 구독을 해제한다.
void UMyInventoryWidget::UnbindFromInventoryComponent()
{
	UnbindCooldownTags();

	for (UMyInventorySlotWidget* SlotWidget : ActiveItemSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->ClearCooldown();
		}
	}
	ActiveItemSlotWidgets.Reset();
	BoundAbilitySystemComponent = nullptr;

	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnInventoryUpdated.RemoveDynamic(this, &UMyInventoryWidget::HandleInventoryUpdated);
		BoundInventoryComponent->OnMesoChanged.RemoveDynamic(this, &UMyInventoryWidget::HandleMesoChanged);
		BoundInventoryComponent = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 인벤토리에 표시된 아이템들의 쿨타임 태그를 ASC에 중복 없이 구독하는 함수
void UMyInventoryWidget::RebindCooldownTags()
{
	UnbindCooldownTags();

	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	for (const UMyInventorySlotWidget* SlotWidget : ActiveItemSlotWidgets)
	{
		if (!SlotWidget)
		{
			continue;
		}

		const FGameplayTag CooldownTag = SlotWidget->GetCooldownTag();
		if (!CooldownTag.IsValid() || CooldownTagDelegateHandles.Contains(CooldownTag))
		{
			continue;
		}

		FDelegateHandle DelegateHandle = BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UMyInventoryWidget::HandleCooldownTagChanged);

		CooldownTagDelegateHandles.Add(CooldownTag, DelegateHandle);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ASC에 등록했던 인벤토리 아이템 쿨타임 태그 구독을 모두 해제하는 함수
void UMyInventoryWidget::UnbindCooldownTags()
{
	if (BoundAbilitySystemComponent)
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : CooldownTagDelegateHandles)
		{
			if (Pair.Key.IsValid() && Pair.Value.IsValid())
			{
				BoundAbilitySystemComponent
					->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved)
					.Remove(Pair.Value);
			}
		}
	}

	CooldownTagDelegateHandles.Reset();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ASC의 쿨타임 태그 변경을 같은 태그를 공유하는 모든 인벤토리 슬롯에 반영하는 함수
// CooldownTag : 추가되거나 제거된 아이템 쿨타임 태그
// NewCount : 변경 후 태그 개수. 0이면 쿨타임이 끝난 상태
void UMyInventoryWidget::HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount)
{
	for (UMyInventorySlotWidget* SlotWidget : ActiveItemSlotWidgets)
	{
		if (!SlotWidget || SlotWidget->GetCooldownTag() != CooldownTag)
		{
			continue;
		}

		if (NewCount <= 0)
		{
			SlotWidget->ClearCooldown();
			continue;
		}

		const float CooldownDuration = SlotWidget->GetCooldownDuration();
		float RemainingCooldown = BoundInventoryComponent
			? BoundInventoryComponent->GetItemCooldownRemaining(SlotWidget->GetItemId())
			: 0.0f;

		if (RemainingCooldown <= 0.0f)
		{
			RemainingCooldown = CooldownDuration;
		}

		SlotWidget->StartCooldownRemaining(RemainingCooldown, CooldownDuration);
	}
}

////////////////////////////
//! \author 준혁
//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerController에 저장된 표시 순서를 현재 인벤토리 항목과 동기화하는 함수
// 삭제된 아이템은 제거하고 새로 들어온 아이템은 기존 표시 순서의 마지막에 추가한다.
void UMyInventoryWidget::SynchronizeDisplayOrder()
{
	if (!BoundInventoryComponent)
	{
		return;
	}

	AMyPlayerController* MyPlayerController = Cast<AMyPlayerController>(GetOwningPlayer());
	if (!MyPlayerController)
	{
		return;
	}

	const TArray<FMyInventoryEntry>& InventoryEntries = BoundInventoryComponent->GetEntries();
	TArray<FName> DisplayOrder = MyPlayerController->GetInventoryDisplayOrder();

	DisplayOrder.RemoveAll([&InventoryEntries](const FName ItemId)
	{
		return !InventoryEntries.ContainsByPredicate([ItemId](const FMyInventoryEntry& Entry)
		{
			return Entry.ItemId == ItemId;
		});
	});

	for (const FMyInventoryEntry& Entry : InventoryEntries)
	{
		if (!DisplayOrder.Contains(Entry.ItemId))
		{
			DisplayOrder.Add(Entry.ItemId);
		}
	}

	MyPlayerController->SetInventoryDisplayOrder(DisplayOrder);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerController에 저장된 표시 순서에 맞는 인벤토리 항목 목록을 만드는 함수
// OutEntries : 화면에 표시할 순서로 구성된 인벤토리 항목 목록
void UMyInventoryWidget::BuildDisplayEntries(TArray<FMyInventoryEntry>& OutEntries) const
{
	OutEntries.Reset();

	if (!BoundInventoryComponent)
	{
		return;
	}

	const TArray<FMyInventoryEntry>& InventoryEntries = BoundInventoryComponent->GetEntries();
	const AMyPlayerController* MyPlayerController = Cast<AMyPlayerController>(GetOwningPlayer());
	if (!MyPlayerController)
	{
		OutEntries = InventoryEntries;
		return;
	}

	for (const FName ItemId : MyPlayerController->GetInventoryDisplayOrder())
	{
		const FMyInventoryEntry* Entry = InventoryEntries.FindByPredicate([ItemId](const FMyInventoryEntry& InventoryEntry)
		{
			return InventoryEntry.ItemId == ItemId;
		});

		if (Entry)
		{
			OutEntries.Add(*Entry);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 표시 중인 인벤토리 항목에 지정한 정렬 조건을 한 번만 적용하는 함수
// SortMode : 적용할 정렬 조건
void UMyInventoryWidget::ApplySortOnce(EMyInventorySortMode SortMode)
{
	if (!BoundInventoryComponent)
	{
		SetSortOptionsVisible(false);
		return;
	}

	SynchronizeDisplayOrder();

	TArray<FMyInventoryEntry> DisplayEntries;
	BuildDisplayEntries(DisplayEntries);

	switch (SortMode)
	{
	case EMyInventorySortMode::ByAcquisition:
		DisplayEntries = BoundInventoryComponent->GetEntries();
		break;

	case EMyInventorySortMode::ByCount:
		DisplayEntries.StableSort([](const FMyInventoryEntry& A, const FMyInventoryEntry& B)
		{
			return A.Count > B.Count;
		});
		break;

	case EMyInventorySortMode::ByName:
	{
		const UMyInventoryComponent* Inventory = BoundInventoryComponent;
		DisplayEntries.StableSort([Inventory](const FMyInventoryEntry& A, const FMyInventoryEntry& B)
		{
			FMyItemData DataA;
			FMyItemData DataB;
			const bool bFoundA = Inventory->FindItemData(A.ItemId, DataA);
			const bool bFoundB = Inventory->FindItemData(B.ItemId, DataB);
			if (!bFoundA || !bFoundB)
			{
				return bFoundA && !bFoundB;
			}

			return DataA.DisplayName.CompareTo(DataB.DisplayName) < 0;
		});
		break;
	}

	case EMyInventorySortMode::ByType:
	{
		// 타입 비교를 위해 정적 데이터를 조회한다. 같은 타입끼리는 이름순으로 정렬한다.
		const UMyInventoryComponent* Inventory = BoundInventoryComponent;
		DisplayEntries.StableSort([Inventory](const FMyInventoryEntry& A, const FMyInventoryEntry& B)
		{
			FMyItemData DataA;
			FMyItemData DataB;
			const bool bFoundA = Inventory->FindItemData(A.ItemId, DataA);
			const bool bFoundB = Inventory->FindItemData(B.ItemId, DataB);
			if (!bFoundA || !bFoundB)
			{
				return bFoundA && !bFoundB;
			}

			if (DataA.ItemType != DataB.ItemType)
			{
				return DataA.ItemType < DataB.ItemType;
			}

			return DataA.DisplayName.CompareTo(DataB.DisplayName) < 0;
		});
		break;
	}

	default:
		break;
	}

	TArray<FName> DisplayOrder;
	DisplayOrder.Reserve(DisplayEntries.Num());
	for (const FMyInventoryEntry& Entry : DisplayEntries)
	{
		DisplayOrder.Add(Entry.ItemId);
	}

	if (AMyPlayerController* MyPlayerController = Cast<AMyPlayerController>(GetOwningPlayer()))
	{
		MyPlayerController->SetInventoryDisplayOrder(DisplayOrder);
	}

	SetSortOptionsVisible(false);
	RefreshInventory();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 정렬 방식 선택 패널의 표시 여부를 변경하는 함수
// bVisible : true면 표시, false면 숨김
void UMyInventoryWidget::SetSortOptionsVisible(bool bVisible)
{
	if (PNL_SortOptions)
	{
		PNL_SortOptions->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 메소 텍스트를 현재 보유량으로 갱신한다.
void UMyInventoryWidget::RefreshMesoText()
{
	if (TXT_Meso)
	{
		const int32 CurrentMeso = BoundInventoryComponent ? BoundInventoryComponent->GetMeso() : 0;
		TXT_Meso->SetText(FText::AsNumber(CurrentMeso));
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Hover 중인 아이템 정보를 고정 위치 툴팁에 표시하는 함수
// ItemId : Hover 중인 아이템 ID
void UMyInventoryWidget::ShowHoveredItem(FName ItemId)
{
	if (ItemId.IsNone() || !BoundInventoryComponent)
	{
		HideTooltip();
		return;
	}

	FMyItemData ItemData;
	const int32 ItemCount = BoundInventoryComponent->GetItemCount(ItemId);
	if (!BoundInventoryComponent->FindItemData(ItemId, ItemData) || ItemCount <= 0)
	{
		HideTooltip();
		return;
	}

	HoveredItemId = ItemId;

	if (WBP_Tooltip)
	{
		WBP_Tooltip->ShowItem(ItemId, ItemData, ItemCount);
		WBP_Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Hover 대상과 관계없이 툴팁을 숨기되 레이아웃 공간은 유지하는 함수
void UMyInventoryWidget::HideTooltip()
{
	HoveredItemId = NAME_None;

	if (WBP_Tooltip)
	{
		WBP_Tooltip->SetVisibility(ESlateVisibility::Hidden);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 더블클릭 사용과 드래그앤드롭 등록으로 대체된 기존 액션 버튼을 숨기는 함수
void UMyInventoryWidget::HideLegacyActionButtons()
{
	if (BTN_Use)
	{
		BTN_Use->SetIsEnabled(false);
		BTN_Use->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (BTN_RegisterKey)
	{
		BTN_RegisterKey->SetIsEnabled(false);
		BTN_RegisterKey->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정 아이템이 사용 가능하고 현재 한 개 이상 남아 있는지 확인하는 함수
// ItemId : 사용 가능 여부를 확인할 아이템 ID
// 반환값 : 더블클릭으로 사용을 요청할 수 있으면 true
bool UMyInventoryWidget::IsItemUsable(FName ItemId) const
{
	if (ItemId.IsNone() || !BoundInventoryComponent)
	{
		return false;
	}

	FMyItemData ItemData;
	return BoundInventoryComponent->FindItemData(ItemId, ItemData)
		&& ItemData.bUsable
		&& BoundInventoryComponent->GetItemCount(ItemId) > 0;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 에디터 목표 시간에 맞도록 인벤토리 슬라이드 애니메이션의 재생 배율을 계산하는 함수
// 반환값 : UMG 애니메이션 재생 배율
float UMyInventoryWidget::CalculateSlideAnimationPlaybackSpeed() const
{
	if (!InventorySlideAnimation)
	{
		return 1.0f;
	}

	const float AnimationLength = InventorySlideAnimation->GetEndTime() - InventorySlideAnimation->GetStartTime();
	return AnimationLength / FMath::Max(SlideAnimationDuration, 0.01f);
}
