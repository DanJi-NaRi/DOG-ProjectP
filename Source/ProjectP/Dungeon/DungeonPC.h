#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "../MyPlayerController.h"
#include "../Streaming/MyMissionTypes.h"
#include "../Streaming/MyStreamingChatTypes.h"
#include "../Widget/MyNoticeTypes.h"
#include "DungeonPC.generated.h"

class ACPP_ObeliskActor;
class UDungeonMainUI;
class UMyDialogueDataAsset;
class UMyDialogueWidget;
class UMyDungeonRevivePanelWidget;
class UMyMissionSettingPopupWidget;
class UMyNoticeWidget;
class UMySurrenderVotePanelWidget;
class AMyPlayerState;
class APawn;
class ADungeonGS;
enum class EPlayerLifeState : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMissionHudSelectionChangedSignature);

UCLASS()
class PROJECTP_API ADungeonPC : public AMyPlayerController
{
    GENERATED_BODY()

public:
    ADungeonPC();

    UFUNCTION(BlueprintPure, Category = "Dungeon|Auth")
    bool IsDungeonAuthVerified() const;

    APawn* GetReconnectPreservedPawn() const;

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Surrender")
    void RequestStartSurrenderVote();

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Surrender")
    void RequestSubmitSurrenderVote(bool bAgree);

    //! 부활 위젯이 선택한 OptionId로 서버에 부활을 요청한다. 실패 사유는 Notice로 돌아온다.
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Revive")
    void RequestRevive(FName OptionId);

    void NotifyPartyVoteSubmitted(float VoteStartServerTime);

    void RequestSpawnTestEnemies(int32 Count);


#if !UE_BUILD_SHIPPING
#endif

    //! [D-4] 서버 → 이 수령자 클라이언트에게만 Donation 결과 버블 표시를 전달한다. (Streaming Manager가 호출)
    UFUNCTION(Client, Reliable)
    void ClientReceiveDonationBubble(const FMyStreamingChatMessageData& BubbleData);

    void SendNoticeToClient(const FText& Message, float DurationSeconds);
    void SendNoticeDataToClient(const FMyNoticeData& NoticeData);
    void SendCountdownNoticeToClient(const FText& MessageFormat, float EndServerTime);

    UFUNCTION(BlueprintPure, Category = "Dungeon|Mission")
    TArray<FMyMissionPublicView> GetMissionPopupViews() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Mission")
    TArray<FMyMissionPublicView> GetMissionHudViews() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Mission")
    int32 GetDesiredMissionHudCount() const;

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Mission")
    void ApplyMissionHudSelection(const TArray<FGuid>& MissionInstanceIds);

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Mission")
    void ResetMissionHudSelection();

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Mission")
    void OpenMissionSettingPopup();

    UFUNCTION(BlueprintPure, Category = "Dungeon|Mission")
    bool IsMissionSettingPopupOpen() const;

    UPROPERTY(BlueprintAssignable, Category = "Dungeon|Mission")
    FOnMissionHudSelectionChangedSignature OnMissionHudSelectionChanged;

    // WBP_HUDLayout에 임베드된 던전 메인 UI가 생성/파괴 시 자신을 등록/해제한다.
    // (기존에는 PC가 직접 CreateWidget + AddToViewport로 생성했으나, HUD 레이어로 병합하며 소유가 레이아웃으로 넘어갔다)
    void RegisterDungeonMainUI(UDungeonMainUI* InDungeonMainUI);
    void UnregisterDungeonMainUI(UDungeonMainUI* InDungeonMainUI);

    // WBP_PrimaryGameLayout에 임베드된 항복 투표 패널이 생성/파괴 시 자신을 등록/해제한다.
    void RegisterSurrenderVotePanel(UMySurrenderVotePanelWidget* InVotePanel);
    void UnregisterSurrenderVotePanel(UMySurrenderVotePanelWidget* InVotePanel);

    //! [서버→클라] 대화를 시작시킨다. 오벨리스크가 범위(개인/파티)에 맞는 PC들에 호출한다.
    //! \param SourceObelisk 대화 출처 오벨리스크. 선택지의 기믹 트리거 통지 대상이 된다. (복제 액터라 클라에서 해석 가능)
    UFUNCTION(Client, Reliable)
    void ClientStartDialogue(const TSoftObjectPtr<UMyDialogueDataAsset>& DialogueAsset, ACPP_ObeliskActor* SourceObelisk);

    //! [클라→서버] 대화 선택지 선택을 통지한다. 검증(대화 데이터와 대조)은 오벨리스크가 서버에서 수행한다.
    UFUNCTION(Server, Reliable)
    void ServerNotifyDialogueChoice(ACPP_ObeliskActor* SourceObelisk, int32 LineIndex, int32 ChoiceIndex);

    //! [클라→서버] 대화를 마지막 줄까지 보고 끝냈음을 통지한다. 검증(활성 세션·트리거 타이밍)은 오벨리스크가 서버에서 수행한다.
    UFUNCTION(Server, Reliable)
    void ServerNotifyDialogueCompleted(ACPP_ObeliskActor* SourceObelisk);

protected:
    virtual void BeginPlay() override;
    virtual void Destroyed() override;
    virtual void SetupInputComponent() override;

    //! 클라이언트에서 PlayerState 복제를 받은 시점에 생명 상태 구독을 건다. (부활 패널 개폐용)
    virtual void OnRep_PlayerState() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Auth")
    void OnDungeonAuthResult(bool bSuccess, const FString& Message);

    //! 던전 입장 시 Persistent 오버레이(레이어 스택들 위)에 추가할 항복 투표 패널 클래스 (BP_DungeonPC 디폴트에서 WBP_SurrenderVotePanel 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|UI")
    TSubclassOf<UMySurrenderVotePanelWidget> SurrenderVotePanelClass;

    //! 던전 입장 시 Persistent 오버레이 최상단에 생성할 Notice 위젯 클래스 (BP_DungeonPC 디폴트에서 WBP_Notice 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|UI")
    TSubclassOf<UMyNoticeWidget> NoticeWidgetClass;

    //! 대화 시작 시 Dialogue 레이어에 푸시할 대화 위젯 클래스 (BP_DungeonPC 디폴트에서 WBP_Dialogue 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|UI")
    TSubclassOf<UMyDialogueWidget> DialogueWidgetClass;

    //! 로컬 플레이어가 사망하면 Modal 레이어에 푸시할 부활 패널 클래스 (BP_DungeonPC 디폴트에서 WBP_DungeonRevivePanel 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|UI")
    TSubclassOf<UMyDungeonRevivePanelWidget> RevivePanelClass;

    // 파티 채널 수신 대상: 던전 인스턴스 전원(= 파티)을 수집한다.
    virtual void GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const override;

    // 던전에서는 메시지에 캐릭터명을 포함한다.
    virtual bool ShouldIncludeMessengerCharacterName() const override;

private:
    UPROPERTY(Transient)
    TObjectPtr<APawn> ReconnectPreservedPawn;

    UPROPERTY(Transient)
    TObjectPtr<UDungeonMainUI> DungeonMainUI;

    TWeakObjectPtr<UMySurrenderVotePanelWidget> SurrenderVotePanel;

    TWeakObjectPtr<UMyDungeonRevivePanelWidget> RevivePanel;

    //! 생명 상태 구독 대상 PlayerState와 핸들. 로컬 컨트롤러에서만 사용한다.
    TWeakObjectPtr<AMyPlayerState> BoundLifeStatePlayerState;

    FDelegateHandle LifeStateChangedDelegateHandle;

    //! 로컬 PlayerState의 생명 상태 변경을 구독하고 현재 상태를 즉시 반영한다.
    void BindLocalLifeStateDelegate();
    void UnbindLocalLifeStateDelegate();

    void HandleLocalLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState);

    //! 사망 상태에 맞춰 Modal 레이어의 부활 패널을 푸시하거나 제거한다.
    void UpdateRevivePanel(bool bIsDead);

    TWeakObjectPtr<ADungeonGS> BoundMissionGameState;

    TArray<FGuid> SelectedMissionHudIds;

    int32 DesiredMissionHudCount = 3;

    //! Mission 설정 팝업 WBP다. 비워 두면 기본 경로에서 지연 로드한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|UI", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UMyMissionSettingPopupWidget> MissionSettingPopupClass;

    TWeakObjectPtr<UMyMissionSettingPopupWidget> MissionSettingPopup;

    void HandleMissionSettingPopupPressed();

    UFUNCTION(Server, Reliable)
    void ServerRequestStartSurrenderVote();

    UFUNCTION(Server, Reliable)
    void ServerRequestSubmitSurrenderVote(bool bAgree);

    UFUNCTION(Server, Reliable)
    void ServerRequestRevive(FName OptionId);

    UFUNCTION(Server, Reliable)
    void ServerSpawnTestEnemies(int32 Count);


    UFUNCTION(Client, Reliable)
    void ClientHandleSurrenderVoteSubmitted(float VoteStartServerTime);

    UFUNCTION(Client, Reliable)
    void ClientReceiveNotice(const FText& Message, float DurationSeconds);

    UFUNCTION(Client, Reliable)
    void ClientReceiveNoticeData(const FMyNoticeData& NoticeData);

    UFUNCTION(Client, Reliable)
    void ClientReceiveCountdownNotice(const FText& MessageFormat, float EndServerTime);

    UFUNCTION(Client, Reliable)
    void ClientReceiveDungeonAuthResult(bool bSuccess, const FString& Message);

    UFUNCTION(Server, Reliable)
    void ServerSubmitDungeonLoginToken(const FString& LoginToken);

    UFUNCTION(Server, Reliable)
    void ServerRequestDemoPlayerInitialization();

    bool bDungeonAuthVerifyInFlight = false;
    FString PendingDungeonLoginToken;

    void HandleEscapePressed();
    void HandleDebugSurrenderVotePreviewPressed();
    void HandleDungeonSessionVerifyResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    UFUNCTION()
    void HandleMissionViewsChanged();

    bool ReconcileMissionHudSelection();
};
