////////////////////////////
//! \page MyStreamingChatPanelWidget.cpp
//! \brief Streaming Chat FIFO 목록, ScrollBottom 추적, Retainer 화면 공간 양끝 Fade를 구현한다.
#include "MyStreamingChatPanelWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/RetainerBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Layout/ArrangedChildren.h"
#include "MyGameplayTags.h"
#include "MyStreamingChatBubbleWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace
{
	constexpr float ScrollBoundaryTolerance = 4.0f;
	const FName TopFadeHeightParameterName(TEXT("TopFadeHeight"));
	const FName BottomFadeHeightParameterName(TEXT("BottomFadeHeight"));
	const FName TopFadeStrengthParameterName(TEXT("TopFadeStrength"));
	const FName BottomFadeStrengthParameterName(TEXT("BottomFadeStrength"));
}

void UMyStreamingChatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LatestEnterDistance = FMath::Max(LatestEnterDistance, 0.0f);
	LatestExitDistance = FMath::Max(LatestExitDistance, LatestEnterDistance + 1.0f);
	ScrollMode = EStreamingChatScrollMode::FollowLatest;

	if (SB_MessageList)
	{
		// 채팅 영역의 휠은 경계에서도 소비하고, 우클릭은 카메라 Orbit으로 전달한다.
		SB_MessageList->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		SB_MessageList->SetAllowRightClickDragScrolling(false);
		SB_MessageList->OnUserScrolled.AddUniqueDynamic(this, &ThisClass::HandleUserScrolled);
	}

	if (BTN_ReturnToLatest)
	{
		BTN_ReturnToLatest->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnToLatestClicked);
		BTN_ReturnToLatest->SetRenderOpacity(0.0f);
		BTN_ReturnToLatest->SetIsEnabled(false);
		BTN_ReturnToLatest->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	InitializeViewportFadeMaterial();
	RegisterChatMessageListener();
	ScheduleDeferredLayoutRefresh(true);
	MarkViewportFadeDirty();
}

void UMyStreamingChatPanelWidget::NativeDestruct()
{
	UnregisterChatMessageListener();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredLayoutTimerHandle);
	}

	bDeferredLayoutRefreshPending = false;
	PendingAnchorWidget.Reset();
	PendingMessagePositions.Reset();
	PendingNewBubbles.Reset();
	ActiveMessageMotions.Reset();
	ViewportFadeMaterial = nullptr;
	bProgrammaticScrollCallbackPending = false;
	bHasObservedScrollBoxSize = false;
	bHasObservedRetainerSize = false;
	bViewportFadeDirty = false;

	if (SB_MessageList)
	{
		SB_MessageList->OnUserScrolled.RemoveAll(this);
	}
	if (BTN_ReturnToLatest)
	{
		BTN_ReturnToLatest->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UMyStreamingChatPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bEnableMotion)
	{
		TickMessageMotions(InDeltaTime);
	}
	else if (!ActiveMessageMotions.IsEmpty())
	{
		for (const FMessageMotionState& Motion : ActiveMessageMotions)
		{
			if (UWidget* Widget = Motion.Widget.Get())
			{
				Widget->SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
		ActiveMessageMotions.Reset();
	}

	if (BTN_ReturnToLatest && !FMath::IsNearlyEqual(ReturnButtonOpacity, ReturnButtonTargetOpacity))
	{
		const float FadeSpeed = 1.0f / FMath::Max(ReturnButtonFadeSeconds, KINDA_SMALL_NUMBER);
		ReturnButtonOpacity = FMath::FInterpConstantTo(
			ReturnButtonOpacity,
			ReturnButtonTargetOpacity,
			InDeltaTime,
			FadeSpeed);
		BTN_ReturnToLatest->SetRenderOpacity(ReturnButtonOpacity);

		if (FMath::IsNearlyZero(ReturnButtonOpacity) && FMath::IsNearlyZero(ReturnButtonTargetOpacity))
		{
			ReturnButtonOpacity = 0.0f;
			BTN_ReturnToLatest->SetRenderOpacity(0.0f);
			BTN_ReturnToLatest->SetIsEnabled(false);
			BTN_ReturnToLatest->SetVisibility(ESlateVisibility::HitTestInvisible);
			bReturnToLatestActive = false;
		}
	}

	if (SB_MessageList)
	{
		const FVector2D CurrentScrollBoxSize = SB_MessageList->GetCachedGeometry().GetLocalSize();
		if (CurrentScrollBoxSize.X > KINDA_SMALL_NUMBER
			&& CurrentScrollBoxSize.Y > KINDA_SMALL_NUMBER)
		{
			if (!bHasObservedScrollBoxSize)
			{
				LastObservedScrollBoxSize = CurrentScrollBoxSize;
				bHasObservedScrollBoxSize = true;
			}
			else if (!CurrentScrollBoxSize.Equals(LastObservedScrollBoxSize, 0.5f))
			{
				if (!IsFollowingLatest() && !bDeferredLayoutRefreshPending)
				{
					CaptureScrollAnchor();
				}
				LastObservedScrollBoxSize = CurrentScrollBoxSize;
				ScheduleDeferredLayoutRefresh(bAutoScrollToLatest && IsFollowingLatest());
				MarkViewportFadeDirty();
			}
		}
	}

	if (RTB_MessageViewport)
	{
		const FVector2D CurrentRetainerSize =
			RTB_MessageViewport->GetCachedGeometry().GetLocalSize();
		if (CurrentRetainerSize.X > KINDA_SMALL_NUMBER
			&& CurrentRetainerSize.Y > KINDA_SMALL_NUMBER)
		{
			if (!bHasObservedRetainerSize)
			{
				LastObservedRetainerSize = CurrentRetainerSize;
				bHasObservedRetainerSize = true;
				MarkViewportFadeDirty();
			}
			else if (!CurrentRetainerSize.Equals(LastObservedRetainerSize, 0.5f))
			{
				LastObservedRetainerSize = CurrentRetainerSize;
				MarkViewportFadeDirty();
			}
		}
	}

	if (bViewportFadeDirty)
	{
		UpdateViewportFadeParameters();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 새 메시지를 FIFO 목록 끝에 추가하고 현재 스크롤 모드에 맞게 화면 위치를 갱신한다.
//! \param MessageData 표시할 스트리밍 채팅 메시지 데이터
//! \return 생성되어 목록에 추가된 말풍선 위젯
UMyStreamingChatBubbleWidget* UMyStreamingChatPanelWidget::AddMessage(
	const FMyStreamingChatMessageData& MessageData)
{
	UPanelWidget* MessageList = GetMessageListPanel();
	if (!MessageList || !ChatBubbleWidgetClass)
	{
		return nullptr;
	}

	if (SB_MessageList)
	{
		UpdateScrollBoundaries(SB_MessageList->GetScrollOffset());
	}
	const bool bShouldFollowLatest = bAutoScrollToLatest && IsFollowingLatest();

	if (bShouldFollowLatest && bEnableMotion)
	{
		if (!bDeferredLayoutRefreshPending)
		{
			CaptureLatestInsertionLayout();
		}
		bDeferredAnimateLatestInsertion = true;
	}
	else if (!bShouldFollowLatest && !bDeferredLayoutRefreshPending)
	{
		CaptureScrollAnchor();
	}

	UMyStreamingChatBubbleWidget* NewBubbleWidget =
		CreateWidget<UMyStreamingChatBubbleWidget>(this, ChatBubbleWidgetClass);
	if (!NewBubbleWidget)
	{
		return nullptr;
	}

	NewBubbleWidget->SetMessage(MessageData);
	UPanelSlot* NewSlot = MessageList->AddChild(NewBubbleWidget);
	if (!NewSlot)
	{
		return nullptr;
	}
	ConfigureMessageSlot(NewSlot);

	if (bShouldFollowLatest && bEnableMotion)
	{
		PendingNewBubbles.Add(NewBubbleWidget);
	}

	TrimOldMessages();
	MarkViewportFadeDirty();
	ScheduleDeferredLayoutRefresh(bShouldFollowLatest);
	return NewBubbleWidget;
}

////////////////////////////
//! \author 장효제
//! \brief 모든 채팅 버블과 표시 상태를 제거하고 ScrollBottom 최신 추적 상태로 초기화한다.
void UMyStreamingChatPanelWidget::ClearMessages()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredLayoutTimerHandle);
	}

	bDeferredLayoutRefreshPending = false;
	bDeferredScrollToLatest = false;
	bDeferredAnimateLatestInsertion = false;
	PendingAnchorWidget.Reset();
	PendingMessagePositions.Reset();
	PendingNewBubbles.Reset();
	ActiveMessageMotions.Reset();

	if (UPanelWidget* MessageList = GetMessageListPanel())
	{
		MessageList->ClearChildren();
	}

	ScrollMode = EStreamingChatScrollMode::FollowLatest;
	bReturningToLatest = false;
	SetReturnToLatestVisible(false);
	MarkViewportFadeDirty();
	ScheduleDeferredLayoutRefresh(true);
}

////////////////////////////
//! \author 장효제
//! \brief 패널이 유지할 최대 FIFO 메시지 개수를 설정하고 초과한 가장 오래된 항목을 제거한다.
//! \param InMaxMessageCount 유지할 최대 메시지 개수
void UMyStreamingChatPanelWidget::SetMaxMessageCount(int32 InMaxMessageCount)
{
	if (SB_MessageList)
	{
		UpdateScrollBoundaries(SB_MessageList->GetScrollOffset());
	}
	const bool bShouldFollowLatest = bAutoScrollToLatest && IsFollowingLatest();
	if (!bShouldFollowLatest && !bDeferredLayoutRefreshPending)
	{
		CaptureScrollAnchor();
	}

	MaxMessageCount = FMath::Max(InMaxMessageCount, 1);
	TrimOldMessages();
	MarkViewportFadeDirty();
	ScheduleDeferredLayoutRefresh(bShouldFollowLatest);
}

void UMyStreamingChatPanelWidget::RegisterChatMessageListener()
{
	UnregisterChatMessageListener();
	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	ChatMessageListenerHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FMyStreamingChatMessageData>(
		MyGameplayTags::Streaming_Channel_UI_Chat,
		this,
		&ThisClass::HandleChatMessage);
}

void UMyStreamingChatPanelWidget::UnregisterChatMessageListener()
{
	if (ChatMessageListenerHandle.IsValid())
	{
		ChatMessageListenerHandle.Unregister();
	}
}

void UMyStreamingChatPanelWidget::HandleChatMessage(
	FGameplayTag Channel,
	const FMyStreamingChatMessageData& MessageData)
{
	AddMessage(MessageData);
}

void UMyStreamingChatPanelWidget::TrimOldMessages()
{
	UPanelWidget* MessageList = GetMessageListPanel();
	if (!MessageList)
	{
		return;
	}

	int32 MessageCount = 0;
	for (int32 Index = 0; Index < MessageList->GetChildrenCount(); ++Index)
	{
		MessageCount += Cast<UMyStreamingChatBubbleWidget>(MessageList->GetChildAt(Index)) ? 1 : 0;
	}

	bool bTrimmedMessage = false;
	while (MessageCount > MaxMessageCount)
	{
		int32 RemovingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < MessageList->GetChildrenCount(); ++Index)
		{
			if (Cast<UMyStreamingChatBubbleWidget>(MessageList->GetChildAt(Index)))
			{
				RemovingIndex = Index;
				break;
			}
		}

		if (RemovingIndex == INDEX_NONE)
		{
			break;
		}

		UWidget* RemovingWidget = MessageList->GetChildAt(RemovingIndex);
		PrepareAnchorForRemoval(RemovingWidget, RemovingIndex);
		PendingMessagePositions.RemoveAll([RemovingWidget](const FPendingMessagePosition& Position)
		{
			return Position.Widget.Get() == RemovingWidget;
		});
		ActiveMessageMotions.RemoveAll([RemovingWidget](const FMessageMotionState& Motion)
		{
			return Motion.Widget.Get() == RemovingWidget;
		});
		MessageList->RemoveChildAt(RemovingIndex);
		--MessageCount;
		bTrimmedMessage = true;
	}

	if (bTrimmedMessage)
	{
		MarkViewportFadeDirty();
	}
}

void UMyStreamingChatPanelWidget::PrepareAnchorForRemoval(
	UWidget* RemovingWidget,
	int32 RemovingIndex)
{
	if (PendingAnchorWidget.Get() != RemovingWidget || !SB_MessageList)
	{
		return;
	}

	UPanelWidget* MessageList = GetMessageListPanel();
	if (!MessageList)
	{
		return;
	}

	UWidget* ReplacementAnchor = nullptr;
	float ReplacementAbsoluteY = 0.0f;
	const FGeometry ViewportGeometry = SB_MessageList->GetCachedGeometry();
	const float ViewportTop = ViewportGeometry.GetAbsolutePosition().Y;
	const float ViewportBottom = ViewportTop + ViewportGeometry.GetAbsoluteSize().Y;

	for (int32 Index = RemovingIndex + 1; Index < MessageList->GetChildrenCount(); ++Index)
	{
		UWidget* Candidate = MessageList->GetChildAt(Index);
		if (!Cast<UMyStreamingChatBubbleWidget>(Candidate))
		{
			continue;
		}

		float CandidateAbsoluteY = 0.0f;
		if (!FindArrangedMessageAbsoluteY(Candidate, CandidateAbsoluteY))
		{
			continue;
		}

		const float CandidateBottom =
			CandidateAbsoluteY + Candidate->GetCachedGeometry().GetAbsoluteSize().Y;
		if (CandidateBottom > ViewportTop && CandidateAbsoluteY < ViewportBottom)
		{
			ReplacementAnchor = Candidate;
			ReplacementAbsoluteY = CandidateAbsoluteY;
			break;
		}
	}

	PendingAnchorWidget = ReplacementAnchor;
	PendingAnchorAbsoluteY = ReplacementAnchor ? ReplacementAbsoluteY : 0.0f;
	PendingScrollOffset = SB_MessageList->GetScrollOffset();
}

UPanelWidget* UMyStreamingChatPanelWidget::GetMessageListPanel() const
{
	return VB_MessageList ? Cast<UPanelWidget>(VB_MessageList) : Cast<UPanelWidget>(SB_MessageList);
}

void UMyStreamingChatPanelWidget::ConfigureMessageSlot(UPanelSlot* MessageSlot) const
{
	if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(MessageSlot))
	{
		VerticalBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		VerticalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	else if (UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(MessageSlot))
	{
		ScrollBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		ScrollBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

////////////////////////////
//! \author 장효제
//! \brief ScrollBottom 거리와 히스테리시스로 FollowLatest 또는 Browsing 모드를 갱신한다.
//! \param CurrentOffset 현재 ScrollBox 오프셋
void UMyStreamingChatPanelWidget::UpdateScrollBoundaries(float CurrentOffset)
{
	const EStreamingChatScrollMode PreviousScrollMode = ScrollMode;
	LatestEnterDistance = FMath::Max(LatestEnterDistance, 0.0f);
	LatestExitDistance = FMath::Max(LatestExitDistance, LatestEnterDistance + 1.0f);

	const float EndOffset = GetMaxScrollOffset();
	const float SafeOffset = FMath::Clamp(CurrentOffset, 0.0f, EndOffset);
	const float DistanceFromBottom = EndOffset - SafeOffset;

	if (EndOffset <= ScrollBoundaryTolerance)
	{
		ScrollMode = EStreamingChatScrollMode::FollowLatest;
	}
	else if (ScrollMode == EStreamingChatScrollMode::FollowLatest)
	{
		const bool bReachedOldestEdge =
			SafeOffset <= ScrollBoundaryTolerance
			&& DistanceFromBottom > LatestEnterDistance;
		if (DistanceFromBottom >= LatestExitDistance || bReachedOldestEdge)
		{
			ScrollMode = EStreamingChatScrollMode::Browsing;
		}
	}
	else if (DistanceFromBottom <= LatestEnterDistance)
	{
		ScrollMode = EStreamingChatScrollMode::FollowLatest;
	}

	if (bReturningToLatest && ScrollMode == EStreamingChatScrollMode::FollowLatest)
	{
		bReturningToLatest = false;
	}
	UpdateReturnButtonState();
	if (ScrollMode != PreviousScrollMode)
	{
		MarkViewportFadeDirty();
	}
}

float UMyStreamingChatPanelWidget::GetMaxScrollOffset() const
{
	return SB_MessageList
		? FMath::Max(SB_MessageList->GetScrollOffsetOfEnd(), 0.0f)
		: 0.0f;
}

////////////////////////////
//! \author 장효제
//! \brief 현재 ScrollOffset에서 가장 오래된 상단 경계까지의 거리를 반환한다.
//! \param CurrentOffset 현재 ScrollBox 오프셋
//! \return 상단 경계까지 Clamp된 거리
float UMyStreamingChatPanelWidget::GetDistanceFromTop(float CurrentOffset) const
{
	const float EndOffset = GetMaxScrollOffset();
	return FMath::Clamp(CurrentOffset, 0.0f, EndOffset);
}

float UMyStreamingChatPanelWidget::GetDistanceFromBottom(float CurrentOffset) const
{
	const float EndOffset = GetMaxScrollOffset();
	return EndOffset - FMath::Clamp(CurrentOffset, 0.0f, EndOffset);
}

bool UMyStreamingChatPanelWidget::IsScrollable() const
{
	return GetMaxScrollOffset() > ScrollBoundaryTolerance;
}

bool UMyStreamingChatPanelWidget::IsFollowingLatest() const
{
	return !SB_MessageList || ScrollMode == EStreamingChatScrollMode::FollowLatest;
}

UWidget* UMyStreamingChatPanelWidget::FindFirstVisibleMessage(float& OutAbsoluteY) const
{
	OutAbsoluteY = 0.0f;
	UPanelWidget* MessageList = GetMessageListPanel();
	if (!SB_MessageList || !MessageList)
	{
		return nullptr;
	}

	const FGeometry ViewportGeometry = SB_MessageList->GetCachedGeometry();
	const float ViewportTop = ViewportGeometry.GetAbsolutePosition().Y;
	const float ViewportBottom = ViewportTop + ViewportGeometry.GetAbsoluteSize().Y;
	UWidget* FirstVisibleChild = nullptr;
	float FirstVisibleY = ViewportBottom;

	for (int32 Index = 0; Index < MessageList->GetChildrenCount(); ++Index)
	{
		UWidget* Child = MessageList->GetChildAt(Index);
		if (!Cast<UMyStreamingChatBubbleWidget>(Child))
		{
			continue;
		}

		float ChildTop = 0.0f;
		if (!FindArrangedMessageAbsoluteY(Child, ChildTop))
		{
			continue;
		}
		const float ChildBottom = ChildTop + Child->GetCachedGeometry().GetAbsoluteSize().Y;
		if (ChildBottom > ViewportTop && ChildTop < ViewportBottom && ChildTop < FirstVisibleY)
		{
			FirstVisibleChild = Child;
			FirstVisibleY = ChildTop;
		}
	}

	OutAbsoluteY = FirstVisibleChild ? FirstVisibleY : 0.0f;
	return FirstVisibleChild;
}

bool UMyStreamingChatPanelWidget::FindArrangedMessageAbsoluteY(
	const UWidget* MessageWidget,
	float& OutAbsoluteY) const
{
	OutAbsoluteY = 0.0f;
	UPanelWidget* MessageList = GetMessageListPanel();
	if (!MessageList || !MessageWidget)
	{
		return false;
	}

	const TSharedPtr<SWidget> MessageListSlateWidget = MessageList->GetCachedWidget();
	const TSharedPtr<SWidget> TargetSlateWidget = MessageWidget->GetCachedWidget();
	if (MessageListSlateWidget.IsValid() && TargetSlateWidget.IsValid())
	{
		FArrangedChildren ArrangedChildren(EVisibility::All);
		MessageListSlateWidget->ArrangeChildren(
			MessageList->GetCachedGeometry(),
			ArrangedChildren,
			true);
		for (const FArrangedWidget& ArrangedChild : ArrangedChildren.GetInternalArray())
		{
			if (ArrangedChild.GetWidgetPtr() == TargetSlateWidget.Get())
			{
				OutAbsoluteY = ArrangedChild.Geometry.GetAbsolutePosition().Y;
				return true;
			}
		}
	}

	OutAbsoluteY = MessageWidget->GetCachedGeometry().GetAbsolutePosition().Y;
	return true;
}

void UMyStreamingChatPanelWidget::CaptureScrollAnchor()
{
	PendingAnchorWidget.Reset();
	if (!SB_MessageList)
	{
		return;
	}

	PendingScrollOffset = SB_MessageList->GetScrollOffset();
	float AnchorAbsoluteY = 0.0f;
	if (UWidget* AnchorWidget = FindFirstVisibleMessage(AnchorAbsoluteY))
	{
		PendingAnchorWidget = AnchorWidget;
		PendingAnchorAbsoluteY = AnchorAbsoluteY;
	}
}

void UMyStreamingChatPanelWidget::CaptureLatestInsertionLayout()
{
	PendingMessagePositions.Reset();
	if (!SB_MessageList)
	{
		return;
	}

	UPanelWidget* MessageList = GetMessageListPanel();
	if (!MessageList)
	{
		return;
	}

	const FGeometry ViewportGeometry = SB_MessageList->GetCachedGeometry();
	const float ViewportTop = ViewportGeometry.GetAbsolutePosition().Y;
	const float ViewportBottom = ViewportTop + ViewportGeometry.GetAbsoluteSize().Y;
	for (int32 Index = 0; Index < MessageList->GetChildrenCount(); ++Index)
	{
		UWidget* Child = MessageList->GetChildAt(Index);
		if (!Cast<UMyStreamingChatBubbleWidget>(Child))
		{
			continue;
		}

		float ChildTop = 0.0f;
		if (!FindArrangedMessageAbsoluteY(Child, ChildTop))
		{
			continue;
		}
		const float ChildBottom = ChildTop + Child->GetCachedGeometry().GetAbsoluteSize().Y;
		if (ChildBottom > ViewportTop && ChildTop < ViewportBottom)
		{
			FPendingMessagePosition& PendingPosition = PendingMessagePositions.AddDefaulted_GetRef();
			PendingPosition.Widget = Child;
			PendingPosition.AbsoluteY = ChildTop;
		}
	}
}

void UMyStreamingChatPanelWidget::ScheduleDeferredLayoutRefresh(bool bScrollToLatest)
{
	bDeferredScrollToLatest |= bScrollToLatest;
	if (bDeferredLayoutRefreshPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bDeferredLayoutRefreshPending = true;
	TWeakObjectPtr<UMyStreamingChatPanelWidget> WeakThis(this);
	DeferredLayoutTimerHandle = World->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ApplyDeferredLayoutRefresh();
		}
	});
}

void UMyStreamingChatPanelWidget::SetProgrammaticScrollOffset(
	float TargetOffset,
	bool bScrollToEnd)
{
	if (!SB_MessageList)
	{
		return;
	}

	const float EndOffset = GetMaxScrollOffset();
	ProgrammaticScrollTargetOffset = bScrollToEnd
		? EndOffset
		: FMath::Clamp(TargetOffset, 0.0f, EndOffset);
	bProgrammaticScrollCallbackPending = true;
	bApplyingProgrammaticScroll = true;
	SB_MessageList->SetScrollOffset(ProgrammaticScrollTargetOffset);
	if (bScrollToEnd)
	{
		SB_MessageList->ScrollToEnd();
	}
	bApplyingProgrammaticScroll = false;
	MarkViewportFadeDirty();
}

bool UMyStreamingChatPanelWidget::ConsumeProgrammaticScrollCallback(float CurrentOffset)
{
	if (bApplyingProgrammaticScroll)
	{
		return true;
	}

	if (!bProgrammaticScrollCallbackPending)
	{
		return false;
	}

	const bool bReachedTarget =
		FMath::Abs(CurrentOffset - ProgrammaticScrollTargetOffset)
		<= FMath::Max(LatestEnterDistance, 1.0f);
	bProgrammaticScrollCallbackPending = false;
	return bReachedTarget;
}

void UMyStreamingChatPanelWidget::ApplyDeferredLayoutRefresh()
{
	bDeferredLayoutRefreshPending = false;
	ForceLayoutPrepass();

	if (SB_MessageList)
	{
		if (bDeferredScrollToLatest)
		{
			ScrollMode = EStreamingChatScrollMode::FollowLatest;
			SetProgrammaticScrollOffset(GetMaxScrollOffset(), true);
		}
		else if (PendingAnchorWidget.IsValid())
		{
			float AnchorAbsoluteY = 0.0f;
			if (FindArrangedMessageAbsoluteY(PendingAnchorWidget.Get(), AnchorAbsoluteY))
			{
				const float GeometryScale = FMath::Max(
					SB_MessageList->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale(),
					KINDA_SMALL_NUMBER);
				const float AnchorDelta =
					(AnchorAbsoluteY - PendingAnchorAbsoluteY) / GeometryScale;
				SetProgrammaticScrollOffset(PendingScrollOffset + AnchorDelta, false);
			}
		}
		else
		{
			SetProgrammaticScrollOffset(PendingScrollOffset, false);
		}
	}

	if (bDeferredAnimateLatestInsertion)
	{
		BeginLatestInsertionMotion();
	}

	bDeferredAnimateLatestInsertion = false;
	bDeferredScrollToLatest = false;
	PendingAnchorWidget.Reset();

	if (SB_MessageList)
	{
		UpdateScrollBoundaries(SB_MessageList->GetScrollOffset());
	}
	MarkViewportFadeDirty();
}

void UMyStreamingChatPanelWidget::BeginLatestInsertionMotion()
{
	if (!bEnableMotion)
	{
		PendingMessagePositions.Reset();
		PendingNewBubbles.Reset();
		return;
	}

	const float GeometryScale = SB_MessageList
		? FMath::Max(
			SB_MessageList->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale(),
			KINDA_SMALL_NUMBER)
		: 1.0f;

	for (const FPendingMessagePosition& PendingPosition : PendingMessagePositions)
	{
		UWidget* Widget = PendingPosition.Widget.Get();
		if (!Widget)
		{
			continue;
		}

		CancelActiveMotionsForWidget(Widget);

		float NewAbsoluteY = 0.0f;
		if (!FindArrangedMessageAbsoluteY(Widget, NewAbsoluteY))
		{
			continue;
		}
		const FVector2D StartTranslation(
			0.0f,
			(PendingPosition.AbsoluteY - NewAbsoluteY) / GeometryScale);
		if (FMath::IsNearlyZero(StartTranslation.Y))
		{
			continue;
		}

		Widget->SetRenderTranslation(StartTranslation);
		FMessageMotionState& Motion = ActiveMessageMotions.AddDefaulted_GetRef();
		Motion.Widget = Widget;
		Motion.StartTranslation = StartTranslation;
		Motion.DurationSeconds = MessageReflowAnimationSeconds;
	}

	for (const TWeakObjectPtr<UMyStreamingChatBubbleWidget>& PendingBubble : PendingNewBubbles)
	{
		UMyStreamingChatBubbleWidget* Bubble = PendingBubble.Get();
		if (!Bubble)
		{
			continue;
		}

		CancelActiveMotionsForWidget(Bubble);

		const FVector2D StartTranslation(0.0f, MessageEntryOffset);
		Bubble->SetRenderTranslation(StartTranslation);
		FMessageMotionState& Motion = ActiveMessageMotions.AddDefaulted_GetRef();
		Motion.Widget = Bubble;
		Motion.StartTranslation = StartTranslation;
		Motion.DurationSeconds = MessageEntryAnimationSeconds;
	}

	PendingMessagePositions.Reset();
	PendingNewBubbles.Reset();
}

void UMyStreamingChatPanelWidget::TickMessageMotions(float DeltaTime)
{
	for (int32 Index = ActiveMessageMotions.Num() - 1; Index >= 0; --Index)
	{
		FMessageMotionState& Motion = ActiveMessageMotions[Index];
		UWidget* Widget = Motion.Widget.Get();
		if (!Widget)
		{
			ActiveMessageMotions.RemoveAtSwap(Index);
			continue;
		}

		Motion.ElapsedSeconds += DeltaTime;
		const float Alpha = FMath::Clamp(
			Motion.ElapsedSeconds / FMath::Max(Motion.DurationSeconds, KINDA_SMALL_NUMBER),
			0.0f,
			1.0f);
		const float EasedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
		Widget->SetRenderTranslation(
			FMath::Lerp(Motion.StartTranslation, FVector2D::ZeroVector, EasedAlpha));

		if (Alpha >= 1.0f)
		{
			Widget->SetRenderTranslation(FVector2D::ZeroVector);
			ActiveMessageMotions.RemoveAtSwap(Index);
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 위젯에 남아 있는 Translation 모션을 취소합니다.
//! \param Widget 모션을 취소할 위젯
void UMyStreamingChatPanelWidget::CancelActiveMotionsForWidget(UWidget* Widget)
{
	if (!IsValid(Widget))
	{
		return;
	}

	ActiveMessageMotions.RemoveAll(
		[Widget](const FMessageMotionState& Motion)
		{
			return Motion.Widget.Get() == Widget;
		});
}

void UMyStreamingChatPanelWidget::MarkViewportFadeDirty()
{
	bViewportFadeDirty = true;
}

////////////////////////////
//! \author 장효제
//! \brief Optional Retainer가 소유한 Dynamic Effect Material을 획득합니다.
void UMyStreamingChatPanelWidget::InitializeViewportFadeMaterial()
{
	if (!RTB_MessageViewport)
	{
		ViewportFadeMaterial = nullptr;
		if (!bHasWarnedMissingRetainer)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[7P_JangHyoje][GodChat] RTB_MessageViewport is not bound. Viewport Fade is disabled until the WBP Retainer migration is completed."));
			bHasWarnedMissingRetainer = true;
		}
		return;
	}

	UMaterialInstanceDynamic* CurrentEffectMaterial =
		RTB_MessageViewport->GetEffectMaterial();
	if (!CurrentEffectMaterial)
	{
		ViewportFadeMaterial = nullptr;
		if (!bHasWarnedMissingEffectMaterial)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[7P_JangHyoje][GodChat] RTB_MessageViewport has no Effect Material MID. Viewport Fade is disabled."));
			bHasWarnedMissingEffectMaterial = true;
		}
		return;
	}

	if (ViewportFadeMaterial != CurrentEffectMaterial)
	{
		ViewportFadeMaterial = CurrentEffectMaterial;
		LastTopFadeHeight = -1.0f;
		LastBottomFadeHeight = -1.0f;
		LastTopFadeStrength = -1.0f;
		LastBottomFadeStrength = -1.0f;
	}

}

////////////////////////////
//! \author 장효제
//! \brief 현재 스크롤 경계에서 Retainer 화면 공간 Fade 파라미터를 계산하고 변경된 값만 MID에 적용합니다.
void UMyStreamingChatPanelWidget::UpdateViewportFadeParameters()
{
	if (bDeferredLayoutRefreshPending)
	{
		return;
	}

	bViewportFadeDirty = false;
	InitializeViewportFadeMaterial();
	if (!ViewportFadeMaterial || !RTB_MessageViewport)
	{
		return;
	}

	float TopStrength = 0.0f;
	float BottomStrength = 0.0f;
	if (bEnableViewportFade && SB_MessageList && IsScrollable())
	{
		const float CurrentOffset = FMath::Clamp(
			SB_MessageList->GetScrollOffset(),
			0.0f,
			GetMaxScrollOffset());
		TopStrength = CalculateFadeWeight(
			GetDistanceFromTop(CurrentOffset),
			TopFadeTransitionDistance);
		BottomStrength = CalculateFadeWeight(
			GetDistanceFromBottom(CurrentOffset),
			BottomFadeTransitionDistance);
	}

	const float SafeTopHeight = FMath::Clamp(TopFadeHeight, 0.0f, 0.5f);
	const float SafeBottomHeight = FMath::Clamp(BottomFadeHeight, 0.0f, 0.5f);
	TopStrength = FMath::Clamp(TopStrength, 0.0f, 1.0f);
	BottomStrength = FMath::Clamp(BottomStrength, 0.0f, 1.0f);
	bool bParametersChanged = false;

	auto SetScalarIfChanged =
		[this, &bParametersChanged](
			const FName ParameterName,
			const float Value,
			float& LastValue)
	{
		if (!FMath::IsNearlyEqual(LastValue, Value))
		{
			ViewportFadeMaterial->SetScalarParameterValue(ParameterName, Value);
			LastValue = Value;
			bParametersChanged = true;
		}
	};

	SetScalarIfChanged(TopFadeHeightParameterName, SafeTopHeight, LastTopFadeHeight);
	SetScalarIfChanged(BottomFadeHeightParameterName, SafeBottomHeight, LastBottomFadeHeight);
	SetScalarIfChanged(TopFadeStrengthParameterName, TopStrength, LastTopFadeStrength);
	SetScalarIfChanged(BottomFadeStrengthParameterName, BottomStrength, LastBottomFadeStrength);

	if (bParametersChanged)
	{
		RTB_MessageViewport->RequestRender();
	}
}

float UMyStreamingChatPanelWidget::CalculateFadeWeight(
	float Distance,
	float TransitionDistance) const
{
	const float T = FMath::Clamp(
		Distance / FMath::Max(TransitionDistance, KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);
	return T * T * (3.0f - 2.0f * T);
}

void UMyStreamingChatPanelWidget::UpdateReturnButtonState()
{
	const bool bShouldShow =
		IsScrollable()
		&& ScrollMode == EStreamingChatScrollMode::Browsing
		&& !bReturningToLatest;
	SetReturnToLatestVisible(bShouldShow);
}

void UMyStreamingChatPanelWidget::SetReturnToLatestVisible(bool bVisible)
{
	ReturnButtonTargetOpacity = bVisible ? 1.0f : 0.0f;
	if (!BTN_ReturnToLatest)
	{
		bReturnToLatestActive = bVisible;
		return;
	}

	if (!bEnableMotion)
	{
		ReturnButtonOpacity = ReturnButtonTargetOpacity;
		BTN_ReturnToLatest->SetRenderOpacity(ReturnButtonOpacity);
		BTN_ReturnToLatest->SetIsEnabled(bVisible && !bReturningToLatest);
		BTN_ReturnToLatest->SetVisibility(
			bVisible ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
		bReturnToLatestActive = bVisible;
		return;
	}

	if (bVisible)
	{
		bReturnToLatestActive = true;
		BTN_ReturnToLatest->SetVisibility(ESlateVisibility::Visible);
		BTN_ReturnToLatest->SetIsEnabled(!bReturningToLatest);
	}
	else
	{
		BTN_ReturnToLatest->SetIsEnabled(false);
		BTN_ReturnToLatest->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (FMath::IsNearlyZero(ReturnButtonOpacity))
		{
			bReturnToLatestActive = false;
		}
	}
}

void UMyStreamingChatPanelWidget::HandleUserScrolled(float CurrentOffset)
{
	const bool bProgrammaticScroll = ConsumeProgrammaticScrollCallback(CurrentOffset);
	if (!bProgrammaticScroll && bReturningToLatest)
	{
		// Return 이동 중 사용자가 다시 스크롤하면 사용자 입력을 우선하고 Browsing을 복구한다.
		bReturningToLatest = false;
	}
	UpdateScrollBoundaries(CurrentOffset);
	MarkViewportFadeDirty();
}

void UMyStreamingChatPanelWidget::HandleReturnToLatestClicked()
{
	if (!bReturnToLatestActive || bReturningToLatest || !SB_MessageList)
	{
		return;
	}

	bReturningToLatest = true;
	BTN_ReturnToLatest->SetIsEnabled(false);
	SetReturnToLatestVisible(false);
	SetProgrammaticScrollOffset(GetMaxScrollOffset(), true);
	ScheduleDeferredLayoutRefresh(true);
	MarkViewportFadeDirty();
}
