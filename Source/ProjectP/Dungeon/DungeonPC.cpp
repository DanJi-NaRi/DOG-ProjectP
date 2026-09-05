#include "DungeonPC.h"

#include "CPP_DungeonCheatManager.h"
#include "CPP_DungeonGM.h"
#include "DungeonGS.h"
#include "../Streaming/MyStreamingManagerComponent.h"
#include "Dialogue/CPP_ObeliskActor.h"
#include "Dialogue/MyDialogueDataAsset.h"
#include "Dialogue/MyDialogueWidget.h"
#include "DungeonMainUI.h"
#include "MySurrenderVotePanelWidget.h"
#include "../MyGameplayTags.h"
#include "../GameInstance/SubSystems/NetSub/AccountSessionSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "../GAS/MyPlayerState.h"
#include "../Player/Types/PlayerLifeTypes.h"
#include "../Widget/HUD/Mission/MyMissionSettingPopupWidget.h"
#include "../Widget/MyNoticeWidget.h"
#include "../Widget/MyUIManagerSubsystem.h"
#include "../Widget/Revive/MyDungeonRevivePanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HttpModule.h"
#include "InputCoreTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    ////////////////////////////
    //! \author 장효제
    //! \brief Mission 공개 View를 남은 시간 오름차순으로 정렬한다.
    void SortMissionViews(TArray<FMyMissionPublicView>& MissionViews)
    {
        MissionViews.StableSort([](const FMyMissionPublicView& Left, const FMyMissionPublicView& Right)
        {
            if (!FMath::IsNearlyEqual(Left.MissionEndsAtServerTime, Right.MissionEndsAtServerTime))
            {
                return Left.MissionEndsAtServerTime < Right.MissionEndsAtServerTime;
            }
            return Left.ActivatedAtServerTime < Right.ActivatedAtServerTime;
        });
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 전용 CheatManager 클래스를 PlayerController에 등록하는 생성자
ADungeonPC::ADungeonPC()
{
    CheatClass = UCPP_DungeonCheatManager::StaticClass();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 UI 로컬 프리뷰용 숫자 키 입력을 개발 빌드에 등록하는 함수
void ADungeonPC::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (InputComponent)
    {
        InputComponent->BindKey(
            EKeys::Escape,
            IE_Pressed,
            this,
            &ThisClass::HandleEscapePressed);

        InputComponent->BindKey(
            EKeys::M,
            IE_Pressed,
            this,
            &ThisClass::HandleMissionSettingPopupPressed);
    }

#if !UE_BUILD_SHIPPING
    if (InputComponent)
    {
        InputComponent->BindKey(
            EKeys::Eight,
            IE_Pressed,
            this,
            &ThisClass::HandleDebugSurrenderVotePreviewPressed);
    }
#endif
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 다른 UI가 ESC 입력을 소비하지 않았을 때 던전 메인 설정창을 여는 함수
void ADungeonPC::HandleEscapePressed()
{
    if (!IsLocalController() || !DungeonMainUI)
    {
        return;
    }

    // Mission 설정 팝업은 GameAndMenu라 게임 입력이 함께 살아 있다.
    // 팝업이 자체 ESC로 닫히는 동안 던전 설정창까지 함께 열리지 않게 막는다.
    if (IsMissionSettingPopupOpen())
    {
        return;
    }

    DungeonMainUI->OpenSettingsModal();
}

////////////////////////////
//! \author 장효제
//! \brief M 입력으로 Mission 설정 팝업을 열고 닫는다.
void ADungeonPC::HandleMissionSettingPopupPressed()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UMyMissionSettingPopupWidget* OpenPopup = MissionSettingPopup.Get())
    {
        OpenPopup->DeactivateWidget();
        MissionSettingPopup.Reset();
        return;
    }

    OpenMissionSettingPopup();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 숫자 8 입력 시 등록된 항복 패널의 찬성 1/3 로컬 프리뷰를 토글하는 함수
void ADungeonPC::HandleDebugSurrenderVotePreviewPressed()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UMySurrenderVotePanelWidget* VotePanel = SurrenderVotePanel.Get())
    {
        VotePanel->ToggleDebugSurrenderVotePreview();
    }
}

void ADungeonPC::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    // 데디케이티드 클라이언트에서는 PlayerState 복제가 늦을 수 있어 OnRep_PlayerState에서도 같은 구독을 시도한다.
    BindLocalLifeStateDelegate();

    if (ADungeonGS* DungeonGS = GetWorld() ? GetWorld()->GetGameState<ADungeonGS>() : nullptr)
    {
        BoundMissionGameState = DungeonGS;
        DungeonGS->OnMissionViewsChanged.AddUniqueDynamic(this, &ThisClass::HandleMissionViewsChanged);
        HandleMissionViewsChanged();
    }

    // - 준혁 -
    // HUD(WBP_HUDLayout)가 인벤토리를 처음 열기 전에도 보이도록 PrimaryLayout을 던전 입장 시점에 미리 생성한다.
    // (기존에는 ToggleInventory가 레이아웃을 처음 생성해서, I 키를 누르기 전까지 HUD가 나타나지 않았다)
    // UI 테스트 슬라이스가 켜져 있으면 슬라이스가 자체 테스트 레이아웃을 생성하므로 건너뛴다.
    if (!bEnableUITestVerticalSlice)
    {
        ULocalPlayer* LocalPlayer = GetLocalPlayer();
        UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
        if (UIManager)
        {
            UIManager->EnsurePrimaryLayout(this);

            // 항복 투표 패널은 레이어 스택이 아닌 Persistent 오버레이(스택들 위)에 상주시킨다.
            // (활성 상주 위젯을 레이어 스택에 넣으면 CommonUI 액티브 루트를 점유해 하위 레이어 입력 설정이 무시됨)
            // 위젯이 NativeConstruct에서 RegisterSurrenderVotePanel로 자기 등록하므로 반환값은 쓰지 않는다.
            if (SurrenderVotePanelClass)
            {
                UIManager->AddPersistentWidget(SurrenderVotePanelClass);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SurrenderVotePanelClass is not set. Assign WBP_SurrenderVotePanel in BP_DungeonPC defaults."));
            }

            // Notice는 항복 패널 뒤에 추가해 Persistent 오버레이 안에서도 더 위에 그려지도록 한다.
            if (NoticeWidgetClass)
            {
                if (!UIManager->EnsureNoticeWidget(this, NoticeWidgetClass))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to ensure WBP_Notice in BP_DungeonPC."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("NoticeWidgetClass is not set. Assign WBP_Notice in BP_DungeonPC defaults."));
            }
        }
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UAccountSessionSubsystem>() : nullptr;
    if (!AccountSessionSubsystem)
    {
        ServerRequestDemoPlayerInitialization();
        return;
    }

    const FString& LoginToken = AccountSessionSubsystem->GetLoginToken();
    if (LoginToken.IsEmpty())
    {
        ServerRequestDemoPlayerInitialization();
        return;
    }

    ServerSubmitDungeonLoginToken(LoginToken);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 PlayerController 파괴 시 재접속 대기 Pawn이 즉시 파괴되지 않도록 소유를 해제하는 함수
void ADungeonPC::Destroyed()
{
    UnbindLocalLifeStateDelegate();
    UpdateRevivePanel(false);

    if (BoundMissionGameState.IsValid())
    {
        BoundMissionGameState->OnMissionViewsChanged.RemoveDynamic(this, &ThisClass::HandleMissionViewsChanged);
        BoundMissionGameState.Reset();
    }

    if (HasAuthority())
    {
        if (APawn* ControlledPawn = GetPawn())
        {
            ReconnectPreservedPawn = ControlledPawn;
            UnPossess();
            ControlledPawn->SetOwner(nullptr);
        }
    }

    Super::Destroyed();
}

////////////////////////////
//! \author 장효제
//! \brief 설정 팝업에 표시할 공개 Mission을 종료 시각 순으로 반환한다.
//! \return Active와 교체 대기 중인 결과 Mission View다.
TArray<FMyMissionPublicView> ADungeonPC::GetMissionPopupViews() const
{
    const ADungeonGS* DungeonGS = GetWorld() ? GetWorld()->GetGameState<ADungeonGS>() : nullptr;
    TArray<FMyMissionPublicView> MissionViews = DungeonGS ? DungeonGS->GetMissionViews() : TArray<FMyMissionPublicView>{};
    SortMissionViews(MissionViews);
    return MissionViews;
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 플레이어가 HUD에 표시하도록 선택한 Mission View만 반환한다.
//! \return 로컬 선택과 종료 시각 순서가 반영된 최대 세 개 View다.
TArray<FMyMissionPublicView> ADungeonPC::GetMissionHudViews() const
{
    TArray<FMyMissionPublicView> Result;
    for (const FMyMissionPublicView& MissionView : GetMissionPopupViews())
    {
        if (SelectedMissionHudIds.Contains(MissionView.MissionInstanceId))
        {
            Result.Add(MissionView);
        }
    }
    return Result;
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 HUD가 자동 보충할 표시 Mission 개수를 반환한다.
//! \return 마지막 적용값인 0~3이다.
int32 ADungeonPC::GetDesiredMissionHudCount() const
{
    return DesiredMissionHudCount;
}

////////////////////////////
//! \author 장효제
//! \brief 팝업의 임시 선택을 현재 유효한 Mission에 한해 로컬 HUD 선택으로 적용한다.
//! \param MissionInstanceIds 사용자가 선택한 최대 세 개 Mission Instance ID다.
void ADungeonPC::ApplyMissionHudSelection(const TArray<FGuid>& MissionInstanceIds)
{
    if (!IsLocalController())
    {
        return;
    }

    SelectedMissionHudIds.Reset();
    for (const FMyMissionPublicView& MissionView : GetMissionPopupViews())
    {
        if (MissionInstanceIds.Contains(MissionView.MissionInstanceId))
        {
            SelectedMissionHudIds.AddUnique(MissionView.MissionInstanceId);
            if (SelectedMissionHudIds.Num() >= 3)
            {
                break;
            }
        }
    }
    DesiredMissionHudCount = SelectedMissionHudIds.Num();
    OnMissionHudSelectionChanged.Broadcast();
}

////////////////////////////
//! \author 장효제
//! \brief HUD 선택을 남은 시간이 짧은 Active Mission 최대 세 개로 초기화한다.
void ADungeonPC::ResetMissionHudSelection()
{
    if (!IsLocalController())
    {
        return;
    }

    DesiredMissionHudCount = 3;
    SelectedMissionHudIds.Reset();
    ReconcileMissionHudSelection();
    OnMissionHudSelectionChanged.Broadcast();
}

////////////////////////////
//! \author 장효제
//! \brief Mission 설정 팝업을 Overlay 레이어에 한 번만 올린다.
void ADungeonPC::OpenMissionSettingPopup()
{
    if (!IsLocalController() || MissionSettingPopup.IsValid())
    {
        return;
    }

    if (!MissionSettingPopupClass)
    {
        // WBP를 아직 만들지 않았어도 조용히 넘어가도록 경고 없이 지연 로드한다.
        MissionSettingPopupClass = LoadClass<UMyMissionSettingPopupWidget>(
            nullptr,
            TEXT("/Game/LeDuat/Widget/Dungeon/WBP_MissionSettingPopup.WBP_MissionSettingPopup_C"),
            nullptr,
            LOAD_NoWarn);
    }
    if (!MissionSettingPopupClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("MissionSettingPopupClass가 없습니다. /Game/LeDuat/Widget/Dungeon/WBP_MissionSettingPopup을 만들거나 BP_DungeonPC 기본값에 지정하세요."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        return;
    }

    MissionSettingPopup = Cast<UMyMissionSettingPopupWidget>(
        UIManager->PushOverlay(MissionSettingPopupClass));
    if (!MissionSettingPopup.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Mission 설정 팝업 Push 실패. WBP_PrimaryGameLayout에 UI.Layer.Overlay 스택이 등록되어 있는지 확인하세요."));
    }
}

////////////////////////////
//! \author 장효제
//! \brief Mission 설정 팝업이 현재 열려 있는지 반환한다.
//! \return 팝업이 살아 있으면 true다.
bool ADungeonPC::IsMissionSettingPopupOpen() const
{
    return MissionSettingPopup.IsValid();
}

////////////////////////////
//! \author 장효제
//! \brief 복제 Mission View 변화에 로컬 선택을 맞추고 HUD 갱신을 알린다.
void ADungeonPC::HandleMissionViewsChanged()
{
    ReconcileMissionHudSelection();
    OnMissionHudSelectionChanged.Broadcast();
}

////////////////////////////
//! \author 장효제
//! \brief 사라진 선택을 제거하고 희망 슬롯 수까지 가장 이른 Active Mission으로 보충한다.
//! \return MissionInstanceId 선택 목록이 변경되었으면 true다.
bool ADungeonPC::ReconcileMissionHudSelection()
{
    const TArray<FGuid> PreviousSelection = SelectedMissionHudIds;
    const TArray<FMyMissionPublicView> MissionViews = GetMissionPopupViews();

    SelectedMissionHudIds.RemoveAll([&MissionViews](const FGuid& MissionInstanceId)
    {
        return !MissionViews.ContainsByPredicate([&MissionInstanceId](const FMyMissionPublicView& MissionView)
        {
            return MissionView.MissionInstanceId == MissionInstanceId;
        });
    });

    while (SelectedMissionHudIds.Num() > DesiredMissionHudCount)
    {
        SelectedMissionHudIds.Pop();
    }

    for (const FMyMissionPublicView& MissionView : MissionViews)
    {
        if (SelectedMissionHudIds.Num() >= DesiredMissionHudCount)
        {
            break;
        }
        if (MissionView.State == EMyMissionState::Active)
        {
            SelectedMissionHudIds.AddUnique(MissionView.MissionInstanceId);
        }
    }

    TArray<FGuid> OrderedSelection;
    for (const FMyMissionPublicView& MissionView : MissionViews)
    {
        if (SelectedMissionHudIds.Contains(MissionView.MissionInstanceId))
        {
            OrderedSelection.Add(MissionView.MissionInstanceId);
        }
    }
    SelectedMissionHudIds = MoveTemp(OrderedSelection);
    return PreviousSelection != SelectedMissionHudIds;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 PlayerController 파괴 중 재접속 복구를 위해 보관한 Pawn을 반환하는 함수
// Return Value : 재접속 복구 대상 Pawn
APawn* ADungeonPC::GetReconnectPreservedPawn() const
{
    return ReconnectPreservedPawn.Get();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 인증이 완료되었는지 확인하는 함수
// Return Value : 인증 완료 여부
bool ADungeonPC::IsDungeonAuthVerified() const
{
    const AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
    return MyPlayerState && MyPlayerState->IsAuthVerified();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 시작 요청을 서버 RPC로 전달하는 함수
void ADungeonPC::RequestStartSurrenderVote()
{
    ServerRequestStartSurrenderVote();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 찬반 요청을 서버 RPC로 전달하는 함수
// bAgree : 항복 투표 찬성 여부
void ADungeonPC::RequestSubmitSurrenderVote(bool bAgree)
{
    ServerRequestSubmitSurrenderVote(bAgree);
}

////////////////////////////
//! \author 장효제
//! \brief 클라이언트에서 PlayerState 복제를 받은 시점에 생명 상태 구독을 갱신한다.
//! \param 없음
//! \return 없음
void ADungeonPC::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    BindLocalLifeStateDelegate();
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 PlayerState의 생명 상태 변경을 구독하고, 이미 복제된 현재 상태도 즉시 반영한다.
//! \param 없음
//! \return 없음
void ADungeonPC::BindLocalLifeStateDelegate()
{
    if (!IsLocalController())
    {
        return;
    }

    AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
    if (!MyPlayerState || BoundLifeStatePlayerState.Get() == MyPlayerState)
    {
        return;
    }

    UnbindLocalLifeStateDelegate();

    BoundLifeStatePlayerState = MyPlayerState;
    LifeStateChangedDelegateHandle = MyPlayerState->OnLifeStateChanged.AddUObject(
        this,
        &ADungeonPC::HandleLocalLifeStateChanged);

    // 사망한 채로 재접속하면 상태 변경 통지가 이미 지나갔을 수 있으므로 현재 상태를 바로 반영한다.
    UpdateRevivePanel(MyPlayerState->IsDead());
}

//////////////////////////////////////////////////////////////////////
// - 장효제 -
// 생명 상태 구독을 해제하는 함수
void ADungeonPC::UnbindLocalLifeStateDelegate()
{
    if (BoundLifeStatePlayerState.IsValid() && LifeStateChangedDelegateHandle.IsValid())
    {
        BoundLifeStatePlayerState->OnLifeStateChanged.Remove(LifeStateChangedDelegateHandle);
    }

    BoundLifeStatePlayerState.Reset();
    LifeStateChangedDelegateHandle.Reset();
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 플레이어의 생명 상태가 바뀌면 부활 패널을 열거나 닫는다.
//! \param OldLifeState 변경 전 생명 상태
//! \param NewLifeState 변경 후 생명 상태
//! \return 없음
void ADungeonPC::HandleLocalLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState)
{
    (void)OldLifeState;

    UpdateRevivePanel(NewLifeState == EPlayerLifeState::Dead);
}

////////////////////////////
//! \author 장효제
//! \brief 사망 시 Modal 레이어에 부활 패널을 푸시하고, 부활하면 제거한다.
//! \param bIsDead 로컬 플레이어의 사망 여부
//! \return 없음
void ADungeonPC::UpdateRevivePanel(bool bIsDead)
{
    if (!bIsDead)
    {
        if (UMyDungeonRevivePanelWidget* ActiveRevivePanel = RevivePanel.Get())
        {
            ULocalPlayer* LocalPlayer = GetLocalPlayer();
            if (UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr)
            {
                UIManager->RemoveWidgetFromLayer(ActiveRevivePanel);
            }
        }
        RevivePanel = nullptr;
        return;
    }

    if (RevivePanel.IsValid() || !IsLocalController())
    {
        return;
    }

    if (!RevivePanelClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("RevivePanelClass is not set. Assign WBP_DungeonRevivePanel in BP_DungeonPC defaults."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        return;
    }

    RevivePanel = Cast<UMyDungeonRevivePanelWidget>(UIManager->PushModal(RevivePanelClass));
    if (!RevivePanel.IsValid())
    {
        // 레이아웃에 UI.Layer.Modal 레이어가 등록돼 있지 않으면 푸시가 조용히 실패한다.
        UE_LOG(LogTemp, Warning, TEXT("Failed to push revive panel. Check that WBP_PrimaryGameLayout registers the UI.Layer.Modal layer."));
    }
}

////////////////////////////
//! \author 장효제
//! \brief 부활 위젯이 고른 옵션으로 서버에 부활을 요청한다.
//! \param OptionId Revive_Options 데이터 에셋의 옵션 ID
//! \return 없음
void ADungeonPC::RequestRevive(FName OptionId)
{
    ServerRequestRevive(OptionId);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 공용 투표를 제출한 로컬 플레이어의 투표 패널을 완료 상태로 전환하는 함수
// VoteStartServerTime : 현재 공용 투표가 시작된 서버 월드 시간
void ADungeonPC::NotifyPartyVoteSubmitted(float VoteStartServerTime)
{
    if (!HasAuthority())
    {
        return;
    }

    ClientHandleSurrenderVoteSubmitted(VoteStartServerTime);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 테스트 적 스폰 요청을 서버 RPC로 전달하는 함수
// Count : 스폰할 테스트 적 수
void ADungeonPC::RequestSpawnTestEnemies(int32 Count)
{
    ServerSpawnTestEnemies(Count);
}


#if !UE_BUILD_SHIPPING
#endif

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 소유 클라이언트로 일반 Notice 표시를 전달하는 함수 (Reliable)
// Message : 표시할 시스템 메시지
// DurationSeconds : 메시지를 표시할 시간, 0 이하이면 위젯 기본값 사용
void ADungeonPC::SendNoticeToClient(const FText& Message, float DurationSeconds)
{
    if (!HasAuthority())
    {
        return;
    }

    ClientReceiveNotice(Message, DurationSeconds);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 소유 클라이언트로 표시 종류와 Rich Text가 포함된 Notice를 전달하는 함수 (Reliable)
// NoticeData : 일반 문구, Rich Text 문구, 표시 시간, 시각 표현 종류를 담은 데이터
void ADungeonPC::SendNoticeDataToClient(const FMyNoticeData& NoticeData)
{
    if (!HasAuthority())
    {
        return;
    }

    ClientReceiveNoticeData(NoticeData);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 소유 클라이언트로 카운트다운 Notice 표시를 전달하는 함수 (Reliable)
// MessageFormat : 남은 초가 {0} 위치에 들어가는 메시지 형식
// EndServerTime : 카운트다운이 끝나는 서버 월드 시간
void ADungeonPC::SendCountdownNoticeToClient(const FText& MessageFormat, float EndServerTime)
{
    if (!HasAuthority())
    {
        return;
    }

    ClientReceiveCountdownNotice(MessageFormat, EndServerTime);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 항복 투표 시작 요청을 처리하는 함수 (Reliable)
void ADungeonPC::ServerRequestStartSurrenderVote_Implementation()
{
    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            if (DungeonGM->StartSurrenderVote(this))
            {
                ClientHandleSurrenderVoteSubmitted(DungeonGM->GetSurrenderVoteStartServerTime());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 테스트 적 스폰 요청을 Dungeon GameMode로 전달하는 함수 (Reliable)
// Count : 스폰할 테스트 적 수
void ADungeonPC::ServerSpawnTestEnemies_Implementation(int32 Count)
{
    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            DungeonGM->SpawnTestEnemiesForStressTest(Count);
        }
    }
}


////////////////////////////
//! \author 장효제
//! \brief [D-4] 수령자 클라이언트에서 던전 GS의 Streaming Manager로 Donation 결과 버블 표시를 넘긴다. (Reliable)
void ADungeonPC::ClientReceiveDonationBubble_Implementation(const FMyStreamingChatMessageData& BubbleData)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    ADungeonGS* DungeonGS = World->GetGameState<ADungeonGS>();
    if (!DungeonGS)
    {
        UE_LOG(LogTemp, Warning, TEXT("ClientReceiveDonationBubble failed. DungeonGS is not available."));
        return;
    }

    UMyStreamingManagerComponent* StreamingManager = DungeonGS->GetStreamingManager();
    if (!StreamingManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("ClientReceiveDonationBubble failed. StreamingManager is not available."));
        return;
    }

    StreamingManager->ClientDisplayDonationBubble(BubbleData);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버가 로컬 플레이어의 항복 투표 제출을 정상 처리했을 때 UI를 완료 상태로 바꾸는 함수 (Reliable)
// VoteStartServerTime : 투표가 시작된 서버 시간
////////////////////////////
//! \author 준혁
//! \brief [클라] 대화 UI를 시작하는 함수. Menu 레이어(상점/인벤토리)를 전부 닫고 HUD 레이어를 숨긴 뒤
//!        Dialogue 레이어에 대화 위젯을 띄운다. Modal 레이어(설정창)와 Toast 레이어(항복 투표)는 유지된다.
//!        HUD 복원은 대화 위젯이 닫힐 때(NativeOnDeactivated) 수행한다.
//! \param DialogueAsset 시작할 대화 데이터에셋 (소프트 참조, 클라에서 로드)
//! \param SourceObelisk 대화 출처 오벨리스크. 선택지 선택 시 서버 통지 대상으로 위젯에 전달된다.
void ADungeonPC::ClientStartDialogue_Implementation(const TSoftObjectPtr<UMyDialogueDataAsset>& DialogueAsset, ACPP_ObeliskActor* SourceObelisk)
{
    const AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
    if (MyPlayerState && MyPlayerState->IsDead())
    {
        return;
    }

    const UMyDialogueDataAsset* Dialogue = DialogueAsset.LoadSynchronous();
    if (!Dialogue)
    {
        UE_LOG(LogTemp, Warning, TEXT("ClientStartDialogue failed - dialogue asset could not be loaded. Path: %s"), *DialogueAsset.ToString());
        return;
    }

    if (!DialogueWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DialogueWidgetClass is not set. Assign WBP_Dialogue in BP_DungeonPC defaults."));
        return;
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (!UIManager)
    {
        return;
    }

    // 이미 떠 있는 대화가 있으면 정리하고 새 대화로 교체한다.
    UIManager->ClearLayer(MyGameplayTags::UI_Layer_Dialogue);

    // 상점/인벤토리 등 Menu 레이어를 전부 닫는다. (각 위젯의 닫힘 처리에서 상호작용 종료 등 자체 정리가 수행된다)
    UIManager->ClearLayer(MyGameplayTags::UI_Layer_Menu);

    // 여기서 HUD를 숨기기만 한다는 것은 스택에서 제거하지 않는다는 의미이며,
    // CommonUI 입력 루트에서는 제외되도록 현재 HUD 위젯을 먼저 비활성화한다.
    // 항복 투표(Toast)와 설정창(Modal)은 별도 레이어라 영향받지 않는다.
    UIManager->SetLayerActive(MyGameplayTags::UI_Layer_HUD, false);
    UIManager->SetLayerVisible(MyGameplayTags::UI_Layer_HUD, false);

    UMyDialogueWidget* DialogueWidget = Cast<UMyDialogueWidget>(UIManager->PushDialogue(DialogueWidgetClass));
    if (!DialogueWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to push dialogue widget. (UI.Layer.Dialogue stack registered in WBP_PrimaryGameLayout?)"));
        UIManager->SetLayerVisible(MyGameplayTags::UI_Layer_HUD, true);
        UIManager->SetLayerActive(MyGameplayTags::UI_Layer_HUD, true);
        return;
    }

    DialogueWidget->StartDialogue(Dialogue, SourceObelisk);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 클라이언트의 대화 선택지 선택을 오벨리스크에 전달하는 함수.
//!        선택지 유효성·기믹 트리거 여부 검증은 대화 데이터를 아는 오벨리스크가 수행한다.
//! \param SourceObelisk 대화 출처 오벨리스크
//! \param LineIndex 선택지가 있던 대화 줄 인덱스
//! \param ChoiceIndex 고른 선택지 인덱스
void ADungeonPC::ServerNotifyDialogueChoice_Implementation(ACPP_ObeliskActor* SourceObelisk, int32 LineIndex, int32 ChoiceIndex)
{
    if (IsValid(SourceObelisk))
    {
        SourceObelisk->NotifyDialogueChoiceOnServer(GetPawn(), LineIndex, ChoiceIndex);
    }
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 클라이언트의 대화 완주 통지를 오벨리스크에 전달하는 함수.
//!        활성 세션·트리거 타이밍 검증은 대화 세션을 아는 오벨리스크가 수행한다.
//! \param SourceObelisk 대화 출처 오벨리스크
void ADungeonPC::ServerNotifyDialogueCompleted_Implementation(ACPP_ObeliskActor* SourceObelisk)
{
    if (IsValid(SourceObelisk))
    {
        SourceObelisk->NotifyDialogueCompletedOnServer(GetPawn());
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 받은 일반 Notice를 로컬 UI Manager에 전달하는 함수 (Reliable)
// Message : 표시할 시스템 메시지
// DurationSeconds : 메시지를 표시할 시간, 0 이하이면 위젯 기본값 사용
void ADungeonPC::ClientReceiveNotice_Implementation(const FText& Message, float DurationSeconds)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (UIManager)
    {
        UIManager->ShowNotice(Message, DurationSeconds);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 받은 표시 종류와 Rich Text가 포함된 Notice를 로컬 UI Manager에 전달하는 함수 (Reliable)
// NoticeData : 일반 문구, Rich Text 문구, 표시 시간, 시각 표현 종류를 담은 데이터
void ADungeonPC::ClientReceiveNoticeData_Implementation(const FMyNoticeData& NoticeData)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (UIManager)
    {
        UIManager->ShowNoticeData(NoticeData);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 받은 카운트다운 Notice를 로컬 UI Manager에 전달하는 함수 (Reliable)
// MessageFormat : 남은 초가 {0} 위치에 들어가는 메시지 형식
// EndServerTime : 카운트다운이 끝나는 서버 월드 시간
void ADungeonPC::ClientReceiveCountdownNotice_Implementation(const FText& MessageFormat, float EndServerTime)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
    if (UIManager)
    {
        UIManager->ShowCountdownNotice(MessageFormat, EndServerTime);
    }
}

void ADungeonPC::ClientHandleSurrenderVoteSubmitted_Implementation(float VoteStartServerTime)
{
    if (UMySurrenderVotePanelWidget* VotePanel = SurrenderVotePanel.Get())
    {
        VotePanel->SetLocalSurrenderVoteSubmitted(true, VoteStartServerTime);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 전송된 던전 인증 결과를 클라이언트에서 받는 함수
// bSuccess : 인증 성공 여부
// Message : 인증 결과 메시지
void ADungeonPC::ClientReceiveDungeonAuthResult_Implementation(bool bSuccess, const FString& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("Dungeon auth result. Success: %s, Message: %s"), bSuccess ? TEXT("true") : TEXT("false"), *Message);
    OnDungeonAuthResult(bSuccess, Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// WBP_HUDLayout에 임베드된 던전 메인 UI가 생성될 때 자신을 등록하는 함수.
// PC는 이 참조로 항복 투표 완료 통지(ClientHandleSurrenderVoteSubmitted) 등을 위젯에 전달한다.
// InDungeonMainUI : 등록할 던전 메인 UI 위젯
void ADungeonPC::RegisterDungeonMainUI(UDungeonMainUI* InDungeonMainUI)
{
    DungeonMainUI = InDungeonMainUI;
}

////////////////////////////
//! \author 준혁
//! \brief WBP_PrimaryGameLayout에 임베드된 항복 투표 패널이 자신을 등록하는 함수
//! \param InVotePanel 등록할 항복 투표 패널 위젯
void ADungeonPC::RegisterSurrenderVotePanel(UMySurrenderVotePanelWidget* InVotePanel)
{
    SurrenderVotePanel = InVotePanel;
}

////////////////////////////
//! \author 준혁
//! \brief 항복 투표 패널이 파괴될 때 등록을 해제하는 함수. 다른 인스턴스가 이미 등록돼 있으면 건드리지 않는다.
//! \param InVotePanel 해제할 항복 투표 패널 위젯
void ADungeonPC::UnregisterSurrenderVotePanel(UMySurrenderVotePanelWidget* InVotePanel)
{
    if (SurrenderVotePanel.Get() == InVotePanel)
    {
        SurrenderVotePanel = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 메인 UI가 파괴될 때 등록을 해제하는 함수. 다른 인스턴스가 이미 등록돼 있으면 건드리지 않는다.
// InDungeonMainUI : 해제할 던전 메인 UI 위젯
void ADungeonPC::UnregisterDungeonMainUI(UDungeonMainUI* InDungeonMainUI)
{
    if (DungeonMainUI == InDungeonMainUI)
    {
        DungeonMainUI = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 던전 입장 로그인 토큰과 던전 세션 권한 검증을 요청하는 함수 (Reliable)
// LoginToken : 로그인 서버에서 발급받은 세션 토큰
void ADungeonPC::ServerSubmitDungeonLoginToken_Implementation(const FString& LoginToken)
{
    if (bDungeonAuthVerifyInFlight)
    {
        return;
    }

    if (LoginToken.IsEmpty())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Login token is empty."));
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    if (!ServerConfigSubsystem)
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Server config subsystem is not available on dungeon server."));
        return;
    }

    const FString VerifyUrl = ServerConfigSubsystem->GetDungeonSessionVerifyUrl();
    const FString ServerAuthKey = ServerConfigSubsystem->GetDungeonStateServerAuthKey();
    if (VerifyUrl.IsEmpty())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session verify URL is empty."));
        return;
    }

    if (ServerAuthKey.IsEmpty())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session server auth key is empty."));
        return;
    }

    FString DungeonSessionId;
    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            DungeonSessionId = DungeonGM->GetRuntimeDungeonSessionId();
        }
    }

    if (DungeonSessionId.IsEmpty())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session ID is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("token"), LoginToken);
    JsonObject->SetStringField(TEXT("dungeonSessionId"), DungeonSessionId);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    bDungeonAuthVerifyInFlight = true;
    PendingDungeonLoginToken = LoginToken;

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(VerifyUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &ADungeonPC::HandleDungeonSessionVerifyResponse);

    if (!HttpRequest->ProcessRequest())
    {
        bDungeonAuthVerifyInFlight = false;
        PendingDungeonLoginToken.Empty();
        ClientReceiveDungeonAuthResult(false, TEXT("Failed to start dungeon session verify request."));
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 토큰이 없는 PIE 또는 시연 환경의 플레이어 초기화를 서버에 요청하는 함수 (Reliable)
void ADungeonPC::ServerRequestDemoPlayerInitialization_Implementation()
{
    ACPP_DungeonGM* DungeonGM = GetWorld() ? GetWorld()->GetAuthGameMode<ACPP_DungeonGM>() : nullptr;
    const bool bInitialized = DungeonGM && DungeonGM->TryInitializeDemoPlayer(this);

    ClientReceiveDungeonAuthResult(
        bInitialized,
        bInitialized
            ? TEXT("Dungeon demo player initialization succeeded.")
            : TEXT("Unauthenticated dungeon initialization is not allowed."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 세션 검증 응답을 처리하는 함수
// Request : HTTP 요청 객체
// Response : HTTP 응답 객체
// bWasSuccessful : HTTP 요청이 성공적으로 완료되었는지 여부
void ADungeonPC::HandleDungeonSessionVerifyResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    bDungeonAuthVerifyInFlight = false;
    const FString VerifiedLoginToken = PendingDungeonLoginToken;
    PendingDungeonLoginToken.Empty();

    if (!bWasSuccessful || !Response.IsValid())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session verify request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseBody = Response->GetContentAsString();

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        ClientReceiveDungeonAuthResult(false, FString::Printf(TEXT("Invalid dungeon session verify response. HTTP %d"), ResponseCode));
        return;
    }

    FString Message;
    JsonObject->TryGetStringField(TEXT("message"), Message);

    bool bOk = false;
    JsonObject->TryGetBoolField(TEXT("ok"), bOk);

    if (ResponseCode < 200 || ResponseCode >= 300 || !bOk)
    {
        ClientReceiveDungeonAuthResult(false, Message.IsEmpty() ? FString::Printf(TEXT("Dungeon session verify failed. HTTP %d"), ResponseCode) : Message);
        return;
    }

    const TSharedPtr<FJsonObject>* UserObject = nullptr;
    if (!JsonObject->TryGetObjectField(TEXT("user"), UserObject) || UserObject == nullptr || !UserObject->IsValid())
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session verify response is missing user data."));
        return;
    }

    double UserIndexNumber = 0.0;
    double CharacterIdNumber = -1.0;
    FString Username;
    (*UserObject)->TryGetNumberField(TEXT("user_Index"), UserIndexNumber);
    (*UserObject)->TryGetStringField(TEXT("username"), Username);
    JsonObject->TryGetNumberField(TEXT("characterId"), CharacterIdNumber);

    const int32 VerifiedUserIndex = static_cast<int32>(UserIndexNumber);
    const int32 SelectedCharacterId = static_cast<int32>(CharacterIdNumber);
    if (VerifiedUserIndex <= 0 ||
        Username.IsEmpty() ||
        (SelectedCharacterId != 100 && SelectedCharacterId != 200 && SelectedCharacterId != 300))
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session verify response has invalid user or character data."));
        return;
    }

    AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
    if (!MyPlayerState)
    {
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon PlayerState is not available."));
        return;
    }

    MyPlayerState->SetAuthenticatedUser(VerifiedUserIndex, Username);
    MyPlayerState->SetSelectedCharacterId(SelectedCharacterId);
    bool bRegisteredInDungeonSession = false;
    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            bRegisteredInDungeonSession = DungeonGM->RegisterAuthenticatedDungeonPlayer(this, VerifiedLoginToken);
        }
    }

    if (!bRegisteredInDungeonSession)
    {
        MyPlayerState->ClearAuthenticatedUser();
        ClientReceiveDungeonAuthResult(false, TEXT("Dungeon session is not active."));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("Dungeon auth verified. UserIndex: %d, Username: %s, CharacterId: %d"), VerifiedUserIndex, *Username, SelectedCharacterId);
    ClientReceiveDungeonAuthResult(true, Message.IsEmpty() ? TEXT("Dungeon auth verified.") : Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 항복 투표 찬반 요청을 처리하는 함수 (Reliable)
// bAgree : 항복 투표 찬성 여부
void ADungeonPC::ServerRequestSubmitSurrenderVote_Implementation(bool bAgree)
{
    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            const bool bVoteAccepted = DungeonGM->SubmitSurrenderVote(this, bAgree);
            if (bVoteAccepted && bAgree)
            {
                ClientHandleSurrenderVoteSubmitted(DungeonGM->GetSurrenderVoteStartServerTime());
            }
        }
    }
}

////////////////////////////
//! \author 장효제
//! \brief [서버] 요청한 옵션으로 부활을 실행하고, 실패 사유는 요청자에게만 Notice로 알린다.
//! \param OptionId Revive_Options 데이터 에셋의 옵션 ID
//! \return 없음
void ADungeonPC::ServerRequestRevive_Implementation(FName OptionId)
{
    UWorld* World = GetWorld();
    ACPP_DungeonGM* DungeonGM = World ? World->GetAuthGameMode<ACPP_DungeonGM>() : nullptr;
    if (!DungeonGM)
    {
        return;
    }

    FString ResultMessage;
    if (!DungeonGM->StartPlayerRevive(this, OptionId, ResultMessage))
    {
        SendNoticeToClient(FText::FromString(ResultMessage), 0.0f);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 채널 수신 대상을 수집한다. 던전 인스턴스는 곧 하나의 파티이므로
// 접속한 모든 PlayerController(전체 대상)를 그대로 파티 대상으로 사용한다.
// OutRecipients : 수신 대상 컨트롤러 목록(출력)
void ADungeonPC::GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const
{
    GetMessengerAllRecipients(OutRecipients);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전에서는 메시지에 캐릭터명(Nefer/Inpu/Heru)을 포함한다.
// 반환값 : 항상 true
bool ADungeonPC::ShouldIncludeMessengerCharacterName() const
{
    return true;
}
