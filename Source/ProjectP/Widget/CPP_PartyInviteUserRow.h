#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyPartyUIDefs.h"
#include "CPP_PartyInviteUserRow.generated.h"

class APlayerState;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPartyInviteUserRowClickedSignature, APlayerState*, TargetPlayerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPartyJoinPartyRowClickedSignature, int32, PartyId);

UCLASS()
class PROJECTP_API UCPP_PartyInviteUserRow : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Lobby|Party")
    FPartyInviteUserRowClickedSignature OnInviteButtonClicked;

    UPROPERTY(BlueprintAssignable, Category = "Lobby|Party")
    FPartyJoinPartyRowClickedSignature OnJoinPartyButtonClicked;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void SetupPartyMember(const FLobbyPartyMemberRowData& NewMemberRowData);

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void SetupInviteTarget(APlayerState* NewTargetPlayerState, const FString& NewDisplayName);

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void SetupPartyJoinTarget(int32 NewTargetPartyId, const FString& NewDisplayName, bool bNewCanRequestJoin, float RemainingCooldownSeconds);

    UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
    void SetupPartyJoinTargetEntry(const FLobbyJoinablePartyEntry& NewPartyJoinTarget);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|Party")
    TObjectPtr<UTextBlock> TXT_Username;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|Party")
    TObjectPtr<UTextBlock> TXT_PartyState;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|Party")
    TObjectPtr<UButton> BTN_PartyInvite;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Lobby|Party")
    TObjectPtr<UTextBlock> TXT_PartyInvite;

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void BP_OnPartyMemberSetup(const FLobbyPartyMemberRowData& MemberRowData);

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void BP_OnInviteTargetSetup(APlayerState* NewTargetPlayerState, const FString& NewDisplayName);

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void BP_OnPartyJoinTargetSetup(const FLobbyJoinablePartyEntry& PartyJoinTarget);

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Party")
    void BP_OnPartyJoinButtonStateChanged(bool bCanClick, int32 RemainingCooldownSeconds);

private:
    UPROPERTY(Transient)
    FLobbyPartyMemberRowData CachedMemberRowData;

    UPROPERTY(Transient)
    TObjectPtr<APlayerState> TargetPlayerState;

    int32 TargetPartyId = -1;
    ELobbyPartyRowMode CurrentRowMode = ELobbyPartyRowMode::InviteTarget;
    bool bCanRequestJoin = false;
    float PartyJoinCooldownRemainingSeconds = 0.0f;

    UFUNCTION()
    void HandlePartyInviteButtonClicked();

    void ValidateWidgetBindings() const;
    void StartPartyJoinCooldown(float CooldownSeconds);
    void UpdatePartyJoinButtonState();
};
