#pragma once

#include "CoreMinimal.h"
#include "Widget/MyActivatableWidget.h"
#include "MyDialogueWidget.generated.h"

class ACPP_ObeliskActor;
class UDataTable;
class UImage;
class UMyDialogueChoiceButton;
class UMyDialogueDataAsset;
class UPanelWidget;
class UTextBlock;

////////////////////////////
//! \class UMyDialogueWidget
//! \brief 화면 중앙 하단에 이미지+텍스트로 대화를 표시하는 위젯. Dialogue 레이어에 푸시된다.
//!        좌클릭으로 줄을 넘기며 마지막 줄에서 닫힌다. 진행은 로컬 전용(파티원 간 동기화 없음).
//!        선택지가 있는 줄에서는 좌클릭 진행이 막히고 선택지 버튼으로만 진행되며,
//!        기믹 트리거 선택지를 고르면 출처 오벨리스크에 서버 RPC로 통지한다.
//!        진행을 통해 마지막 줄을 지나 닫힐 때(완주)는 출처 오벨리스크가 완주 타이밍이면 서버에 완주를 통지한다.
//!        닫힐 때 HUD 레이어를 복원하고, 진행 중이던 상호작용 세션이 있으면 종료한다.
//! \note WBP 루트에는 클릭을 받을 풀스크린 히트 영역(알파 0 Border 등, Visibility=Visible)이 필요하다.
//!       선택지용으로 PNL_Choices(VerticalBox 등)를 배치하고 ChoiceButtonClass에 WBP_DialogueChoiceButton을 지정한다.
UCLASS()
class PROJECTP_API UMyDialogueWidget : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UMyDialogueWidget();

    //! 대화를 시작한다. 푸시 직후 호출된다.
    //! \param InSourceObelisk 대화 출처 오벨리스크. 선택지의 기믹 트리거 통지 대상 (없으면 nullptr)
    void StartDialogue(const UMyDialogueDataAsset* InDialogue, ACPP_ObeliskActor* InSourceObelisk);

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

protected:
    //! 선택지 버튼 위젯 클래스 (WBP_Dialogue 디폴트에서 WBP_DialogueChoiceButton 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue")
    TSubclassOf<UMyDialogueChoiceButton> ChoiceButtonClass;

    //! 선택지 버튼 사이의 세로 간격. WBP_Dialogue의 Class Defaults에서 조절한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dialogue", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float ChoiceButtonSpacing = 20.0f;

private:
    //! 현재 대사 줄의 화자 이름을 표시하는 텍스트 블록
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Name;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> IMG_Speaker;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Dialogue;

    //! 선택지 버튼들이 담길 컨테이너 (VerticalBox 등)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> PNL_Choices;

    //! 신 화자의 이름과 초상화를 자동으로 채울 표다. 비워두면 기본 경로에서 지연 로드한다.
    UPROPERTY(EditDefaultsOnly, Category = "Dialogue", meta = (AllowPrivateAccess = "true", RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyGodPresentationRow"))
    TObjectPtr<UDataTable> GodPresentationTable;

    UPROPERTY(Transient)
    TObjectPtr<const UMyDialogueDataAsset> ActiveDialogue;

    //! 대화 출처 오벨리스크. 레벨 액터라 대화 도중 파괴될 수 있어 약참조로 든다.
    TWeakObjectPtr<ACPP_ObeliskActor> SourceObelisk;

    int32 CurrentLineIndex = 0;

    //! 현재 줄의 선택지 버튼들. RebuildChoices에서 다시 채운다.
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMyDialogueChoiceButton>> ChoiceButtons;

    //! 현재 선택된 선택지 인덱스. 선택지가 표시되는 동안에는 항상 하나가 선택 상태다.
    int32 SelectedChoiceIndex = INDEX_NONE;

    void ShowCurrentLine();
    void AdvanceDialogue();
    bool CurrentLineHasChoices() const;
    void RebuildChoices();
    void SetSelectedChoice(int32 NewIndex);
    void StepSelectedChoice(int32 Delta);
    void HandleChoiceSelected(int32 ChoiceIndex);
    void NotifyDialogueCompleted();
    void SetSpeakerImageVisibility(bool bVisible);
    void RestoreHUDLayer();
    void SetInteractionBlocked(bool bBlocked);
    void RequestEndInteraction();
};
