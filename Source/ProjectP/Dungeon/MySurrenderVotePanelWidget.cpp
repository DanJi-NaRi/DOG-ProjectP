#include "MySurrenderVotePanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "DungeonPC.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 실제 투표 상태를 변경하지 않는 항복 UI 프리뷰 상태를 생성하는 함수
// CurrentServerTime : 프리뷰 투표 시작에 사용할 현재 서버 시간
// DurationSeconds : 프리뷰 투표 제한 시간
// AgreeCount : 표시할 찬성 인원 수
// RequiredCount : 표시할 전체 필요 인원 수
// Return Value : 항복 UI가 표시할 로컬 프리뷰 상태
FDungeonSurrenderVoteState MySurrenderVotePreview::MakeVoteState(
    float CurrentServerTime,
    float DurationSeconds,
    int32 AgreeCount,
    int32 RequiredCount)
{
    FDungeonSurrenderVoteState PreviewState;
    PreviewState.VoteType = EDungeonPartyVoteType::Surrender;
    PreviewState.bVoteInProgress = true;
    PreviewState.RequiredCount = FMath::Max(RequiredCount, 1);
    PreviewState.AgreeCount = FMath::Clamp(AgreeCount, 0, PreviewState.RequiredCount);
    PreviewState.DisagreeCount = 0;
    PreviewState.VoteStartServerTime = CurrentServerTime;
    PreviewState.VoteEndServerTime = CurrentServerTime + FMath::Max(DurationSeconds, 1.0f);
    return PreviewState;
}

void UMySurrenderVotePanelWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    if (!IsDesignTime())
    {
        return;
    }

    bHasCachedSurrenderVoteState = false;

    if (!bShowDesignTimePreview)
    {
        UpdateSurrenderVotePanelVisibility(false);
        return;
    }

    if (HorizontalBox_VoteCount)
    {
        HorizontalBox_VoteCount->ClearChildren();
    }
    VoteCountBoxes.Reset();

    FDungeonSurrenderVoteState PreviewState = MySurrenderVotePreview::MakeVoteState(
        GetSyncedServerWorldTimeSeconds(),
        30.0f,
        PreviewAgreeCount,
        PreviewPlayerCount);

    ApplySurrenderVoteState(PreviewState);
}

////////////////////////////
//! \author 준혁
//! \brief 패널 초기 상태를 숨김으로 설정하고, PC가 참조할 수 있게 자기 등록하는 함수 (DungeonMainUI에서 분리 이동)
void UMySurrenderVotePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // WBP_PrimaryGameLayout에 임베드되어 레이아웃이 이 위젯을 생성하므로, PC가 참조할 수 있게 스스로 등록한다.
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer()))
    {
        DungeonPC->RegisterSurrenderVotePanel(this);
    }

    UpdateSurrenderVotePanelVisibility(false);
    EnsureSurrenderVoteCompletedText();
    RefreshLocalSurrenderVoteSubmissionUI();
    RefreshSurrenderVoteUI();
}

void UMySurrenderVotePanelWidget::NativeDestruct()
{
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer()))
    {
        DungeonPC->UnregisterSurrenderVotePanel(this);
    }

    Super::NativeDestruct();
}

void UMySurrenderVotePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    RefreshSurrenderVoteUI();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어가 현재 항복 투표에 참여 완료했는지 UI 상태를 저장하는 함수
// bSubmitted : 투표 완료 표시 여부
// VoteStartServerTime : 투표 시작 서버 시간
void UMySurrenderVotePanelWidget::SetLocalSurrenderVoteSubmitted(bool bSubmitted, float VoteStartServerTime)
{
    bLocalSurrenderVoteSubmitted = bSubmitted;
    LocalSubmittedVoteStartServerTime = bSubmitted ? VoteStartServerTime : 0.0f;

    RefreshLocalSurrenderVoteSubmissionUI();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 실제 서버 투표에 영향을 주지 않는 찬성 1/3 항복 UI 프리뷰를 켜거나 끄는 함수
void UMySurrenderVotePanelWidget::ToggleDebugSurrenderVotePreview()
{
    bDebugSurrenderVotePreviewActive = !bDebugSurrenderVotePreviewActive;
    bHasCachedSurrenderVoteState = false;

    if (bDebugSurrenderVotePreviewActive)
    {
        DebugSurrenderVotePreviewState = MySurrenderVotePreview::MakeVoteState(
            GetSyncedServerWorldTimeSeconds(),
            30.0f,
            1,
            3);
        ApplySurrenderVoteState(DebugSurrenderVotePreviewState);
        return;
    }

    RefreshSurrenderVoteUI();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 GameState의 항복 투표 상태를 UI에 반영하는 함수
void UMySurrenderVotePanelWidget::RefreshSurrenderVoteUI()
{
    const UWorld* World = GetWorld();
    const ADungeonGS* DungeonGS = World ? World->GetGameState<ADungeonGS>() : nullptr;

    if (DungeonGS && DungeonGS->GetSurrenderVoteState().bVoteInProgress)
    {
        bDebugSurrenderVotePreviewActive = false;
    }

    if (bDebugSurrenderVotePreviewActive)
    {
        ApplySurrenderVoteState(DebugSurrenderVotePreviewState);
        return;
    }

    if (!DungeonGS)
    {
        UpdateSurrenderVotePanelVisibility(false);
        return;
    }

    const FDungeonSurrenderVoteState& VoteState = DungeonGS->GetSurrenderVoteState();
    ApplySurrenderVoteState(VoteState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 상태에 맞게 패널, 박스, 시간바를 갱신하는 함수
// VoteState : GameState에서 복제된 항복 투표 상태
void UMySurrenderVotePanelWidget::ApplySurrenderVoteState(const FDungeonSurrenderVoteState& VoteState)
{
    const bool bNewVoteStarted =
        VoteState.bVoteInProgress &&
        (!bHasCachedSurrenderVoteState ||
        !CachedSurrenderVoteState.bVoteInProgress ||
        !FMath::IsNearlyEqual(CachedSurrenderVoteState.VoteStartServerTime, VoteState.VoteStartServerTime));

    if (!VoteState.bVoteInProgress && bLocalSurrenderVoteSubmitted)
    {
        SetLocalSurrenderVoteSubmitted(false);
    }
    else if (bNewVoteStarted && bLocalSurrenderVoteSubmitted && LocalSubmittedVoteStartServerTime <= 0.0f)
    {
        LocalSubmittedVoteStartServerTime = VoteState.VoteStartServerTime;
    }
    else if (bNewVoteStarted && !FMath::IsNearlyEqual(LocalSubmittedVoteStartServerTime, VoteState.VoteStartServerTime))
    {
        SetLocalSurrenderVoteSubmitted(false);
    }

    const bool bStateChanged =
        !bHasCachedSurrenderVoteState ||
        CachedSurrenderVoteState.VoteType != VoteState.VoteType ||
        CachedSurrenderVoteState.bVoteInProgress != VoteState.bVoteInProgress ||
        CachedSurrenderVoteState.AgreeCount != VoteState.AgreeCount ||
        CachedSurrenderVoteState.DisagreeCount != VoteState.DisagreeCount ||
        CachedSurrenderVoteState.RequiredCount != VoteState.RequiredCount ||
        CachedSurrenderVoteState.VoteStartServerTime != VoteState.VoteStartServerTime ||
        CachedSurrenderVoteState.VoteEndServerTime != VoteState.VoteEndServerTime ||
        CachedSurrenderVoteState.LobbyTravelServerTime != VoteState.LobbyTravelServerTime;

    if (bStateChanged)
    {
        UpdateVoteTitle(VoteState.VoteType);
        UpdateSurrenderVotePanelVisibility(VoteState.bVoteInProgress);
        EnsureVoteCountBoxes(VoteState.RequiredCount);
        UpdateVoteCountBoxColors(VoteState.AgreeCount, VoteState.DisagreeCount);

        CachedSurrenderVoteState = VoteState;
        bHasCachedSurrenderVoteState = true;
    }

    RefreshLocalSurrenderVoteSubmissionUI();
    UpdateSurrenderTimeProgress(VoteState);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 투표 참여 인원 수에 맞게 투표 상태 박스를 생성하는 함수
// RequiredCount : 표시해야 할 전체 투표 박스 개수
void UMySurrenderVotePanelWidget::EnsureVoteCountBoxes(int32 RequiredCount)
{
    if (!HorizontalBox_VoteCount || !WidgetTree)
    {
        return;
    }

    RequiredCount = FMath::Max(0, RequiredCount);
    if (VoteCountBoxes.Num() == RequiredCount)
    {
        return;
    }

    HorizontalBox_VoteCount->ClearChildren();
    VoteCountBoxes.Reset();

    for (int32 BoxIndex = 0; BoxIndex < RequiredCount; ++BoxIndex)
    {
        USizeBox* VoteBoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        UBorder* VoteBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        if (!VoteBoxSize || !VoteBox)
        {
            continue;
        }

        VoteBoxSize->SetWidthOverride(VoteBoxWidth);
        VoteBoxSize->SetHeightOverride(VoteBoxHeight);
        VoteBox->SetBrushFromTexture(PendingVoteTexture);
        VoteBox->SetBrushColor(PendingVoteTexture ? FLinearColor::White : PendingBoxColor);
        VoteBoxSize->AddChild(VoteBox);

        UPanelSlot* PanelSlot = HorizontalBox_VoteCount->AddChild(VoteBoxSize);
        if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
        {
            HorizontalBoxSlot->SetPadding(VoteBoxPadding);
            HorizontalBoxSlot->SetHorizontalAlignment(VoteBoxHorizontalAlignment);
            HorizontalBoxSlot->SetVerticalAlignment(VoteBoxVerticalAlignment);
        }

        VoteCountBoxes.Add(VoteBox);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 찬성, 반대, 대기 상태에 맞게 투표 박스 색상을 갱신하는 함수
// AgreeCount : 찬성한 인원 수
// DisagreeCount : 반대한 인원 수
// 찬성 텍스처가 지정되면 찬성 칸에 사용하고, 나머지 칸에는 미투표 텍스처를 사용함
// 반대가 발생하면 투표 패널 자체가 사라지므로 반대 전용 텍스처는 표시하지 않음
void UMySurrenderVotePanelWidget::UpdateVoteCountBoxColors(int32 AgreeCount, int32 DisagreeCount)
{
    AgreeCount = FMath::Clamp(AgreeCount, 0, VoteCountBoxes.Num());
    (void)DisagreeCount;

    for (int32 BoxIndex = 0; BoxIndex < VoteCountBoxes.Num(); ++BoxIndex)
    {
        UBorder* VoteBox = VoteCountBoxes[BoxIndex];
        if (!VoteBox)
        {
            continue;
        }

        const bool bIsAgreeVote = BoxIndex < AgreeCount;
        UTexture2D* VoteTexture = bIsAgreeVote ? AgreeVoteTexture.Get() : PendingVoteTexture.Get();

        VoteBox->SetBrushFromTexture(VoteTexture);
        VoteBox->SetBrushColor(
            VoteTexture
                ? FLinearColor::White
                : (bIsAgreeVote ? AgreeBoxColor : PendingBoxColor));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 패널 표시 여부를 갱신하는 함수
// bVoteInProgress : 항복 투표 패널을 표시할지 여부
void UMySurrenderVotePanelWidget::UpdateSurrenderVotePanelVisibility(bool bVoteInProgress)
{
    if (Border_SurrenderPanel)
    {
        Border_SurrenderPanel->SetVisibility(bVoteInProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameState의 서버 동기화 시간을 가져오는 함수
// Return Value : 서버와 동기화된 월드 시간, GameState가 없으면 로컬 월드 시간
float UMySurrenderVotePanelWidget::GetSyncedServerWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }

    const ADungeonGS* DungeonGS = World->GetGameState<ADungeonGS>();
    return DungeonGS ? static_cast<float>(DungeonGS->GetServerWorldTimeSeconds()) : World->GetTimeSeconds();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 남은 시간 진행바를 갱신하는 함수
// VoteState : GameState에서 복제된 항복 투표 상태
void UMySurrenderVotePanelWidget::UpdateSurrenderTimeProgress(const FDungeonSurrenderVoteState& VoteState) const
{
    if (!ProgressBar_TimeLimit)
    {
        return;
    }

    if (!VoteState.bVoteInProgress)
    {
        ProgressBar_TimeLimit->SetPercent(0.0f);
        return;
    }

    const UWorld* World = GetWorld();
    if (!World || VoteState.VoteEndServerTime <= 0.0f)
    {
        ProgressBar_TimeLimit->SetPercent(1.0f);
        return;
    }

    const float RemainingSeconds = FMath::Max(0.0f, VoteState.VoteEndServerTime - GetSyncedServerWorldTimeSeconds());
    const float TotalSeconds = FMath::Max(1.0f, VoteState.VoteEndServerTime - VoteState.VoteStartServerTime);

    ProgressBar_TimeLimit->SetPercent(FMath::Clamp(RemainingSeconds / TotalSeconds, 0.0f, 1.0f));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 완료 안내 텍스트를 찾거나 동적으로 생성하는 함수
// Return Value : 항복 투표 완료 안내 텍스트 위젯
UTextBlock* UMySurrenderVotePanelWidget::EnsureSurrenderVoteCompletedText()
{
    if (TXT_SurrenderVoteCompleted)
    {
        TXT_SurrenderVoteCompleted->SetText(FText::FromString(TEXT("투표를 완료했습니다.")));
        return TXT_SurrenderVoteCompleted;
    }

    if (!WidgetTree || !VerticalBox_SurrenderPanel)
    {
        return nullptr;
    }

    TXT_SurrenderVoteCompleted = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_SurrenderVoteCompleted"));
    if (!TXT_SurrenderVoteCompleted)
    {
        return nullptr;
    }

    TXT_SurrenderVoteCompleted->SetText(FText::FromString(TEXT("투표를 완료했습니다.")));
    TXT_SurrenderVoteCompleted->SetJustification(ETextJustify::Center);
    TXT_SurrenderVoteCompleted->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    TXT_SurrenderVoteCompleted->SetVisibility(ESlateVisibility::Collapsed);

    UVerticalBoxSlot* CompletedTextSlot = VerticalBox_SurrenderPanel->AddChildToVerticalBox(TXT_SurrenderVoteCompleted);
    if (CompletedTextSlot)
    {
        CompletedTextSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
        CompletedTextSlot->SetHorizontalAlignment(HAlign_Center);
    }

    return TXT_SurrenderVoteCompleted;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 투표 완료 상태에 맞춰 Yes No 버튼과 완료 문구 표시를 갱신하는 함수
void UMySurrenderVotePanelWidget::RefreshLocalSurrenderVoteSubmissionUI()
{
    const bool bVoteInProgress = bHasCachedSurrenderVoteState && CachedSurrenderVoteState.bVoteInProgress;
    const bool bShowCompletedText = bVoteInProgress && bLocalSurrenderVoteSubmitted;

    if (HorizontalBox_Vote)
    {
        HorizontalBox_Vote->SetVisibility(bShowCompletedText ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    if (UTextBlock* CompletedText = EnsureSurrenderVoteCompletedText())
    {
        CompletedText->SetVisibility(bShowCompletedText ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 공용 투표 종류에 맞춰 패널 제목 문구를 변경하는 함수
// VoteType : 현재 서버에서 진행 중인 공용 투표 종류
void UMySurrenderVotePanelWidget::UpdateVoteTitle(EDungeonPartyVoteType VoteType) const
{
    if (!TXT_Title)
    {
        return;
    }

    const FText TitleText = VoteType == EDungeonPartyVoteType::GimmickReset
        ? NSLOCTEXT("DungeonPartyVote", "GimmickResetTitle", "기믹 초기화")
        : NSLOCTEXT("DungeonPartyVote", "SurrenderTitle", "방송 종료");
    TXT_Title->SetText(TitleText);
}

