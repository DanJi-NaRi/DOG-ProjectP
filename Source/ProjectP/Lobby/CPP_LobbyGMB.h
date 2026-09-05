// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "CPP_LobbyGMB.generated.h"

class AController;
class APlayerController;
class APlayerState;

/**
 * 
 */
UCLASS()
class PROJECTP_API ACPP_LobbyGMB : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACPP_LobbyGMB();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void NotifyLobbyUserTelemetryChanged();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool CreateParty(APlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool LeaveParty(APlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool RequestPartyInvite(APlayerController* RequestingController, APlayerState* TargetPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool AcceptPartyInvite(APlayerController* RequestingController, int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool DeclinePartyInvite(APlayerController* RequestingController, int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool RequestPartyJoin(APlayerController* RequestingController, int32 PartyId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool AcceptPartyJoinRequest(APlayerController* RequestingController, APlayerState* ApplicantPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool DeclinePartyJoinRequest(APlayerController* RequestingController, APlayerState* ApplicantPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool SelectPartyCharacter(APlayerController* RequestingController, int32 SelectedCharacterId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Character")
	bool SelectLobbyCharacter(APlayerController* RequestingController, int32 SelectedCharacterId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Party")
	bool SetPartyReady(APlayerController* RequestingController, bool bNewIsReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Dungeon")
	bool EnterDungeon(APlayerController* RequestingController);

	static constexpr int32 MaxPartyMemberCount = 3;

private:
	static constexpr float PartyInviteCooldownSeconds = 5.0f;
	static constexpr float LobbyTelemetryIntervalSeconds = 3.0f;

	TSet<int32> PendingDungeonAllocationPartyIds;
	int32 CurrentLobbyClientCount = 0;
	int32 TotalLobbyClientConnectCount = 0;
	int32 TotalLobbyClientDisconnectCount = 0;
	FTimerHandle LobbyTelemetryTimerHandle;

	bool IsLobbyTokenVerificationRequired() const;
	bool CanUsePartyFeature(APlayerController* RequestingController) const;
	APlayerController* FindPlayerControllerByPlayerState(APlayerState* TargetPlayerState) const;
	void SendPartyInviteRequestMessage(APlayerController* TargetController, const FString& Message) const;
	void HandleDungeonServerAllocationResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 PartyId, TWeakObjectPtr<APlayerController> RequestingController, TArray<TWeakObjectPtr<APlayerController>> PartyControllers);
	void RequestDungeonServerShutdown(const FString& DungeonSessionId) const;
	void ReportLobbyTelemetry();
	void HandleLobbyTelemetryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
