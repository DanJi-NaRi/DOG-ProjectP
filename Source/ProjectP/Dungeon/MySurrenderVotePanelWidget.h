#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DungeonGS.h"
#include "MySurrenderVotePanelWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

namespace MySurrenderVotePreview
{
    PROJECTP_API FDungeonSurrenderVoteState MakeVoteState(
        float CurrentServerTime,
        float DurationSeconds,
        int32 AgreeCount,
        int32 RequiredCount);
}

//! 항복 투표 패널. DungeonMainUI에서 분리해 WBP_PrimaryGameLayout 캔버스(레이어 스택들 위)에 임베드한다.
//! (대화 등으로 HUD 레이어가 숨겨져도 투표 UI는 계속 보여야 하기 때문)
//! 주의: 활성 상태로 상주하는 위젯을 CommonUI 레이어 스택에 넣으면 액티브 루트를 점유해
//!       아래 레이어(대화/상점 등)의 입력 설정(Menu)이 적용되지 않는다. 그래서 스택이 아닌 임베드 방식.
//! 투표가 없을 때는 스스로 내부 요소를 숨기며, 생성 시 ADungeonPC에 자기 등록한다.
//! Yes/No 버튼은 WBP 그래프에서 DungeonPC의 RequestSubmitSurrenderVote를 호출한다.
UCLASS()
class PROJECTP_API UMySurrenderVotePanelWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    void SetLocalSurrenderVoteSubmitted(bool bSubmitted, float VoteStartServerTime = 0.0f);
    void ToggleDebugSurrenderVotePreview();

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> HorizontalBox_Vote;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VerticalBox_SurrenderPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> HorizontalBox_VoteCount;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> ProgressBar_TimeLimit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> Border_SurrenderPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SurrenderVoteCompleted;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Title;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    float VoteBoxWidth = 42.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    float VoteBoxHeight = 16.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    FMargin VoteBoxPadding = FMargin(1.5f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    TEnumAsByte<EHorizontalAlignment> VoteBoxHorizontalAlignment = HAlign_Fill;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    TEnumAsByte<EVerticalAlignment> VoteBoxVerticalAlignment = VAlign_Fill;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    FLinearColor AgreeBoxColor = FLinearColor(0.0f, 1.0f, 0.08f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender")
    FLinearColor PendingBoxColor = FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender|Vote Box")
    TObjectPtr<UTexture2D> AgreeVoteTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender|Vote Box")
    TObjectPtr<UTexture2D> PendingVoteTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender|Preview")
    bool bShowDesignTimePreview = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Dungeon|Surrender|Preview",
        meta = (ClampMin = "1", UIMin = "1", UIMax = "4", EditCondition = "bShowDesignTimePreview"))
    int32 PreviewPlayerCount = 3;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Dungeon|Surrender|Preview",
        meta = (ClampMin = "0", UIMin = "0", UIMax = "4", EditCondition = "bShowDesignTimePreview"))
    int32 PreviewAgreeCount = 1;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UBorder>> VoteCountBoxes;

    FDungeonSurrenderVoteState CachedSurrenderVoteState;

    bool bHasCachedSurrenderVoteState = false;

    bool bLocalSurrenderVoteSubmitted = false;

    float LocalSubmittedVoteStartServerTime = 0.0f;

    bool bDebugSurrenderVotePreviewActive = false;

    FDungeonSurrenderVoteState DebugSurrenderVotePreviewState;

    void RefreshSurrenderVoteUI();
    void ApplySurrenderVoteState(const FDungeonSurrenderVoteState& VoteState);
    void EnsureVoteCountBoxes(int32 RequiredCount);
    void UpdateVoteCountBoxColors(int32 AgreeCount, int32 DisagreeCount);
    void UpdateSurrenderVotePanelVisibility(bool bVoteInProgress);
    float GetSyncedServerWorldTimeSeconds() const;
    void UpdateSurrenderTimeProgress(const FDungeonSurrenderVoteState& VoteState) const;
    UTextBlock* EnsureSurrenderVoteCompletedText();
    void RefreshLocalSurrenderVoteSubmissionUI();
    void UpdateVoteTitle(EDungeonPartyVoteType VoteType) const;
};
