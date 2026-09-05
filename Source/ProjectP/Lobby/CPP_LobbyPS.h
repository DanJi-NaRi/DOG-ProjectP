// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../GAS/MyPlayerState.h"
#include "CPP_LobbyPS.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLobbyPartyInfoChangedSignature, int32, NewPartyId, bool, bNewIsPartyLeader);

/**
 * 
 */
UCLASS()
class PROJECTP_API ACPP_LobbyPS : public AMyPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Party")
	FLobbyPartyInfoChangedSignature OnPartyInfoChanged;

	UFUNCTION(BlueprintPure, Category = "Lobby|Party")
	int32 GetPartyId() const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Party")
	bool IsPartyLeader() const;

	void SetPartyInfo(int32 NewPartyId, bool bNewIsPartyLeader);

	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	const FString& GetLobbyConnectedAt() const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	int64 GetLobbyConnectedUnixTimestamp() const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Player")
	bool IsLobbyAuthVerified() const;

	void SetLobbyConnectedAt(const FDateTime& NewConnectedAtUtc);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_PartyInfo, BlueprintReadOnly, Category = "Lobby|Party")
	int32 PartyId = -1;

	UPROPERTY(ReplicatedUsing = OnRep_PartyInfo, BlueprintReadOnly, Category = "Lobby|Party")
	bool bIsPartyLeader = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Player")
	FString LobbyConnectedAt;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Player")
	int64 LobbyConnectedUnixTimestamp = 0;
private:
	UFUNCTION()
	void OnRep_PartyInfo();

	void BroadcastPartyInfoChanged();
	
};
