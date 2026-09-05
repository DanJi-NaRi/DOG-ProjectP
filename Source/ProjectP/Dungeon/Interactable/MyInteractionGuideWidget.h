#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "MyInteractionGuideWidget.generated.h"

class UMyDialogueChoiceButton;
class UPanelWidget;
class UPlayerInteractionComponent;

////////////////////////////
//! \class UMyInteractionGuideWidget
//! \brief 상호작용 가능 액터 근처에서 해당 액터의 상호작용 옵션 목록을 표시하는 HUD 상주 위젯.
//!        선택 상태의 원본은 PlayerInteractionComponent이며(휠/F키는 게임 입력으로 컴포넌트에 전달됨),
//!        이 위젯은 표시 갱신과 마우스 입력(호버=선택 이동, 클릭=확정) 전달만 담당하는 뷰다.
//!        CommonUI 레이어 스택에 넣지 않고 WBP_HUDLayout에 임베드한다(액티브 루트 점유 금지).
//! \note WBP에는 PNL_Options(VerticalBox 등)를 배치하고 OptionButtonClass에 WBP_DialogueChoiceButton을 지정한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyInteractionGuideWidget : public UCommonUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    //! 옵션 버튼 위젯 클래스 (WBP_InteractionGuide 디폴트에서 WBP_DialogueChoiceButton 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteractionGuide")
    TSubclassOf<UMyDialogueChoiceButton> OptionButtonClass;

    //! 옵션 버튼 사이의 세로 간격
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteractionGuide", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float OptionButtonSpacing = 12.0f;

    //! 소유 폰의 PlayerInteractionComponent 탐색 주기(초). 던전 입장 시 폰 교체를 따라잡기 위해 주기적으로 확인한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InteractionGuide", meta = (ClampMin = "0.1"))
    float AcquireInterval = 0.5f;

private:
    //! 옵션 버튼들이 담길 컨테이너 (VerticalBox 등)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> PNL_Options;

    //! 현재 이벤트를 구독 중인 상호작용 컴포넌트. 폰이 교체되면 재바인딩된다.
    UPROPERTY(Transient)
    TObjectPtr<UPlayerInteractionComponent> BoundInteractionComponent;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMyDialogueChoiceButton>> OptionButtons;

    FTimerHandle AcquireTimerHandle;

    //! 소유 폰의 상호작용 컴포넌트를 찾아 이벤트를 (재)바인딩한다.
    void AcquireInteractionComponent();

    UFUNCTION()
    void HandleEntriesChanged();

    UFUNCTION()
    void HandleSelectionChanged(int32 NewSelectedIndex);

    void HandleEntryHovered(int32 EntryIndex);
    void HandleEntryClicked(int32 EntryIndex);

    //! 주변 모든 후보의 통합 상호작용 항목 목록으로부터 버튼들을 다시 만든다. 항목이 없으면 위젯을 숨긴다.
    void RebuildOptions();
};
