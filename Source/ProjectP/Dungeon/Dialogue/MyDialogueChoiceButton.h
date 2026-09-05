#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "MyDialogueChoiceButton.generated.h"

class UButton;
class UImage;
class UTextBlock;

//! 선택지 버튼이 클릭되면 자신의 선택지 인덱스와 함께 발화한다. (위젯 내부용, 대화 위젯이 바인딩)
DECLARE_DELEGATE_OneParam(FOnDialogueChoiceClickedSignature, int32 /*ChoiceIndex*/);

//! 선택지 버튼에 마우스가 올라오면 자신의 선택지 인덱스와 함께 발화한다. 대화 위젯이 선택 상태를 이 버튼으로 옮긴다.
DECLARE_DELEGATE_OneParam(FOnDialogueChoiceHoveredSignature, int32 /*ChoiceIndex*/);

////////////////////////////
//! \class UMyDialogueChoiceButton
//! \brief 대화 선택지 한 개를 표시하는 버튼 위젯. 대화 위젯이 선택지 수만큼 동적으로 생성한다.
//! \note WBP에는 BTN_Choice(Button)와 TXT_Choice(TextBlock)를 배치한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyDialogueChoiceButton : public UUserWidget
{
    GENERATED_BODY()

public:
    //! 선택지 인덱스와 표시 텍스트를 설정한다. 생성 직후 호출된다.
    void SetupChoice(int32 InChoiceIndex, const FText& InText);

    //! 선택 상태를 설정한다. 선택된 버튼에만 상호작용 가이드(TXT/Border_InteractionGuide)가 보인다.
    void SetSelected(bool bInSelected);

    //! 상호작용 가이드에서 선택된 버튼을 WBP의 Hovered 브러시로 표시할지 설정한다.
    void SetUseSelectedHoverVisual(bool bInUseSelectedHoverVisual);

    //! 이 버튼이 담당하는 선택지/옵션 인덱스를 반환한다. (목록 필터링으로 버튼 위치와 인덱스가 다를 수 있음)
    int32 GetChoiceIndex() const { return ChoiceIndex; }

    FOnDialogueChoiceClickedSignature OnChoiceClicked;
    FOnDialogueChoiceHoveredSignature OnChoiceHovered;

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Choice;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Choice;

    //! 선택된 선택지에만 표시하는 상호작용 가이드 이미지
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> TXT_InteractionGuide;

    //! 선택된 선택지에만 표시하는 상호작용 가이드 보더 (타입 무관하게 이름으로 바인딩)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> Border_InteractionGuide;

    UFUNCTION()
    void HandleClicked();

    UFUNCTION()
    void HandleHovered();

    void ApplyInteractionGuideVisibility();

    void ApplySelectedButtonStyle();

    int32 ChoiceIndex = INDEX_NONE;

    //! 현재 선택 여부. SetSelected가 NativeConstruct보다 먼저 불려도 상태가 유지되도록 보관한다.
    bool bIsSelected = false;

    //! 공용 ChoiceButton 중 상호작용 가이드에서 생성된 버튼만 선택 상태에 Hovered 브러시를 사용한다.
    bool bUseSelectedHoverVisual = false;

    //! WBP에 설정된 Normal/Hovered 브러시를 선택 해제 시 정확히 복구하기 위한 원본 버튼 스타일이다.
    FButtonStyle DefaultButtonStyle;

    bool bDefaultButtonStyleCached = false;
};
