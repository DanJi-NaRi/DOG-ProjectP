#include "MyInteractionGuideWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Dungeon/Dialogue/MyDialogueChoiceButton.h"
#include "Dungeon/Interactable/Components/InteractableComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/Components/PlayerInteractionComponent.h"
#include "TimerManager.h"

////////////////////////////
//! \author 준혁
//! \brief 기본 숨김으로 시작하고, 소유 폰의 상호작용 컴포넌트를 주기적으로 탐색하는 타이머를 시작하는 함수.
//!        (던전 입장 시 인증 후 폰이 교체되므로 1회 탐색으로는 부족하다)
void UMyInteractionGuideWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetVisibility(ESlateVisibility::Collapsed);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AcquireTimerHandle, this,
            &UMyInteractionGuideWidget::AcquireInteractionComponent, AcquireInterval, true, 0.0f);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 탐색 타이머를 정리하고 이벤트 바인딩을 해제하는 함수
void UMyInteractionGuideWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AcquireTimerHandle);
    }

    if (BoundInteractionComponent)
    {
        BoundInteractionComponent->OnInteractionEntriesChanged.RemoveDynamic(this, &UMyInteractionGuideWidget::HandleEntriesChanged);
        BoundInteractionComponent->OnInteractionOptionSelectionChanged.RemoveDynamic(this, &UMyInteractionGuideWidget::HandleSelectionChanged);
        BoundInteractionComponent = nullptr;
    }

    Super::NativeDestruct();
}

////////////////////////////
//! \author 준혁
//! \brief 소유 폰의 상호작용 컴포넌트를 찾아 후보/선택 변경 이벤트를 (재)바인딩하는 함수.
//!        폰이 교체되어 컴포넌트가 바뀌면 이전 바인딩을 해제하고 새로 바인딩한 뒤 표시를 갱신한다.
void UMyInteractionGuideWidget::AcquireInteractionComponent()
{
    const APawn* OwningPawn = GetOwningPlayerPawn();
    UPlayerInteractionComponent* NewComponent = OwningPawn ? OwningPawn->FindComponentByClass<UPlayerInteractionComponent>() : nullptr;

    if (NewComponent == BoundInteractionComponent)
    {
        return;
    }

    if (BoundInteractionComponent)
    {
        BoundInteractionComponent->OnInteractionEntriesChanged.RemoveDynamic(this, &UMyInteractionGuideWidget::HandleEntriesChanged);
        BoundInteractionComponent->OnInteractionOptionSelectionChanged.RemoveDynamic(this, &UMyInteractionGuideWidget::HandleSelectionChanged);
    }

    BoundInteractionComponent = NewComponent;

    if (BoundInteractionComponent)
    {
        BoundInteractionComponent->OnInteractionEntriesChanged.AddUniqueDynamic(this, &UMyInteractionGuideWidget::HandleEntriesChanged);
        BoundInteractionComponent->OnInteractionOptionSelectionChanged.AddUniqueDynamic(this, &UMyInteractionGuideWidget::HandleSelectionChanged);
    }

    RebuildOptions();
}

////////////////////////////
//! \author 준혁
//! \brief 주변 후보 또는 후보의 옵션이 바뀌면 통합 항목 버튼 목록을 다시 만드는 함수
void UMyInteractionGuideWidget::HandleEntriesChanged()
{
    RebuildOptions();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 소진 옵션이 숨겨지면 버튼 위치와 옵션 인덱스가 어긋나므로 버튼이 담당하는 실제 옵션 인덱스로 비교
//! \brief 선택된 옵션이 바뀌면 각 버튼의 선택 표시(상호작용 가이드 이미지)를 갱신하는 함수
//! \param NewSelectedIndex 새로 선택된 옵션 인덱스 (옵션이 없으면 INDEX_NONE)
void UMyInteractionGuideWidget::HandleSelectionChanged(int32 NewSelectedIndex)
{
    for (UMyDialogueChoiceButton* OptionButton : OptionButtons)
    {
        if (OptionButton)
        {
            OptionButton->SetSelected(OptionButton->GetChoiceIndex() == NewSelectedIndex);
        }
    }
}

////////////////////////////
//! \author 준혁
//! \brief 항목 버튼에 마우스가 올라오면 선택 상태를 해당 통합 항목으로 옮기는 함수
//! \param EntryIndex 호버된 통합 상호작용 항목 인덱스
void UMyInteractionGuideWidget::HandleEntryHovered(int32 EntryIndex)
{
    if (BoundInteractionComponent)
    {
        BoundInteractionComponent->SetSelectedInteractionEntry(EntryIndex);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 항목 버튼 클릭을 상호작용 확정으로 처리하는 함수. (F키 확정과 동일한 TryInteract 경로)
//! \param EntryIndex 클릭된 통합 상호작용 항목 인덱스
void UMyInteractionGuideWidget::HandleEntryClicked(int32 EntryIndex)
{
    if (BoundInteractionComponent)
    {
        BoundInteractionComponent->SetSelectedInteractionEntry(EntryIndex);
        BoundInteractionComponent->TryInteract();
    }
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 옵션 미등록 액터도 기본 옵션 1개로 가이드가 뜨도록 유효 옵션 기준으로 변경
//! \brief 주변 모든 후보의 통합 상호작용 항목 목록으로부터 버튼들을 다시 만드는 함수. 항목이 없으면 위젯 전체를 숨긴다.
//!        상호작용 가능한 모든 액터 근처에서 가이드가 뜬다(옵션 미등록 액터는 기본 옵션 1개).
//!        버튼은 대화 선택지와 동일한 UMyDialogueChoiceButton을 재사용한다.
void UMyInteractionGuideWidget::RebuildOptions()
{
    OptionButtons.Reset();

    if (!PNL_Options)
    {
        return;
    }

    PNL_Options->ClearChildren();

    const TArray<FPlayerInteractionEntry>* Entries = BoundInteractionComponent
        ? &BoundInteractionComponent->GetInteractionEntries()
        : nullptr;

    if (!Entries || Entries->IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!OptionButtonClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("OptionButtonClass is not set. Assign WBP_DialogueChoiceButton in WBP_InteractionGuide defaults."));
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 루트는 히트 대상에서 제외해 버튼 이외 영역이 게임 클릭을 가로채지 않게 한다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // 컴포넌트가 이미 사용할 수 있는 항목만 펼쳐 두므로 버튼 인덱스는 통합 항목 인덱스와 일치한다.
    for (int32 EntryIndex = 0; EntryIndex < Entries->Num(); ++EntryIndex)
    {
        UMyDialogueChoiceButton* OptionButton = CreateWidget<UMyDialogueChoiceButton>(GetOwningPlayer(), OptionButtonClass);
        if (!OptionButton)
        {
            continue;
        }

        OptionButton->SetupChoice(EntryIndex, (*Entries)[EntryIndex].DisplayText);
        OptionButton->SetUseSelectedHoverVisual(true);
        OptionButton->OnChoiceClicked.BindUObject(this, &UMyInteractionGuideWidget::HandleEntryClicked);
        OptionButton->OnChoiceHovered.BindUObject(this, &UMyInteractionGuideWidget::HandleEntryHovered);

        UPanelSlot* OptionSlot = PNL_Options->AddChild(OptionButton);
        if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(OptionSlot))
        {
            VerticalBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, OptionButtonSpacing));
        }

        OptionButtons.Add(OptionButton);
    }

    // 모든 옵션이 소진돼 표시할 버튼이 없으면 숨긴다. (보통은 CanInteract가 후보에서 먼저 제외한다)
    if (OptionButtons.IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    HandleSelectionChanged(BoundInteractionComponent->GetSelectedInteractionEntryIndex());
}
