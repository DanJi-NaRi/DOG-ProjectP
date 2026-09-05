#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dungeon/Interactable/Components/InteractableComponent.h"
#include "MyDialogueDataAsset.h"
#include "CPP_ObeliskActor.generated.h"

class UStaticMeshComponent;

//! \enum EObeliskGimmickTriggerTiming 기믹 트리거가 발동되는 시점
UENUM(BlueprintType)
enum class EObeliskGimmickTriggerTiming : uint8
{
    OnInteract          UMETA(DisplayName = "상호작용 즉시"),
    OnDialogueChoice    UMETA(DisplayName = "대화 선택지 선택 시"),
    OnDialogueCompleted UMETA(DisplayName = "대화를 끝까지 봤을 때"),
    Manual              UMETA(DisplayName = "수동 (BP에서 TriggerGimmick 호출)"),
};

//! \enum EObeliskResponseRule 최초/재사용 상황에 따른 응답 선택 규칙
UENUM(BlueprintType)
enum class EObeliskResponseRule : uint8
{
    AlwaysPrimary            UMETA(DisplayName = "항상 기본 응답"),
    FirstGlobalThenRepeat    UMETA(DisplayName = "전체 최초만 기본, 이후 반복 응답"),
    FirstPerPlayerThenRepeat UMETA(DisplayName = "사용자별 최초만 기본, 이후 반복 응답"),
};

//! \struct FObeliskResponseDefinition 오벨리스크가 상호작용에 내는 응답 하나(범위 + 대화)
USTRUCT(BlueprintType)
struct PROJECTP_API FObeliskResponseDefinition
{
    GENERATED_BODY()

    //! 대화 시작 범위: 개인(상호작용자만) / 파티 전원
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    EMyDialogueScope DialogueScope = EMyDialogueScope::Personal;

    //! 표시할 대화 데이터에셋
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    TSoftObjectPtr<UMyDialogueDataAsset> Dialogue;
};

//! \struct FObeliskInteractionEntry 상호작용 옵션 하나에 대응하는 응답 세트.
//!         가이드 UI에 DisplayText로 표시되고, 선택되면 ResponseRule에 따라 기본/반복 응답 중 하나가 전송된다.
USTRUCT(BlueprintType)
struct PROJECTP_API FObeliskInteractionEntry
{
    GENERATED_BODY()

    //! 상호작용 가이드 UI에 표시할 이름
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    FText DisplayText;

    //! 최초/재사용 상황에 따른 응답 선택 규칙. 최초 판정은 이 옵션 기준이다
    //! (다른 옵션을 먼저 사용했어도 이 옵션이 처음이면 최초로 판정).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    EObeliskResponseRule ResponseRule = EObeliskResponseRule::AlwaysPrimary;

    //! 기본 응답 (AlwaysPrimary면 항상, First~ 규칙이면 최초에 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    FObeliskResponseDefinition PrimaryResponse;

    //! 반복 응답 (First~ 규칙에서 최초가 아닌 경우 사용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    FObeliskResponseDefinition RepeatResponse;

    //! GimmickTriggerTiming이 '상호작용 즉시(OnInteract)'일 때 이 옵션 선택이 기믹 트리거를 래치하는지.
    //! (예: "기믹 발동" 옵션만 true, "대화만 보기" 옵션은 false) 다른 타이밍에서는 영향 없다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    bool bTriggersGimmickOnInteract = true;

    //! 이 옵션의 사용 제한. 전체 1회/플레이어별 1회면 소진 후 가이드 UI에서 사라진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    EInteractionUsageMode UsageMode = EInteractionUsageMode::Unlimited;

    //! 이 옵션이 표시되기 위한 선행 옵션(엔트리) 인덱스(-1=없음). 선행 옵션이 한 번 사용된 후에만 가이드에 나타난다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk", meta = (ClampMin = "-1"))
    int32 PrerequisiteOptionIndex = INDEX_NONE;

    //! true면 선행 옵션을 직접 사용한 플레이어에게만 표시, false면 파티 누구든 사용하면 전원에게 표시된다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk")
    bool bPrerequisitePerPlayer = false;
};

//! [서버] 기믹 트리거가 래치될 때 1회 발화한다. Zone의 ClearComponent_GimmickSolved가 구독한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FObeliskGimmickTriggeredSignature, AActor*, InInstigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FObeliskGimmickResetVoteRequestedSignature, AActor*, InInstigator);

////////////////////////////
//! \class ACPP_ObeliskActor
//! \brief F키 상호작용으로 대화를 시작하는 오벨리스크 액터. (상점 액터와 동일한 상호작용 골격)
//! \note 3인 멀티 기준: 상호작용 승인은 InteractableComponent(서버 권위 상태 관리자)가 하고,
//!       오벨리스크는 승인 Context(최초 전체/최초 사용자)를 바탕으로 응답(기본/반복, 개인/파티)을 선택해
//!       대화 시작 RPC를 보낸다. 이후 대화 진행(줄 넘기기)은 각 클라이언트 로컬이며 동기화하지 않는다.
//!       점유·사용 제한(독점, 전체 1회, 사용자별 1회)은 InteractableComponent 정책으로 설정한다.
//!       기믹 활성화는 서버 래치 플래그(bGimmickTriggered)로 관리한다: GimmickTriggerTiming에 따라
//!       상호작용 즉시 / 대화 선택지 선택 시 / 대화를 끝까지 봤을 때 / BP 수동 호출 시점에 TriggerGimmick이 래치되고,
//!       OnGimmickTriggered를 구독한 Zone의 ClearComponent_GimmickSolved가 기믹을 Activate한다.
//!       (대화 시스템은 기믹을 모른다) OnDialogueTriggered는 대화 시작 시점의 서버 연출 훅으로 남는다.
//!       선택지·완주 검증은 사용자별로 실제 전송한 Dialogue를 기록한 활성 세션을 기준으로 한다.
//!       파티 대화를 받은 비점유자의 선택·완주는 로컬 UI 진행에만 쓰이고 게임 상태를 바꾸지 못한다.
UCLASS()
class PROJECTP_API ACPP_ObeliskActor : public AActor
{
    GENERATED_BODY()

public:
    ACPP_ObeliskActor();

    //! [서버] 기믹 트리거를 래치한다(1회만 발화, 이후 호출 무시). Manual 타이밍에서는 BP가 직접 호출한다.
    UFUNCTION(BlueprintCallable, Category = "Obelisk|Gimmick")
    void TriggerGimmick(AActor* InInstigator);

    //! [서버] Zone 재시작 등으로 재도전할 수 있게 트리거 래치와 활성 대화 세션을 되돌린다.
    void ResetGimmickTrigger();

    //! [서버] 기믹 트리거가 이미 래치됐는지 반환한다. 늦게 구독한 Zone 어댑터가 놓친 발화를 보정할 때 사용한다.
    bool IsGimmickTriggered() const { return bGimmickTriggered; }

    //! [서버] 클라이언트가 고른 대화 선택지를 검증하고, 기믹 트리거 선택지면 래치한다. DungeonPC 서버 RPC가 호출한다.
    //!        요청 사용자의 활성 세션(실제 전송한 Dialogue)을 기준으로 검증하며, 세션이 없는 비점유자는 거절된다.
    void NotifyDialogueChoiceOnServer(AActor* InInstigator, int32 LineIndex, int32 ChoiceIndex);

    //! [서버] 클라이언트의 대화 완주(마지막 줄까지 진행) 통지를 검증하고 기믹 트리거를 래치한다. DungeonPC 서버 RPC가 호출한다.
    //!        선택지와 동일하게 활성 세션 보유자(상호작용자)만 수락하며, 세션이 없는 비점유자는 거절된다.
    void NotifyDialogueCompletedOnServer(AActor* InInstigator);

    //! 기믹 트리거 발동 시점을 반환한다. 클라이언트가 완주 통지 RPC를 보낼지 판단할 때 사용한다(서버가 재검증한다).
    EObeliskGimmickTriggerTiming GetGimmickTriggerTiming() const { return GimmickTriggerTiming; }

    //! [서버] 기믹 트리거 래치 시 1회 발화. Zone의 ClearComponent_GimmickSolved가 구독한다.
    UPROPERTY(BlueprintAssignable, Category = "Obelisk|Gimmick")
    FObeliskGimmickTriggeredSignature OnGimmickTriggered;

    //! [서버] 대화에서 기믹 초기화 투표 선택지가 선택되면 발화한다. Zone 어댑터가 공용 투표를 요청한다.
    UPROPERTY(BlueprintAssignable, Category = "Obelisk|Gimmick")
    FObeliskGimmickResetVoteRequestedSignature OnGimmickResetVoteRequested;

protected:
    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Obelisk")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Obelisk")
    TObjectPtr<UInteractableComponent> InteractableComponent;

    //! 상호작용 옵션별 응답 세트 (레벨 인스턴스별 지정). 배열 순서 = 가이드 UI 표시 순서 = 옵션 인덱스.
    //! BeginPlay에서 DisplayText들이 InteractableComponent 옵션 목록으로 동기화된다.
    //! (기존 단일 ResponseRule/PrimaryResponse/RepeatResponse를 대체 — 배치 인스턴스는 에디터에서 재설정 필요)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk|Response")
    TArray<FObeliskInteractionEntry> InteractionEntries;

    //! 기믹 트리거 발동 시점. 레벨 인스턴스별로 상황에 맞게 지정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obelisk|Gimmick")
    EObeliskGimmickTriggerTiming GimmickTriggerTiming = EObeliskGimmickTriggerTiming::OnInteract;

    //! [서버] 대화가 시작될 때 발화하는 연출 훅. (기믹 활성화 연결은 OnGimmickTriggered로 이관됨)
    UFUNCTION(BlueprintImplementableEvent, Category = "Obelisk")
    void OnDialogueTriggered(AActor* Interactor);

private:
    //! [서버] 사용자별 활성 대화 세션. 선택지 검증 기준이 되는 "실제 전송한 Dialogue"를 보관한다.
    struct FObeliskDialogueSession
    {
        TWeakObjectPtr<AActor> Interactor;
        TSoftObjectPtr<UMyDialogueDataAsset> SentDialogue;
    };

    UFUNCTION()
    void HandleInteractionStarted(const FInteractionStartContext& Context);

    UFUNCTION()
    void HandleInteractionEnded(AActor* Interactor);

    //! 선택된 엔트리 안에서 승인 Context(최초 전체/최초 사용자)를 바탕으로 기본/반복 응답을 선택한다.
    static const FObeliskResponseDefinition& SelectResponse(const FObeliskInteractionEntry& Entry, const FInteractionStartContext& Context);

    //! 선택된 응답 범위에 맞는 대상들에게 대화 시작 RPC를 보낸다.
    void SendDialogueToTargets(const FObeliskResponseDefinition& Response, AActor* Interactor);

    //! Interactor의 인증 사용자 ID를 해석한다.
    static int32 ExtractUserId(const AActor* Interactor);
	class UMyStreamingManagerComponent* GetStreamingManager() const;
	void ClearStreamingDialogueSessions();

    //! [서버] 사용자 ID -> 활성 대화 세션
    TMap<int32, FObeliskDialogueSession> ActiveDialogueSessions;

    //! [서버] 기믹 트리거 래치. 한 번 true가 되면 ResetGimmickTrigger 전까지 재발동하지 않는다.
    bool bGimmickTriggered = false;
};
