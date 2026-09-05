#pragma once

#include "CoreMinimal.h"
#include "LobbyPartyUIDefs.h"
#include "MyActivatableWidget.h"
#include "CPP_PartyInvitePopup.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class PROJECTP_API UCPP_PartyInvitePopup : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UCPP_PartyInvitePopup();

    void InitializeInviteInfo(const FLobbyReceivedPartyInviteInfo& InviteInfo);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_YES;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_NO;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_InviteMessage;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_PartyNum;

    UPROPERTY(Transient)
    FLobbyReceivedPartyInviteInfo CachedInviteInfo;

    UFUNCTION()
    void HandleYesClicked();

    UFUNCTION()
    void HandleNoClicked();

    void BindPopupButtons();
    void UnbindPopupButtons();
    void RefreshInviteText();
    void ClosePopup();
};
