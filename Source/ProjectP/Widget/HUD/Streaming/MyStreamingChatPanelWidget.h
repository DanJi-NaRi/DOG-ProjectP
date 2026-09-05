////////////////////////////
//! \page MyStreamingChatPanelWidget.h
//! \brief Streaming Chat FIFO 목록, ScrollBottom 추적, Retainer 화면 공간 양끝 Fade를 관리하는 HUD 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Streaming/MyStreamingChatTypes.h"
#include "MyStreamingChatPanelWidget.generated.h"

class UButton;
class UMaterialInstanceDynamic;
class UMyStreamingChatBubbleWidget;
class UPanelSlot;
class UPanelWidget;
class URetainerBox;
class UScrollBox;
class UVerticalBox;
class UWidget;

////////////////////////////
//! \class UMyStreamingChatPanelWidget
//! \author 장효제
//! \brief 오래된 메시지부터 최신 메시지까지 FIFO로 표시하고 로컬 스크롤·페이드 상태를 관리한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyStreamingChatPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|StreamingChat")
	UMyStreamingChatBubbleWidget* AddMessage(const FMyStreamingChatMessageData& MessageData);

	UFUNCTION(BlueprintCallable, Category = "UI|StreamingChat")
	void ClearMessages();

	UFUNCTION(BlueprintCallable, Category = "UI|StreamingChat")
	void SetMaxMessageCount(int32 InMaxMessageCount);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat")
	TSubclassOf<UMyStreamingChatBubbleWidget> ChatBubbleWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat", meta = (ClampMin = "1"))
	int32 MaxMessageCount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat")
	bool bAutoScrollToLatest = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Scroll", meta = (ClampMin = "0.0"))
	float LatestEnterDistance = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Scroll", meta = (ClampMin = "0.0"))
	float LatestExitDistance = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Motion", meta = (ClampMin = "0.01"))
	float ReturnButtonFadeSeconds = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ViewportFade")
	bool bEnableViewportFade = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ViewportFade", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TopFadeHeight = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ViewportFade", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BottomFadeHeight = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ViewportFade", meta = (ClampMin = "16.0", ClampMax = "160.0"))
	float TopFadeTransitionDistance = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|ViewportFade", meta = (ClampMin = "16.0", ClampMax = "160.0"))
	float BottomFadeTransitionDistance = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Motion")
	bool bEnableMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Motion", meta = (ClampMin = "0.01"))
	float MessageEntryAnimationSeconds = 0.13f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Motion", meta = (ClampMin = "0.0"))
	float MessageEntryOffset = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StreamingChat|Motion", meta = (ClampMin = "0.01"))
	float MessageReflowAnimationSeconds = 0.14f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UScrollBox> SB_MessageList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<URetainerBox> RTB_MessageViewport;

	//! ScrollBox의 단일 자식. 실제 버블은 오래된 메시지부터 최신 메시지 순서로 들어간다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> VB_MessageList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BTN_ReturnToLatest;

private:
	enum class EStreamingChatScrollMode : uint8
	{
		FollowLatest,
		Browsing
	};

	struct FPendingMessagePosition
	{
		TWeakObjectPtr<UWidget> Widget;
		float AbsoluteY = 0.0f;
	};

	struct FMessageMotionState
	{
		TWeakObjectPtr<UWidget> Widget;
		FVector2D StartTranslation = FVector2D::ZeroVector;
		float ElapsedSeconds = 0.0f;
		float DurationSeconds = 0.0f;
	};

	void RegisterChatMessageListener();
	void UnregisterChatMessageListener();
	void HandleChatMessage(FGameplayTag Channel, const FMyStreamingChatMessageData& MessageData);
	void TrimOldMessages();
	void PrepareAnchorForRemoval(UWidget* RemovingWidget, int32 RemovingIndex);
	UPanelWidget* GetMessageListPanel() const;
	void ConfigureMessageSlot(UPanelSlot* MessageSlot) const;

	void UpdateScrollBoundaries(float CurrentOffset);
	float GetMaxScrollOffset() const;
	float GetDistanceFromTop(float CurrentOffset) const;
	float GetDistanceFromBottom(float CurrentOffset) const;
	bool IsScrollable() const;
	bool IsFollowingLatest() const;

	UWidget* FindFirstVisibleMessage(float& OutAbsoluteY) const;
	bool FindArrangedMessageAbsoluteY(const UWidget* MessageWidget, float& OutAbsoluteY) const;
	void CaptureScrollAnchor();
	void CaptureLatestInsertionLayout();
	void ScheduleDeferredLayoutRefresh(bool bScrollToLatest);
	void ApplyDeferredLayoutRefresh();
	void SetProgrammaticScrollOffset(float TargetOffset, bool bScrollToEnd);
	bool ConsumeProgrammaticScrollCallback(float CurrentOffset);

	void BeginLatestInsertionMotion();
	void TickMessageMotions(float DeltaTime);
	void CancelActiveMotionsForWidget(UWidget* Widget);

	void MarkViewportFadeDirty();
	void InitializeViewportFadeMaterial();
	void UpdateViewportFadeParameters();
	float CalculateFadeWeight(float Distance, float TransitionDistance) const;

	void UpdateReturnButtonState();
	void SetReturnToLatestVisible(bool bVisible);

	UFUNCTION()
	void HandleUserScrolled(float CurrentOffset);

	UFUNCTION()
	void HandleReturnToLatestClicked();

	FGameplayMessageListenerHandle ChatMessageListenerHandle;
	FTimerHandle DeferredLayoutTimerHandle;
	TWeakObjectPtr<UWidget> PendingAnchorWidget;
	TArray<FPendingMessagePosition> PendingMessagePositions;
	TArray<TWeakObjectPtr<UMyStreamingChatBubbleWidget>> PendingNewBubbles;
	TArray<FMessageMotionState> ActiveMessageMotions;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ViewportFadeMaterial;
	float PendingAnchorAbsoluteY = 0.0f;
	float PendingScrollOffset = 0.0f;
	float ProgrammaticScrollTargetOffset = 0.0f;
	float ReturnButtonOpacity = 0.0f;
	float ReturnButtonTargetOpacity = 0.0f;
	float LastTopFadeHeight = -1.0f;
	float LastBottomFadeHeight = -1.0f;
	float LastTopFadeStrength = -1.0f;
	float LastBottomFadeStrength = -1.0f;
	FVector2D LastObservedScrollBoxSize = FVector2D::ZeroVector;
	FVector2D LastObservedRetainerSize = FVector2D::ZeroVector;
	EStreamingChatScrollMode ScrollMode = EStreamingChatScrollMode::FollowLatest;
	bool bDeferredLayoutRefreshPending = false;
	bool bDeferredScrollToLatest = false;
	bool bDeferredAnimateLatestInsertion = false;
	bool bApplyingProgrammaticScroll = false;
	bool bProgrammaticScrollCallbackPending = false;
	bool bHasObservedScrollBoxSize = false;
	bool bHasObservedRetainerSize = false;
	bool bViewportFadeDirty = true;
	bool bHasWarnedMissingRetainer = false;
	bool bHasWarnedMissingEffectMaterial = false;
	bool bReturnToLatestActive = false;
	bool bReturningToLatest = false;
};
