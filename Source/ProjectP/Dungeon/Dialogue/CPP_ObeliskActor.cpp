////////////////////////////
//! \file CPP_ObeliskActor.cpp
//! \brief 오벨리스크 액터 구현 파일이다.
//! \editor 준혁 - 상호작용 상태 관리 설계 적용: 단일 Dialogue/DialogueScope를 Primary/Repeat 응답 정의로,
//!         bTriggerOnce/bTriggered를 InteractableComponent UsageMode/사용 기록으로 마이그레이션.
//!         선택지 검증을 사용자별 활성 세션(실제 전송한 Dialogue) 기준으로 강화.
#include "CPP_ObeliskActor.h"

#include "Components/StaticMeshComponent.h"
#include "Dungeon/DungeonPC.h"
#include "Dungeon/DungeonGS.h"
#include "Engine/World.h"
#include "GAS/MyPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Streaming/MyStreamingManagerComponent.h"

////////////////////////////
//! \author 준혁
//! \brief 오벨리스크 액터를 생성한다. 메시와 상호작용 컴포넌트를 붙인다.
//!        상호작용 상태(개폐·점유·사용 기록)가 클라이언트 프롬프트에 반영되도록 복제를 켠다.
ACPP_ObeliskActor::ACPP_ObeliskActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    SetRootComponent(MeshComponent);

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 상호작용 엔트리의 표시 이름을 InteractableComponent 옵션 목록으로 동기화하는 처리 추가
//! \brief 서버 상호작용 승인/종료 이벤트에 응답 처리와 세션 정리를 바인딩하고,
//!        가이드 UI가 읽을 옵션 목록을 등록한다. (에디터 배치 데이터라 서버/클라 양쪽에서 동일하게 구성)
void ACPP_ObeliskActor::BeginPlay()
{
    Super::BeginPlay();

    if (InteractableComponent)
    {
        InteractableComponent->OnInteractionStarted.AddUniqueDynamic(this, &ACPP_ObeliskActor::HandleInteractionStarted);
        InteractableComponent->OnInteractionEnded.AddUniqueDynamic(this, &ACPP_ObeliskActor::HandleInteractionEnded);

        // 엔트리 표시 이름·사용 제한·선행 옵션 조건을 옵션 목록으로 등록한다. 데이터 원본은 이 액터 한 곳이다.
        TArray<FInteractionOption> Options;
        Options.Reserve(InteractionEntries.Num());
        for (const FObeliskInteractionEntry& Entry : InteractionEntries)
        {
            FInteractionOption& Option = Options.AddDefaulted_GetRef();
            Option.DisplayText = Entry.DisplayText;
            Option.UsageMode = Entry.UsageMode;
            Option.PrerequisiteOptionIndex = Entry.PrerequisiteOptionIndex;
            Option.bPrerequisitePerPlayer = Entry.bPrerequisitePerPlayer;
        }
        InteractableComponent->SetInteractionOptions(Options);
    }
}

////////////////////////////
//! \author 장효제
//! \brief 오벨리스크 종료 전에 남아 있는 서버 Dialogue 세션을 SmallTalk 차단 원본에서 제거한다.
//! \param EndPlayReason Actor 종료 이유다.
void ACPP_ObeliskActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		ClearStreamingDialogueSessions();
	}
	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 승인 Context 기반 응답 선택·세션 기록으로 재작성 (기존 단일 Dialogue 전송을 대체)
//! \brief [서버] 상호작용 승인 시 응답을 선택해 범위에 맞는 클라이언트들에 대화 시작 RPC를 보내는 함수.
//!        응답 Dialogue가 비어 있으면 점유를 복구(Abort)하고 RPC를 보내지 않는다.
//!        트리거 타이밍이 '상호작용 즉시'면 기믹 트리거를 래치한다.
//! \param Context 서버가 승인한 시작 Context
void ACPP_ObeliskActor::HandleInteractionStarted(const FInteractionStartContext& Context)
{
    if (!HasAuthority())
    {
        return;
    }

    // 옵션 인덱스는 InteractableComponent가 승인 시점에 이미 검증했다. 여기서 어긋나면 배치/동기화 오류다.
    if (!InteractionEntries.IsValidIndex(Context.SelectedOptionIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("[Obelisk] 상호작용 엔트리 미지정/인덱스 불일치 - 상호작용을 중단 복구한다. Obelisk: %s, OptionIndex: %d, Entries: %d"),
            *GetNameSafe(this), Context.SelectedOptionIndex, InteractionEntries.Num());

        if (InteractableComponent)
        {
            InteractableComponent->AbortInteraction(Context.Interactor);
        }
        return;
    }

    const FObeliskInteractionEntry& Entry = InteractionEntries[Context.SelectedOptionIndex];
    const FObeliskResponseDefinition& Response = SelectResponse(Entry, Context);

    if (Response.Dialogue.IsNull())
    {
        // 응답 미지정은 배치 오류다. 점유를 복구해 영구 Busy를 막고 RPC를 보내지 않는다.
        UE_LOG(LogTemp, Error, TEXT("[Obelisk] 응답 Dialogue 미지정 - 상호작용을 중단 복구한다. Obelisk: %s, FirstGlobal: %d, FirstForUser: %d"),
            *GetNameSafe(this), Context.bFirstGlobalInteraction ? 1 : 0, Context.bFirstForInteractor ? 1 : 0);

        if (InteractableComponent)
        {
            InteractableComponent->AbortInteraction(Context.Interactor);
        }
        return;
    }

    // 선택지 검증 기준이 되는 "실제 전송한 Dialogue"를 사용자 세션에 기록한다.
    if (Context.InteractorUserId >= 0)
    {
        FObeliskDialogueSession& Session = ActiveDialogueSessions.FindOrAdd(Context.InteractorUserId);
        Session.Interactor = Context.Interactor;
        Session.SentDialogue = Response.Dialogue;
		if (UMyStreamingManagerComponent* StreamingManager = GetStreamingManager())
		{
			StreamingManager->NotifyStoryDialogueStarted(this, Context.InteractorUserId);
		}
    }
    else
    {
        // 인증 ID가 없으면 선택지 세션을 만들 수 없다. 선택지 타이밍 기믹은 이 사용자로는 발동되지 않는다.
        UE_LOG(LogTemp, Warning, TEXT("[Obelisk] 인증 사용자 ID 없음 - 선택지 세션 미기록. Obelisk: %s, Interactor: %s"),
            *GetNameSafe(this), *GetNameSafe(Context.Interactor));
    }

    SendDialogueToTargets(Response, Context.Interactor);

    // 상호작용 즉시 타이밍에서는 엔트리별 플래그가 켜진 옵션만 기믹을 발동한다.
    if (GimmickTriggerTiming == EObeliskGimmickTriggerTiming::OnInteract && Entry.bTriggersGimmickOnInteract)
    {
        TriggerGimmick(Context.Interactor);
    }

    // 대화 시작 시점의 서버 연출 훅. (기믹 활성화 연결은 OnGimmickTriggered로 이관됨)
    OnDialogueTriggered(Context.Interactor);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 상호작용 종료(일반 종료·중단 공통) 시 해당 사용자의 활성 대화 세션을 정리하는 함수
//! \param Interactor 상호작용을 종료한 플레이어 폰
void ACPP_ObeliskActor::HandleInteractionEnded(AActor* Interactor)
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 UserId = ExtractUserId(Interactor);
    if (UserId >= 0)
    {
		if (ActiveDialogueSessions.Remove(UserId) > 0)
		{
			if (UMyStreamingManagerComponent* StreamingManager = GetStreamingManager())
			{
				StreamingManager->NotifyStoryDialogueEnded(this, UserId);
			}
		}
    }

    // 접속 종료 등으로 Interactor가 무효가 된 세션도 함께 정리한다.
    for (auto It = ActiveDialogueSessions.CreateIterator(); It; ++It)
    {
        if (!It->Value.Interactor.IsValid())
        {
			if (UMyStreamingManagerComponent* StreamingManager = GetStreamingManager())
			{
				StreamingManager->NotifyStoryDialogueEnded(this, It.Key());
			}
            It.RemoveCurrent();
        }
    }
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 최초 판정을 액터 단위 플래그에서 옵션별 플래그로 변경.
//!         다른 옵션을 먼저 사용했어도 이 옵션이 처음이면 Primary가 나온다.
//! \brief 선택된 엔트리 안에서 승인 Context(옵션별 최초 전체/최초 사용자)를 바탕으로 기본/반복 응답을 선택하는 함수.
//!        서버가 선택하며 클라이언트는 최초 여부를 다시 계산하지 않는다.
//! \param Entry 승인된 옵션 인덱스에 대응하는 상호작용 엔트리
//! \param Context 서버가 승인한 시작 Context
//! \return 선택된 응답 정의
const FObeliskResponseDefinition& ACPP_ObeliskActor::SelectResponse(const FObeliskInteractionEntry& Entry, const FInteractionStartContext& Context)
{
    switch (Entry.ResponseRule)
    {
    case EObeliskResponseRule::FirstGlobalThenRepeat:
        return Context.bFirstGlobalForOption ? Entry.PrimaryResponse : Entry.RepeatResponse;
    case EObeliskResponseRule::FirstPerPlayerThenRepeat:
        return Context.bFirstForInteractorForOption ? Entry.PrimaryResponse : Entry.RepeatResponse;
    case EObeliskResponseRule::AlwaysPrimary:
    default:
        return Entry.PrimaryResponse;
    }
}

////////////////////////////
//! \author 준혁
//! \brief 선택된 응답 범위(개인/파티)에 맞는 클라이언트들에 대화 시작 RPC를 보내는 함수
//! \param Response 선택된 응답 정의
//! \param Interactor 상호작용한 플레이어 폰
void ACPP_ObeliskActor::SendDialogueToTargets(const FObeliskResponseDefinition& Response, AActor* Interactor)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (Response.DialogueScope == EMyDialogueScope::Party)
    {
        // 던전 인스턴스 = 파티 하나이므로 접속 중인 모든 PC가 곧 파티 전원이다.
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(It->Get()))
            {
                const AMyPlayerState* TargetPlayerState = DungeonPC->GetPlayerState<AMyPlayerState>();
                if (!TargetPlayerState || TargetPlayerState->IsDead())
                {
                    continue;
                }

                DungeonPC->ClientStartDialogue(Response.Dialogue, this);
            }
        }
    }
    else
    {
        const APawn* InteractorPawn = Cast<APawn>(Interactor);
        ADungeonPC* DungeonPC = InteractorPawn ? InteractorPawn->GetController<ADungeonPC>() : nullptr;
        if (DungeonPC)
        {
            DungeonPC->ClientStartDialogue(Response.Dialogue, this);
        }
    }
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 기믹 트리거를 래치하고 구독자(Zone 어댑터)에 1회 발화하는 함수. 중복 호출은 무시된다.
//! \param InInstigator 트리거를 유발한 액터(상호작용/선택지 플레이어 폰, BP 수동 호출 시 임의)
void ACPP_ObeliskActor::TriggerGimmick(AActor* InInstigator)
{
    if (!HasAuthority() || bGimmickTriggered)
    {
        return;
    }

    bGimmickTriggered = true;
    OnGimmickTriggered.Broadcast(InInstigator);
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - Zone 리셋 시 활성 대화 세션도 함께 정리하도록 확장
//! \brief [서버] Zone 재시작 등 재도전을 위해 기믹 트리거 래치와 활성 대화 세션을 되돌리는 함수
void ACPP_ObeliskActor::ResetGimmickTrigger()
{
    if (!HasAuthority())
    {
        return;
    }

    bGimmickTriggered = false;
	ClearStreamingDialogueSessions();
    ActiveDialogueSessions.Reset();
}

////////////////////////////
//! \author 장효제
//! \brief 현재 Dungeon GameState가 소유한 Streaming Manager를 반환한다.
//! \return 서버 Streaming Manager이며 없으면 nullptr이다.
UMyStreamingManagerComponent* ACPP_ObeliskActor::GetStreamingManager() const
{
	const UWorld* World = GetWorld();
	const ADungeonGS* DungeonGS = World ? World->GetGameState<ADungeonGS>() : nullptr;
	return DungeonGS ? DungeonGS->GetStreamingManager() : nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief 이 오벨리스크가 소유한 모든 서버 Dialogue 세션의 SmallTalk 차단을 제거한다.
void ACPP_ObeliskActor::ClearStreamingDialogueSessions()
{
	if (UMyStreamingManagerComponent* StreamingManager = GetStreamingManager())
	{
		for (const TPair<int32, FObeliskDialogueSession>& Pair : ActiveDialogueSessions)
		{
			StreamingManager->NotifyStoryDialogueEnded(this, Pair.Key);
		}
	}
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 오벨리스크 기본 에셋 기준 검증을 사용자별 활성 세션(실제 전송한 Dialogue) 기준으로 강화.
//!         세션이 없는 비점유자(파티 대화 수신자 포함)의 선택은 게임 상태를 바꾸지 못한다.
//! \brief [서버] 클라이언트가 고른 대화 선택지를 검증하고, 기믹 트리거 선택지면 래치하는 함수.
//! \param InInstigator 선택지를 고른 플레이어 폰
//! \param LineIndex 선택지가 있던 대화 줄 인덱스
//! \param ChoiceIndex 고른 선택지 인덱스
void ACPP_ObeliskActor::NotifyDialogueChoiceOnServer(AActor* InInstigator, int32 LineIndex, int32 ChoiceIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 UserId = ExtractUserId(InInstigator);
    if (UserId < 0)
    {
        return;
    }

    // 활성 세션이 없는 사용자(비점유자, 파티 대화 수신자)는 거절한다.
    const FObeliskDialogueSession* Session = ActiveDialogueSessions.Find(UserId);
    if (!Session || Session->Interactor.Get() != InInstigator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Obelisk] 선택지 거절 - 활성 세션 없음(비점유자). Obelisk: %s, Instigator: %s"),
            *GetNameSafe(this), *GetNameSafe(InInstigator));
        return;
    }

    // 실제 전송한 Dialogue를 기준으로 인덱스를 검증한다. (클라이언트 입력을 신뢰하지 않는다)
    const UMyDialogueDataAsset* DialogueData = Session->SentDialogue.IsNull() ? nullptr : Session->SentDialogue.LoadSynchronous();
    if (!DialogueData || !DialogueData->Lines.IsValidIndex(LineIndex))
    {
        return;
    }

    const TArray<FMyDialogueChoice>& Choices = DialogueData->Lines[LineIndex].Choices;
    if (!Choices.IsValidIndex(ChoiceIndex))
    {
        return;
    }

    const FMyDialogueChoice& Choice = Choices[ChoiceIndex];
    if (Choice.bStartsGimmickResetVote)
    {
        OnGimmickResetVoteRequested.Broadcast(InInstigator);
        return;
    }

    if (GimmickTriggerTiming == EObeliskGimmickTriggerTiming::OnDialogueChoice && Choice.bTriggersGimmick)
    {
        TriggerGimmick(InInstigator);
    }
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 클라이언트의 대화 완주(마지막 줄까지 진행) 통지를 검증하고 기믹 트리거를 래치하는 함수.
//!        대화 진행 자체는 클라이언트 로컬이라 서버가 완주 여부를 재현할 수 없으므로,
//!        활성 세션(실제 대화를 전송받은 상호작용자)인지 검증하는 것으로 신뢰 범위를 제한한다(선택지 검증과 동일 수준).
//! \param InInstigator 대화를 끝까지 본 플레이어 폰
void ACPP_ObeliskActor::NotifyDialogueCompletedOnServer(AActor* InInstigator)
{
    if (!HasAuthority() || GimmickTriggerTiming != EObeliskGimmickTriggerTiming::OnDialogueCompleted)
    {
        return;
    }

    const int32 UserId = ExtractUserId(InInstigator);
    if (UserId < 0)
    {
        return;
    }

    // 활성 세션이 없는 사용자(비점유자, 파티 대화 수신자)는 거절한다. (선택지 검증과 동일 정책)
    const FObeliskDialogueSession* Session = ActiveDialogueSessions.Find(UserId);
    if (!Session || Session->Interactor.Get() != InInstigator)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Obelisk] 완주 통지 거절 - 활성 세션 없음(비점유자). Obelisk: %s, Instigator: %s"),
            *GetNameSafe(this), *GetNameSafe(InInstigator));
        return;
    }

    TriggerGimmick(InInstigator);
}

////////////////////////////
//! \author 준혁
//! \brief Interactor의 인증 사용자 ID를 해석하는 함수.
//!        인증이 없는 PIE 테스트에서는 PlayerId로 폴백한다(InteractableComponent와 동일 규칙 유지 필수 —
//!        세션 키가 컴포넌트 Context의 InteractorUserId와 일치해야 선택지 검증이 통과한다).
//! \param Interactor 상호작용 플레이어 폰
//! \return 인증 사용자 ID, 없으면 INDEX_NONE
int32 ACPP_ObeliskActor::ExtractUserId(const AActor* Interactor)
{
    const APawn* InteractorPawn = Cast<APawn>(Interactor);
    const AMyPlayerState* InteractorPS = InteractorPawn ? InteractorPawn->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!InteractorPS)
    {
        return INDEX_NONE;
    }

    const int32 UserId = InteractorPS->GetUserIndex();
    return (UserId > 0) ? UserId : InteractorPS->GetPlayerId();
}
