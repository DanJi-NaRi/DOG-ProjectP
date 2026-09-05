// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CPP_LobbyGSB.generated.h"

class APlayerState;
class ACPP_LobbyPS;

/**
 * 
 */


UENUM(BlueprintType)
enum class ELobbyPartyConnectionState : uint8
{
    Offline,
    Online,
    InGame,
    OutGame
};

UENUM(BlueprintType)
enum class ELobbyPartyState : uint8
{
    Lobby,
    Dungeon
};


USTRUCT(BlueprintType)
struct FLobbyPartyMemberInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 UserIndex = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Player")
    FString Username;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    ELobbyPartyConnectionState ConnectionState = ELobbyPartyConnectionState::Offline;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    bool bIsReady = false;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 SelectedCharacterId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Player")
    TObjectPtr<APlayerState> PlayerState = nullptr;
};


// 파티 구조체
USTRUCT(BlueprintType)
struct FLobbyPartyInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 LeaderUserIndex = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    ELobbyPartyState PartyState = ELobbyPartyState::Lobby;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Dungeon")
    FString CurrentDungeonSessionId;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TArray<FLobbyPartyMemberInfo> Members; // 파티 멤버 배열
};


USTRUCT(BlueprintType)
struct FLobbyOnlineUserInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Player")
    FString Username;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    bool bIsPartyLeader = false;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Player")
    TObjectPtr<APlayerState> PlayerState = nullptr;
};

USTRUCT(BlueprintType)
struct FLobbyPartyInviteInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TObjectPtr<APlayerState> InviterPlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TObjectPtr<APlayerState> TargetPlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    float SentServerTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FLobbyPartyJoinRequestInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TObjectPtr<APlayerState> ApplicantPlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TObjectPtr<APlayerState> LeaderPlayerState = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    float SentServerTime = 0.0f;
};



UCLASS()
class PROJECTP_API ACPP_LobbyGSB : public AGameStateBase
{
	GENERATED_BODY()
	
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    const TArray<FLobbyPartyInfo>& GetParties() const;

    UFUNCTION(BlueprintCallable, Category = "Lobby|Player")
    void GetOnlineUsers(TArray<FLobbyOnlineUserInfo>& OutOnlineUsers) const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    const TArray<FLobbyPartyInviteInfo>& GetPendingPartyInvites() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    const TArray<FLobbyPartyJoinRequestInfo>& GetPendingPartyJoinRequests() const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Party")
    int32 GetPartyMemberCount(int32 PartyId) const;

    int32 AddParty(APlayerState* LeaderPlayerState);

    bool AddPlayerToParty(APlayerState* JoiningPlayerState, int32 PartyId, int32 MaxPartyMemberCount);

    bool RemovePlayerFromParty(APlayerState* LeavingPlayerState);

    bool SetPartyMemberConnectionState(APlayerState* MemberPlayerState, ELobbyPartyConnectionState NewConnectionState);

    bool GetPartyMemberConnectionState(APlayerState* MemberPlayerState, ELobbyPartyConnectionState& OutConnectionState) const;

    bool SetPartyMembersConnectionState(int32 PartyId, ELobbyPartyConnectionState NewConnectionState);

    bool SetPartyDungeonSession(int32 PartyId, const FString& NewDungeonSessionId);

    bool ClearPartyDungeonSession(int32 PartyId);

    bool RestorePartyMemberOnline(APlayerState* MemberPlayerState);

    bool SetPartyMemberSelectedCharacter(APlayerState* MemberPlayerState, int32 NewSelectedCharacterId);

    bool GetPartyMemberSelectedCharacter(APlayerState* MemberPlayerState, int32& OutSelectedCharacterId) const;

    bool SetPartyMemberReady(APlayerState* MemberPlayerState, bool bNewIsReady);

    UFUNCTION(BlueprintPure, Category = "Lobby|Dungeon")
    bool CanPartyEnterDungeon(int32 PartyId, int32 RequiredMemberCount) const;

    bool ClearPartyReadyState(int32 PartyId);

    bool AddPartyInvite(APlayerState* InviterPlayerState, APlayerState* TargetPlayerState, int32 PartyId, float CurrentServerTime, float CooldownSeconds, float& OutRemainingCooldownSeconds);

    bool HasPendingPartyInvite(APlayerState* TargetPlayerState, int32 PartyId) const;

    bool RemovePendingPartyInvites(APlayerState* TargetPlayerState, int32 PartyId);

    bool AddPartyJoinRequest(APlayerState* ApplicantPlayerState, APlayerState* LeaderPlayerState, int32 PartyId, float CurrentServerTime, float CooldownSeconds, float& OutRemainingCooldownSeconds);

    bool HasPendingPartyJoinRequest(APlayerState* ApplicantPlayerState, int32 PartyId) const;

    bool RemovePendingPartyJoinRequests(APlayerState* ApplicantPlayerState, int32 PartyId);

protected:
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Party")
    TArray<FLobbyPartyInfo> Parties; // 파티 배열

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Party")
    TArray<FLobbyPartyInviteInfo> PendingPartyInvites;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Party")
    TArray<FLobbyPartyJoinRequestInfo> PendingPartyJoinRequests;

private:
    int32 GetStablePartyUserIndex(const ACPP_LobbyPS* LobbyPS) const;
    FLobbyPartyMemberInfo MakePartyMemberInfo(ACPP_LobbyPS* LobbyPS, ELobbyPartyConnectionState InitialConnectionState) const;
    FLobbyPartyMemberInfo* FindPartyMemberByUserIndex(FLobbyPartyInfo& PartyInfo, int32 UserIndex);
    const FLobbyPartyMemberInfo* FindPartyMemberByUserIndex(const FLobbyPartyInfo& PartyInfo, int32 UserIndex) const;
    void SyncPartyInfoToConnectedMembers(FLobbyPartyInfo& PartyInfo) const;
    bool TransferPartyLeaderToFirstConnectedMember(FLobbyPartyInfo& PartyInfo, ELobbyPartyConnectionState RequiredConnectionState);
    void RemovePendingPartyInvitesByPlayerState(APlayerState* PlayerState);
    void RemovePendingPartyJoinRequestsByPlayerState(APlayerState* PlayerState);
    bool IsValidSelectedCharacterId(int32 SelectedCharacterId) const;
};
