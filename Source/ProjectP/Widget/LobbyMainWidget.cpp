#include "LobbyMainWidget.h"

#include "../CPP_LogoutRequestAsyncAction.h"
#include "../GAS/MyPlayerState.h"
#include "../GameInstance/SubSystems/NetSub/AccountSessionSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/SessionMaintenanceSubsystem.h"
#include "../Lobby/CPP_LobbyPC.h"
#include "../MyGameplayTags.h"
#include "Components/Button.h"
#include "CPP_EntryCharacterSelectWidget.h"
#include "CPP_LobbyPartyPanel.h"
#include "CPP_PartyInvitePopup.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MyPrimaryGameLayout.h"
#include "MyUIManagerSubsystem.h"
#include "TimerManager.h"

namespace
{
    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // EntryCharacterSelectWidgetClass가 비어 있을 때 사용할 기본 입장 캐릭터 선택 위젯 경로
    const TCHAR* DefaultEntryCharacterSelectWidgetClassPath = TEXT("/Game/LeDuat/Widget/Lobby/WBP_EntryCharacterSelect.WBP_EntryCharacterSelect_C");

    //////////////////////////////////////////////////////////////////////
    // - Codex -
    // LobbyPartyPanelClass가 비어 있을 때 사용할 기본 파티 패널 위젯 경로
    const TCHAR* DefaultLobbyPartyPanelClassPath = TEXT("/Game/LeDuat/Widget/Lobby/WBP_LobbyPartyPanel.WBP_LobbyPartyPanel_C");

    //////////////////////////////////////////////////////////////////////
    // - Codex -
    // PartyInvitePopupClass가 비어 있을 때 사용할 기본 파티 초대 팝업 위젯 경로
    const TCHAR* DefaultPartyInvitePopupClassPath = TEXT("/Game/LeDuat/Widget/Lobby/WBP_PartyInvitePopup.WBP_PartyInvitePopup_C");
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로비 메인 위젯이 활성화됐을 때 게임 입력과 UI 입력을 함께 사용하도록 기본 입력 정책을 설정하는 생성자
ULobbyMainWidget::ULobbyMainWidget()
{
    InputMode = EMyWidgetInputMode::GameAndMenu;
    MouseCaptureMode = EMouseCaptureMode::CaptureDuringRightMouseDown;
}

void ULobbyMainWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer()))
    {
        LobbyPC->RegisterLobbyMainWidget(this);
    }

    if (BTN_Exit)
    {
        BTN_Exit->OnClicked.RemoveDynamic(this, &ULobbyMainWidget::HandleExitClicked);
        BTN_Exit->OnClicked.AddDynamic(this, &ULobbyMainWidget::HandleExitClicked);
    }

    if (BTN_OpenPartyPanel)
    {
        BTN_OpenPartyPanel->OnClicked.RemoveDynamic(this, &ULobbyMainWidget::HandleOpenPartyPanelClicked);
        BTN_OpenPartyPanel->OnClicked.AddDynamic(this, &ULobbyMainWidget::HandleOpenPartyPanelClicked);
    }

    StartEntryCharacterSelectFlow();
}

void ULobbyMainWidget::NativeDestruct()
{
    if (ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(GetOwningPlayer()))
    {
        LobbyPC->UnregisterLobbyMainWidget(this);
    }

    if (BTN_Exit)
    {
        BTN_Exit->OnClicked.RemoveDynamic(this, &ULobbyMainWidget::HandleExitClicked);
    }

    if (BTN_OpenPartyPanel)
    {
        BTN_OpenPartyPanel->OnClicked.RemoveDynamic(this, &ULobbyMainWidget::HandleOpenPartyPanelClicked);
    }

    if (UCPP_LobbyPartyPanel* PartyPanel = ActivePartyPanel.Get())
    {
        PartyPanel->DeactivateWidget();
    }
    ActivePartyPanel.Reset();

    StopEntryCharacterSelectRetry();

    if (EntryCharacterSelectWidget)
    {
        EntryCharacterSelectWidget->DeactivateWidget();
        EntryCharacterSelectWidget = nullptr;
    }

    FinishLogoutRequest();

    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 UI에서 로그인 토큰을 사용해 로그아웃 요청을 시작하는 함수
void ULobbyMainWidget::RequestLogoutFromLobby()
{
    if (ActiveLogoutRequest)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UAccountSessionSubsystem>()
        : nullptr;
    const FString LoginToken = AccountSessionSubsystem
        ? AccountSessionSubsystem->GetLoginToken()
        : TEXT("");

    if (LoginToken.IsEmpty())
    {
        FinishLogoutAndOpenLoginMap();
        return;
    }

    ActiveLogoutRequest = UCPP_LogoutRequestAsyncAction::RequestLogout(this, LoginToken, LogoutRequestServerUrl);
    if (ActiveLogoutRequest == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create logout request."));
        FinishLogoutAndOpenLoginMap();
        return;
    }

    ActiveLogoutRequest->OnSuccess.AddDynamic(this, &ULobbyMainWidget::HandleLogoutRequestSuccess);
    ActiveLogoutRequest->OnFailure.AddDynamic(this, &ULobbyMainWidget::HandleLogoutRequestFailure);

    SetExitButtonEnabled(false);
    ActiveLogoutRequest->Activate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 나가기 버튼 클릭 시 로그아웃 요청을 시작하는 함수
void ULobbyMainWidget::HandleExitClicked()
{
    RequestLogoutFromLobby();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로비 메인 UI의 파티 버튼 클릭을 파티 패널 토글 처리로 전달하는 함수
void ULobbyMainWidget::HandleOpenPartyPanelClicked()
{
    TogglePartyPanel();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티 패널이 열려 있으면 닫고, 닫혀 있으면 Menu 레이어 스택에 추가하는 함수
void ULobbyMainWidget::TogglePartyPanel()
{
    if (UCPP_LobbyPartyPanel* PartyPanel = ActivePartyPanel.Get())
    {
        if (PartyPanel->IsActivated())
        {
            PartyPanel->DeactivateWidget();
            ActivePartyPanel.Reset();
            return;
        }

        ActivePartyPanel.Reset();
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot open lobby party panel because MyUIManagerSubsystem is missing."));
        return;
    }

    UClass* PanelClass = LobbyPartyPanelClass.Get();
    if (!PanelClass)
    {
        PanelClass = LoadClass<UCPP_LobbyPartyPanel>(nullptr, DefaultLobbyPartyPanelClassPath);
    }

    if (!PanelClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot open lobby party panel because its widget class is missing."));
        return;
    }

    UCPP_LobbyPartyPanel* PartyPanel = Cast<UCPP_LobbyPartyPanel>(UIManager->PushMenu(PanelClass));
    if (!PartyPanel)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot open lobby party panel because pushing it to UI.Layer.Menu failed."));
        return;
    }

    ActivePartyPanel = PartyPanel;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 받은 파티 초대 또는 가입 신청 정보를 초기화한 팝업을 Modal 레이어에 추가하는 함수
// InviteInfo : 팝업에 표시하고 수락 또는 거절 처리에 사용할 파티 요청 정보
void ULobbyMainWidget::PresentPartyInvite(const FLobbyReceivedPartyInviteInfo& InviteInfo)
{
    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    UMyPrimaryGameLayout* PrimaryLayout = UIManager ? UIManager->GetPrimaryLayout() : nullptr;
    if (!PrimaryLayout)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot present party invite because the lobby primary layout is missing."));
        return;
    }

    UClass* PopupClass = PartyInvitePopupClass.Get();
    if (!PopupClass)
    {
        PopupClass = LoadClass<UCPP_PartyInvitePopup>(nullptr, DefaultPartyInvitePopupClassPath);
    }

    if (!PopupClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot present party invite because WBP_PartyInvitePopup is not parented to UCPP_PartyInvitePopup."));
        return;
    }

    UCPP_PartyInvitePopup* Popup = PrimaryLayout->PushWidgetToLayerStack<UCPP_PartyInvitePopup>(
        MyGameplayTags::UI_Layer_Modal,
        PopupClass,
        [&InviteInfo](UCPP_PartyInvitePopup& NewPopup)
        {
            NewPopup.InitializeInviteInfo(InviteInfo);
        });

    if (!Popup)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot present party invite because pushing it to UI.Layer.Modal failed."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 성공 후 로컬 로그인 정보를 정리하고 로그인 맵으로 이동하는 함수
// Message : 로그아웃 서버가 전달한 성공 메시지
void ULobbyMainWidget::HandleLogoutRequestSuccess(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("Logout request succeeded. Message: %s"), *Message);
    FinishLogoutRequest();
    FinishLogoutAndOpenLoginMap();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 실패 후 필요 시 로컬 로그인 정보를 정리하고 로그인 맵으로 이동하는 함수
// Message : 로그아웃 요청 실패 이유 메시지
void ULobbyMainWidget::HandleLogoutRequestFailure(const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("Logout request failed. Message: %s"), *Message);
    FinishLogoutRequest();

    if (bReturnToLoginOnLogoutFailure)
    {
        FinishLogoutAndOpenLoginMap();
        return;
    }

    SetExitButtonEnabled(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 세션 ping과 로컬 로그인 정보를 정리한 뒤 로그인 맵으로 이동하는 함수
void ULobbyMainWidget::FinishLogoutAndOpenLoginMap()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (USessionMaintenanceSubsystem* SessionMaintenanceSubsystem = GameInstance->GetSubsystem<USessionMaintenanceSubsystem>())
        {
            SessionMaintenanceSubsystem->StopSessionPing();
        }

        if (UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance->GetSubsystem<UAccountSessionSubsystem>())
        {
            AccountSessionSubsystem->ClearLoginInfo();
        }
    }

    if (LoginMapName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot open login map because LoginMapName is none."));
        SetExitButtonEnabled(true);
        return;
    }

    UGameplayStatics::OpenLevel(this, LoginMapName);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 완료 후 델리게이트 연결과 요청 객체를 정리하는 함수
void ULobbyMainWidget::FinishLogoutRequest()
{
    if (ActiveLogoutRequest)
    {
        ActiveLogoutRequest->OnSuccess.RemoveDynamic(this, &ULobbyMainWidget::HandleLogoutRequestSuccess);
        ActiveLogoutRequest->OnFailure.RemoveDynamic(this, &ULobbyMainWidget::HandleLogoutRequestFailure);
        ActiveLogoutRequest = nullptr;
    }

    SetExitButtonEnabled(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 중복 실행을 막기 위해 로비 나가기 버튼 활성 상태를 변경하는 함수
// bShouldEnable : 로비 나가기 버튼 활성화 여부
void ULobbyMainWidget::SetExitButtonEnabled(bool bShouldEnable) const
{
    if (BTN_Exit)
    {
        BTN_Exit->SetIsEnabled(bShouldEnable);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 입장 캐릭터 선택 단계를 시작하는 함수
// PlayerState 복제가 끝나기 전일 수 있으므로 재시도 타이머로 표시 시점을 잡는다.
void ULobbyMainWidget::StartEntryCharacterSelectFlow()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float RetryInterval = FMath::Max(0.1f, EntryCharacterSelectRetryIntervalSeconds);
    World->GetTimerManager().SetTimer(
        EntryCharacterSelectTimerHandle,
        this,
        &ULobbyMainWidget::TryShowEntryCharacterSelect,
        RetryInterval,
        true,
        0.0f);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 입장 캐릭터 선택 표시 재시도 타이머를 정리하는 함수
void ULobbyMainWidget::StopEntryCharacterSelectRetry()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EntryCharacterSelectTimerHandle);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PlayerState가 준비되면 로비 입장 캐릭터 선택 위젯을 표시하는 함수
// 이미 캐릭터를 선택한 상태(재입장 등)이면 표시하지 않는다.
// 던전 종료 등 서버 이동으로 재진입한 경우에는 GameInstance에 보관된 직전 사용 캐릭터를
// 자동 선택 요청하고, 성공할 때까지(또는 타임아웃까지) 선택 위젯을 띄우지 않는다.
void ULobbyMainWidget::TryShowEntryCharacterSelect()
{
    APlayerController* OwningPlayer = GetOwningPlayer();
    AMyPlayerState* MyPlayerState = OwningPlayer ? OwningPlayer->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!MyPlayerState)
    {
        // PlayerState 복제 대기 중이면 타이머가 다시 시도한다.
        return;
    }

    UGameInstance* GameInstance = OwningPlayer->GetGameInstance();
    UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UAccountSessionSubsystem>() : nullptr;

    if (MyPlayerState->GetSelectedCharacterId() != -1)
    {
        // 선택 완료(자동 복원 성공 포함) 상태이므로 보관값을 정리하고 종료한다.
        if (AccountSessionSubsystem)
        {
            AccountSessionSubsystem->ClearLastUsedCharacterId();
        }

        StopEntryCharacterSelectRetry();
        return;
    }

    // 서버 이동으로 재진입한 경우: 직전 사용 캐릭터를 자동 선택 요청하고 선택 위젯은 띄우지 않는다.
    const int32 LastUsedCharacterId = AccountSessionSubsystem ? AccountSessionSubsystem->GetLastUsedCharacterId() : -1;
    if (LastUsedCharacterId != -1 && EntryAutoSelectElapsedSeconds < EntryAutoSelectTimeoutSeconds)
    {
        EntryAutoSelectElapsedSeconds += FMath::Max(0.1f, EntryCharacterSelectRetryIntervalSeconds);

        // 로비 인증이 필요한 서버에서는 인증이 끝난 뒤에 요청해야 서버가 거부하지 않는다.
        const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
        const bool bRequireLobbyAuth = !ServerConfigSubsystem || ServerConfigSubsystem->IsLobbyTokenVerificationRequired();

        ACPP_LobbyPC* LobbyPC = Cast<ACPP_LobbyPC>(OwningPlayer);
        if (LobbyPC && (!bRequireLobbyAuth || LobbyPC->IsLobbyAuthVerified()))
        {
            LobbyPC->RequestSelectLobbyCharacter(LastUsedCharacterId);
        }

        // 선택 결과(SelectedCharacterId 복제)가 도착할 때까지 타이머로 재시도한다.
        return;
    }

    if (LastUsedCharacterId != -1 && AccountSessionSubsystem)
    {
        // 자동 복원이 시간 내에 끝나지 않으면 보관값을 버리고 신규 입장 흐름으로 넘어간다.
        AccountSessionSubsystem->ClearLastUsedCharacterId();
    }

    StopEntryCharacterSelectRetry();

    UClass* WidgetClass = EntryCharacterSelectWidgetClass.Get();
    if (!WidgetClass)
    {
        WidgetClass = LoadClass<UCPP_EntryCharacterSelectWidget>(nullptr, DefaultEntryCharacterSelectWidgetClassPath);
    }

    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyMainWidget cannot show entry character select. Widget class is not set and default class load failed."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    UMyPrimaryGameLayout* PrimaryLayout = UIManager ? UIManager->GetPrimaryLayout() : nullptr;
    if (!PrimaryLayout)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyMainWidget cannot show entry character select because the lobby primary layout is missing."));
        return;
    }

    EntryCharacterSelectWidget = PrimaryLayout->PushWidgetToLayerStack<UCPP_EntryCharacterSelectWidget>(
        MyGameplayTags::UI_Layer_Modal,
        WidgetClass);
    if (!EntryCharacterSelectWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyMainWidget failed to push entry character select widget to UI.Layer.Modal."));
        return;
    }
}
