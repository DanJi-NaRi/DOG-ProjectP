////////////////////////////
//! \page MyPrimaryGameLayout.cpp
//! \brief CommonUI 기반 최상위 UI 루트 레이아웃 기능을 구현한다.
//!

#include "MyPrimaryGameLayout.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void UMyPrimaryGameLayout::RegisterLayer(
    FGameplayTag LayerTag,
    UCommonActivatableWidgetContainerBase* LayerWidget)
{
    if (IsDesignTime() || !LayerTag.IsValid() || !LayerWidget)
    {
        return;
    }

    LayerWidget->OnTransitioningChanged.AddUObject(this, &ThisClass::OnWidgetStackTransitioning);
    LayerWidget->SetTransitionDuration(0.0);

    Layers.Add(LayerTag, LayerWidget);
}

void UMyPrimaryGameLayout::FindAndRemoveWidgetFromLayer(UCommonActivatableWidget* ActivatableWidget)
{
    if (!ActivatableWidget)
    {
        return;
    }

    for (const TPair<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>>& LayerKVP : Layers)
    {
        if (LayerKVP.Value)
        {
            LayerKVP.Value->RemoveWidget(*ActivatableWidget);
        }
    }
}

UCommonActivatableWidgetContainerBase* UMyPrimaryGameLayout::GetLayerWidget(FGameplayTag LayerTag) const
{
    return Layers.FindRef(LayerTag);
}

////////////////////////////
//! \author 준혁
//! \brief 상주 위젯을 레이어 스택들 위 최상단 오버레이에 풀스크린으로 추가하는 함수
//! \param Widget 추가할 상주 위젯
//! \return 추가 성공 여부
bool UMyPrimaryGameLayout::AddPersistentWidget(UUserWidget* Widget)
{
    UOverlay* TargetOverlay = GetOrCreatePersistentOverlay();
    if (!Widget || !TargetOverlay)
    {
        return false;
    }

    UOverlaySlot* OverlaySlot = TargetOverlay->AddChildToOverlay(Widget);
    if (!OverlaySlot)
    {
        return false;
    }

    OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
    OverlaySlot->SetVerticalAlignment(VAlign_Fill);
    return true;
}

////////////////////////////
//! \author 준혁
//! \brief 상주 위젯 전용 오버레이를 루트 캔버스 마지막 자식(최상단)으로 런타임 생성해 반환하는 함수
//! \return 상주 오버레이, 루트가 캔버스가 아니면 nullptr
UOverlay* UMyPrimaryGameLayout::GetOrCreatePersistentOverlay()
{
    if (PersistentOverlay)
    {
        return PersistentOverlay;
    }

    UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
    if (!RootCanvas || !WidgetTree)
    {
        UE_LOG(LogTemp, Warning, TEXT("PrimaryGameLayout root is not a CanvasPanel. Cannot create persistent overlay. Layout: %s"), *GetNameSafe(this));
        return nullptr;
    }

    UOverlay* NewOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Persistent"));
    if (!NewOverlay)
    {
        return nullptr;
    }

    UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(NewOverlay);
    if (!CanvasSlot)
    {
        return nullptr;
    }

    CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    CanvasSlot->SetOffsets(FMargin(0.0f));
    CanvasSlot->SetZOrder(1000);
    NewOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    PersistentOverlay = NewOverlay;
    return PersistentOverlay;
}

void UMyPrimaryGameLayout::OnWidgetStackTransitioning(UCommonActivatableWidgetContainerBase* Widget, bool bIsTransitioning)
{
}
