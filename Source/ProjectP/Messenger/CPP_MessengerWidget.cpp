// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_MessengerWidget.h"
#include "CPP_MessengerMessageBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/MenuAnchor.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "../GameInstance/MyGameInstance.h"
#include "../MyPlayerController.h"

namespace
{
    constexpr int32 InactiveTabZOrder = 0;
    constexpr int32 ActiveTabZOrder = 1;
    constexpr int32 HoveredTabZOrder = 2;
}

void UCPP_MessengerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 버튼/입력 상호작용을 C++에서 바인딩한다. (WBP 이벤트 그래프 작업 불필요)
    if (BTN_TabAll)
    {
        BTN_TabAll->OnClicked.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabAllClicked);
        BTN_TabAll->OnHovered.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabAllHovered);
        BTN_TabAll->OnUnhovered.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabAllUnhovered);
    }
    if (BTN_TabParty)
    {
        BTN_TabParty->OnClicked.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabPartyClicked);
        BTN_TabParty->OnHovered.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabPartyHovered);
        BTN_TabParty->OnUnhovered.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleTabPartyUnhovered);
    }
    if (BTN_Send)
    {
        BTN_Send->OnClicked.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleSendClicked);
    }
    if (BTN_SendChannel)
    {
        BTN_SendChannel->OnClicked.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleSendChannelClicked);
    }
    if (BTN_ChatSetting)
    {
        BTN_ChatSetting->OnClicked.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleChatSettingClicked);
    }
    if (ETB_Input)
    {
        ETB_Input->OnTextCommitted.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleInputCommitted);
        ETB_Input->OnTextChanged.AddUniqueDynamic(this, &UCPP_MessengerWidget::HandleInputTextChanged);

        // Enter 전송 후에도 포커스를 유지해 연속으로 입력할 수 있게 한다. (빈 입력 Enter로만 비활성화)
        ETB_Input->SetClearKeyboardFocusOnCommit(false);
    }
    if (SB_MessageList)
    {
        // 메시지 목록의 휠은 경계에서도 소비하고, 우클릭은 카메라 Orbit으로 전달한다.
        SB_MessageList->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
        SB_MessageList->SetAllowRightClickDragScrolling(false);
    }

    // 초기 상태 설정
    ActiveTab = EMessengerChannel::All;
    SendChannel = EMessengerChannel::All;
    HoveredTabButton = nullptr;

    if (const UMyGameInstance* MyGameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        bChatBackgroundEnabled = MyGameInstance->IsChatBackgroundEnabled();
        ChatFontSizeLevel = MyGameInstance->GetChatFontSizeLevel();
    }

    ApplyChatBackground();
    CacheTabBrushes();
    UpdateTabAppearance();
    UpdateTabZOrder();
    UpdateSendChannelLabel();
    RebuildMessageList();
    HideCheatSuggestions();

    // 소유 PlayerController에 자기 자신을 등록한다. (HUD 임베드/단독 무관하게 수신 경로 연결)
    if (AMyPlayerController* OwningPC = Cast<AMyPlayerController>(GetOwningPlayer()))
    {
        OwningPC->SetMessengerWidget(this);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 수신한 메시지를 히스토리에 저장하고, 현재 탭 필터를 통과하면 화면에 추가한다.
// Message : 수신한 메시지 데이터
void UCPP_MessengerWidget::ReceiveMessengerMessage(const FMessengerMessage& Message)
{
    MessageHistory.Add(Message);
    TrimHistory();

    if (PassesActiveTabFilter(Message))
    {
        AppendMessageBlock(Message);
        EnforceVisibleCap();

        if (bAutoScrollToEnd && SB_MessageList)
        {
            SB_MessageList->ScrollToEnd();
        }
    }
}

////////////////////////////
//! \author 장효제
//! \brief 메신저 히스토리를 변경하지 않고 UI 검증용 빈 메시지 블록을 추가한다.
//! \param Count 추가할 빈 메시지 블록 수
void UCPP_MessengerWidget::AddEmptyTestMessages(int32 Count)
{
    if (!SB_MessageList || !MessageBlockClass)
    {
        return;
    }

    const int32 SafeCount = FMath::Clamp(Count, 0, MaxVisibleMessages);
    for (int32 Index = 0; Index < SafeCount; ++Index)
    {
        UCPP_MessengerMessageBlock* Block =
            CreateWidget<UCPP_MessengerMessageBlock>(GetOwningPlayer(), MessageBlockClass);
        if (!Block)
        {
            break;
        }

        Block->SetEmptyTestMessage();
        Block->SetMessageFontSize(GetCurrentChatFontSize());
        SB_MessageList->AddChild(Block);
    }

    EnforceVisibleCap();
    if (bAutoScrollToEnd)
    {
        SB_MessageList->ScrollToEnd();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 보고 있는 탭을 변경하고 목록을 다시 그린다.
// Tab : 변경할 탭(전체/파티)
void UCPP_MessengerWidget::SetActiveTab(EMessengerChannel Tab)
{
    if (ActiveTab == Tab)
    {
        return;
    }

    ActiveTab = Tab;
    UpdateTabAppearance();
    UpdateTabZOrder();
    RebuildMessageList();
    OnActiveTabChanged(Tab);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전송 채널을 설정하고 라벨/BP 이벤트를 갱신한다.
// Channel : 설정할 전송 채널(전체/파티)
void UCPP_MessengerWidget::SetSendChannel(EMessengerChannel Channel)
{
    SendChannel = Channel;
    UpdateSendChannelLabel();
    OnSendChannelChanged(Channel);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입력창의 텍스트를 현재 전송 채널로 전송하고 입력창을 비운다.
void UCPP_MessengerWidget::SubmitInput()
{
    if (!ETB_Input)
    {
        return;
    }

    const FString Text = ETB_Input->GetText().ToString().TrimStartAndEnd();
    ETB_Input->SetText(FText::GetEmpty());

    if (Text.IsEmpty())
    {
        return;
    }

    if (AMyPlayerController* OwningPC = Cast<AMyPlayerController>(GetOwningPlayer()))
    {
        OwningPC->SendMessengerMessage(SendChannel, Text);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅 입력창에 키보드 포커스를 줘 채팅창을 활성화한다.
// 채팅창 비활성 상태에서 Enter를 눌렀을 때 PC(HandleChatFocusKeyPressed)가 호출한다.
void UCPP_MessengerWidget::FocusChatInput()
{
    if (ETB_Input && !ETB_Input->HasKeyboardFocus())
    {
        ETB_Input->SetKeyboardFocus();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅창 배경 표시 여부를 변경하고 현재 실행의 GameInstance에 보관하는 함수
// bEnabled : 채팅창 배경 표시 여부
void UCPP_MessengerWidget::SetChatBackgroundEnabled(bool bEnabled)
{
    bChatBackgroundEnabled = bEnabled;

    if (UMyGameInstance* MyGameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        MyGameInstance->SetChatBackgroundEnabled(bEnabled);
    }

    ApplyChatBackground();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 채팅창 배경 표시 설정을 반환하는 함수
// Return Value : 채팅창 배경 표시 여부
bool UCPP_MessengerWidget::IsChatBackgroundEnabled() const
{
    return bChatBackgroundEnabled;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅 메시지 폰트 크기 단계를 변경하고 현재 실행의 GameInstance에 보관하는 함수
// InLevel : 폰트 크기 단계(0=작음, 1=중간, 2=큼)
void UCPP_MessengerWidget::SetChatFontSizeLevel(int32 InLevel)
{
    ChatFontSizeLevel = FMath::Clamp(InLevel, 0, 2);

    if (UMyGameInstance* MyGameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        MyGameInstance->SetChatFontSizeLevel(ChatFontSizeLevel);
    }

    ApplyChatFontSizeToVisibleMessages();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 채팅 메시지 폰트 크기 단계를 반환하는 함수
// Return Value : 폰트 크기 단계(0=작음, 1=중간, 2=큼)
int32 UCPP_MessengerWidget::GetChatFontSizeLevel() const
{
    return ChatFontSizeLevel;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 열려 있는 채팅 설정 Menu Anchor를 닫는 함수
void UCPP_MessengerWidget::CloseChatSettings()
{
    if (MA_ChatSettings && MA_ChatSettings->IsOpen())
    {
        MA_ChatSettings->Close();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전체 탭 버튼 클릭 처리.
void UCPP_MessengerWidget::HandleTabAllClicked()
{
    SetActiveTab(EMessengerChannel::All);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 탭 버튼 클릭 처리.
void UCPP_MessengerWidget::HandleTabPartyClicked()
{
    SetActiveTab(EMessengerChannel::Party);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전체 탭에 마우스가 올라오면 해당 탭을 가장 앞으로 이동한다.
void UCPP_MessengerWidget::HandleTabAllHovered()
{
    HoveredTabButton = BTN_TabAll;
    UpdateTabAppearance();
    UpdateTabZOrder();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전체 탭에서 마우스가 벗어나면 활성 탭 기준의 표시 순서로 복구한다.
void UCPP_MessengerWidget::HandleTabAllUnhovered()
{
    if (HoveredTabButton == BTN_TabAll)
    {
        HoveredTabButton = nullptr;
    }
    UpdateTabAppearance();
    UpdateTabZOrder();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 탭에 마우스가 올라오면 해당 탭을 가장 앞으로 이동한다.
void UCPP_MessengerWidget::HandleTabPartyHovered()
{
    HoveredTabButton = BTN_TabParty;
    UpdateTabAppearance();
    UpdateTabZOrder();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 탭에서 마우스가 벗어나면 활성 탭 기준의 표시 순서로 복구한다.
void UCPP_MessengerWidget::HandleTabPartyUnhovered()
{
    if (HoveredTabButton == BTN_TabParty)
    {
        HoveredTabButton = nullptr;
    }
    UpdateTabAppearance();
    UpdateTabZOrder();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전송 버튼 클릭 처리.
void UCPP_MessengerWidget::HandleSendClicked()
{
    SubmitInput();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전송 채널 토글 버튼 클릭 처리. 전체<->파티를 번갈아 설정한다.
void UCPP_MessengerWidget::HandleSendChannelClicked()
{
    SetSendChannel(SendChannel == EMessengerChannel::All ? EMessengerChannel::Party : EMessengerChannel::All);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 톱니바퀴 버튼 클릭 시 채팅 설정 Menu Anchor를 토글하는 함수
void UCPP_MessengerWidget::HandleChatSettingClicked()
{
    if (!MA_ChatSettings)
    {
        return;
    }

    if (MA_ChatSettings->IsOpen())
    {
        MA_ChatSettings->Close();
        return;
    }

    if (MA_ChatSettings->ShouldOpenDueToClick())
    {
        MA_ChatSettings->Open(true);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입력창에서 Enter로 확정했을 때의 동작을 처리한다.
// 메시지가 있으면 전송 후 포커스를 유지하고, 비어 있거나 공백뿐이면 채팅창을 비활성화한다.
// Text : 확정된 텍스트
// CommitMethod : 확정 방식(Enter/포커스 해제 등)
void UCPP_MessengerWidget::HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod != ETextCommit::OnEnter)
    {
        return;
    }

    const FString Trimmed = Text.ToString().TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        // 빈 입력 Enter : 입력창을 비우고 포커스를 게임 뷰포트로 돌려 채팅창을 비활성화한다.
        if (ETB_Input)
        {
            ETB_Input->SetText(FText::GetEmpty());
        }
        HideCheatSuggestions();
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
        return;
    }

    SubmitInput();

    // 전송 후에도 포커스를 유지해 바로 다음 메시지를 입력할 수 있게 한다.
    if (ETB_Input)
    {
        ETB_Input->SetKeyboardFocus();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입력창 텍스트가 바뀔 때마다 치트 자동완성 목록을 갱신한다.
// '/'로 시작하는 동안에만 목록을 표시한다.
// Text : 현재 입력 텍스트
void UCPP_MessengerWidget::HandleInputTextChanged(const FText& Text)
{
    // 자동완성 적용으로 인한 SetText 재진입이면 무시한다.
    if (bApplyingCheatSuggestion)
    {
        return;
    }

    const FString InputText = Text.ToString();
    if (!InputText.StartsWith(TEXT("/")))
    {
        HideCheatSuggestions();
        return;
    }

    RefreshCheatSuggestions(InputText);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 자동완성 목록이 표시 중일 때 위/아래/Enter/Esc 키를 가로채 처리한다.
// 목록이 없으면 기본 동작(입력창 커밋 등)으로 넘긴다.
// InGeometry : 위젯 지오메트리
// InKeyEvent : 키 이벤트
// 반환값 : 처리했으면 Handled
FReply UCPP_MessengerWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!InKeyEvent.IsRepeat() && InKeyEvent.GetKey() == EKeys::Escape && MA_ChatSettings && MA_ChatSettings->IsOpen())
    {
        CloseChatSettings();
        return FReply::Handled();
    }

    // 채팅창 활성(입력창 포커스) 중 Tab : 전송 채널을 전체<->파티로 토글한다. (기본 포커스 이동 방지)
    if (InKeyEvent.GetKey() == EKeys::Tab && ETB_Input && ETB_Input->HasKeyboardFocus())
    {
        SetSendChannel(SendChannel == EMessengerChannel::All ? EMessengerChannel::Party : EMessengerChannel::All);
        return FReply::Handled();
    }

    if (AreCheatSuggestionsVisible())
    {
        const FKey Key = InKeyEvent.GetKey();
        const int32 SuggestionCount = CurrentArgumentSuggestions.IsEmpty()
            ? CurrentCheatSuggestions.Num()
            : CurrentArgumentSuggestions.Num();

        if (Key == EKeys::Up)
        {
            SelectedCheatSuggestionIndex = (SelectedCheatSuggestionIndex - 1 + SuggestionCount) % SuggestionCount;
            UpdateCheatSuggestionHighlight();
            return FReply::Handled();
        }

        if (Key == EKeys::Down)
        {
            SelectedCheatSuggestionIndex = (SelectedCheatSuggestionIndex + 1) % SuggestionCount;
            UpdateCheatSuggestionHighlight();
            return FReply::Handled();
        }

        if (Key == EKeys::Enter)
        {
            ApplySelectedCheatSuggestion();
            return FReply::Handled();
        }

        if (Key == EKeys::Escape)
        {
            HideCheatSuggestions();
            return FReply::Handled();
        }
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 자동완성 목록이 화면에 표시 중인지 확인한다.
// 반환값 : 표시 중이면 true
bool UCPP_MessengerWidget::AreCheatSuggestionsVisible() const
{
    return VB_CheatSuggestions &&
        VB_CheatSuggestions->GetVisibility() == ESlateVisibility::Visible &&
        (!CurrentCheatSuggestions.IsEmpty() || !CurrentArgumentSuggestions.IsEmpty());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 입력 텍스트 기준으로 치트 자동완성 목록을 다시 만든다.
// 명령 이름 입력 중('/'뒤 공백 전)에는 명령 후보를, 공백 이후에는 명령별 인자 후보(아이템 ID 등)를 표시한다.
// InputText : '/'로 시작하는 현재 입력 텍스트
void UCPP_MessengerWidget::RefreshCheatSuggestions(const FString& InputText)
{
    if (!VB_CheatSuggestions)
    {
        return;
    }

    UCPP_CheatCommandSubsystem* CheatSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UCPP_CheatCommandSubsystem>() : nullptr;
    if (!CheatSubsystem)
    {
        HideCheatSuggestions();
        return;
    }

    // 공백이 나오면 명령 이름 입력이 끝난 것이므로 인자 후보 단계로 넘어간다.
    const FString AfterSlash = InputText.Mid(1);
    if (AfterSlash.Contains(TEXT(" ")))
    {
        RefreshArgumentSuggestions(CheatSubsystem, InputText, AfterSlash);
        return;
    }

    TArray<FCheatCommandInfo> MatchingCommands;
    CheatSubsystem->GetMatchingCommands(AfterSlash, MatchingCommands);
    if (MatchingCommands.IsEmpty())
    {
        HideCheatSuggestions();
        return;
    }

    if (MatchingCommands.Num() > MaxCheatSuggestions)
    {
        MatchingCommands.SetNum(MaxCheatSuggestions);
    }

    CurrentArgumentSuggestions.Reset();
    CurrentCheatSuggestions = MoveTemp(MatchingCommands);
    SelectedCheatSuggestionIndex = FMath::Clamp(SelectedCheatSuggestionIndex, 0, CurrentCheatSuggestions.Num() - 1);
    RebuildCheatSuggestionRows();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인자 입력 단계의 자동완성 목록을 만든다. (예: "/additem Po" -> 아이템 ID 후보)
// 입력 중인 마지막 토큰을 접두어로 보고 명령별 제공자에게 후보를 요청한다.
// CheatSubsystem : 치트 레지스트리 서브시스템
// InputText : '/' 포함 전체 입력 텍스트
// AfterSlash : '/' 제외 입력 텍스트 (공백 포함)
void UCPP_MessengerWidget::RefreshArgumentSuggestions(UCPP_CheatCommandSubsystem* CheatSubsystem, const FString& InputText, const FString& AfterSlash)
{
    int32 FirstSpaceIndex = INDEX_NONE;
    AfterSlash.FindChar(TEXT(' '), FirstSpaceIndex);
    const FString CommandName = AfterSlash.Left(FirstSpaceIndex);
    const FString ArgsText = AfterSlash.Mid(FirstSpaceIndex + 1);

    // 마지막 토큰이 입력 중인 인자. 공백으로 끝나면 새 인자를 시작한 것이다.
    TArray<FString> Tokens;
    ArgsText.ParseIntoArray(Tokens, TEXT(" "), true);
    const bool bStartingNewArg = ArgsText.IsEmpty() || ArgsText.EndsWith(TEXT(" "));
    const int32 ArgIndex = bStartingNewArg ? Tokens.Num() : Tokens.Num() - 1;
    const FString Prefix = bStartingNewArg ? FString() : Tokens.Last();

    TArray<FString> Suggestions;
    CheatSubsystem->GetArgumentSuggestions(CommandName, ArgIndex, Prefix, Suggestions);
    if (Suggestions.IsEmpty())
    {
        HideCheatSuggestions();
        return;
    }

    if (Suggestions.Num() > MaxCheatSuggestions)
    {
        Suggestions.SetNum(MaxCheatSuggestions);
    }

    CurrentCheatSuggestions.Reset();
    CurrentArgumentSuggestions = MoveTemp(Suggestions);
    ArgumentSuggestionBaseText = InputText.Left(InputText.Len() - Prefix.Len());
    SelectedCheatSuggestionIndex = FMath::Clamp(SelectedCheatSuggestionIndex, 0, CurrentArgumentSuggestions.Num() - 1);
    RebuildCheatSuggestionRows();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 자동완성 목록으로 텍스트 행 위젯들을 다시 만들어 컨테이너에 채운다.
void UCPP_MessengerWidget::RebuildCheatSuggestionRows()
{
    if (!VB_CheatSuggestions)
    {
        return;
    }

    VB_CheatSuggestions->ClearChildren();

    // 인자 모드면 인자 후보(아이템 ID 등)를, 아니면 명령 후보를 표시한다.
    if (!CurrentArgumentSuggestions.IsEmpty())
    {
        for (const FString& Suggestion : CurrentArgumentSuggestions)
        {
            UTextBlock* Row = NewObject<UTextBlock>(this);
            if (!Row)
            {
                continue;
            }

            Row->SetText(FText::FromString(Suggestion));
            FSlateFontInfo RowFont = Row->GetFont();
            RowFont.Size = CheatSuggestionFontSize;
            Row->SetFont(RowFont);
            Row->SetAutoWrapText(true);
            Row->SetWrapTextAt(459.0f);
            VB_CheatSuggestions->AddChildToVerticalBox(Row);
        }
    }
    else
    {
        for (const FCheatCommandInfo& Info : CurrentCheatSuggestions)
        {
            UTextBlock* Row = NewObject<UTextBlock>(this);
            if (!Row)
            {
                continue;
            }

            Row->SetText(FText::FromString(FString::Printf(TEXT("%s  -  %s"), *Info.Usage, *Info.Description)));
            FSlateFontInfo RowFont = Row->GetFont();
            RowFont.Size = CheatSuggestionFontSize;
            Row->SetFont(RowFont);
            Row->SetAutoWrapText(true);
            Row->SetWrapTextAt(459.0f);
            VB_CheatSuggestions->AddChildToVerticalBox(Row);
        }
    }

    UpdateCheatSuggestionHighlight();
    VB_CheatSuggestions->SetVisibility(ESlateVisibility::Visible);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 선택 인덱스에 맞게 자동완성 행 색상을 갱신한다.
void UCPP_MessengerWidget::UpdateCheatSuggestionHighlight()
{
    if (!VB_CheatSuggestions)
    {
        return;
    }

    const int32 ChildCount = VB_CheatSuggestions->GetChildrenCount();
    for (int32 i = 0; i < ChildCount; ++i)
    {
        if (UTextBlock* Row = Cast<UTextBlock>(VB_CheatSuggestions->GetChildAt(i)))
        {
            const bool bSelected = (i == SelectedCheatSuggestionIndex);
            Row->SetColorAndOpacity(FSlateColor(bSelected ? CheatSuggestionSelectedColor : CheatSuggestionNormalColor));
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 자동완성 목록을 숨기고 상태를 초기화한다.
void UCPP_MessengerWidget::HideCheatSuggestions()
{
    CurrentCheatSuggestions.Reset();
    CurrentArgumentSuggestions.Reset();
    ArgumentSuggestionBaseText.Reset();
    SelectedCheatSuggestionIndex = 0;

    if (VB_CheatSuggestions)
    {
        VB_CheatSuggestions->ClearChildren();
        VB_CheatSuggestions->SetVisibility(ESlateVisibility::Collapsed);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택된 자동완성 항목(명령 또는 인자)을 입력창에 채운다.
// 채운 뒤 입력창 포커스를 유지하고, 다음 단계의 후보 목록(예: 명령 선택 직후 인자 후보)을 바로 갱신한다.
void UCPP_MessengerWidget::ApplySelectedCheatSuggestion()
{
    if (!ETB_Input)
    {
        HideCheatSuggestions();
        return;
    }

    FString CompletedText;

    // 인자 모드: 입력 중이던 접두어를 선택한 후보로 교체한다.
    if (!CurrentArgumentSuggestions.IsEmpty())
    {
        if (!CurrentArgumentSuggestions.IsValidIndex(SelectedCheatSuggestionIndex))
        {
            HideCheatSuggestions();
            return;
        }

        CompletedText = ArgumentSuggestionBaseText + CurrentArgumentSuggestions[SelectedCheatSuggestionIndex] + TEXT(" ");
    }
    else
    {
        if (!CurrentCheatSuggestions.IsValidIndex(SelectedCheatSuggestionIndex))
        {
            HideCheatSuggestions();
            return;
        }

        CompletedText = FString::Printf(TEXT("/%s "), *CurrentCheatSuggestions[SelectedCheatSuggestionIndex].Name);
    }

    bApplyingCheatSuggestion = true;
    ETB_Input->SetText(FText::FromString(CompletedText));
    bApplyingCheatSuggestion = false;

    HideCheatSuggestions();

    // 명령을 채웠으면 곧바로 인자 후보를 띄운다. (예: "/additem " -> 아이템 ID 목록)
    RefreshCheatSuggestions(CompletedText);

    ETB_Input->SetKeyboardFocus();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 탭 필터 통과 여부를 판정한다. 전체 탭은 모든 메시지, 파티 탭은 파티 채널만.
// Message : 판정할 메시지
// 반환값 : 현재 탭에 표시 대상이면 true
bool UCPP_MessengerWidget::PassesActiveTabFilter(const FMessengerMessage& Message) const
{
    if (ActiveTab == EMessengerChannel::All)
    {
        return true;
    }

    return Message.Channel == EMessengerChannel::Party;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 탭 기준으로 목록을 비우고 히스토리에서 다시 그린다. (최근 MaxVisibleMessages개만)
void UCPP_MessengerWidget::RebuildMessageList()
{
    if (!SB_MessageList)
    {
        return;
    }

    SB_MessageList->ClearChildren();

    // 현재 탭에 표시할 메시지 인덱스만 모은다.
    TArray<int32> VisibleIndices;
    for (int32 i = 0; i < MessageHistory.Num(); ++i)
    {
        if (PassesActiveTabFilter(MessageHistory[i]))
        {
            VisibleIndices.Add(i);
        }
    }

    // 최근 MaxVisibleMessages개만 표시
    const int32 StartIndex = FMath::Max(0, VisibleIndices.Num() - MaxVisibleMessages);
    for (int32 i = StartIndex; i < VisibleIndices.Num(); ++i)
    {
        AppendMessageBlock(MessageHistory[VisibleIndices[i]]);
    }

    if (bAutoScrollToEnd)
    {
        SB_MessageList->ScrollToEnd();
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 메시지 한 줄 블록을 생성해 스크롤 박스에 추가한다.
// Message : 표시할 메시지
void UCPP_MessengerWidget::AppendMessageBlock(const FMessengerMessage& Message)
{
    if (!SB_MessageList || !MessageBlockClass)
    {
        return;
    }

    UCPP_MessengerMessageBlock* Block = CreateWidget<UCPP_MessengerMessageBlock>(GetOwningPlayer(), MessageBlockClass);
    if (!Block)
    {
        return;
    }

    Block->SetMessage(Message);
    Block->SetMessageFontSize(GetCurrentChatFontSize());
    SB_MessageList->AddChild(Block);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 히스토리가 최대 보관 수를 넘으면 오래된 것부터 제거한다.
void UCPP_MessengerWidget::TrimHistory()
{
    while (MessageHistory.Num() > MaxMessageHistory)
    {
        MessageHistory.RemoveAt(0);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 화면 블록 수가 최대 표시 수를 넘으면 위(오래된)에서부터 제거한다.
void UCPP_MessengerWidget::EnforceVisibleCap()
{
    if (!SB_MessageList)
    {
        return;
    }

    while (SB_MessageList->GetChildrenCount() > MaxVisibleMessages)
    {
        UWidget* Oldest = SB_MessageList->GetChildAt(0);
        if (!Oldest)
        {
            break;
        }
        SB_MessageList->RemoveChild(Oldest);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전송 채널 라벨(TXT_SendChannel)을 현재 채널 텍스트로 갱신한다.
void UCPP_MessengerWidget::UpdateSendChannelLabel()
{
    if (!TXT_SendChannel)
    {
        return;
    }

    const FText Label = (SendChannel == EMessengerChannel::Party)
        ? FText::FromString(TEXT("파티"))
        : FText::FromString(TEXT("전체"));
    TXT_SendChannel->SetText(Label);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 배경 설정에 맞는 채팅창 배경 텍스처를 적용하는 함수
void UCPP_MessengerWidget::ApplyChatBackground()
{
    if (!BRD_MessageBackground)
    {
        return;
    }

    UTexture2D* BackgroundTexture = bChatBackgroundEnabled
        ? ChatBackgroundEnabledTexture
        : ChatBackgroundDisabledTexture;
    if (BackgroundTexture)
    {
        BRD_MessageBackground->SetBrushFromTexture(BackgroundTexture);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 화면에 표시된 모든 메시지 블록에 선택한 폰트 크기를 적용하는 함수
void UCPP_MessengerWidget::ApplyChatFontSizeToVisibleMessages()
{
    if (!SB_MessageList)
    {
        return;
    }

    const int32 FontSize = GetCurrentChatFontSize();
    const int32 ChildCount = SB_MessageList->GetChildrenCount();
    for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
    {
        if (UCPP_MessengerMessageBlock* MessageBlock = Cast<UCPP_MessengerMessageBlock>(SB_MessageList->GetChildAt(ChildIndex)))
        {
            MessageBlock->SetMessageFontSize(FontSize);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 폰트 크기 단계에 대응하는 실제 폰트 크기를 반환하는 함수
// Return Value : 적용할 폰트 크기
int32 UCPP_MessengerWidget::GetCurrentChatFontSize() const
{
    switch (ChatFontSizeLevel)
    {
    case 0:
        return SmallChatFontSize;
    case 2:
        return LargeChatFontSize;
    default:
        return MediumChatFontSize;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 위젯 블루프린트에 설정된 비활성/활성 탭 브러시를 최초 한 번 저장한다.
void UCPP_MessengerWidget::CacheTabBrushes()
{
    if (bTabBrushesCached || !BTN_TabAll || !BTN_TabParty)
    {
        return;
    }

    TabAllInactiveBrush = BTN_TabAll->GetStyle().Normal;
    TabAllActiveBrush = BTN_TabAll->GetStyle().Hovered;
    TabPartyInactiveBrush = BTN_TabParty->GetStyle().Normal;
    TabPartyActiveBrush = BTN_TabParty->GetStyle().Hovered;
    bTabBrushesCached = true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// Hover 중이면 해당 탭만, Hover 중이 아니면 활성 탭만 선택 이미지로 표시한다.
void UCPP_MessengerWidget::UpdateTabAppearance()
{
    if (!bTabBrushesCached)
    {
        return;
    }

    UButton* DisplayedActiveButton = HoveredTabButton;
    if (!DisplayedActiveButton)
    {
        DisplayedActiveButton = ActiveTab == EMessengerChannel::All ? BTN_TabAll : BTN_TabParty;
    }

    FButtonStyle AllStyle = BTN_TabAll->GetStyle();
    AllStyle.SetNormal(DisplayedActiveButton == BTN_TabAll ? TabAllActiveBrush : TabAllInactiveBrush);
    BTN_TabAll->SetStyle(AllStyle);

    FButtonStyle PartyStyle = BTN_TabParty->GetStyle();
    PartyStyle.SetNormal(DisplayedActiveButton == BTN_TabParty ? TabPartyActiveBrush : TabPartyInactiveBrush);
    BTN_TabParty->SetStyle(PartyStyle);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 비활성 탭은 0, 활성 탭은 1, 마우스가 올라온 탭은 2로 Canvas Panel ZOrder를 갱신한다.
void UCPP_MessengerWidget::UpdateTabZOrder()
{
    const int32 AllZOrder = HoveredTabButton == BTN_TabAll
        ? HoveredTabZOrder
        : (ActiveTab == EMessengerChannel::All ? ActiveTabZOrder : InactiveTabZOrder);
    const int32 PartyZOrder = HoveredTabButton == BTN_TabParty
        ? HoveredTabZOrder
        : (ActiveTab == EMessengerChannel::Party ? ActiveTabZOrder : InactiveTabZOrder);

    if (BTN_TabAll)
    {
        if (UCanvasPanelSlot* AllSlot = Cast<UCanvasPanelSlot>(BTN_TabAll->Slot))
        {
            AllSlot->SetZOrder(AllZOrder);
        }
    }

    if (BTN_TabParty)
    {
        if (UCanvasPanelSlot* PartySlot = Cast<UCanvasPanelSlot>(BTN_TabParty->Slot))
        {
            PartySlot->SetZOrder(PartyZOrder);
        }
    }
}
