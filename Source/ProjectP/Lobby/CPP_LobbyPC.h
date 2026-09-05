// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "../MyPlayerController.h"
#include "../Widget/LobbyPartyUIDefs.h"
#include "CPP_LobbyPC.generated.h"

class APlayerState;
class ULobbyMainWidget;
class UMyPrimaryGameLayout;

/**
 * 
 */
UCLASS()
class PROJECTP_API ACPP_LobbyPC : public AMyPlayerController
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestCreateParty();

	UFUNCTION(Server, Reliable)
	void ServerRequestCreateParty();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestLeaveParty();

	UFUNCTION(Server, Reliable)
	void ServerRequestLeaveParty();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestInvitePlayer(APlayerState* TargetPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerRequestInvitePlayer(APlayerState* TargetPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestAcceptPartyInvite(int32 PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRequestAcceptPartyInvite(int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestDeclinePartyInvite(int32 PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeclinePartyInvite(int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestJoinParty(int32 PartyId);

	UFUNCTION(Server, Reliable)
	void ServerRequestJoinParty(int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestAcceptPartyJoinRequest(APlayerState* ApplicantPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerRequestAcceptPartyJoinRequest(APlayerState* ApplicantPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestDeclinePartyJoinRequest(APlayerState* ApplicantPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerRequestDeclinePartyJoinRequest(APlayerState* ApplicantPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestSelectPartyCharacter(int32 SelectedCharacterId);

	UFUNCTION(Server, Reliable)
	void ServerRequestSelectPartyCharacter(int32 SelectedCharacterId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Character")
	void RequestSelectLobbyCharacter(int32 SelectedCharacterId);

	UFUNCTION(Server, Reliable)
	void ServerRequestSelectLobbyCharacter(int32 SelectedCharacterId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	void RequestSetPartyReady(bool bNewIsReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestSetPartyReady(bool bNewIsReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Dungeon")
	void RequestEnterDungeon();

	UFUNCTION(Server, Reliable)
	void ServerRequestEnterDungeon();

	UFUNCTION(Client, Reliable)
	void ClientReceivePartyInviteRequestMessage(const FString& Message);

	UFUNCTION(Client, Reliable)
	void ClientReceivePartyInvite(const FLobbyReceivedPartyInviteInfo& InviteInfo);

	UFUNCTION(Client, Reliable)
	void ClientReceiveLobbyAuthResult(bool bSuccess, const FString& Message);

	UFUNCTION(BlueprintPure, Category = "Lobby|Auth")
	bool IsLobbyAuthVerified() const;

    void RegisterLobbyMainWidget(ULobbyMainWidget* InLobbyMainWidget);
    void UnregisterLobbyMainWidget(ULobbyMainWidget* InLobbyMainWidget);

    UFUNCTION(BlueprintPure, Category = "Lobby|UI")
    ULobbyMainWidget* GetLobbyMainWidget() const;

	virtual void BeginPlay() override;

protected:
    //! 로비에서 사용할 CommonUI 최상위 레이아웃. C2B_LobbyPC 기본값에서 WBP_LobbyPrimaryGameLayout을 지정한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|UI")
    TSubclassOf<UMyPrimaryGameLayout> LobbyPrimaryLayoutClass;

	UFUNCTION(Server, Reliable)
	void ServerSetUsername(const FString& NewUsername);

	UFUNCTION(Server, Reliable)
	void ServerSubmitLoginToken(const FString& LoginToken);

	// 파티 채널 수신 대상: 같은 PartyId 멤버들을 수집한다.
	virtual void GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const override;

private:
	bool bLobbyAuthVerifyInFlight = false;

    UPROPERTY(Transient)
    TWeakObjectPtr<ULobbyMainWidget> LobbyMainWidget;

	void HandleSessionVerifyResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
