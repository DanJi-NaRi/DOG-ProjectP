// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Types/SlateEnums.h"
#include "MessengerTypes.h"
#include "../GameInstance/SubSystems/Cheat/CPP_CheatCommandSubsystem.h"
#include "CPP_MessengerWidget.generated.h"

class UScrollBox;
class UBorder;
class UButton;
class UEditableText;
class UMenuAnchor;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UCPP_MessengerMessageBlock;

/**
 * 좌측 하단 메신저 위젯.
 * - 전체/파티 탭 필터 (전체 탭 = 전체+파티 통합, 파티 탭 = 파티만)
 * - 입력창 + 전송 채널 토글 + 전송 (모든 상호작용을 C++에서 바인딩)
 * 실제 배치/디자인은 WBP_Messenger 에서 하며, 아래 BindWidget 이름을 그대로 사용해야 한다.
 */
UCLASS()
class PROJECTP_API UCPP_MessengerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 서버에서 수신한 메시지를 히스토리에 저장하고 현재 탭이면 화면에 표시한다.
    UFUNCTION(BlueprintCallable, Category = "Messenger")
    void ReceiveMessengerMessage(const FMessengerMessage& Message);

    void AddEmptyTestMessages(int32 Count);

    // 보고 있는 탭을 변경한다. (전체/파티)
    UFUNCTION(BlueprintCallable, Category = "Messenger")
    void SetActiveTab(EMessengerChannel Tab);

    // 전송 채널을 설정한다. (전체/파티) - 드롭다운 등 별도 UI에서도 호출 가능
    UFUNCTION(BlueprintCallable, Category = "Messenger")
    void SetSendChannel(EMessengerChannel Channel);

    // 입력창의 텍스트를 현재 전송 채널로 전송한다.
    UFUNCTION(BlueprintCallable, Category = "Messenger")
    void SubmitInput();

    // 채팅 입력창에 키보드 포커스를 준다. (Enter로 채팅창 활성화할 때 PC가 호출)
    UFUNCTION(BlueprintCallable, Category = "Messenger")
    void FocusChatInput();

    UFUNCTION(BlueprintCallable, Category = "Messenger|Settings")
    void SetChatBackgroundEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Messenger|Settings")
    bool IsChatBackgroundEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Messenger|Settings")
    void SetChatFontSizeLevel(int32 InLevel);

    UFUNCTION(BlueprintPure, Category = "Messenger|Settings")
    int32 GetChatFontSizeLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Messenger|Settings")
    void CloseChatSettings();

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // 한 줄 메시지로 사용할 블록 위젯 클래스 (WBP_MessengerMessageBlock 지정)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger")
    TSubclassOf<UCPP_MessengerMessageBlock> MessageBlockClass;

    // 보관할 최대 메시지 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger", meta = (ClampMin = "1"))
    int32 MaxMessageHistory = 200;

    // 화면에 동시에 표시할 최대 블록 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger", meta = (ClampMin = "1"))
    int32 MaxVisibleMessages = 100;

    // 메시지 추가 시 맨 아래로 자동 스크롤할지 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger")
    bool bAutoScrollToEnd = true;

    // --- BindWidget : WBP에서 아래와 동일한 이름으로 배치해야 한다 ---
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> SB_MessageList;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_TabAll;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_TabParty;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> ETB_Input;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Send;

    // 전송 채널 토글 버튼/라벨 (선택적: 드롭다운 방식 쓰면 생략 가능)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_SendChannel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SendChannel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ChatSetting;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMenuAnchor> MA_ChatSettings;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BRD_MessageBackground;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger|Settings")
    TObjectPtr<UTexture2D> ChatBackgroundEnabledTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger|Settings")
    TObjectPtr<UTexture2D> ChatBackgroundDisabledTexture;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger|Settings", meta = (ClampMin = "1"))
    int32 SmallChatFontSize = 9;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger|Settings", meta = (ClampMin = "1"))
    int32 MediumChatFontSize = 12;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger|Settings", meta = (ClampMin = "1"))
    int32 LargeChatFontSize = 15;

    // 치트 자동완성 목록 컨테이너 (WBP에서 입력창 위에 배치, 없으면 자동완성 기능 꺼짐)
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VB_CheatSuggestions;

    // 치트 자동완성 목록 최대 표시 수
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger|Cheat", meta = (ClampMin = "1"))
    int32 MaxCheatSuggestions = 8;

    // 치트 자동완성 일반 항목 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger|Cheat")
    FLinearColor CheatSuggestionNormalColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);

    // 치트 자동완성 선택 항목 색
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger|Cheat")
    FLinearColor CheatSuggestionSelectedColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

    // 치트 자동완성 항목 폰트 크기 (기본 엔진 TextBlock은 24라 목록용으로는 크다)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Messenger|Cheat", meta = (ClampMin = "6"))
    int32 CheatSuggestionFontSize = 12;

    // 탭 변경 시 호출 (선택적 시각 강조용 BP 이벤트)
    UFUNCTION(BlueprintImplementableEvent, Category = "Messenger")
    void OnActiveTabChanged(EMessengerChannel Tab);

    // 전송 채널 변경 시 호출 (선택적 시각 강조용 BP 이벤트)
    UFUNCTION(BlueprintImplementableEvent, Category = "Messenger")
    void OnSendChannelChanged(EMessengerChannel Channel);

private:
    EMessengerChannel ActiveTab = EMessengerChannel::All;
    EMessengerChannel SendChannel = EMessengerChannel::All;

    // 수신한 전체 메시지 히스토리(탭 전환 시 재필터에 사용)
    TArray<FMessengerMessage> MessageHistory;

    UFUNCTION()
    void HandleTabAllClicked();

    UFUNCTION()
    void HandleTabPartyClicked();

    UFUNCTION()
    void HandleTabAllHovered();

    UFUNCTION()
    void HandleTabAllUnhovered();

    UFUNCTION()
    void HandleTabPartyHovered();

    UFUNCTION()
    void HandleTabPartyUnhovered();

    UFUNCTION()
    void HandleSendClicked();

    UFUNCTION()
    void HandleSendChannelClicked();

    UFUNCTION()
    void HandleChatSettingClicked();

    UFUNCTION()
    void HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleInputTextChanged(const FText& Text);

    UPROPERTY(Transient)
    FSlateBrush TabAllInactiveBrush;

    UPROPERTY(Transient)
    FSlateBrush TabAllActiveBrush;

    UPROPERTY(Transient)
    FSlateBrush TabPartyInactiveBrush;

    UPROPERTY(Transient)
    FSlateBrush TabPartyActiveBrush;

    UPROPERTY(Transient)
    TObjectPtr<UButton> HoveredTabButton;

    bool bTabBrushesCached = false;
    bool bChatBackgroundEnabled = false;
    int32 ChatFontSizeLevel = 1;

    // --- 치트 자동완성 내부 상태/처리 ---
    TArray<FCheatCommandInfo> CurrentCheatSuggestions;

    // 인자 자동완성 후보 (예: /additem 뒤 아이템 ID 목록). 비어 있지 않으면 인자 모드로 동작한다.
    TArray<FString> CurrentArgumentSuggestions;

    // 인자 후보 선택 시 앞에 붙일 텍스트 (입력 중이던 접두어를 제외한 부분, 예: "/additem ")
    FString ArgumentSuggestionBaseText;

    int32 SelectedCheatSuggestionIndex = 0;
    bool bApplyingCheatSuggestion = false;

    bool AreCheatSuggestionsVisible() const;
    void RefreshCheatSuggestions(const FString& InputText);
    void RefreshArgumentSuggestions(UCPP_CheatCommandSubsystem* CheatSubsystem, const FString& InputText, const FString& AfterSlash);
    void RebuildCheatSuggestionRows();
    void UpdateCheatSuggestionHighlight();
    void HideCheatSuggestions();
    void ApplySelectedCheatSuggestion();

    bool PassesActiveTabFilter(const FMessengerMessage& Message) const;
    void RebuildMessageList();
    void AppendMessageBlock(const FMessengerMessage& Message);
    void TrimHistory();
    void EnforceVisibleCap();
    void UpdateSendChannelLabel();
    void ApplyChatBackground();
    void ApplyChatFontSizeToVisibleMessages();
    int32 GetCurrentChatFontSize() const;
    void CacheTabBrushes();
    void UpdateTabAppearance();
    void UpdateTabZOrder();
};
