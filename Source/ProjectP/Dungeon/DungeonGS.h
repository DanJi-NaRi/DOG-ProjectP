#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Streaming/MyMissionTypes.h"
#include "DungeonGS.generated.h"

class UMyStreamingManagerComponent;
class UDungeonReviveDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMissionViewsChangedSignature);

UENUM(BlueprintType)
enum class EDungeonPartyVoteType : uint8
{
    None UMETA(DisplayName = "없음"),
    Surrender UMETA(DisplayName = "방송 종료"),
    GimmickReset UMETA(DisplayName = "기믹 초기화"),
};

USTRUCT(BlueprintType)
struct FDungeonSurrenderVoteState
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Vote")
    EDungeonPartyVoteType VoteType = EDungeonPartyVoteType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    bool bVoteInProgress = false;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    int32 AgreeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    int32 DisagreeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    int32 RequiredCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    float VoteStartServerTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    float VoteEndServerTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    float CooldownEndServerTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    float LobbyTravelServerTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Surrender")
    FString ResultMessage;
};

UCLASS()
class PROJECTP_API ADungeonGS : public AGameStateBase
{
    GENERATED_BODY()

public:
    ADungeonGS();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Surrender")
    const FDungeonSurrenderVoteState& GetSurrenderVoteState() const;

    void SetSurrenderVoteState(const FDungeonSurrenderVoteState& NewState);

    //! \brief 던전 GS가 소유한 Streaming Manager Component를 반환한다. (서버/클라 공통 접근용)
    UMyStreamingManagerComponent* GetStreamingManager() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Revive")
    UDungeonReviveDataAsset* GetReviveData() const;

    UFUNCTION(BlueprintPure, Category = "Dungeon|Mission")
    TArray<FMyMissionPublicView> GetMissionViews() const;

    void SetMissionViews(const TArray<FMyMissionPublicView>& NewMissionViews);

    UPROPERTY(BlueprintAssignable, Category = "Dungeon|Mission")
    FOnMissionViewsChangedSignature OnMissionViewsChanged;

protected:
    // 스트리밍매니저는 던전GS가 갖는다
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Streaming", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UMyStreamingManagerComponent> StreamingManagerComponent;

    //! 서버 검증과 향후 클라이언트 부활 UI가 함께 읽는 정적 옵션 데이터다.
    //! BP_DungeonGS 디폴트에서 프로젝트용 ReviveDataAsset을 지정한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Revive")
    TObjectPtr<UDungeonReviveDataAsset> ReviveData;

    UPROPERTY(ReplicatedUsing = OnRep_SurrenderVoteState, BlueprintReadOnly, Category = "Dungeon|Surrender")
    FDungeonSurrenderVoteState SurrenderVoteState;

    UFUNCTION()
    void OnRep_SurrenderVoteState();

    UFUNCTION(BlueprintImplementableEvent, Category = "Dungeon|Surrender")
    void OnSurrenderVoteStateChanged();

    UPROPERTY(ReplicatedUsing = OnRep_MissionViews, BlueprintReadOnly, Category = "Dungeon|Mission")
    TArray<FMyMissionPublicView> MissionViews;

    UFUNCTION()
    void OnRep_MissionViews();
};
