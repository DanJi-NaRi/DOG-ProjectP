#pragma once

#include "CoreMinimal.h"
#include "MyActivatableWidget.h"
#include "Engine/TimerHandle.h"
#include "LobbyPartyUIDefs.h"
#include "LobbyMainWidget.generated.h"

class UButton;
class UCPP_EntryCharacterSelectWidget;
class UCPP_LobbyPartyPanel;
class UCPP_LogoutRequestAsyncAction;
class UCPP_PartyInvitePopup;

UCLASS(BlueprintType, Blueprintable)
class PROJECTP_API ULobbyMainWidget : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    ULobbyMainWidget();

    UFUNCTION(BlueprintCallable, Category = "Lobby|Login")
    void RequestLogoutFromLobby();

    void PresentPartyInvite(const FLobbyReceivedPartyInviteInfo& InviteInfo);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Login")
    FName LoginMapName = TEXT("Map_Login");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Login")
    FString LogoutRequestServerUrl;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Login")
    bool bReturnToLoginOnLogoutFailure = true;

    //! \brief 로비 입장 시 표시할 입장 전용 캐릭터 선택 위젯 클래스. 비워두면 WBP_EntryCharacterSelect를 기본으로 로드한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character")
    TSubclassOf<UCPP_EntryCharacterSelectWidget> EntryCharacterSelectWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character")
    float EntryCharacterSelectRetryIntervalSeconds = 0.25f;

    //! \brief 던전 복귀 등 서버 이동 재진입 시 직전 사용 캐릭터 자동 복원을 기다리는 최대 시간(초). 초과하면 신규 입장처럼 선택 위젯을 띄운다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character")
    float EntryAutoSelectTimeoutSeconds = 8.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party")
    TSubclassOf<UCPP_LobbyPartyPanel> LobbyPartyPanelClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Party")
    TSubclassOf<UCPP_PartyInvitePopup> PartyInvitePopupClass;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Exit;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_OpenPartyPanel;

    UPROPERTY(Transient)
    TObjectPtr<UCPP_LogoutRequestAsyncAction> ActiveLogoutRequest;

    UPROPERTY(Transient)
    TObjectPtr<UCPP_EntryCharacterSelectWidget> EntryCharacterSelectWidget;

    UPROPERTY(Transient)
    TWeakObjectPtr<UCPP_LobbyPartyPanel> ActivePartyPanel;

    FTimerHandle EntryCharacterSelectTimerHandle;

    // 직전 사용 캐릭터 자동 복원 요청을 시작한 뒤 누적된 대기 시간(초)
    float EntryAutoSelectElapsedSeconds = 0.0f;

    UFUNCTION()
    void HandleExitClicked();

    UFUNCTION()
    void HandleOpenPartyPanelClicked();

    UFUNCTION()
    void HandleLogoutRequestSuccess(const FString& Message);

    UFUNCTION()
    void HandleLogoutRequestFailure(const FString& Message);

    void FinishLogoutAndOpenLoginMap();
    void FinishLogoutRequest();
    void SetExitButtonEnabled(bool bShouldEnable) const;
    void StartEntryCharacterSelectFlow();
    void StopEntryCharacterSelectRetry();
    void TryShowEntryCharacterSelect();
    void TogglePartyPanel();
};
