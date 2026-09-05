#include "MyNoticeWidget.h"

#include "Components/Border.h"
#include "Components/RichTextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Widget/RichText/MyRichTextDecorators.h"

void UMyNoticeWidget::NativeConstruct()
{
    Super::NativeConstruct();

	MyRichText::ConfigureDecorators(TXT_Notice);
    ApplyNoticePresentation(EMyNoticePresentationType::Default);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UMyNoticeWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NoticeTimerHandle);
    }

    PendingNotices.Reset();
    Super::NativeDestruct();
}

void UMyNoticeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bCountdownActive)
    {
        UpdateCountdownNotice();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 일반 Notice를 표시하거나 현재 Notice 뒤의 대기열에 추가하는 함수
// Message : 표시할 시스템 메시지
// DurationSeconds : 메시지를 표시할 시간, 0 이하이면 기본 표시 시간 사용
void UMyNoticeWidget::ShowNotice(const FText& Message, float DurationSeconds)
{
    FMyNoticeData NoticeData;
    NoticeData.Message = Message;
    NoticeData.DurationSeconds = DurationSeconds;
    ShowNoticeData(NoticeData);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 표시 종류와 선택적 Rich Text가 포함된 Notice를 표시하거나 현재 Notice 뒤의 대기열에 추가하는 함수
// NoticeData : 일반 문구, Rich Text 문구, 표시 시간, 시각 표현 종류를 담은 데이터
void UMyNoticeWidget::ShowNoticeData(const FMyNoticeData& NoticeData)
{
    if (NoticeData.Message.IsEmpty() || bCountdownActive)
    {
        return;
    }

    const bool bMatchesCurrentNotice =
        bNoticeVisible &&
        TXT_Notice &&
        TXT_Notice->GetText().EqualTo(NoticeData.Message) &&
        ActiveNoticePresentationType == NoticeData.PresentationType;
    const bool bMatchesPendingNotice = PendingNotices.ContainsByPredicate(
        [&NoticeData](const FMyNoticeData& PendingNotice)
        {
            return PendingNotice.Message.EqualTo(NoticeData.Message) &&
                PendingNotice.PresentationType == NoticeData.PresentationType;
        });

    // 같은 Notice가 입력 연타로 중복 누적되어 전체 표시 시간이 길어지는 것을 막는다.
    if (bMatchesCurrentNotice || bMatchesPendingNotice)
    {
        return;
    }

    FMyNoticeData ResolvedNoticeData = NoticeData;
    ResolvedNoticeData.DurationSeconds = NoticeData.DurationSeconds > 0.0f
        ? NoticeData.DurationSeconds
        : DefaultNoticeDuration;

    if (bNoticeVisible)
    {
        PendingNotices.Add(MoveTemp(ResolvedNoticeData));
        return;
    }

    DisplayNotice(ResolvedNoticeData);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버 종료 시간을 기준으로 최우선 카운트다운 Notice를 표시하는 함수
// MessageFormat : 남은 초가 {0} 위치에 들어가는 메시지 형식
// EndServerTime : 카운트다운이 끝나는 서버 월드 시간
void UMyNoticeWidget::ShowCountdownNotice(const FText& MessageFormat, float EndServerTime)
{
    if (MessageFormat.IsEmpty() || EndServerTime <= GetSyncedServerWorldTimeSeconds())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NoticeTimerHandle);
    }

    PendingNotices.Reset();
    ActiveCountdownMessageFormat = MessageFormat;
    CountdownEndServerTime = EndServerTime;
    LastDisplayedCountdownSeconds = INDEX_NONE;
    bCountdownActive = true;
    bNoticeVisible = true;

    ApplyNoticePresentation(EMyNoticePresentationType::Default);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    UpdateCountdownNotice();
    if (bCountdownActive)
    {
        OnNoticeShown();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 Notice와 대기 중인 Notice를 모두 정리하는 함수
void UMyNoticeWidget::ClearNotice()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NoticeTimerHandle);
    }

    PendingNotices.Reset();
    ActiveCountdownMessageFormat = FText::GetEmpty();
    CountdownEndServerTime = 0.0f;
    LastDisplayedCountdownSeconds = INDEX_NONE;
    bCountdownActive = false;

    HideNoticeVisual();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 일반 Notice를 즉시 화면에 표시하고 종료 타이머를 시작하는 함수
// Message : NoticeData.Message에 담긴 일반 시스템 메시지
// DurationSeconds : NoticeData.DurationSeconds에 담긴 메시지 표시 시간
// NoticeData : 표시할 문구, 표시 시간, 시각 표현 종류를 담은 데이터
void UMyNoticeWidget::DisplayNotice(const FMyNoticeData& NoticeData)
{
    bNoticeVisible = true;

    SetNoticeText(NoticeData.Message);
    ApplyNoticePresentation(NoticeData.PresentationType);

    SetVisibility(ESlateVisibility::HitTestInvisible);
    OnNoticeShown();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            NoticeTimerHandle,
            this,
            &UMyNoticeWidget::HandleNoticeDurationElapsed,
            FMath::Max(NoticeData.DurationSeconds, 0.1f),
            false);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 일반 Notice의 표시 시간이 끝났을 때 다음 메시지로 전환하는 함수
void UMyNoticeWidget::HandleNoticeDurationElapsed()
{
    DisplayNextPendingNotice();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 대기열의 다음 Notice를 표시하고 대기열이 비었으면 화면을 숨기는 함수
void UMyNoticeWidget::DisplayNextPendingNotice()
{
    if (PendingNotices.IsEmpty())
    {
        HideNoticeVisual();
        return;
    }

    const FMyNoticeData NextNotice = PendingNotices[0];
    PendingNotices.RemoveAt(0);
    DisplayNotice(NextNotice);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버 동기화 시간을 사용해 카운트다운 문구를 갱신하는 함수
void UMyNoticeWidget::UpdateCountdownNotice()
{
    const int32 RemainingSeconds = FMath::Max(
        0,
        FMath::CeilToInt(CountdownEndServerTime - GetSyncedServerWorldTimeSeconds()));

    if (RemainingSeconds <= 0)
    {
        ClearNotice();
        return;
    }

    if (LastDisplayedCountdownSeconds == RemainingSeconds)
    {
        return;
    }

    LastDisplayedCountdownSeconds = RemainingSeconds;
    SetNoticeText(FText::Format(ActiveCountdownMessageFormat, RemainingSeconds));
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Notice 표시 요소를 숨기고 활성 상태를 초기화하는 함수
void UMyNoticeWidget::HideNoticeVisual()
{
    const bool bWasVisible = bNoticeVisible;
    bNoticeVisible = false;

    SetNoticeText(FText::GetEmpty());
    ApplyNoticePresentation(EMyNoticePresentationType::Default);

    SetVisibility(ESlateVisibility::Collapsed);
    if (bWasVisible)
    {
        OnNoticeHidden();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
////////////////////////////
//! \author 장효제
//! \brief 단일 RichText 문구를 Notice에 적용한다.
//! \param Message 표시할 RichText 문구다.
void UMyNoticeWidget::SetNoticeText(const FText& Message)
{
    if (TXT_Notice)
    {
        TXT_Notice->SetText(Message);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Notice 종류에 따라 Donation Halo 가시성을 적용하고 블루프린트에 연출 변경을 알리는 함수
// PresentationType : Default 또는 Donation 시각 표현 종류
void UMyNoticeWidget::ApplyNoticePresentation(EMyNoticePresentationType PresentationType)
{
    ActiveNoticePresentationType = PresentationType;
    if (BDR_Halo)
    {
        const ESlateVisibility HaloVisibility =
            PresentationType == EMyNoticePresentationType::Donation
                ? ESlateVisibility::HitTestInvisible
                : ESlateVisibility::Hidden;
        BDR_Halo->SetVisibility(HaloVisibility);
    }

    OnNoticePresentationChanged(PresentationType);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 클라이언트가 알고 있는 서버 동기화 월드 시간을 반환하는 함수
// Return Value : 서버 동기화 시간, GameState가 없으면 로컬 월드 시간
float UMyNoticeWidget::GetSyncedServerWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }

    const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
    return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : World->GetTimeSeconds();
}
