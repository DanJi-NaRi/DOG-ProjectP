#include "MyUIManagerSubsystem.h"

#include "MyNoticeWidget.h"
#include "MyPrimaryGameLayout.h"
#include "../MyGameplayTags.h"
#include "MyActivatableWidget.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "GameFramework/PlayerController.h"

void UMyUIManagerSubsystem::Deinitialize()
{
    // LocalPlayer 파기(=PIE 종료/트래블) 시 PrimaryLayout을 뷰포트에서 내리고 참조를 끊는다.
    // 이 경로가 없으면 위젯이 세션 간 잔존하며, 에디터에서는 GameInstance GC를 막아
    // PlayLevel.cpp의 "PIE object still referenced" assert로 이어질 수 있다.
    NoticeWidget = nullptr;

    if (PrimaryLayout)
    {
        if (PrimaryLayout->IsInViewport())
        {
            PrimaryLayout->RemoveFromParent();
        }
        PrimaryLayout = nullptr;
    }

    Super::Deinitialize();
}

UMyPrimaryGameLayout* UMyUIManagerSubsystem::EnsurePrimaryLayout(APlayerController* OwningPlayer)
{
    TSubclassOf<UMyPrimaryGameLayout> LoadedLayoutClass = PrimaryLayoutClass.LoadSynchronous();
    return EnsurePrimaryLayoutUsingClass(OwningPlayer, LoadedLayoutClass);
}

UMyPrimaryGameLayout* UMyUIManagerSubsystem::EnsurePrimaryLayoutUsingClass(
    APlayerController* OwningPlayer,
    TSubclassOf<UMyPrimaryGameLayout> LayoutClass)
{
    if (!OwningPlayer || !OwningPlayer->IsLocalPlayerController())
    {
        return nullptr;
    }

    UClass* LayoutUClass = *LayoutClass;
    if (!LayoutUClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("PrimaryLayoutClass is not set."));
        return nullptr;
    }

    const bool bCanReusePrimaryLayout =
        PrimaryLayout &&
        PrimaryLayout->IsInViewport() &&
        PrimaryLayout->GetOwningPlayer() == OwningPlayer &&
        PrimaryLayout->GetClass() == LayoutUClass;

    if (bCanReusePrimaryLayout)
    {
        return PrimaryLayout;
    }

    if (PrimaryLayout)
    {
        NoticeWidget = nullptr;
        PrimaryLayout->RemoveFromParent();
        PrimaryLayout = nullptr;
    }

    PrimaryLayout = CreateWidget<UMyPrimaryGameLayout>(OwningPlayer, LayoutUClass);
    if (PrimaryLayout)
    {
        PrimaryLayout->AddToPlayerScreen(1000);
    }

    return PrimaryLayout;
}

UMyPrimaryGameLayout* UMyUIManagerSubsystem::GetPrimaryLayout() const
{
    return PrimaryLayout;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// PersistentOverlay에 Notice 위젯을 한 개만 생성하거나 기존 위젯을 재사용하는 함수
// OwningPlayer : Notice 위젯을 소유할 로컬 플레이어 컨트롤러
// WidgetClass : 생성할 WBP_Notice 클래스
// Return Value : 생성하거나 재사용한 Notice 위젯, 실패 시 nullptr
UMyNoticeWidget* UMyUIManagerSubsystem::EnsureNoticeWidget(
    APlayerController* OwningPlayer,
    TSubclassOf<UMyNoticeWidget> WidgetClass)
{
    UClass* NoticeWidgetClass = *WidgetClass;
    if (!PrimaryLayout ||
        !OwningPlayer ||
        !OwningPlayer->IsLocalPlayerController() ||
        PrimaryLayout->GetOwningPlayer() != OwningPlayer ||
        !NoticeWidgetClass)
    {
        return nullptr;
    }

    const bool bCanReuseNoticeWidget =
        IsValid(NoticeWidget.Get()) &&
        NoticeWidget->GetParent() != nullptr &&
        NoticeWidget->GetOwningPlayer() == OwningPlayer &&
        NoticeWidget->GetClass() == NoticeWidgetClass;

    if (bCanReuseNoticeWidget)
    {
        return NoticeWidget;
    }

    if (IsValid(NoticeWidget.Get()))
    {
        NoticeWidget->RemoveFromParent();
    }
    NoticeWidget = nullptr;

    NoticeWidget = Cast<UMyNoticeWidget>(AddPersistentWidget(WidgetClass));
    if (!NoticeWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to ensure notice widget. Class: %s"), *GetNameSafe(NoticeWidgetClass));
    }

    return NoticeWidget;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생성된 Notice 위젯에 일반 시스템 메시지 표시를 요청하는 함수
// Message : 표시할 시스템 메시지
// DurationSeconds : 메시지를 표시할 시간, 0 이하이면 위젯 기본값 사용
void UMyUIManagerSubsystem::ShowNotice(const FText& Message, float DurationSeconds)
{
    if (!IsValid(NoticeWidget.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowNotice failed because NoticeWidget is not initialized."));
        return;
    }

    NoticeWidget->ShowNotice(Message, DurationSeconds);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생성된 Notice 위젯에 표시 종류와 선택적 Rich Text가 포함된 데이터 표시를 요청하는 함수
// NoticeData : 일반 문구, Rich Text 문구, 표시 시간, 시각 표현 종류를 담은 데이터
void UMyUIManagerSubsystem::ShowNoticeData(const FMyNoticeData& NoticeData)
{
    if (!IsValid(NoticeWidget.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowNoticeData failed because NoticeWidget is not initialized."));
        return;
    }

    NoticeWidget->ShowNoticeData(NoticeData);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생성된 Notice 위젯에 서버 시간 기준 카운트다운 표시를 요청하는 함수
// MessageFormat : 남은 초가 {0} 위치에 들어가는 메시지 형식
// EndServerTime : 카운트다운이 끝나는 서버 월드 시간
void UMyUIManagerSubsystem::ShowCountdownNotice(const FText& MessageFormat, float EndServerTime)
{
    if (!IsValid(NoticeWidget.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("ShowCountdownNotice failed because NoticeWidget is not initialized."));
        return;
    }

    NoticeWidget->ShowCountdownNotice(MessageFormat, EndServerTime);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 생성된 Notice 위젯의 현재 메시지와 대기열을 정리하는 함수
void UMyUIManagerSubsystem::ClearNotice()
{
    if (IsValid(NoticeWidget.Get()))
    {
        NoticeWidget->ClearNotice();
    }
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushWidgetToLayerStack(
    FGameplayTag LayerTag,
    TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    if (!PrimaryLayout || !WidgetClass)
    {
        return nullptr;
    }

    return PrimaryLayout->PushWidgetToLayerStack<UMyActivatableWidget>(LayerTag, *WidgetClass);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로비 기본 레이어에 Activatable 위젯을 추가하는 함수
// WidgetClass : 로비 기본 레이어에 추가할 위젯 클래스
// Return Value : 스택에 추가된 위젯, 실패 시 nullptr
UMyActivatableWidget* UMyUIManagerSubsystem::PushLobby(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Lobby, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushHUD(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_HUD, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushMenu(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Menu, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushModal(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Modal, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushDialogue(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Dialogue, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushToast(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Toast, WidgetClass);
}

UMyActivatableWidget* UMyUIManagerSubsystem::PushOverlay(TSubclassOf<UMyActivatableWidget> WidgetClass)
{
    return PushWidgetToLayerStack(MyGameplayTags::UI_Layer_Overlay, WidgetClass);
}

void UMyUIManagerSubsystem::RemoveWidgetFromLayer(UMyActivatableWidget* ActivatableWidget)
{
    if (PrimaryLayout)
    {
        PrimaryLayout->FindAndRemoveWidgetFromLayer(ActivatableWidget);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 레이어 스택의 모든 위젯을 닫는 함수. 대화 시작 시 Menu 레이어(상점/인벤토리) 정리에 사용한다.
//! \param LayerTag 정리할 UI 레이어 태그
void UMyUIManagerSubsystem::ClearLayer(FGameplayTag LayerTag)
{
    UCommonActivatableWidgetContainerBase* Layer = PrimaryLayout ? PrimaryLayout->GetLayerWidget(LayerTag) : nullptr;
    if (Layer)
    {
        Layer->ClearWidgets();
    }
}

////////////////////////////
//! \author 준혁
//! \brief 레이어 스택 자체를 숨기거나 원래 Visibility로 복원하는 함수. 대화 중 HUD 숨김에 사용한다.
//! \param LayerTag 대상 UI 레이어 태그
//! \param bVisible true면 저장된 원래 Visibility로 복원, false면 Collapsed로 숨김
void UMyUIManagerSubsystem::SetLayerVisible(FGameplayTag LayerTag, bool bVisible)
{
    UCommonActivatableWidgetContainerBase* Layer = PrimaryLayout ? PrimaryLayout->GetLayerWidget(LayerTag) : nullptr;
    if (!Layer)
    {
        return;
    }

    if (bVisible)
    {
        const ESlateVisibility* SavedVisibility = SavedLayerVisibilities.Find(LayerTag);
        Layer->SetVisibility(SavedVisibility ? *SavedVisibility : ESlateVisibility::SelfHitTestInvisible);
        SavedLayerVisibilities.Remove(LayerTag);
        return;
    }

    if (!SavedLayerVisibilities.Contains(LayerTag))
    {
        SavedLayerVisibilities.Add(LayerTag, Layer->GetVisibility());
    }
    Layer->SetVisibility(ESlateVisibility::Collapsed);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이어 스택에서 현재 표시 중인 위젯의 CommonUI 활성 상태를 변경하는 함수
// LayerTag : 활성 상태를 변경할 UI 레이어 태그
// bActive : true면 활성화, false면 비활성화
void UMyUIManagerSubsystem::SetLayerActive(FGameplayTag LayerTag, bool bActive)
{
    UCommonActivatableWidgetContainerBase* Layer = PrimaryLayout ? PrimaryLayout->GetLayerWidget(LayerTag) : nullptr;
    UCommonActivatableWidget* ActiveWidget = Layer ? Layer->GetActiveWidget() : nullptr;
    if (!ActiveWidget)
    {
        return;
    }

    if (bActive)
    {
        if (!ActiveWidget->IsActivated())
        {
            ActiveWidget->ActivateWidget();
        }
        return;
    }

    if (ActiveWidget->IsActivated())
    {
        ActiveWidget->DeactivateWidget();
    }
}

////////////////////////////
//! \author 준혁
//! \brief 상주 위젯을 생성해 PrimaryLayout의 최상단 Persistent 오버레이에 추가하는 함수
//! \param WidgetClass 생성할 위젯 클래스
//! \return 생성된 위젯, 실패 시 nullptr
UUserWidget* UMyUIManagerSubsystem::AddPersistentWidget(TSubclassOf<UUserWidget> WidgetClass)
{
    if (!PrimaryLayout || !WidgetClass)
    {
        return nullptr;
    }

    APlayerController* OwningPlayer = PrimaryLayout->GetOwningPlayer();
    UUserWidget* NewWidget = CreateWidget<UUserWidget>(OwningPlayer, WidgetClass);
    if (!NewWidget || !PrimaryLayout->AddPersistentWidget(NewWidget))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to add persistent widget. Class: %s"), *GetNameSafe(*WidgetClass));
        return nullptr;
    }

    return NewWidget;
}
