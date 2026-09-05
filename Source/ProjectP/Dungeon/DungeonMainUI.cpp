#include "DungeonMainUI.h"

#include "../CPP_LogoutRequestAsyncAction.h"
#include "../GameInstance/SubSystems/NetSub/AccountSessionSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/SessionMaintenanceSubsystem.h"
#include "../Item/MyInventoryComponent.h"
#include "../Widget/MyUIManagerSubsystem.h"
#include "DungeonPC.h"
#include "DungeonSettingsModal.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "../GAS/MyPlayerState.h"

void UDungeonMainUI::NativeConstruct()
{
    Super::NativeConstruct();

    // WBP_HUDLayout에 임베드되어 레이아웃이 이 위젯을 생성하므로, PC가 참조할 수 있게 스스로 등록한다.
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer()))
    {
        DungeonPC->RegisterDungeonMainUI(this);
    }

    if (BTN_Settings)
    {
        BTN_Settings->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleSettingsClicked);
        BTN_Settings->OnClicked.AddDynamic(this, &UDungeonMainUI::HandleSettingsClicked);
    }

    if (BTN_Inventory)
    {
        BTN_Inventory->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleInventoryClicked);
        BTN_Inventory->OnClicked.AddDynamic(this, &UDungeonMainUI::HandleInventoryClicked);
    }

    if (BTN_God)
    {
        BTN_God->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleGodClicked);
        BTN_God->OnClicked.AddDynamic(this, &UDungeonMainUI::HandleGodClicked);
    }

    SetSelectedCharacterIdText(-1);
    RefreshSelectedCharacterIdText();
    EnsureInventoryBinding();
}

void UDungeonMainUI::NativeDestruct()
{
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer()))
    {
        DungeonPC->UnregisterDungeonMainUI(this);
    }

    if (BTN_Settings)
    {
        BTN_Settings->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleSettingsClicked);
    }

    if (BTN_Inventory)
    {
        BTN_Inventory->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleInventoryClicked);
    }

    if (BTN_God)
    {
        BTN_God->OnClicked.RemoveDynamic(this, &UDungeonMainUI::HandleGodClicked);
    }

    if (UDungeonSettingsModal* SettingsModal = ActiveSettingsModal.Get())
    {
        SettingsModal->DeactivateWidget();
    }

    if (BoundInventoryComponent)
    {
        BoundInventoryComponent->OnMesoChanged.RemoveDynamic(this, &UDungeonMainUI::HandleMesoChanged);
        BoundInventoryComponent = nullptr;
    }

    FinishLogoutRequest();

    Super::NativeDestruct();
}

void UDungeonMainUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    RefreshSelectedCharacterIdText();
    EnsureInventoryBinding();
}

////////////////////////////
//! \author 준혁
//! \brief PlayerState의 인벤토리 컴포넌트에 메소 변경 델리게이트를 바인딩하는 함수. 복제 전이면 다음 틱에 재시도한다.
void UDungeonMainUI::EnsureInventoryBinding()
{
    if (BoundInventoryComponent)
    {
        return;
    }

    const APlayerController* OwningPC = GetOwningPlayer();
    const APlayerState* OwningPS = OwningPC ? OwningPC->PlayerState : nullptr;
    UMyInventoryComponent* InventoryComponent = OwningPS ? OwningPS->FindComponentByClass<UMyInventoryComponent>() : nullptr;
    if (!InventoryComponent)
    {
        return;
    }

    BoundInventoryComponent = InventoryComponent;
    BoundInventoryComponent->OnMesoChanged.AddUniqueDynamic(this, &UDungeonMainUI::HandleMesoChanged);
    HandleMesoChanged(InventoryComponent->GetMeso());
}

////////////////////////////
//! \author 준혁
//! \brief 메소 변경 알림 처리. 보유 메소 텍스트를 갱신하는 함수
//! \param NewMeso 변경된 메소량
void UDungeonMainUI::HandleMesoChanged(int32 NewMeso)
{
    if (TXT_Meso)
    {
        TXT_Meso->SetText(FText::AsNumber(NewMeso));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 UI에서 로그인 토큰을 사용해 로그아웃 요청을 시작하는 함수
void UDungeonMainUI::RequestLogoutFromDungeon()
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
        UE_LOG(LogTemp, Warning, TEXT("Failed to create dungeon logout request."));
        FinishLogoutAndOpenLoginMap();
        return;
    }

    ActiveLogoutRequest->OnSuccess.AddDynamic(this, &UDungeonMainUI::HandleLogoutRequestSuccess);
    ActiveLogoutRequest->OnFailure.AddDynamic(this, &UDungeonMainUI::HandleLogoutRequestFailure);

    SetExitButtonEnabled(false);
    ActiveLogoutRequest->Activate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 선택 캐릭터 ID 표시 텍스트를 갱신하는 함수
// SelectedCharacterId : 던전 입장 전 선택 확정된 캐릭터 ID
void UDungeonMainUI::SetSelectedCharacterIdText(int32 SelectedCharacterId)
{
    CachedSelectedCharacterId = SelectedCharacterId;

    if (!TXT_SelectedCharacterId)
    {
        return;
    }

    if (SelectedCharacterId == 100 ||
        SelectedCharacterId == 200 ||
        SelectedCharacterId == 300)
    {
        TXT_SelectedCharacterId->SetText(FText::FromString(FString::Printf(TEXT("Character ID : %d"), SelectedCharacterId)));
        return;
    }

    TXT_SelectedCharacterId->SetText(FText::FromString(TEXT("Character ID : -")));
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 버튼 클릭 시 I키와 동일한 인벤토리 토글 경로를 호출하는 함수
void UDungeonMainUI::HandleInventoryClicked()
{
    if (AMyPlayerController* MyPC = Cast<AMyPlayerController>(GetOwningPlayer()))
    {
        MyPC->ToggleInventory();
    }
}

////////////////////////////
//! \author 장효제
//! \brief 신 페이지 버튼 클릭 시 G키와 동일한 신 페이지 토글 경로를 호출하는 함수
void UDungeonMainUI::HandleGodClicked()
{
    if (AMyPlayerController* MyPC = Cast<AMyPlayerController>(GetOwningPlayer()))
    {
        MyPC->ToggleGodPage();
    }
}

////////////////////////////
//! \author 준혁
//! \brief 설정 버튼 클릭 시 항복/게임 종료를 선택할 수 있는 설정 모달을 Modal 레이어에 띄우는 함수
void UDungeonMainUI::HandleSettingsClicked()
{
    OpenSettingsModal();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 설정 버튼 또는 ESC 입력으로 항복/게임 종료를 선택할 수 있는 메인 설정창을 여는 함수
void UDungeonMainUI::OpenSettingsModal()
{
    // 닫힌 모달은 GC 전까지 weak 포인터가 유효하게 남으므로 활성 상태로만 중복 오픈을 판정한다.
    if (const UDungeonSettingsModal* ExistingModal = ActiveSettingsModal.Get())
    {
        if (ExistingModal->IsActivated())
        {
            return;
        }
    }

    if (!SettingsModalClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DungeonMainUI SettingsModalClass is not set. Assign WBP_DungeonSettingsModal in WBP_DungeonMainUI defaults."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        return;
    }

    UMyActivatableWidget* PushedWidget = UIManager->PushModal(SettingsModalClass);
    if (!PushedWidget)
    {
        // WBP_PrimaryLayout에 UI.Layer.Modal 스택이 아직 등록되지 않은 경우 Menu 레이어로 대체한다.
        UE_LOG(LogTemp, Warning, TEXT("PushModal failed (UI.Layer.Modal stack not registered?). Falling back to Menu layer."));
        PushedWidget = UIManager->PushMenu(SettingsModalClass);
    }

    UDungeonSettingsModal* SettingsModal = Cast<UDungeonSettingsModal>(PushedWidget);
    if (!SettingsModal)
    {
        return;
    }

    SettingsModal->SetOwningMainUI(this);
    ActiveSettingsModal = SettingsModal;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 성공 후 로컬 로그인 정보를 정리하고 로그인 맵으로 이동하는 함수
// Message : 로그아웃 서버가 전달한 성공 메시지
void UDungeonMainUI::HandleLogoutRequestSuccess(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("Dungeon logout request succeeded. Message: %s"), *Message);
    FinishLogoutRequest();
    FinishLogoutAndOpenLoginMap();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 실패 후 필요 시 로컬 로그인 정보를 정리하고 로그인 맵으로 이동하는 함수
// Message : 로그아웃 요청 실패 이유 메시지
void UDungeonMainUI::HandleLogoutRequestFailure(const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("Dungeon logout request failed. Message: %s"), *Message);
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
void UDungeonMainUI::FinishLogoutAndOpenLoginMap()
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
void UDungeonMainUI::FinishLogoutRequest()
{
    if (ActiveLogoutRequest)
    {
        ActiveLogoutRequest->OnSuccess.RemoveDynamic(this, &UDungeonMainUI::HandleLogoutRequestSuccess);
        ActiveLogoutRequest->OnFailure.RemoveDynamic(this, &UDungeonMainUI::HandleLogoutRequestFailure);
        ActiveLogoutRequest = nullptr;
    }

    SetExitButtonEnabled(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그아웃 요청 중복 실행을 막기 위해 설정 모달의 액션 버튼 활성 상태를 변경하는 함수
// bShouldEnable : 항복/게임 종료 버튼 활성화 여부
void UDungeonMainUI::SetExitButtonEnabled(bool bShouldEnable) const
{
    if (UDungeonSettingsModal* SettingsModal = ActiveSettingsModal.Get())
    {
        SettingsModal->SetActionButtonsEnabled(bShouldEnable);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 PlayerState에 복제된 선택 캐릭터 ID를 읽어 UI 텍스트를 갱신하는 함수
void UDungeonMainUI::RefreshSelectedCharacterIdText()
{
    const APlayerController* PlayerController = GetOwningPlayer();
    const AMyPlayerState* MyPlayerState = PlayerController ? PlayerController->GetPlayerState<AMyPlayerState>() : nullptr;
    const int32 SelectedCharacterId = MyPlayerState ? MyPlayerState->GetSelectedCharacterId() : -1;
    if (CachedSelectedCharacterId == SelectedCharacterId)
    {
        return;
    }

    SetSelectedCharacterIdText(SelectedCharacterId);
}

// 항복 투표 UI는 MySurrenderVotePanelWidget으로 분리되었다. (Toast 레이어 상주, ADungeonPC가 푸시)
