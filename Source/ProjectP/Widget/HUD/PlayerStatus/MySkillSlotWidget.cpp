#include "MySkillSlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "Widget/Inventory/MyItemDragDropOperation.h"

void UMySkillSlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (IsDesignTime())
    {
        InputKeyText = PreviewInputKeyText;
        SkillName = PreviewSkillName;
        SkillIcon = PreviewIcon;
        bLocked = bPreviewLocked;
    }
    // - 준혁 -
    // 런타임에 SetInputKeyText를 호출해 주는 패널이 없는 배치(아이템 퀵슬롯 1~4 등)는
    // WBP 인스턴스에 입력해 둔 PreviewInputKeyText를 키 텍스트 기본값으로 사용한다.
    // (InputKeyText가 Transient라 런타임 초기값이 비어 있어, 그대로 두면 ApplyDisplay가 TXT_Key를 빈 텍스트로 덮어썼다)
    else if (InputKeyText.IsEmpty())
    {
        InputKeyText = PreviewInputKeyText;
    }

    // 보유 개수(TXT_Count)는 런타임 기본 숨김. WBP 디폴트 텍스트("N")가 스킬 슬롯에서 그대로 보이지 않도록.
    // 아이템 퀵슬롯은 패널이 SetCountText로 실제 개수를 넣을 때 다시 보이게 된다.
    if (!IsDesignTime() && TXT_Count)
    {
        TXT_Count->SetVisibility(ESlateVisibility::Collapsed);
    }

    ApplyDisplay();
    ApplyCooldownStyle();
    ApplyCooldownDisplay(0.0f, CooldownDuration);
}

void UMySkillSlotWidget::NativeDestruct()
{
    ClearCooldown();
    Super::NativeDestruct();
}

void UMySkillSlotWidget::SetSkillData(const FMySkillDataEntry& SkillData)
{
    InputTag = SkillData.InputTag;
    CooldownTag = SkillData.CooldownTag;
    CooldownDuration = FMath::Max(SkillData.CooldownDuration, 0.0f);
    SkillName = SkillData.DisplayName;
    SkillIcon = SkillData.Icon;

    ApplyDisplay();
}

void UMySkillSlotWidget::ClearSkillData()
{
    InputTag = FGameplayTag();
    CooldownTag = FGameplayTag();
    CooldownDuration = 0.0f;
    SkillName = FText::GetEmpty();
    SkillIcon = nullptr;

    ClearCooldown();
    ApplyDisplay();
}

void UMySkillSlotWidget::SetInputKeyText(FText InInputKeyText)
{
    InputKeyText = MoveTemp(InInputKeyText);
    ApplyDisplay();
}

void UMySkillSlotWidget::SetSkillIcon(UTexture2D* InIcon)
{
    SkillIcon = InIcon;
    ApplyDisplay();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 퀵슬롯의 아이템 유무에 따라 프레임 이미지를 변경하는 함수
// bHasItem : 아이템이 등록되어 있으면 true, 빈 슬롯이면 false
void UMySkillSlotWidget::SetItemFrameActive(bool bHasItem)
{
    if (!IMG_Frame)
    {
        return;
    }

    const TSoftObjectPtr<UTexture2D>& FrameTexture = bHasItem
        ? ItemOnFrameTexture
        : ItemOffFrameTexture;

    // 두 개의 작은 UI 텍스처만 사용하므로 슬롯 갱신 시 동기 로드한다.
    if (UTexture2D* LoadedFrameTexture = FrameTexture.LoadSynchronous())
    {
        IMG_Frame->SetBrushFromTexture(LoadedFrameTexture);
    }
}

void UMySkillSlotWidget::StartCooldown(float Duration)
{
    StartCooldownRemaining(Duration, Duration);
}

////////////////////////////
//! \author 준혁
//! \brief 남은 시간부터 진행되는 쿨다운을 시작한다. 재등록/재접속 시 진행 중인 쿨타임을 중간부터 표시한다.
//! \param RemainingTime 남은 쿨타임(초)
//! \param Duration 전체 쿨타임(초)
void UMySkillSlotWidget::StartCooldownRemaining(float RemainingTime, float Duration)
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
        SetCooldown(RemainingTime, Duration);
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

void UMySkillSlotWidget::SetCooldown(float RemainingTime, float Duration)
{
    Duration = FMath::Max(Duration, 0.0f);
    RemainingTime = FMath::Clamp(RemainingTime, 0.0f, Duration);

    CooldownDuration = Duration;
    ApplyCooldownDisplay(RemainingTime, Duration);
}

void UMySkillSlotWidget::ClearCooldown()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    CooldownEndWorldTime = 0.0f;
    ApplyCooldownDisplay(0.0f, CooldownDuration);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 강제 쿨타임 초기화 시 WBP_SkillSlot에서 구성한 블루프린트 효과를 재생하는 함수
void UMySkillSlotWidget::PlayCooldownResetEffect()
{
    BP_OnCooldownResetEffect();
}

void UMySkillSlotWidget::SetLocked(bool bInLocked)
{
    bLocked = bInLocked;
    ApplyDisplay();
}

////////////////////////////
//! \author 준혁
//! \brief 보유 개수 텍스트를 설정한다. 빈 텍스트면 숨긴다. (아이템 퀵슬롯 표시용)
//! \param InCountText 표시할 개수 텍스트
void UMySkillSlotWidget::SetCountText(FText InCountText)
{
    if (TXT_Count)
    {
        TXT_Count->SetVisibility(InCountText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
        TXT_Count->SetText(MoveTemp(InCountText));
    }
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 드래그앤드랍 수신 허용 여부를 설정한다. 허용 시 드랍 히트테스트가 가능하도록 Visible로 바꾼다.
//! \param bInAcceptsItemDrop 수신 허용 여부
void UMySkillSlotWidget::SetAcceptsItemDrop(bool bInAcceptsItemDrop)
{
    bAcceptsItemDrop = bInAcceptsItemDrop;
    ApplyItemIconScale();

    if (bAcceptsItemDrop)
    {
        SetVisibility(ESlateVisibility::Visible);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯 드래그에 필요한 슬롯 인덱스와 현재 아이템 ID를 저장하는 함수
// InQuickSlotIndex : 이 위젯이 나타내는 퀵슬롯 인덱스
// InItemId : 현재 퀵슬롯에 등록된 아이템 ID, 빈 슬롯이면 NAME_None
void UMySkillSlotWidget::SetQuickSlotContext(int32 InQuickSlotIndex, FName InItemId)
{
    QuickSlotIndex = InQuickSlotIndex;
    QuickSlotItemId = InItemId;

    if (!QuickSlotItemId.IsNone())
    {
        ClearItemDropPreview();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 퀵슬롯 여부에 따라 IMG_Icon의 중앙 기준 표시 배율을 적용하는 함수
void UMySkillSlotWidget::ApplyItemIconScale()
{
    if (!IMG_Icon)
    {
        return;
    }

    const float AppliedScale = bAcceptsItemDrop
        ? FMath::Max(QuickSlotItemIconScale, 0.1f)
        : 1.0f;

    IMG_Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    IMG_Icon->SetRenderScale(FVector2D(AppliedScale, AppliedScale));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯 아이템을 좌클릭한 상태로 움직일 때 드래그 감지를 시작하는 함수
// InGeometry : 퀵슬롯 위젯의 지오메트리
// InMouseEvent : 마우스 입력 이벤트
// 반환값 : 드래그 감지를 시작하거나 기본 위젯 처리를 수행한 결과
FReply UMySkillSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bAcceptsItemDrop
        && QuickSlotIndex != INDEX_NONE
        && !QuickSlotItemId.IsNone()
        && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return UWidgetBlueprintLibrary::DetectDragIfPressed(
            InMouseEvent,
            this,
            EKeys::LeftMouseButton).NativeReply;
    }

    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯에서 드래그가 감지되면 아이템과 출발 슬롯 정보를 담은 드래그 오퍼레이션을 생성하는 함수
// InGeometry : 퀵슬롯 위젯의 지오메트리
// InMouseEvent : 마우스 입력 이벤트
// OutOperation : 생성한 드래그 오퍼레이션
void UMySkillSlotWidget::NativeOnDragDetected(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    if (!bAcceptsItemDrop || QuickSlotIndex == INDEX_NONE || QuickSlotItemId.IsNone())
    {
        return;
    }

    UMyItemDragDropOperation* DragOperation = NewObject<UMyItemDragDropOperation>(this);
    DragOperation->ItemId = QuickSlotItemId;
    DragOperation->ItemIcon = SkillIcon;
    DragOperation->SourceQuickSlotIndex = QuickSlotIndex;
    DragOperation->Pivot = EDragPivot::MouseDown;
    DragOperation->OnDragCancelled.AddUniqueDynamic(this, &ThisClass::HandleItemDragCancelled);

    if (SkillIcon)
    {
        DragOperation->DefaultDragVisual = UMyItemDragDropOperation::CreateItemDragVisual(
            this,
            SkillIcon,
            ItemDragVisualSize,
            InGeometry.GetLocalSize());
    }

    OutOperation = DragOperation;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 드래그가 빈 퀵슬롯에 진입하면 반투명 아이콘 미리보기를 표시하는 함수
// InGeometry : 퀵슬롯 위젯의 지오메트리
// InDragDropEvent : 드래그 이벤트
// InOperation : 현재 드래그 오퍼레이션
void UMySkillSlotWidget::NativeOnDragEnter(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

    const UMyItemDragDropOperation* ItemOperation = Cast<UMyItemDragDropOperation>(InOperation);
    if (bAcceptsItemDrop
        && QuickSlotItemId.IsNone()
        && ItemOperation
        && !ItemOperation->ItemId.IsNone())
    {
        ShowItemDropPreview(ItemOperation->ItemIcon);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 아이템 드래그가 퀵슬롯을 벗어나면 반투명 아이콘 미리보기를 제거하는 함수
// InDragDropEvent : 드래그 이벤트
// InOperation : 현재 드래그 오퍼레이션
void UMySkillSlotWidget::NativeOnDragLeave(
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    ClearItemDropPreview();
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 드래그가 슬롯 위를 지날 때 유효한 드랍 대상임을 알린다.
//! \return 아이템 드랍을 받을 수 있으면 true
bool UMySkillSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (bAcceptsItemDrop && Cast<UMyItemDragDropOperation>(InOperation))
    {
        return true;
    }

    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 드랍을 수신해 OnItemDropped로 알린다. 실제 퀵슬롯 등록은 패널이 처리한다.
//! \return 드랍을 소비했으면 true
bool UMySkillSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const UMyItemDragDropOperation* ItemOperation = Cast<UMyItemDragDropOperation>(InOperation);
    ClearItemDropPreview();

    if (bAcceptsItemDrop)
    {
        if (ItemOperation && !ItemOperation->ItemId.IsNone())
        {
            UE_LOG(LogTemp, Log, TEXT("[ItemDnD] Drop received on slot '%s' - ItemId=%s"), *GetNameSafe(this), *ItemOperation->ItemId.ToString());
            OnItemDropped.Broadcast(this, ItemOperation->ItemId, ItemOperation->SourceQuickSlotIndex);
            return true;
        }
    }
    else if (ItemOperation)
    {
        // 아이템 드랍이 왔지만 이 슬롯은 수신이 꺼져 있음 → 패널 초기화(InitializeSlots)가 이 슬롯을 못 찾은 상태
        UE_LOG(LogTemp, Warning, TEXT("[ItemDnD] Drop hit slot '%s' but bAcceptsItemDrop=false. (패널의 Slot_1~Slot_4 바인딩에서 빠진 슬롯)"), *GetNameSafe(this));
    }

    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UMySkillSlotWidget::ApplyDisplay()
{
    if (IMG_Icon)
    {
        UTexture2D* DisplayIcon = bShowingItemDropPreview
            ? ItemDropPreviewIcon.Get()
            : SkillIcon.Get();

        if (DisplayIcon)
        {
            IMG_Icon->SetBrushFromTexture(DisplayIcon);
            IMG_Icon->SetRenderOpacity(bShowingItemDropPreview ? ItemDropPreviewOpacity : 1.0f);
            IMG_Icon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            // 아이콘이 없으면 숨긴다 (빈 아이템 퀵슬롯이 이전 아이콘/기본 브러시를 보여주지 않도록)
            IMG_Icon->SetRenderOpacity(1.0f);
            IMG_Icon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 키 텍스트가 비어 있으면 WBP에서 직접 입력해 둔 TXT_Key 텍스트를 지우지 않고 유지한다.
    if (TXT_Key && !InputKeyText.IsEmpty())
    {
        TXT_Key->SetText(InputKeyText);
    }

    if (TXT_Name)
    {
        TXT_Name->SetText(SkillName);
    }

    if (IMG_Locked)
    {
        IMG_Locked->SetVisibility(bLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 빈 퀵슬롯에 드롭될 아이템 아이콘을 반투명하게 미리 표시하는 함수
// InIcon : 미리 표시할 아이템 아이콘
void UMySkillSlotWidget::ShowItemDropPreview(UTexture2D* InIcon)
{
    if (!bAcceptsItemDrop || !QuickSlotItemId.IsNone() || !InIcon)
    {
        return;
    }

    ItemDropPreviewIcon = InIcon;
    bShowingItemDropPreview = true;
    ApplyDisplay();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯에 표시 중인 반투명 드롭 미리보기를 제거하는 함수
void UMySkillSlotWidget::ClearItemDropPreview()
{
    if (!bShowingItemDropPreview && !ItemDropPreviewIcon)
    {
        return;
    }

    bShowingItemDropPreview = false;
    ItemDropPreviewIcon = nullptr;
    ApplyDisplay();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯에서 시작한 아이템이 다른 슬롯이 아닌 영역에 마우스로 드롭됐음을 패널에 알리는 함수
// Operation : 취소된 아이템 드래그 오퍼레이션
void UMySkillSlotWidget::HandleItemDragCancelled(UDragDropOperation* Operation)
{
    const UMyItemDragDropOperation* ItemOperation = Cast<UMyItemDragDropOperation>(Operation);
    if (!ItemOperation
        || !ItemOperation->WasDroppedOutsideByPointer()
        || ItemOperation->SourceQuickSlotIndex != QuickSlotIndex
        || ItemOperation->ItemId.IsNone())
    {
        return;
    }

    OnItemDroppedOutside.Broadcast(this, ItemOperation->ItemId);
}

////////////////////////////
//! \author 준혁
//! \brief 쿨다운 오버레이 색과 남은 초 텍스트의 폰트/색을 프로퍼티 값으로 적용한다.
//!        폰트는 Font Family가 지정된 경우에만 덮어쓰고, 미지정이면 WBP에서 설정한 폰트를 유지한다.
void UMySkillSlotWidget::ApplyCooldownStyle()
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

////////////////////////////
//! \author 준혁
//! \brief 쿨다운 진행 상태를 오버레이/남은 초 텍스트에 반영한다.
//!        IMG_Cooldown 브러시가 머티리얼이면 스칼라 파라미터(CooldownProgressParamName)로 라디얼 진행을,
//!        텍스처면 기존 방식(아래 기준 세로 스케일)으로 표현한다.
//! \param RemainingTime 남은 쿨타임(초)
//! \param Duration 전체 쿨타임(초)
void UMySkillSlotWidget::ApplyCooldownDisplay(float RemainingTime, float Duration)
{
    const bool bHasCooldown = Duration > 0.0f && RemainingTime > 0.0f;
    const float FillAlpha = bHasCooldown ? FMath::Clamp(RemainingTime / Duration, 0.0f, 1.0f) : 0.0f;

    if (IMG_Cooldown)
    {
        IMG_Cooldown->SetVisibility(bHasCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

        if (Cast<UMaterialInterface>(IMG_Cooldown->GetBrush().GetResourceObject()))
        {
            // GetDynamicMaterial은 최초 호출 시 MID를 만들어 브러시에 꽂아 주고 이후엔 재사용한다
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

void UMySkillSlotWidget::HandleCooldownTimerTick()
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
