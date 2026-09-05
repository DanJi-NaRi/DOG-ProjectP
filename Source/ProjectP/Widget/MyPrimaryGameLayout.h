////////////////////////////
//! \page MyPrimaryGameLayout.h
//! \brief CommonUI 기반 최상위 UI 루트 레이아웃을 정의한다.
//!

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "MyPrimaryGameLayout.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
class UOverlay;
class UUserWidget;

UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyPrimaryGameLayout : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    template <typename ActivatableWidgetT = UCommonActivatableWidget>
    ActivatableWidgetT* PushWidgetToLayerStack(FGameplayTag LayerTag, UClass* ActivatableWidgetClass)
    {
        return PushWidgetToLayerStack<ActivatableWidgetT>(LayerTag, ActivatableWidgetClass, [](ActivatableWidgetT&) {});
    }

    template <typename ActivatableWidgetT = UCommonActivatableWidget>
    ActivatableWidgetT* PushWidgetToLayerStack(
        FGameplayTag LayerTag,
        UClass* ActivatableWidgetClass,
        TFunctionRef<void(ActivatableWidgetT&)> InitInstanceFunc)
    {
        static_assert(TIsDerivedFrom<ActivatableWidgetT, UCommonActivatableWidget>::IsDerived, "Only CommonActivatableWidgets can be pushed to UI layer stacks.");

        if (!ActivatableWidgetClass)
        {
            return nullptr;
        }

        if (UCommonActivatableWidgetContainerBase* Layer = GetLayerWidget(LayerTag))
        {
            return Layer->AddWidget<ActivatableWidgetT>(ActivatableWidgetClass, InitInstanceFunc);
        }

        return nullptr;
    }

    UFUNCTION(BlueprintCallable, Category = "UI")
    void FindAndRemoveWidgetFromLayer(UCommonActivatableWidget* ActivatableWidget);

    UFUNCTION(BlueprintCallable, Category = "UI")
    UCommonActivatableWidgetContainerBase* GetLayerWidget(FGameplayTag LayerTag) const;

    //! 상주 위젯(항복 투표 패널 등)을 레이어 스택들 위 최상단에 추가한다.
    //! 활성 상주 위젯을 레이어 스택에 넣으면 CommonUI 액티브 루트를 점유해 하위 레이어의
    //! 입력 설정이 무시되므로, 스택 밖 전용 오버레이(런타임 생성)에 담는다.
    bool AddPersistentWidget(UUserWidget* Widget);

protected:
    UFUNCTION(BlueprintCallable, Category = "UI|Layer", meta = (Categories = "UI.Layer"))
    void RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget);

    void OnWidgetStackTransitioning(UCommonActivatableWidgetContainerBase* Widget, bool bIsTransitioning);

private:
    UOverlay* GetOrCreatePersistentOverlay();

    UPROPERTY(Transient, meta = (Categories = "UI.Layer"))
    TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;

    //! 상주 위젯 전용 오버레이. 루트 캔버스 마지막 자식으로 런타임 생성되어 항상 최상단에 그려진다.
    UPROPERTY(Transient)
    TObjectPtr<UOverlay> PersistentOverlay;
};
