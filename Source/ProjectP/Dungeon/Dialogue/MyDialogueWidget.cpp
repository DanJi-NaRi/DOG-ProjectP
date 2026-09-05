#include "MyDialogueWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Dungeon/Dialogue/CPP_ObeliskActor.h"
#include "Dungeon/Dialogue/MyDialogueChoiceButton.h"
#include "Dungeon/Dialogue/MyDialogueDataAsset.h"
#include "Dungeon/DungeonPC.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "God/MyGodPresentationTypes.h"
#include "MyGameplayTags.h"
#include "Player/Components/PlayerInteractionComponent.h"
#include "Widget/MyUIManagerSubsystem.h"

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief 대화 방은 비전투 확정이므로 게임 입력을 차단하는 메뉴 입력 모드로 설정하는 생성자.
//!        F키(선택지 확정)를 위젯이 직접 받을 수 있도록 키보드 포커스를 허용한다.
UMyDialogueWidget::UMyDialogueWidget()
{
    InputMode = EMyWidgetInputMode::Menu;
    SetIsFocusable(true);
}

////////////////////////////
//! \author 준혁
//! \brief 대화를 시작하고 첫 줄을 표시하는 함수
//! \param InDialogue 표시할 대화 데이터에셋
//! \param InSourceObelisk 대화 출처 오벨리스크. 선택지의 기믹 트리거 통지 대상 (없으면 nullptr)
void UMyDialogueWidget::StartDialogue(const UMyDialogueDataAsset* InDialogue, ACPP_ObeliskActor* InSourceObelisk)
{
    ActiveDialogue = InDialogue;
    SourceObelisk = InSourceObelisk;
    CurrentLineIndex = 0;

    if (!ActiveDialogue || ActiveDialogue->Lines.IsEmpty())
    {
        DeactivateWidget();
        return;
    }

    ShowCurrentLine();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief 대화 표시 중 상점/오벨리스크 등 새 상호작용 시작을 차단하고 키보드 포커스를 가져오는 함수
void UMyDialogueWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    SetInteractionBlocked(true);
    SetKeyboardFocus();
}

////////////////////////////
//! \author 준혁
//! \brief CommonUI가 활성화 시 포커스를 줄 대상으로 이 위젯 자신을 반환하는 함수.
//!        선택지 버튼이 아닌 위젯이 포커스를 들고 F키/휠 입력을 중앙에서 처리한다.
//! \return 이 위젯
UWidget* UMyDialogueWidget::NativeGetDesiredFocusTarget() const
{
    return const_cast<UMyDialogueWidget*>(this);
}

////////////////////////////
//! \author 준혁
//! \brief 대화 종료 시 상호작용 차단을 풀고 HUD 레이어를 복원하며 진행 중이던 상호작용 세션을 종료하는 함수
void UMyDialogueWidget::NativeOnDeactivated()
{
    ActiveDialogue = nullptr;
    SourceObelisk = nullptr;
    ChoiceButtons.Reset();
    SelectedChoiceIndex = INDEX_NONE;
    RequestEndInteraction();
    SetInteractionBlocked(false);
    RestoreHUDLayer();

    Super::NativeOnDeactivated();
}

////////////////////////////
//! \author 준혁
//! \brief 좌클릭으로 대화를 넘기는 함수. 선택지가 표시된 줄에서는 넘기지 않는다(클릭은 소비해 게임 입력 누출 방지).
//! \param InGeometry 위젯 지오메트리
//! \param InMouseEvent 마우스 이벤트
//! \return 좌클릭이면 Handled
FReply UMyDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (!CurrentLineHasChoices())
        {
            AdvanceDialogue();
        }
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 빠른 연속 클릭(광클)은 더블클릭 이벤트로 들어오므로, 게임 입력(기본공격)으로 새지 않게
//!        더블클릭도 대화 넘기기로 처리하는 함수. 선택지 줄에서는 마찬가지로 넘기지 않는다.
//! \param InGeometry 위젯 지오메트리
//! \param InMouseEvent 마우스 이벤트
//! \return 좌클릭이면 Handled
FReply UMyDialogueWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (!CurrentLineHasChoices())
        {
            AdvanceDialogue();
        }
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 마우스 휠로 선택지 선택을 위/아래로 옮기는 함수. 휠 업이면 위 선택지, 휠 다운이면 아래 선택지.
//!        끝에서 더 돌리면 반대편으로 순환한다. 선택지가 없는 줄에서는 처리하지 않는다.
//! \param InGeometry 위젯 지오메트리
//! \param InMouseEvent 마우스 이벤트(휠 델타 포함)
//! \return 선택지가 표시 중이면 Handled
FReply UMyDialogueWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (CurrentLineHasChoices() && !ChoiceButtons.IsEmpty())
    {
        StepSelectedChoice(InMouseEvent.GetWheelDelta() > 0.0f ? -1 : 1);
        return FReply::Handled();
    }

    return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief F키/스페이스바/W/S 입력을 처리하는 함수.
//!        일반 줄에서는 F/스페이스바로 대화 넘기기, 선택지 줄에서는 F키(고정 상호작용 키)만 선택 확정.
//!        선택지 줄에서 W/S는 휠과 동일하게 선택을 위/아래로 옮긴다(순환).
//!        키를 꾹 누르면 발생하는 반복 입력은 무시해 대화가 한 번에 여러 줄 넘어가는 것을 막는다.
//! \param InGeometry 위젯 지오메트리
//! \param InKeyEvent 키 이벤트
//! \return F키/스페이스바/W/S를 소비하면 Handled
FReply UMyDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();

    if (CurrentLineHasChoices() && (Key == EKeys::W || Key == EKeys::S))
    {
        if (!InKeyEvent.IsRepeat())
        {
            StepSelectedChoice(Key == EKeys::W ? -1 : 1);
        }
        return FReply::Handled();
    }

    if (Key == EKeys::F || Key == EKeys::SpaceBar)
    {
        if (InKeyEvent.IsRepeat())
        {
            return FReply::Handled();
        }

        if (CurrentLineHasChoices())
        {
            // 선택지 줄에서 스페이스바는 오입력 방지를 위해 소비만 하고 아무것도 하지 않는다.
            if (Key == EKeys::F && SelectedChoiceIndex != INDEX_NONE)
            {
                HandleChoiceSelected(SelectedChoiceIndex);
            }
        }
        else
        {
            AdvanceDialogue();
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

////////////////////////////
//! \author 준혁
//! \brief 현재 줄의 화자 이름, 텍스트, 화자 이미지를 표시하는 함수. 이미지가 비어 있는 줄에서는 이미지를 숨긴다.
void UMyDialogueWidget::ShowCurrentLine()
{
    if (!ActiveDialogue || !ActiveDialogue->Lines.IsValidIndex(CurrentLineIndex))
    {
        return;
    }

    const FMyDialogueLine& Line = ActiveDialogue->Lines[CurrentLineIndex];

    // 화자가 신이면 DT_GodPresentation의 이름과 초상화가 줄에 직접 넣은 값보다 우선한다.
    FText SpeakerName = Line.SpeakerName;
    TSoftObjectPtr<UTexture2D> SpeakerImage = Line.SpeakerImage;
    if (Line.SpeakerGodTag.IsValid())
    {
        if (!GodPresentationTable)
        {
            GodPresentationTable = MyGodPresentation::LoadDefaultTable();
        }

        if (const FMyGodPresentationRow* Presentation =
            MyGodPresentation::FindByTag(GodPresentationTable, Line.SpeakerGodTag))
        {
            SpeakerName = Presentation->DisplayName;
            SpeakerImage = Presentation->FullPortrait.IsNull() ? Presentation->Icon : Presentation->FullPortrait;
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("DT_GodPresentation에 SpeakerGodTag 행이 없습니다: %s"),
                *Line.SpeakerGodTag.ToString());
        }
    }

    if (TXT_Name)
    {
        TXT_Name->SetText(SpeakerName);
    }

    if (TXT_Dialogue)
    {
        TXT_Dialogue->SetText(Line.Text);
    }

    UTexture2D* SpeakerTexture = SpeakerImage.IsNull() ? nullptr : SpeakerImage.LoadSynchronous();
    if (IMG_Speaker && SpeakerTexture)
    {
        IMG_Speaker->SetBrushFromTexture(SpeakerTexture, true);
    }
    SetSpeakerImageVisibility(SpeakerTexture != nullptr);

    RebuildChoices();
}

////////////////////////////
//! \author 준혁
//! \brief 현재 줄에 선택지가 있는지 판정하는 함수. 선택지 줄에서는 좌클릭 진행이 막힌다.
//! \return 선택지가 있으면 true
bool UMyDialogueWidget::CurrentLineHasChoices() const
{
    return ActiveDialogue
        && ActiveDialogue->Lines.IsValidIndex(CurrentLineIndex)
        && !ActiveDialogue->Lines[CurrentLineIndex].Choices.IsEmpty();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief 현재 줄의 선택지 버튼들을 다시 만드는 함수. 선택지가 없는 줄에서는 컨테이너를 숨긴다.
//!        선택지가 있으면 항상 첫 번째 선택지를 선택 상태로 시작하고, 키보드 포커스를 위젯으로 되가져온다.
void UMyDialogueWidget::RebuildChoices()
{
    ChoiceButtons.Reset();
    SelectedChoiceIndex = INDEX_NONE;

    if (!PNL_Choices)
    {
        if (CurrentLineHasChoices())
        {
            UE_LOG(LogTemp, Warning, TEXT("Dialogue line has choices but PNL_Choices is not bound. Add PNL_Choices to WBP_Dialogue."));
        }
        return;
    }

    PNL_Choices->ClearChildren();

    if (!CurrentLineHasChoices())
    {
        PNL_Choices->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!ChoiceButtonClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("ChoiceButtonClass is not set. Assign WBP_DialogueChoiceButton in WBP_Dialogue defaults."));
        PNL_Choices->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    PNL_Choices->SetVisibility(ESlateVisibility::Visible);

    const TArray<FMyDialogueChoice>& Choices = ActiveDialogue->Lines[CurrentLineIndex].Choices;
    for (int32 ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
    {
        UMyDialogueChoiceButton* ChoiceButton = CreateWidget<UMyDialogueChoiceButton>(GetOwningPlayer(), ChoiceButtonClass);
        if (!ChoiceButton)
        {
            continue;
        }

        ChoiceButton->SetupChoice(ChoiceIndex, Choices[ChoiceIndex].Text);
        ChoiceButton->OnChoiceClicked.BindUObject(this, &UMyDialogueWidget::HandleChoiceSelected);
        ChoiceButton->OnChoiceHovered.BindUObject(this, &UMyDialogueWidget::SetSelectedChoice);

        UPanelSlot* ChoiceSlot = PNL_Choices->AddChild(ChoiceButton);
        if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(ChoiceSlot))
        {
            const float BottomPadding = ChoiceIndex < Choices.Num() - 1 ? ChoiceButtonSpacing : 0.0f;
            VerticalBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
        }

        ChoiceButtons.Add(ChoiceButton);
    }

    if (!ChoiceButtons.IsEmpty())
    {
        // 선택지가 표시되는 동안에는 항상 하나가 선택 상태여야 한다. 첫 선택지로 시작.
        SetSelectedChoice(0);

        // 직전 선택지 클릭으로 포커스가 (지금은 제거된) 버튼에 넘어갔을 수 있으므로 F키 입력을 위해 되찾는다.
        SetKeyboardFocus();
    }
}

////////////////////////////
//! \author 준혁
//! \brief 선택 상태를 지정한 선택지로 옮기는 함수. 각 버튼의 상호작용 가이드 표시를 갱신한다.
//!        호버 델리게이트에도 직접 바인딩되어 마우스 호버로도 선택이 옮겨진다.
//! \param NewIndex 새로 선택할 선택지 인덱스
void UMyDialogueWidget::SetSelectedChoice(int32 NewIndex)
{
    if (!ChoiceButtons.IsValidIndex(NewIndex))
    {
        return;
    }

    SelectedChoiceIndex = NewIndex;

    for (int32 ButtonIndex = 0; ButtonIndex < ChoiceButtons.Num(); ++ButtonIndex)
    {
        if (ChoiceButtons[ButtonIndex])
        {
            ChoiceButtons[ButtonIndex]->SetSelected(ButtonIndex == NewIndex);
        }
    }
}

////////////////////////////
//! \author 준혁
//! \brief 선택 상태를 위/아래로 한 칸 옮기는 함수. 끝을 넘어가면 반대편으로 순환한다.
//! \param Delta 이동 방향(-1: 위, +1: 아래)
void UMyDialogueWidget::StepSelectedChoice(int32 Delta)
{
    if (ChoiceButtons.IsEmpty())
    {
        return;
    }

    const int32 BaseIndex = (SelectedChoiceIndex != INDEX_NONE) ? SelectedChoiceIndex : 0;
    const int32 Count = ChoiceButtons.Num();
    SetSelectedChoice(((BaseIndex + Delta) % Count + Count) % Count);
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 대화를 끝내는 선택지(범위 밖 이동)에서 완주 통지 추가
//! \brief 선택지 선택을 처리하는 함수. 기믹 트리거 선택지면 출처 오벨리스크에 서버 RPC로 통지하고,
//!        NextLineIndex에 따라 분기 이동한다(-1이면 다음 줄, 범위 밖이면 대화 종료).
//!        진행은 로컬 전용이라 파티 대화에서는 각자 선택하며, 기믹 트리거는 서버 래치로 1회만 발동한다.
//! \param ChoiceIndex 고른 선택지 인덱스
void UMyDialogueWidget::HandleChoiceSelected(int32 ChoiceIndex)
{
    if (!ActiveDialogue || !ActiveDialogue->Lines.IsValidIndex(CurrentLineIndex))
    {
        return;
    }

    const TArray<FMyDialogueChoice>& Choices = ActiveDialogue->Lines[CurrentLineIndex].Choices;
    if (!Choices.IsValidIndex(ChoiceIndex))
    {
        return;
    }

    const FMyDialogueChoice& Choice = Choices[ChoiceIndex];

    if ((Choice.bTriggersGimmick || Choice.bStartsGimmickResetVote) && SourceObelisk.IsValid())
    {
        if (ADungeonPC* DungeonPC = GetOwningPlayer<ADungeonPC>())
        {
            DungeonPC->ServerNotifyDialogueChoice(SourceObelisk.Get(), CurrentLineIndex, ChoiceIndex);
        }
    }

    const int32 NextIndex = (Choice.NextLineIndex >= 0) ? Choice.NextLineIndex : CurrentLineIndex + 1;
    if (!ActiveDialogue->Lines.IsValidIndex(NextIndex))
    {
        // 대화를 끝내는 선택지도 끝까지 본 것(완주)으로 판정한다.
        NotifyDialogueCompleted();
        DeactivateWidget();
        return;
    }

    CurrentLineIndex = NextIndex;
    ShowCurrentLine();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 마지막 줄을 넘겨 닫힐 때 완주 통지 추가
//! \brief 다음 줄로 넘어가고, 마지막 줄이었으면 완주를 통지하고 대화를 닫는 함수
void UMyDialogueWidget::AdvanceDialogue()
{
    ++CurrentLineIndex;

    if (!ActiveDialogue || CurrentLineIndex >= ActiveDialogue->Lines.Num())
    {
        if (ActiveDialogue)
        {
            // 진행을 통해 끝에 도달한 경우만 완주다. (새 대화 교체 등 외부 닫힘은 완주가 아니다)
            NotifyDialogueCompleted();
        }
        DeactivateWidget();
        return;
    }

    ShowCurrentLine();
}

////////////////////////////
//! \author 준혁
//! \brief 대화를 마지막 줄까지 보고 끝냈음을 출처 오벨리스크에 서버 RPC로 통지하는 함수.
//!        오벨리스크의 트리거 타이밍이 '대화를 끝까지 봤을 때'가 아니면 보내지 않는다(서버가 재검증한다).
//!        상호작용 종료(NativeOnDeactivated의 RequestEndInteraction)보다 먼저 호출해야
//!        서버의 활성 대화 세션이 정리되기 전에 검증된다. 파티 대화를 받은 비점유자의 통지는 서버가 거절한다.
void UMyDialogueWidget::NotifyDialogueCompleted()
{
    if (!SourceObelisk.IsValid()
        || SourceObelisk->GetGimmickTriggerTiming() != EObeliskGimmickTriggerTiming::OnDialogueCompleted)
    {
        return;
    }

    if (ADungeonPC* DungeonPC = GetOwningPlayer<ADungeonPC>())
    {
        DungeonPC->ServerNotifyDialogueCompleted(SourceObelisk.Get());
    }
}

////////////////////////////
//! \author 준혁
//! \brief 화자 이미지 표시를 토글하는 함수. 이미지 없는 줄에서도 레이아웃이 흔들리지 않도록
//!        Hidden(안 보이지만 자리는 유지)을 사용한다.
//! \param bVisible 화자 이미지 표시 여부
void UMyDialogueWidget::SetSpeakerImageVisibility(bool bVisible)
{
    if (IMG_Speaker)
    {
        IMG_Speaker->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 대화 표시 중 새 상호작용 시작을 차단하거나 해제하는 함수
//! \param bBlocked 차단 여부
void UMyDialogueWidget::SetInteractionBlocked(bool bBlocked)
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UPlayerInteractionComponent* InteractionComponent = OwningPawn ? OwningPawn->FindComponentByClass<UPlayerInteractionComponent>() : nullptr;
    if (InteractionComponent)
    {
        InteractionComponent->SetInteractionBlocked(bBlocked);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 대화 시작 시 숨겼던 HUD 레이어를 되돌리는 함수
void UMyDialogueWidget::RestoreHUDLayer()
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
//! \brief 오벨리스크 상호작용으로 시작된 세션이 남아있으면 종료하는 함수. (상점 위젯과 동일 패턴)
//!        파티 대화로 받은 비상호작용자는 진행 중 세션이 없어 아무것도 하지 않는다.
void UMyDialogueWidget::RequestEndInteraction()
{
    APawn* OwningPawn = GetOwningPlayerPawn();
    UPlayerInteractionComponent* InteractionComponent = OwningPawn ? OwningPawn->FindComponentByClass<UPlayerInteractionComponent>() : nullptr;

    // TryInteract는 진행 중인 상호작용이 있으면 종료를 요청하는 토글이다. 진행 중일 때만 호출해 새 시작을 막는다.
    if (InteractionComponent && InteractionComponent->IsInteracting())
    {
        InteractionComponent->TryInteract();
    }
}
