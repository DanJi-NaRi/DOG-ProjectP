#pragma once

#include "CoreMinimal.h"
#include "../Lobby/CPP_LobbyGSB.h"
#include "LobbyPartyUIDefs.generated.h"

class APlayerState;

UENUM(BlueprintType)
enum class ELobbyPartyPopupRequestType : uint8
{
    PartyInvite,
    PartyJoinRequest
};

USTRUCT(BlueprintType)
struct FLobbyReceivedPartyInviteInfo
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    FString InviterName;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 CurrentPartyMemberCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 MaxPartyMemberCount = 3;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    ELobbyPartyPopupRequestType RequestType = ELobbyPartyPopupRequestType::PartyInvite;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    TObjectPtr<APlayerState> ApplicantPlayerState = nullptr;
};

UENUM(BlueprintType)
enum class ELobbyPartyRowMode : uint8
{
    PartyMember,
    InviteTarget,
    PartyJoinTarget
};

USTRUCT(BlueprintType)
struct FLobbyPartyMemberRowData
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    FString ConnectionStateText;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    ELobbyPartyConnectionState ConnectionState = ELobbyPartyConnectionState::Offline;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    bool bIsReady = false;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    bool bIsPartyLeader = false;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 SelectedCharacterId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Player")
    TObjectPtr<APlayerState> PlayerState = nullptr;
};

USTRUCT(BlueprintType)
struct FLobbyJoinablePartyEntry
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 PartyId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 CurrentMemberCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    int32 MaxMemberCount = 3;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    bool bCanRequestJoin = false;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Party")
    float RemainingCooldownSeconds = 0.0f;
};
