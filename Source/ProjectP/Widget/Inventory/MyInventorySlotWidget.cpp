////////////////////////////
//! \page MyInventorySlotWidget.cpp
//! \brief 인벤토리 아이템 슬롯 위젯 구현 파일이다.
#include "Widget/Inventory/MyInventorySlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Widget/Inventory/MyItemDragDropOperation.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 격자 아이콘의 중앙 기준 표시 배율을 적용하는 함수
void UMyInventorySlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (IMG_Icon)
    {
        const float AppliedScale = FMath::Max(InventoryItemIconScale, 0.1f);
        IMG_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        IMG_Icon->SetRenderScale(FVector2D(AppliedScale, AppliedScale));
    }

	ApplyCooldownStyle();
	ApplyCooldownDisplay(0.0f, CooldownDuration);
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼 클릭 델리게이트를 바인딩한다.
void UMyInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BTN_Slot)
	{
		BTN_Slot->OnClicked.AddUniqueDynamic(this, &UMyInventorySlotWidget::HandleSlotButtonClicked);
		BTN_Slot->OnHovered.AddUniqueDynamic(this, &UMyInventorySlotWidget::HandleSlotHovered);
		BTN_Slot->OnUnhovered.AddUniqueDynamic(this, &UMyInventorySlotWidget::HandleSlotUnhovered);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼 클릭 델리게이트를 해제한다.
void UMyInventorySlotWidget::NativeDestruct()
{
	ClearCooldown();

	if (BTN_Slot)
	{
		BTN_Slot->OnClicked.RemoveDynamic(this, &UMyInventorySlotWidget::HandleSlotButtonClicked);
		BTN_Slot->OnHovered.RemoveDynamic(this, &UMyInventorySlotWidget::HandleSlotHovered);
		BTN_Slot->OnUnhovered.RemoveDynamic(this, &UMyInventorySlotWidget::HandleSlotUnhovered);
	}

	Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯에 표시할 아이템의 아이콘과 개수를 설정한다.
//! \param InItemId 아이템 ID
//! \param InItemData 아이템 정적 데이터
//! \param InCount 보유 개수
void UMyInventorySlotWidget::SetItem(FName InItemId, const FMyItemData& InItemData, int32 InCount)
{
	ClearCooldown();

	ItemId = InItemId;
	CachedItemData = InItemData;

	if (IMG_Icon)
	{
		// 소량의 아이콘 텍스처만 다루므로 동기 로드로 단순화한다
		if (UTexture2D* IconTexture = InItemData.Icon.LoadSynchronous())
		{
			IMG_Icon->SetBrushFromTexture(IconTexture);
			IMG_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (IMG_IconBackground)
			{
				IMG_IconBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else
		{
			IMG_Icon->SetVisibility(ESlateVisibility::Hidden);

			if (IMG_IconBackground)
			{
				IMG_IconBackground->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}

	if (TXT_Count)
	{
		TXT_Count->SetText(FText::AsNumber(InCount));
		TXT_Count->SetVisibility(InCount > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 빈 슬롯 상태로 만든다. 5x6 고정 그리드의 빈 칸 표시에 사용한다.
void UMyInventorySlotWidget::SetEmpty()
{
	ClearCooldown();

	ItemId = NAME_None;
	CachedItemData = FMyItemData();

	if (IMG_Icon)
	{
		IMG_Icon->SetVisibility(ESlateVisibility::Hidden);
	}

	if (IMG_IconBackground)
	{
		IMG_IconBackground->SetVisibility(ESlateVisibility::Hidden);
	}

	if (TXT_Count)
	{
		TXT_Count->SetVisibility(ESlateVisibility::Hidden);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 남은 시간부터 인벤토리 슬롯의 쿨타임 표시와 갱신 타이머를 시작하는 함수
// RemainingTime : 남아 있는 쿨타임(초)
// Duration : 전체 쿨타임(초)
void UMyInventorySlotWidget::StartCooldownRemaining(float RemainingTime, float Duration)
{
	Duration = FMath::Max(Duration, 0.0f);
	RemainingTime = FMath::Clamp(RemainingTime, 0.0f, Duration);
	if (RemainingTime <= 0.0f)
	{
		ClearCooldown();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		CooldownDuration = Duration;
		ApplyCooldownDisplay(RemainingTime, Duration);
		return;
	}

	CooldownDuration = Duration;
	CooldownEndWorldTime = World->GetTimeSeconds() + RemainingTime;
	ApplyCooldownDisplay(RemainingTime, Duration);

	World->GetTimerManager().SetTimer(
		CooldownTimerHandle,
		this,
		&ThisClass::HandleCooldownTimerTick,
		CooldownTickInterval,
		true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 슬롯의 쿨타임 갱신 타이머와 표시를 초기화하는 함수
void UMyInventorySlotWidget::ClearCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}

	CooldownEndWorldTime = 0.0f;
	ApplyCooldownDisplay(0.0f, CooldownDuration);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 슬롯의 쿨타임 오버레이와 남은 시간 텍스트 스타일을 적용하는 함수
void UMyInventorySlotWidget::ApplyCooldownStyle()
{
	if (IMG_Cooldown)
	{
		IMG_Cooldown->SetColorAndOpacity(CooldownOverlayColor);
	}

	if (TXT_Cooldown)
	{
		if (CooldownFont.FontObject)
		{
			TXT_Cooldown->SetFont(CooldownFont);
		}

		TXT_Cooldown->SetColorAndOpacity(FSlateColor(CooldownTextColor));
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 남은 쿨타임 비율과 초 단위 시간을 인벤토리 슬롯 위젯에 반영하는 함수
// RemainingTime : 남아 있는 쿨타임(초)
// Duration : 전체 쿨타임(초)
void UMyInventorySlotWidget::ApplyCooldownDisplay(float RemainingTime, float Duration)
{
	const bool bHasCooldown = Duration > 0.0f && RemainingTime > 0.0f;
	const float FillAlpha = bHasCooldown ? FMath::Clamp(RemainingTime / Duration, 0.0f, 1.0f) : 0.0f;

	if (IMG_Cooldown)
	{
		IMG_Cooldown->SetVisibility(bHasCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

		if (Cast<UMaterialInterface>(IMG_Cooldown->GetBrush().GetResourceObject()))
		{
			if (UMaterialInstanceDynamic* CooldownMID = IMG_Cooldown->GetDynamicMaterial())
			{
				CooldownMID->SetScalarParameterValue(CooldownProgressParamName, FillAlpha);
			}
		}
		else
		{
			IMG_Cooldown->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			IMG_Cooldown->SetRenderScale(FVector2D(1.0f, FillAlpha));
		}
	}

	if (TXT_Cooldown)
	{
		TXT_Cooldown->SetVisibility(bHasCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		TXT_Cooldown->SetText(bHasCooldown
			? FText::AsNumber(FMath::Max(1, FMath::CeilToInt(RemainingTime)))
			: FText::GetEmpty());
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 월드 시간을 기준으로 인벤토리 슬롯의 남은 쿨타임을 주기적으로 갱신하는 함수
void UMyInventorySlotWidget::HandleCooldownTimerTick()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		ClearCooldown();
		return;
	}

	const float RemainingTime = CooldownEndWorldTime - World->GetTimeSeconds();
	if (RemainingTime <= 0.0f)
	{
		ClearCooldown();
		return;
	}

	ApplyCooldownDisplay(RemainingTime, CooldownDuration);
}

////////////////////////////
//! \author 준혁
//! \brief 슬롯 버튼의 더블클릭 경로에서 아이템 ID를 브로드캐스트한다.
void UMyInventorySlotWidget::HandleSlotButtonClicked()
{
	// 첫 클릭은 NativeOnPreviewMouseButtonDown이 드래그 감지를 위해 소비한다.
	// UE 5.7의 두 번째 클릭은 SButton::OnMouseButtonDoubleClick 경로로 들어와 OnClicked를 발생시킨다.
	if (bIsHovered && !ItemId.IsNone())
	{
		OnSlotDoubleClicked.Broadcast(ItemId);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 슬롯에 마우스 Hover가 시작됐음을 부모 인벤토리 위젯에 알리는 함수
void UMyInventorySlotWidget::HandleSlotHovered()
{
	bIsHovered = true;
	OnSlotHovered.Broadcast(ItemId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 슬롯에서 마우스 Hover가 끝났음을 부모 인벤토리 위젯에 알리는 함수
void UMyInventorySlotWidget::HandleSlotUnhovered()
{
	bIsHovered = false;
	OnSlotUnhovered.Broadcast(ItemId);
}

////////////////////////////
//! \author 준혁
//! \brief 좌클릭 press를 터널링 단계에서 가로채 드래그 감지를 시작한다.
//!        BTN_Slot이 press를 소비하면 부모 위젯의 드래그 감지가 불가능하므로 이 단계에서 처리한다.
//!        첫 클릭은 이 경로가 소비하고, 더블클릭은 Slate의 별도 DoubleClick 경로로 BTN_Slot에 전달된다.
//! \param InGeometry 위젯 지오메트리
//! \param InMouseEvent 마우스 이벤트
//! \return 드래그 감지가 설정된 Reply (아이템이 없으면 기본 처리)
FReply UMyInventorySlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!ItemId.IsNone() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 드래그가 감지되면 아이템 ID를 담은 드래그 오퍼레이션과 드래그 비주얼(동일 슬롯 위젯 사본)을 생성한다.
//! \param InGeometry 위젯 지오메트리
//! \param InMouseEvent 마우스 이벤트
//! \param OutOperation 생성한 드래그 오퍼레이션(출력)
void UMyInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (ItemId.IsNone())
	{
		return;
	}

	UMyItemDragDropOperation* DragOperation = NewObject<UMyItemDragDropOperation>(this);
	DragOperation->ItemId = ItemId;
	DragOperation->Pivot = EDragPivot::MouseDown;
	// 드랍이 아무 대상에도 처리되지 않으면 원인 파악용 로그를 남긴다
	DragOperation->OnDragCancelled.AddUniqueDynamic(this, &UMyInventorySlotWidget::HandleDragCancelled);

	// 드래그 비주얼: 슬롯 전체(버튼 배경 포함)가 아니라 아이콘 이미지만 커서에 따라다니게 한다.
	// 크기는 WBP 디폴트의 DragVisualSize로 조절한다.
	// (SetDesiredSizeOverride는 아직 Slate가 구성되지 않은 위젯에는 적용되지 않으므로
	//  브러시의 ImageSize로 크기를 지정한다)
	UTexture2D* IconTexture = CachedItemData.Icon.LoadSynchronous();
	DragOperation->ItemIcon = IconTexture;

	if (IconTexture)
	{
		DragOperation->DefaultDragVisual = UMyItemDragDropOperation::CreateItemDragVisual(
			this,
			IconTexture,
			DragVisualSize,
			InGeometry.GetLocalSize());
	}

	OutOperation = DragOperation;
	UE_LOG(LogTemp, Log, TEXT("[ItemDnD] Drag started - ItemId=%s, DragVisualSize=(%.0f, %.0f)"), *ItemId.ToString(), DragVisualSize.X, DragVisualSize.Y);
}

////////////////////////////
//! \author 준혁
//! \brief 드랍이 어떤 대상에도 처리되지 않았을 때 진단 로그를 남긴다.
//! \param Operation 취소된 드래그 오퍼레이션
void UMyInventorySlotWidget::HandleDragCancelled(UDragDropOperation* Operation)
{
	const UMyItemDragDropOperation* ItemOperation = Cast<UMyItemDragDropOperation>(Operation);
	UE_LOG(LogTemp, Warning, TEXT("[ItemDnD] Drop was NOT handled by any target - ItemId=%s. (HUD 슬롯이 다른 위젯에 가려졌거나, 슬롯이 드랍 수신 상태가 아님)"),
		ItemOperation ? *ItemOperation->ItemId.ToString() : TEXT("Unknown"));
}
