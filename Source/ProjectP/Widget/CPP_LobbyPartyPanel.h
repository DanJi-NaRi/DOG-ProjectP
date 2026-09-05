#pragma once

#include "CoreMinimal.h"
#include "MyActivatableWidget.h"
#include "LobbyPartyUIDefs.h"
#include "CPP_LobbyPartyPanel.generated.h"

class UButton;
class UCPP_CharacterSelectWidget;
class UCPP_PartyInviteUserRow;
class UTextBlock;
class UScrollBox;
class UWidget;
class UPanelWidget;
class UVerticalBox;
class APlayerState;
class ACPP_LobbyPS;
struct FLobbyOnlineUserInfo;

UENUM(BlueprintType)
enum class ELobbyPartyPanelMenu : uint8
{
    MyParty,
    PartyInvite,
    PartyJoin
};

UCLASS()
class PROJECTP_API UCPP_LobbyPartyPanel : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UCPP_LobbyPartyPanel();

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void RefreshPartyMemberList();

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    bool CanEnterDungeon() const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    bool IsLocalPlayerInParty() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    bool IsLocalPlayerReady() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    bool CanLocalPlayerSetReady() const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void RequestSelectCharacter(int32 SelectedCharacterId);

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    bool RequestSetReady(bool bNewIsReady);

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    int32 GetLocalSelectedCharacterId() const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void GetConfirmedCharacterIds(TArray<int32>& OutCharacterIds) const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void OnLocalPartyInfoChanged(int32 NewPartyId, bool bNewIsPartyLeader);

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void OnPartyPanelMenuChanged(ELobbyPartyPanelMenu NewMenu);

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void OnSelectCharacterRequested(const TArray<int32>& ConfirmedCharacterIds, int32 CurrentSelectedCharacterId);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party")
    TSubclassOf<UCPP_PartyInviteUserRow> PartyInviteUserRowClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character")
    TSubclassOf<UCPP_CharacterSelectWidget> CharacterSelectWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText CreatePartyActionText = FText::FromString(TEXT("파티 생성"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText LeavePartyActionText = FText::FromString(TEXT("파티 탈퇴"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText NoPartyStatusText = FText::FromString(TEXT("현재 파티 없음"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText PartyStatusFormatText = FText::FromString(TEXT("PartyId: {PartyId} , {LeaderName}님의 파티"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText ReadyActionText = FText::FromString(TEXT("준비 완료"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party|Text")
    FText CancelReadyActionText = FText::FromString(TEXT("준비 취소"));

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_MyParty;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_PartyInvite;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Menu3;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_EnterDungeon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_SelectCharacter;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Ready;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_PartyAction;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_CreateParty;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_LeaveParty;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_Menu3;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_PartyAction;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_PartyStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Ready;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SizeBox_PartyAction;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SizeBox_EnterDungeon;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SizeBox_SelectCharacter;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidget> SizeBox_Ready;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> ScrollBox_PartyMemberlist;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> Panel_PartyRows;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VerticalBox_PartyMemberlist;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> MemberTextBlocks;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UCPP_PartyInviteUserRow>> InviteUserRows;

    UPROPERTY(Transient)
    TObjectPtr<UCPP_CharacterSelectWidget> CharacterSelectWidget;

    TArray<FString> CachedMemberNames;
    TMap<int32, float> PartyJoinCooldownEndTimes;
    int32 CachedPartyId = -1;
    ELobbyPartyPanelMenu CurrentMenu = ELobbyPartyPanelMenu::MyParty;
    ELobbyPartyPanelMenu CachedMenu = ELobbyPartyPanelMenu::MyParty;
    bool bHasCachedMemberList = false;

    UPROPERTY(Transient)
    TObjectPtr<ACPP_LobbyPS> BoundLocalLobbyPS;

    UFUNCTION()
    void HandleMyPartyMenuClicked();

    UFUNCTION()
    void HandlePartyInviteMenuClicked();

    UFUNCTION()
    void HandlePartyJoinMenuClicked();

    UFUNCTION()
    void HandleInviteTargetButtonClicked(APlayerState* TargetPlayerState);

    UFUNCTION()
    void HandleJoinPartyButtonClicked(int32 PartyId);

    UFUNCTION()
    void HandleEnterDungeonButtonClicked();

    UFUNCTION()
    void HandleSelectCharacterButtonClicked();

    UFUNCTION()
    void HandleReadyButtonClicked();

    UFUNCTION()
    void HandlePartyActionButtonClicked();

    UFUNCTION()
    void HandleCreatePartyButtonClicked();

    UFUNCTION()
    void HandleLeavePartyButtonClicked();

    UFUNCTION()
    void HandleLocalPartyInfoChanged(int32 NewPartyId, bool bNewIsPartyLeader);

    void BindMenuButtons();
    void ValidateWidgetBindings() const;
    void BindLocalPlayerState();
    void UnbindLocalPlayerState();
    void UpdatePartyActionButtonState();
    void UpdatePartyDisplayText();
    void UpdatePartyActionText();
    void UpdatePartyStatusText();
    void UpdateEnterDungeonButtonState();
    void UpdateSelectCharacterButtonState();
    void UpdateReadyButtonState();
    void ShowCharacterSelectWidget(const TArray<int32>& ConfirmedCharacterIds, int32 CurrentSelectedCharacterId);
    void RefreshCharacterSelectWidgetState();
    void SetCurrentMenu(ELobbyPartyPanelMenu NewMenu);
    void BuildCurrentMenuNames(TArray<FString>& OutNames, int32& OutPartyId) const;
    void CacheMemberTextBlocks();
    void EnsurePartyMemberListScrollBox();
    UPanelWidget* GetPartyRowsPanel() const;
    void BuildCurrentPartyMemberNames(TArray<FString>& OutMemberNames, int32& OutPartyId) const;
    void BuildCurrentPartyMemberRows(TArray<FLobbyPartyMemberRowData>& OutMemberRows, int32& OutPartyId) const;
    void BuildPartyInviteTargetNames(TArray<FString>& OutInviteTargetNames, int32& OutPartyId) const;
    void BuildPartyInviteTargets(TArray<FLobbyOnlineUserInfo>& OutInviteTargets, int32& OutPartyId) const;
    void BuildPartyJoinTargetNames(TArray<FString>& OutPartyJoinTargetNames, int32& OutPartyId) const;
    void BuildPartyJoinTargets(TArray<FLobbyJoinablePartyEntry>& OutPartyJoinTargets, int32& OutPartyId) const;
    void ApplyCurrentMenuRows(const TArray<FString>& MemberNames);
    void ApplyPartyMemberRows(const TArray<FLobbyPartyMemberRowData>& MemberRows);
    void AddPartyMemberRow(const FLobbyPartyMemberRowData& MemberRow);
    void AddPartyMemberTextRow(const FString& MemberName);
    void ApplyPartyInviteTargets(const TArray<FLobbyOnlineUserInfo>& InviteTargets);
    void AddPartyInviteTargetRow(const FLobbyOnlineUserInfo& InviteTarget);
    void ApplyPartyJoinTargets(const TArray<FLobbyJoinablePartyEntry>& PartyJoinTargets);
    void AddPartyJoinTargetRow(const FLobbyJoinablePartyEntry& PartyJoinTarget);
    float GetPartyJoinCooldownRemainingSeconds(int32 PartyId) const;
};
