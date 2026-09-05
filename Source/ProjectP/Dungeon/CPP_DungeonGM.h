#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "GameFramework/GameModeBase.h"
#include "DungeonGS.h"
#include "DungeonReconnectTypes.h"
#include "CPP_DungeonGM.generated.h"

class AController;
class APawn;
class APlayerController;
class APlayerStart;
class APlayerState;
class AMyPlayerState;
class APlayerCharacterBase;
class ACPP_EnemyBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMyAttributeSet;
enum class EPlayerLifeState : uint8;

USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonInitialPlayerData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0"))
    int32 Meso = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "1"))
    int32 CharacterLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0"))
    int32 CharacterExp = 0;

    //! 0이면 레벨 스탯을 적용한 뒤 계산된 최대 체력으로 초기화한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0.0"))
    float Health = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0.0"))
    float Shield = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0.0"))
    float MoveCharge = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dungeon|Player Initialization", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float CurseGauge = 0.0f;
};

UCLASS()
class PROJECTP_API ACPP_DungeonGM : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACPP_DungeonGM();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Party")
    int32 GetCurrentPlayerCount() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Party")
    int32 GetRequiredPlayerCount() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Session")
    bool IsDungeonSessionActive() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Session")
    int32 GetInGameUserCount() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Session")
    int32 GetOutGameUserCount() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Session")
    FString GetRuntimeDungeonSessionId() const;

    float GetSurrenderVoteStartServerTime() const;

    bool RegisterAuthenticatedDungeonPlayer(APlayerController* PlayerController, const FString& LoginToken);

    bool TryInitializeDemoPlayer(APlayerController* PlayerController);

    bool StartSurrenderVote(APlayerController* RequestingController);

    bool StartGimmickResetVote(
        APlayerController* RequestingController,
        UObject* VoteContext,
        const FSimpleDelegate& OnVotePassed);

    bool SubmitSurrenderVote(APlayerController* VotingController, bool bAgree);

    void CancelGimmickResetVote(UObject* VoteContext);

    bool IsPartyVoteInProgress() const { return bSurrenderVoteInProgress; }

    //! 서버에서 데이터 옵션을 검증하고 비용을 즉시 차감한 뒤 지연 부활을 시작한다.
    bool StartPlayerRevive(APlayerController* RequestingController, FName OptionId, FString& OutResultMessage);

    void SpawnTestEnemiesForStressTest(int32 Count);

    // 치트/테스트 스폰에서 공용으로 쓰는 기본 적 클래스와 1회 스폰 상한. (BP_DungeonGM 디폴트에서 지정)
    TSubclassOf<ACPP_EnemyBase> GetStressTestEnemyClass() const { return StressTestEnemyClass; }
    int32 GetMaxStressTestEnemySpawnCount() const { return MaxStressTestEnemySpawnCount; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Party", meta = (ClampMin = "1"))
    int32 RequiredPlayerCount = 3;

    //! 최초 던전 입장 플레이어에게 할당할 PlayerStart의 PlayerStartTag.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Spawn")
    FName DungeonEntryPlayerStartTag = TEXT("DungeonEntry");

    //! PlayerStart 주변에 다른 Pawn이 있을 때 점유된 위치로 판단할 최소 거리.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Spawn", meta = (ClampMin = "0.0"))
    float DungeonEntryMinimumPawnDistance = 150.0f;

    //! 선택 캐릭터 ID(100/200/300)별 스폰할 폰 클래스. BP_DungeonGM 디폴트에서 BP_Heru/BP_inpu/BP_Nefer 지정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Character")
    TMap<int32, TSoftClassPtr<APawn>> CharacterPawnClasses;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender", meta = (ClampMin = "1.0"))
    float SurrenderVoteDurationSeconds = 30.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender", meta = (ClampMin = "0.0"))
    float SurrenderVoteCooldownSeconds = 60.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Surrender", meta = (ClampMin = "0.0"))
    float ReturnToLobbyDelaySeconds = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Session", meta = (ClampMin = "0.0"))
    float EmptyDungeonShutdownDelaySeconds = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Reconnect", meta = (ClampMin = "0.0"))
    float DisconnectedPawnDisableDelaySeconds = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Player Initialization")
    FDungeonInitialPlayerData InitialPlayerData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Player Initialization")
    bool bAllowUnauthenticatedDemoInitialization = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Stress Test")
    TSubclassOf<ACPP_EnemyBase> StressTestEnemyClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Stress Test", meta = (ClampMin = "1", ClampMax = "5000"))
    int32 MaxStressTestEnemySpawnCount = 1000;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Stress Test", meta = (ClampMin = "100.0"))
    float StressTestEnemySpawnSpacing = 200.0f;

    //! 1인 테스트에서는 전멸 판정을 무시해 사망 후 개인 부활을 검증할 수 있게 한다. 실제 파티 전멸 규칙에는 영향이 없다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Revive|Testing")
    bool bAllowSoloReviveTesting = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Party")
    void OnDungeonPlayerCountChanged(int32 CurrentCount, int32 RequiredCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Party")
    void OnRequiredDungeonPlayersReady();

private:
    struct FPendingDungeonRevive
    {
        TWeakObjectPtr<APlayerCharacterBase> Character;
        FName OptionId;
        int32 ConsumedMeso = 0;
        float HealthPercent = 0.0f;
        TSubclassOf<UGameplayEffect> PostReviveEffectClass;
        FTimerHandle TimerHandle;
    };

    int32 CurrentPlayerCount = 0;

    bool bRequiredPlayerCountReached = false;

    bool bSurrenderVoteInProgress = false;

    EDungeonPartyVoteType ActivePartyVoteType = EDungeonPartyVoteType::None;

    bool bReturnToLobbyCountdownInProgress = false;

    bool bDungeonSessionActive = false;

    TSet<TWeakObjectPtr<APlayerState>> SurrenderAgreePlayerStates;

    TSet<int32> InGameUserIndexes;

    TSet<int32> OutGameUserIndexes;

    TSet<TWeakObjectPtr<APlayerState>> DemoInitializedPlayerStates;

    TMap<int32, TWeakObjectPtr<APawn>> DisconnectedPawns;

    TMap<int32, FTimerHandle> DisconnectedPawnDisableTimerHandles;

    TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedDungeonEntryPlayerStarts;

    TMap<TWeakObjectPtr<AMyPlayerState>, FPendingDungeonRevive> PendingDungeonRevives;

    int32 SurrenderDisagreeCount = 0;

    FTimerHandle SurrenderVoteTimerHandle;

    FTimerHandle ReturnToLobbyTimerHandle;

    FTimerHandle EmptyDungeonShutdownTimerHandle;

    float SurrenderVoteStartServerTime = 0.0f;

    float ReturnToLobbyServerTime = 0.0f;

    float LastSurrenderVoteFailedTime = -60.0f;

    TWeakObjectPtr<UObject> ActiveGimmickResetVoteContext;

    FSimpleDelegate GimmickResetVotePassedDelegate;

    void NotifyPlayerCountChanged();
    void CheckRequiredPlayersReady();
    bool IsDungeonEntryPlayerStartAvailable(const APlayerStart* PlayerStart, const AController* Player) const;
    FTransform MakeStressTestEnemySpawnTransform(int32 SpawnIndex) const;
    bool StoreDisconnectedPlayer(AController* Exiting);
    FDungeonAttributeSnapshot MakeDungeonAttributeSnapshot(const UMyAttributeSet* AttributeSet) const;
    TArray<FDungeonSkillCooldownSnapshot> MakeDungeonSkillCooldownSnapshot(const UAbilitySystemComponent* AbilitySystemComponent) const;
    bool ResolveCurrentDungeonStep(EDungeonReconnectStep& OutDungeonStep) const;
    bool RestoreDisconnectedPlayer(APlayerController* NewPlayer, int32 UserIndex, bool& bOutRestored);
    void EnsureSelectedCharacterPawn(APlayerController* PlayerController);
    bool InitializeNewPlayerData(APlayerController* PlayerController) const;
    APawn* ResolveReconnectPawn(APlayerController* NewPlayer, const FDungeonReconnectSnapshot& Snapshot, const FTransform& SpawnTransform);
    bool ResolveReconnectSpawnTransform(APlayerController* NewPlayer, const FDungeonReconnectSnapshot& Snapshot, FTransform& OutTransform);
    bool ApplyDungeonAttributeSnapshot(AMyPlayerState* MyPlayerState, const FDungeonAttributeSnapshot& Snapshot) const;
    bool ApplyDungeonSkillCooldownSnapshot(UAbilitySystemComponent* AbilitySystemComponent, const FDungeonReconnectSnapshot& Snapshot) const;
    void ClearDisconnectedPawnTracking(int32 UserIndex);
    void ScheduleDisconnectedPawnDisable(int32 UserIndex, APawn* Pawn);
    void DisableDisconnectedPawn(int32 UserIndex);
    void SetPawnReconnectInactive(APawn* Pawn, bool bInactive);
    bool MoveAuthenticatedPlayerToOutGame(APlayerState* ExitingPlayerState);
    void EndDungeonSessionIfNoInGameUsers();
    void ScheduleEmptyDungeonShutdown();
    void CancelEmptyDungeonShutdown();
    void HandleEmptyDungeonShutdownTimer();
    void CleanupDungeonSessionForShutdown();
    void RequestDungeonServerShutdown();
    void HandleDungeonServerShutdownResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void ReportDungeonMemberState(int32 UserIndex, const FString& ConnectionState);
    void HandleReportDungeonMemberStateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 UserIndex, FString ConnectionState);
    void BindPlayerLifeState(AMyPlayerState* MyPlayerState);
    void HandlePlayerLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState);
    bool AreAllRegisteredPlayersDead() const;
    void CompletePendingRevive(TWeakObjectPtr<AMyPlayerState> PlayerState);
    void CancelPendingRevive(AMyPlayerState* PlayerState);
    void CancelAllPendingRevives();
    void SendVoteNotice(APlayerController* TargetController, const FText& Message) const;
    void FailSurrenderVote(const FString& ReasonMessage);
    void CompleteActivePartyVote();
    void CompleteSurrenderVote();
    void HandleSurrenderVoteTimeout();
    void HandleReturnToLobbyCountdownFinished();
    void ResetSurrenderVote();
    void TravelAllPlayersToLobby();
    APlayerState* GetSurrenderVoterState(APlayerController* VotingController) const;
    int32 GetSurrenderAgreeCount() const;
    int32 GetSurrenderRequiredPlayerCount() const;
    void UpdateSurrenderVoteState(const FString& ResultMessage = TEXT(""));
};
