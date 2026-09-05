#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "GauntletLoginLobbySubsystem.generated.h"

class APlayerController;
class APlayerState;
class ACPP_LobbyGSB;
class ACPP_LobbyPC;
class ADungeonGS;
class ADungeonPC;
class ULoginRequestAsyncAction;
struct FLobbyPartyInfo;

struct FGauntletLoginLobbyAccountCredentials
{
    int32 ClientIndex = 0;
    FString ID;
    FString Password;
    FString Username;
};

enum class EGauntletPartyDungeonStep : uint8
{
    None,
    CreateParty,
    InviteClient2,
    AcceptInvite,
    RequestJoin,
    AcceptJoinRequest,
    SelectCharacter,
    SetReady,
    EnterDungeon,
    DungeonAuth,
    SurrenderVote,
    LobbyReturn
};

UCLASS()
class PROJECTP_API UGauntletLoginLobbySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool bIsRunning = false;
    int32 ClientIndex = 0;
    FString FailureCase;
    FString CredentialsFilePath;
    FString TestAuth;
    FGauntletLoginLobbyAccountCredentials AccountCredentials;
    TMap<int32, FGauntletLoginLobbyAccountCredentials> AllAccountCredentials;

    UPROPERTY()
    TObjectPtr<ULoginRequestAsyncAction> ActiveLoginRequest = nullptr;

    FHttpRequestPtr ActiveClearLoginTokenRequest;
    FTimerHandle StartTestTimerHandle;
    FTimerHandle LobbyAuthWaitTimerHandle;
    FTimerHandle PartyDungeonFlowTimerHandle;
    double LobbyAuthWaitDeadlineSeconds = 0.0;
    double PartyDungeonStepDeadlineSeconds = 0.0;
    bool bRunPartyDungeonFlow = false;
    bool bPartyDungeonStepActionSent = false;
    EGauntletPartyDungeonStep PartyDungeonStep = EGauntletPartyDungeonStep::None;

    void StartTestIfRequested();
    bool ReadCommandLine(FString& OutFailureReason);
    bool LoadCredentials(FString& OutFailureReason);
    bool ResolveCredentialsFilePath(FString& OutResolvedPath) const;
    bool ReadAccountCredentials(const TSharedPtr<FJsonObject>& RootObject, FString& OutFailureReason);
    void RunExpectedFailureLogin();

    UFUNCTION()
    void HandleExpectedFailureResult(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    UFUNCTION()
    void HandleExpectedFailureUnexpectedSuccess(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    void ClearDuplicateLoginToken();
    void HandleClearLoginTokenResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void RunSuccessLogin();

    UFUNCTION()
    void HandleSuccessLoginResult(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    UFUNCTION()
    void HandleSuccessLoginFailure(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    void TravelToLobby();
    void HandleLobbyTravelResult(bool bSuccess, const FString& Message);
    void WaitForLobbyAuthResult();
    void StartPartyDungeonFlowAfterLobbyAuth();
    void TickPartyDungeonFlow();
    void SetPartyDungeonStep(EGauntletPartyDungeonStep NewStep, double TimeoutSeconds);
    void HandlePartyDungeonCreateParty();
    void HandlePartyDungeonInviteClient2();
    void HandlePartyDungeonAcceptInvite();
    void HandlePartyDungeonRequestJoin();
    void HandlePartyDungeonAcceptJoinRequest();
    void HandlePartyDungeonSelectCharacter();
    void HandlePartyDungeonSetReady();
    void HandlePartyDungeonEnterDungeon();
    void HandlePartyDungeonAuth();
    void HandlePartyDungeonSurrenderVote();
    void HandlePartyDungeonLobbyReturn();
    bool TryGetTestPartyInfo(FLobbyPartyInfo& OutPartyInfo) const;
    bool IsTestPartyMember(const FLobbyPartyInfo& PartyInfo, int32 TargetClientIndex) const;
    bool IsFullTestParty(const FLobbyPartyInfo& PartyInfo) const;
    bool AreAllTestCharactersSelected(const FLobbyPartyInfo& PartyInfo) const;
    bool AreAllTestMembersReady(const FLobbyPartyInfo& PartyInfo) const;
    bool HasPendingInviteForClient(int32 TargetClientIndex, int32 PartyId) const;
    bool HasPendingJoinRequestForClient(int32 ApplicantClientIndex, int32 PartyId) const;
    bool DoesPlayerStateMatchClient(APlayerState* PlayerState, int32 TargetClientIndex) const;
    APlayerState* FindLobbyPlayerStateForClient(int32 TargetClientIndex) const;
    ACPP_LobbyGSB* GetLobbyGameState() const;
    ACPP_LobbyPC* GetLobbyPlayerController() const;
    ADungeonGS* GetDungeonGameState() const;
    ADungeonPC* GetDungeonPlayerController() const;
    int32 GetPartyDungeonCharacterIdForClient() const;
    const FGauntletLoginLobbyAccountCredentials* GetCredentialsForClient(int32 TargetClientIndex) const;
    void FinishPartyDungeonSuccess();
    void FinishPartyDungeonFailure(const FString& Reason);
    void FinishSuccess();
    void FinishFailure(const FString& Reason);
    void ClearActiveLoginRequest();
    void ClearLobbyTravelWait();
    void ClearPartyDungeonFlowWait();
    APlayerController* GetFirstLocalPlayerController() const;
    FString BuildClearLoginTokenUrl() const;
};
