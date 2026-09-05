#include "MyDialogueChoiceButton.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief 버튼 클릭/호버 이벤트를 바인딩하고 현재 선택 상태를 가이드 표시에 반영하는 함수
void UMyDialogueChoiceButton::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Choice)
    {
        BTN_Choice->OnClicked.AddUniqueDynamic(this, &UMyDialogueChoiceButton::HandleClicked);
        BTN_Choice->OnHovered.AddUniqueDynamic(this, &UMyDialogueChoiceButton::HandleHovered);

        if (!bDefaultButtonStyleCached)
        {
            DefaultButtonStyle = BTN_Choice->GetStyle();
            bDefaultButtonStyleCached = true;
        }
    }

    // SetSelected가 Construct보다 먼저 불렸을 수 있으므로 강제 숨김이 아니라 보관된 상태를 적용한다.
    ApplyInteractionGuideVisibility();
    ApplySelectedButtonStyle();
}

////////////////////////////
//! \author 준혁
//! \brief 선택 상태를 설정하고 상호작용 가이드 표시에 반영하는 함수. 대화 위젯이 선택 이동 시 호출한다.
//! \param bInSelected 이 버튼이 현재 선택된 선택지인지 여부
void UMyDialogueChoiceButton::SetSelected(bool bInSelected)
{
    bIsSelected = bInSelected;
    ApplyInteractionGuideVisibility();
    ApplySelectedButtonStyle();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 공용 ChoiceButton 중 상호작용 가이드 버튼에만 선택 상태의 Hovered 브러시 강제 표시를 설정하는 함수
// bInUseSelectedHoverVisual : 선택 상태에서 Hovered 브러시를 기본 표시로 사용할지 여부
void UMyDialogueChoiceButton::SetUseSelectedHoverVisual(bool bInUseSelectedHoverVisual)
{
    bUseSelectedHoverVisual = bInUseSelectedHoverVisual;
    ApplySelectedButtonStyle();
}

////////////////////////////
//! \author 준혁
//! \brief 선택지 인덱스와 표시 텍스트를 설정하는 함수. 대화 위젯이 생성 직후 호출한다.
//! \param InChoiceIndex 이 버튼이 담당하는 선택지 인덱스
//! \param InText 버튼에 표시할 텍스트
void UMyDialogueChoiceButton::SetupChoice(int32 InChoiceIndex, const FText& InText)
{
    ChoiceIndex = InChoiceIndex;

    if (TXT_Choice)
    {
        TXT_Choice->SetText(InText);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 버튼 클릭을 선택지 인덱스와 함께 대화 위젯에 전달하는 함수
void UMyDialogueChoiceButton::HandleClicked()
{
    OnChoiceClicked.ExecuteIfBound(ChoiceIndex);
}

////////////////////////////
//! \author 준혁
//! \brief 마우스가 버튼 위에 올라오면 대화 위젯에 알려 선택 상태를 이 버튼으로 옮기게 하는 함수.
//!        선택 해제는 하지 않는다(항상 선택지 하나가 선택 상태를 유지해야 하므로).
void UMyDialogueChoiceButton::HandleHovered()
{
    OnChoiceHovered.ExecuteIfBound(ChoiceIndex);
}

////////////////////////////
//! \author 준혁
//! \brief 선택 여부에 따라 상호작용 가이드(이미지+보더)의 표시를 갱신하는 함수.
//!        숨길 때 Hidden을 사용해 안 보여도 레이아웃 자리는 유지한다.
void UMyDialogueChoiceButton::ApplyInteractionGuideVisibility()
{
    const ESlateVisibility GuideVisibility = bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

    if (TXT_InteractionGuide)
    {
        TXT_InteractionGuide->SetVisibility(GuideVisibility);
    }

    if (Border_InteractionGuide)
    {
        Border_InteractionGuide->SetVisibility(GuideVisibility);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 휠과 마우스가 공유하는 선택 상태에 따라 WBP의 Hovered 브러시를 버튼의 기본 표시로 적용하거나 원본 스타일로 복구하는 함수
void UMyDialogueChoiceButton::ApplySelectedButtonStyle()
{
    if (!BTN_Choice || !bDefaultButtonStyleCached)
    {
        return;
    }

    FButtonStyle AppliedStyle = DefaultButtonStyle;
    if (bUseSelectedHoverVisual && bIsSelected)
    {
        AppliedStyle.Normal = DefaultButtonStyle.Hovered;
    }

    BTN_Choice->SetStyle(AppliedStyle);
}
