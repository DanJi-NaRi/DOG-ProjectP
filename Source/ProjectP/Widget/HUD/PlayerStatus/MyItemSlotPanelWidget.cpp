////////////////////////////
//! \page MyItemSlotPanelWidget.cpp
//! \brief HUD 하단의 아이템 퀵슬롯(1~4) 패널 위젯 구현 파일이다.
#include "MyItemSlotPanelWidget.h"

#include "AbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#include "GAS/MyPlayerState.h"
#include "Item/MyInventoryComponent.h"
#include "MySkillSlotWidget.h"

namespace
{
	constexpr float InventoryBindRetryInterval = 0.2f;
	constexpr int32 MaxInventoryBindRetryCount = 25;
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 드랍 수신을 초기화하고 인벤토리 컴포넌트 구독을 시작한다.
void UMyItemSlotPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SlotCooldownTags.Init(FGameplayTag(), 4);
	SlotCooldownDurations.Init(0.0f, 4);

	InitializeSlots();

	if (!BindToInventoryComponent())
	{
		ScheduleBindRetry();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 재시도 타이머와 인벤토리 구독을 해제한다.
void UMyItemSlotPanelWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}
	BindRetryTimerHandle.Invalidate();
	BindRetryCount = 0;

	UnbindCooldownTags();
	UnbindFromInventoryComponent();

	Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 위젯들의 아이템 드랍 수신을 켜고 드랍 델리게이트를 바인딩한다.
void UMyItemSlotPanelWidget::InitializeSlots()
{
	int32 BoundSlotCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		UMySkillSlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (!SlotWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Slot_%d is not bound. Name the slot widgets Slot_1 ~ Slot_4 in the WBP."), SlotIndex + 1);
			continue;
		}

		SlotWidget->SetAcceptsItemDrop(true);
        SlotWidget->SetQuickSlotContext(SlotIndex, NAME_None);
		SlotWidget->OnItemDropped.AddUniqueDynamic(this, &UMyItemSlotPanelWidget::HandleItemDropped);
        SlotWidget->OnItemDroppedOutside.AddUniqueDynamic(this, &UMyItemSlotPanelWidget::HandleItemDroppedOutside);
		++BoundSlotCount;
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemSlotPanel] Initialized - BoundSlots: %d / 4"), BoundSlotCount);
}

////////////////////////////
//! \author 준혁
//! \brief 소유 플레이어 PlayerState의 인벤토리 컴포넌트를 찾아 델리게이트를 구독하고 표시를 갱신한다.
//! \return 구독 성공 여부 (PlayerState 복제 전이면 false)
bool UMyItemSlotPanelWidget::BindToInventoryComponent()
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	const AMyPlayerState* MyPS = OwningPlayer ? OwningPlayer->GetPlayerState<AMyPlayerState>() : nullptr;
	UMyInventoryComponent* InventoryComponent = MyPS ? MyPS->GetInventoryComponent() : nullptr;
	if (!InventoryComponent)
	{
		return false;
	}

	if (BoundInventoryComponent == InventoryComponent)
	{
		return true;
	}

	UnbindFromInventoryComponent();
	BoundInventoryComponent = InventoryComponent;

	// 쿨타임 태그 구독용 ASC (인벤토리와 같은 PlayerState 소유)
	BoundAbilitySystemComponent = MyPS->GetAbilitySystemComponent();

	BoundInventoryComponent->OnQuickSlotChanged.AddUniqueDynamic(this, &UMyItemSlotPanelWidget::HandleQuickSlotChanged);
	BoundInventoryComponent->OnInventoryUpdated.AddUniqueDynamic(this, &UMyItemSlotPanelWidget::HandleInventoryUpdated);

	RefreshAllSlots();
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 컴포넌트 델리게이트 구독을 해제한다.
void UMyItemSlotPanelWidget::UnbindFromInventoryComponent()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnQuickSlotChanged.RemoveDynamic(this, &UMyItemSlotPanelWidget::HandleQuickSlotChanged);
		BoundInventoryComponent->OnInventoryUpdated.RemoveDynamic(this, &UMyItemSlotPanelWidget::HandleInventoryUpdated);
		BoundInventoryComponent = nullptr;
	}

	BoundAbilitySystemComponent = nullptr;
}

////////////////////////////
//! \author 준혁
//! \brief PlayerState 복제 지연에 대비해 인벤토리 구독을 재시도 예약한다.
void UMyItemSlotPanelWidget::ScheduleBindRetry()
{
	UWorld* World = GetWorld();
	if (!World || BindRetryTimerHandle.IsValid() || BindRetryCount >= MaxInventoryBindRetryCount)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		BindRetryTimerHandle,
		this,
		&UMyItemSlotPanelWidget::HandleBindRetry,
		InventoryBindRetryInterval,
		false);
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 구독 재시도 타이머 콜백.
void UMyItemSlotPanelWidget::HandleBindRetry()
{
	BindRetryTimerHandle.Invalidate();
	++BindRetryCount;

	if (!BindToInventoryComponent() && BindRetryCount < MaxInventoryBindRetryCount)
	{
		ScheduleBindRetry();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 모든 퀵슬롯 표시를 다시 그린다.
void UMyItemSlotPanelWidget::RefreshAllSlots()
{
	const int32 SlotCount = BoundInventoryComponent ? BoundInventoryComponent->GetQuickSlotCount() : 4;
	for (int32 SlotIndex = 0; SlotIndex < FMath::Max(SlotCount, 4); ++SlotIndex)
	{
		RefreshSlot(SlotIndex);
	}

	RebindCooldownTags();
}

////////////////////////////
//! \author 준혁
//! \brief 지정한 퀵슬롯 한 칸의 아이콘/개수/잠금 표시를 갱신한다.
//! \param SlotIndex 갱신할 퀵슬롯 인덱스
void UMyItemSlotPanelWidget::RefreshSlot(int32 SlotIndex)
{
	UMySkillSlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (!SlotWidget)
	{
		return;
	}

	const FName ItemId = BoundInventoryComponent ? BoundInventoryComponent->GetQuickSlotItem(SlotIndex) : NAME_None;
    SlotWidget->SetQuickSlotContext(SlotIndex, ItemId);

	FMyItemData ItemData;
    if (ItemId.IsNone() || !BoundInventoryComponent->FindItemData(ItemId, ItemData))
    {
        // 빈 슬롯: 아이콘/개수/쿨다운 표시를 지운다
        SlotWidget->SetItemFrameActive(false);
        SlotWidget->SetSkillIcon(nullptr);
        SlotWidget->SetCountText(FText::GetEmpty());
        SlotWidget->ClearCooldown();

		if (SlotCooldownTags.IsValidIndex(SlotIndex))
		{
			SlotCooldownTags[SlotIndex] = FGameplayTag();
			SlotCooldownDurations[SlotIndex] = 0.0f;
		}
        return;
    }

    SlotWidget->SetItemFrameActive(true);

    // 소량의 아이콘 텍스처만 다루므로 동기 로드로 단순화한다 (인벤토리 슬롯과 동일 정책)
    UTexture2D* IconTexture = ItemData.Icon.LoadSynchronous();
	if (!IconTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Icon is not set or failed to load - ItemId: %s. (DataTable Row의 Icon 지정 확인)"), *ItemId.ToString());
	}
	SlotWidget->SetSkillIcon(IconTexture);

	// 보유 개수 표시. 0이 되어도 등록은 유지되고 사용 시도만 서버에서 거부된다.
	const int32 Count = BoundInventoryComponent->GetItemCount(ItemId);
	SlotWidget->SetCountText(FText::AsNumber(Count));

	// 쿨타임 태그/전체시간을 기억해 두고, 이미 진행 중인 쿨타임이 있으면 남은 시간부터 표시한다
	if (SlotCooldownTags.IsValidIndex(SlotIndex))
	{
		SlotCooldownTags[SlotIndex] = ItemData.CooldownTag;
		SlotCooldownDurations[SlotIndex] = ItemData.CooldownSeconds;
	}

	const float RemainingCooldown = BoundInventoryComponent->GetItemCooldownRemaining(ItemId);
	if (RemainingCooldown > 0.0f)
	{
		SlotWidget->StartCooldownRemaining(RemainingCooldown, ItemData.CooldownSeconds);
	}
	else
	{
		SlotWidget->ClearCooldown();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 퀵슬롯 등록 변경 알림 콜백. 해당 칸만 갱신한다.
//! \param SlotIndex 변경된 퀵슬롯 인덱스
//! \param ItemId 등록된 아이템 ID (해제 시 NAME_None)
void UMyItemSlotPanelWidget::HandleQuickSlotChanged(int32 SlotIndex, FName ItemId)
{
	RefreshSlot(SlotIndex);
	RebindCooldownTags();
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯에 등록된 아이템들의 쿨타임 태그를 ASC에 다시 구독한다. 등록 구성이 바뀔 때마다 호출된다.
void UMyItemSlotPanelWidget::RebindCooldownTags()
{
	UnbindCooldownTags();

	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayTag& CooldownTag : SlotCooldownTags)
	{
		if (!CooldownTag.IsValid() || CooldownTagDelegateHandles.Contains(CooldownTag))
		{
			continue;
		}

		FDelegateHandle DelegateHandle = BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UMyItemSlotPanelWidget::HandleCooldownTagChanged);

		CooldownTagDelegateHandles.Add(CooldownTag, DelegateHandle);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 구독 중인 쿨타임 태그 이벤트를 모두 해제한다.
void UMyItemSlotPanelWidget::UnbindCooldownTags()
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

////////////////////////////
//! \author 준혁
//! \brief 쿨타임 태그 변경 알림. 태그가 생기면 해당 태그를 쓰는 모든 슬롯의 쿨다운 표시를 시작한다.
//!        (포션 3종처럼 태그를 공유하는 슬롯들이 함께 쿨다운에 들어간다)
//! \param CooldownTag 변경된 쿨타임 태그
//! \param NewCount 태그 카운트 (0이면 쿨타임 종료)
void UMyItemSlotPanelWidget::HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount)
{
	for (int32 SlotIndex = 0; SlotIndex < SlotCooldownTags.Num(); ++SlotIndex)
	{
		if (SlotCooldownTags[SlotIndex] != CooldownTag)
		{
			continue;
		}

		UMySkillSlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (!SlotWidget)
		{
			continue;
		}

		if (NewCount > 0)
		{
			SlotWidget->StartCooldown(SlotCooldownDurations[SlotIndex]);
		}
		else
		{
			SlotWidget->ClearCooldown();
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 변경(획득/소모) 알림 콜백. 보유 개수 표시를 갱신한다.
void UMyItemSlotPanelWidget::HandleInventoryUpdated()
{
	RefreshAllSlots();
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리에서 드래그한 아이템이 슬롯에 드랍되면 해당 칸에 퀵슬롯 등록한다.
//!        퀵슬롯에서 시작한 드래그이면 출발지를 확인해 이동 또는 교환을 처리한다.
//! \param SlotWidget 드랍을 받은 슬롯 위젯
//! \param ItemId 드랍된 아이템 ID
//! \param SourceQuickSlotIndex 출발 퀵슬롯 인덱스, 인벤토리에서 시작했으면 INDEX_NONE
void UMyItemSlotPanelWidget::HandleItemDropped(
    UMySkillSlotWidget* SlotWidget,
    FName ItemId,
    int32 SourceQuickSlotIndex)
{
	const int32 SlotIndex = FindSlotIndex(SlotWidget);
	if (SlotIndex == INDEX_NONE || !BoundInventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Drop assign failed - SlotIndex: %d, InventoryComponent: %s, ItemId: %s"),
			SlotIndex, *GetNameSafe(BoundInventoryComponent), *ItemId.ToString());
		return;
	}

	// 사용 불가 아이템(재료 등)은 퀵슬롯 등록 대상이 아니다
	FMyItemData ItemData;
	if (!BoundInventoryComponent->FindItemData(ItemId, ItemData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Drop assign failed - ItemId '%s' is not in the ItemDataTable."), *ItemId.ToString());
		return;
	}
	if (!ItemData.bUsable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Drop assign rejected - ItemId '%s' has bUsable=false. (DataTable Row에서 bUsable을 켜야 등록 가능)"), *ItemId.ToString());
		return;
	}

    if (SourceQuickSlotIndex != INDEX_NONE)
    {
        const FName RegisteredSourceItemId = BoundInventoryComponent->GetQuickSlotItem(SourceQuickSlotIndex);
        if (RegisteredSourceItemId != ItemId)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Move/swap rejected - SourceSlotIndex: %d, DragItemId: %s, RegisteredItemId: %s"),
                SourceQuickSlotIndex,
                *ItemId.ToString(),
                *RegisteredSourceItemId.ToString());
            return;
        }

        const bool bMovedOrSwapped = BoundInventoryComponent->MoveOrSwapQuickSlot(SourceQuickSlotIndex, SlotIndex);
        UE_LOG(LogTemp, Log, TEXT("[ItemSlotPanel] Move/swap %s - SourceSlotIndex: %d, TargetSlotIndex: %d, ItemId: %s"),
            bMovedOrSwapped ? TEXT("OK") : TEXT("FAILED"),
            SourceQuickSlotIndex,
            SlotIndex,
            *ItemId.ToString());
        return;
    }

	const bool bAssigned = BoundInventoryComponent->AssignQuickSlot(SlotIndex, ItemId);
	UE_LOG(LogTemp, Log, TEXT("[ItemSlotPanel] Inventory drop assign %s - SlotIndex: %d, ItemId: %s"),
		bAssigned ? TEXT("OK") : TEXT("FAILED"), SlotIndex, *ItemId.ToString());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯 아이템을 다른 퀵슬롯이 아닌 영역에 드롭했을 때 출발 슬롯 등록을 해제하는 함수
// SlotWidget : 드래그를 시작한 퀵슬롯 위젯
// ItemId : 드래그를 시작할 때 등록되어 있던 아이템 ID
void UMyItemSlotPanelWidget::HandleItemDroppedOutside(UMySkillSlotWidget* SlotWidget, FName ItemId)
{
    const int32 SourceSlotIndex = FindSlotIndex(SlotWidget);
    if (SourceSlotIndex == INDEX_NONE || !BoundInventoryComponent)
    {
        return;
    }

    const FName RegisteredItemId = BoundInventoryComponent->GetQuickSlotItem(SourceSlotIndex);
    if (RegisteredItemId != ItemId)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ItemSlotPanel] Outside drop clear rejected - SourceSlotIndex: %d, DragItemId: %s, RegisteredItemId: %s"),
            SourceSlotIndex,
            *ItemId.ToString(),
            *RegisteredItemId.ToString());
        return;
    }

    BoundInventoryComponent->ClearQuickSlot(SourceSlotIndex);
    UE_LOG(LogTemp, Log, TEXT("[ItemSlotPanel] Outside drop clear OK - SourceSlotIndex: %d, ItemId: %s"),
        SourceSlotIndex,
        *ItemId.ToString());
}

////////////////////////////
//! \author 준혁
//! \brief 퀵슬롯 인덱스에 해당하는 슬롯 위젯을 반환한다.
//! \param SlotIndex 퀵슬롯 인덱스 (0~3)
//! \return 슬롯 위젯 (WBP에 없으면 nullptr)
UMySkillSlotWidget* UMyItemSlotPanelWidget::GetSlotWidget(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0: return Slot_1;
	case 1: return Slot_2;
	case 2: return Slot_3;
	case 3: return Slot_4;
	default: return nullptr;
	}
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 위젯이 몇 번째 퀵슬롯인지 찾는다.
//! \param SlotWidget 찾을 슬롯 위젯
//! \return 퀵슬롯 인덱스 (없으면 INDEX_NONE)
int32 UMyItemSlotPanelWidget::FindSlotIndex(const UMySkillSlotWidget* SlotWidget) const
{
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		if (SlotWidget && GetSlotWidget(SlotIndex) == SlotWidget)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}
