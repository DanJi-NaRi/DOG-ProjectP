////////////////////////////
//! \file CPP_ClearComponent_GimmickSolved.cpp
//! \brief 기믹 클리어를 Zone 클리어 조건으로 연결하는 어댑터 구현 파일이다.
//! \author 준혁
#include "CPP_ClearComponent_GimmickSolved.h"
#include "../../Dungeon/CPP_DungeonGM.h"
#include "../../Dungeon/DungeonPC.h"
#include "../../Dungeon/Dialogue/CPP_ObeliskActor.h"
#include "../../Dungeon/Gimmick/CPP_GimmickBase.h"
#include "../../Dungeon/Interactable/Components/InteractableComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "MyGameplayTags.h"
#include "Streaming/MyStreamingGimmickTypes.h"

////////////////////////////
//! \author 준혁
//! \brief Zone Active 전에는 상호작용이 불가능하도록 TriggerActor를 잠그는 함수
void UCPP_ClearComponent_GimmickSolved::BeginPlay()
{
	Super::BeginPlay();

	if (HasZoneAuthority() && TriggerActor)
	{
		SetTriggerInteractionEnabled(false);
	}
}

////////////////////////////
//! \author 준혁
//! \brief Zone Active 시 기믹 감시를 시작하는 함수. TriggerActor가 지정되어 있으면 기믹을 바로 켜지 않고
//!        상호작용만 개방하며(파티원 전원 진입 -> 오벨리스크 개방), 미지정이면 기존처럼 즉시 활성화한다.
void UCPP_ClearComponent_GimmickSolved::ActivateClearCondition_Implementation()
{
	// 부모가 감시 활성화 + (이미 충족 시) 재방송을 처리한다.
	Super::ActivateClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	if (!TargetGimmick)
	{
		// [임시 주석] 테스트 이후 삭제 예정 - 레벨 배치 실수를 바로 알 수 있게 경고
		UE_LOG(LogTemp, Warning, TEXT("[ZoneGimmick] TargetGimmick 미지정 - Zone: %s. 이 Zone은 클리어될 수 없다."), *GetNameSafe(GetOwner()));
		return;
	}

	TargetGimmick->OnGimmickStateChanged.AddUniqueDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleGimmickStateChanged);

	if (TriggerActor)
	{
		UInteractableComponent* TriggerInteractable = GetTriggerInteractable();
		if (TriggerInteractable)
		{
			if (ACPP_ObeliskActor* Obelisk = Cast<ACPP_ObeliskActor>(TriggerActor.Get()))
			{
				// 오벨리스크는 트리거 시점(상호작용 즉시/대화 선택지/수동)을 자체 판단해 래치 후 1회 발화한다.
				Obelisk->OnGimmickTriggered.AddUniqueDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleTriggerActivated);
                Obelisk->OnGimmickResetVoteRequested.AddUniqueDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleGimmickResetVoteRequested);

				// 구독 전에 이미 래치된 경우(BP 수동 호출, 설정 실수로 조기 개방 등) 놓친 발화를 보정한다.
				if (Obelisk->IsGimmickTriggered())
				{
					ActivateGimmick();
				}
			}
			else
			{
				// 일반 상호작용 액터는 승인된 상호작용 시작 즉시를 트리거로 간주한다.
				TriggerInteractable->OnInteractionStarted.AddUniqueDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleTriggerInteractionStarted);
			}

			TriggerInteractable->SetInteractionEnabled(true);
			return;
		}

		// 설정 실수가 진행 조건을 우회하는 Fail Open이 되지 않도록, 즉시 활성화 폴백은 사용하지 않는다.
		// 상호작용은 비활성으로 유지되고 기믹도 활성화되지 않는다. (이 Zone은 배치 수정 전까지 클리어 불가)
		UE_LOG(LogTemp, Error, TEXT("[ZoneGimmick] TriggerActor에 InteractableComponent가 없음 - Zone: %s, TriggerActor: %s. 기믹을 활성화하지 않는다."),
			*GetNameSafe(GetOwner()), *GetNameSafe(TriggerActor));
		return;
	}

	ActivateGimmick();
}

////////////////////////////
//! \author 준혁
//! \brief Zone Clear 등 감시 종료 시 TriggerActor 상호작용을 닫는 함수. 기믹은 건드리지 않는다.
//!        (기믹 Deactivate는 보상 Revert를 유발하므로 여기서 호출하면 안 된다)
void UCPP_ClearComponent_GimmickSolved::DeactivateClearCondition_Implementation()
{
	Super::DeactivateClearCondition_Implementation();

    if (!HasZoneAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (ACPP_DungeonGM* DungeonGM = World->GetAuthGameMode<ACPP_DungeonGM>())
        {
            DungeonGM->CancelGimmickResetVote(this);
        }
    }

    if (TargetGimmick)
	{
		TargetGimmick->SetElementInteractionsEnabled(false);
	}

	UnbindTrigger();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 상호작용 점유·사용 기록 초기화(ResetInteractionState) 추가
//! \brief Zone 재시작 등 리셋 시 기믹을 리셋하고 TriggerActor 상호작용을 다시 잠그는 함수.
//!        오벨리스크의 기믹 트리거 래치와 상호작용 점유·사용 기록도 함께 되돌려 재도전이 가능하게 한다.
void UCPP_ClearComponent_GimmickSolved::ResetClearCondition_Implementation()
{
	Super::ResetClearCondition_Implementation();

	if (!HasZoneAuthority())
	{
		return;
	}

	if (TargetGimmick)
	{
		TargetGimmick->Deactivate();
		TargetGimmick->ResetGimmick();
	}

	UnbindTrigger();

	if (UInteractableComponent* TriggerInteractable = GetTriggerInteractable())
	{
		TriggerInteractable->ResetInteractionState();
	}

	if (ACPP_ObeliskActor* Obelisk = Cast<ACPP_ObeliskActor>(TriggerActor.Get()))
	{
		Obelisk->ResetGimmickTrigger();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 기믹이 Solved가 되면 Zone에 클리어를 통보하는 함수
//! \param NewState 변경된 기믹 상태
void UCPP_ClearComponent_GimmickSolved::HandleGimmickStateChanged(EGimmickState /*OldState*/, EGimmickState NewState)
{
	if (NewState != EGimmickState::Solved)
	{
		return;
	}

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[ZoneGimmick] 기믹 Solved -> Zone 클리어 통보 - Zone: %s, Gimmick: %s"),
		*GetNameSafe(GetOwner()), *GetNameSafe(TargetGimmick));

	// 감시 비활성/이미 클리어 상태 처리는 부모(MarkClearSatisfied)가 래치로 보장한다.
	MarkClearSatisfied();
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 오벨리스크의 기믹 트리거 발화 시 기믹을 활성화하는 함수. 재발화는 Activate 내부 가드로 무시된다.
//! \param InInstigator 트리거를 유발한 액터 (미사용, 시그니처 일치용)
void UCPP_ClearComponent_GimmickSolved::HandleTriggerActivated(AActor* /*InInstigator*/)
{
	if (!HasZoneAuthority() || !IsClearConditionActive() || !TargetGimmick)
	{
		return;
	}

	ActivateGimmick();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크 대화의 초기화 선택을 현재 Zone 대상의 공용 만장일치 투표로 연결하는 함수
// InInstigator : 기믹 초기화 투표를 요청한 플레이어 액터
void UCPP_ClearComponent_GimmickSolved::HandleGimmickResetVoteRequested(AActor* InInstigator)
{
    if (!HasZoneAuthority() || !IsClearConditionActive() || !TargetGimmick ||
        TargetGimmick->GetGimmickState() != EGimmickState::Active)
	{
		return;
	}

    UWorld* World = GetWorld();
    APawn* RequestingPawn = Cast<APawn>(InInstigator);
    ADungeonPC* RequestingController = RequestingPawn
        ? Cast<ADungeonPC>(RequestingPawn->GetController())
        : Cast<ADungeonPC>(InInstigator);
    ACPP_DungeonGM* DungeonGM = World ? World->GetAuthGameMode<ACPP_DungeonGM>() : nullptr;
    if (!World || !RequestingController || !DungeonGM)
    {
        return;
    }

    const FSimpleDelegate VotePassedDelegate = FSimpleDelegate::CreateUObject(
        this,
        &UCPP_ClearComponent_GimmickSolved::HandleGimmickResetVotePassed);

    // 충돌 안내가 쿨타임 안내보다 우선되어야 하므로 진행 중 투표는 GameMode에서 먼저 거절시킨다.
    if (DungeonGM->IsPartyVoteInProgress())
    {
        DungeonGM->StartGimmickResetVote(RequestingController, this, VotePassedDelegate);
        return;
    }

    const float CurrentServerTime = World->GetTimeSeconds();
    if (CurrentServerTime < GimmickResetCooldownEndServerTime)
    {
        const int32 RemainingCooldownSeconds = FMath::Max(
            0,
            FMath::CeilToInt(GimmickResetCooldownEndServerTime - CurrentServerTime));
        RequestingController->SendNoticeToClient(
            FText::FromString(FString::Printf(TEXT("기믹 초기화 쿨타임이 %d초 남아 있습니다."), RemainingCooldownSeconds)),
            0.0f);
        return;
    }

    DungeonGM->StartGimmickResetVote(RequestingController, this, VotePassedDelegate);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 초기화 투표 만장일치 가결 후 대상 상태를 재검증하고 실제 초기화와 10초 쿨타임을 시작하는 함수
void UCPP_ClearComponent_GimmickSolved::HandleGimmickResetVotePassed()
{
    UWorld* World = GetWorld();
    if (!World || !HasZoneAuthority() || !IsClearConditionActive() || !TargetGimmick ||
        TargetGimmick->GetGimmickState() != EGimmickState::Active)
    {
        return;
    }

    GimmickResetCooldownEndServerTime = World->GetTimeSeconds() + FMath::Max(GimmickResetCooldownSeconds, 0.0f);
    TargetGimmick->ResetGimmick();
	++PartyResetCount;

	const FName GimmickId = TargetGimmick->GetGimmickId();
	if (GimmickId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[ZoneGimmick] 파티 초기화 Streaming Fact 미발행 - GimmickId가 비어 있음. Gimmick=%s PartyResetCount=%d"),
			*GetNameSafe(TargetGimmick),
			PartyResetCount);
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(this))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[ZoneGimmick] 파티 초기화 Streaming Fact 미발행 - GameplayMessageSubsystem 없음. GimmickId=%s PartyResetCount=%d"),
			*GimmickId.ToString(),
			PartyResetCount);
		return;
	}

	FMyStreamingGimmickResetPayload Payload;
	Payload.GimmickId = GimmickId;
	Payload.PartyResetCount = PartyResetCount;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_Gimmick,
		Payload);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 일반 상호작용 액터의 승인된 상호작용 시작 시 기믹을 활성화하는 함수
//! \param Context 서버가 승인한 시작 Context (미사용, 시그니처 일치용)
void UCPP_ClearComponent_GimmickSolved::HandleTriggerInteractionStarted(const FInteractionStartContext& /*Context*/)
{
	if (!HasZoneAuthority() || !IsClearConditionActive() || !TargetGimmick)
	{
		return;
	}

	ActivateGimmick();
}

////////////////////////////
//! \author 준혁
//! \brief TriggerActor의 트리거 구독을 해제하고 상호작용을 잠그는 함수. 감시 종료/리셋 공통 정리.
void UCPP_ClearComponent_GimmickSolved::UnbindTrigger()
{
	if (UInteractableComponent* TriggerInteractable = GetTriggerInteractable())
	{
		TriggerInteractable->OnInteractionStarted.RemoveDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleTriggerInteractionStarted);
		TriggerInteractable->SetInteractionEnabled(false);
	}

	if (ACPP_ObeliskActor* Obelisk = Cast<ACPP_ObeliskActor>(TriggerActor.Get()))
	{
		Obelisk->OnGimmickTriggered.RemoveDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleTriggerActivated);
        Obelisk->OnGimmickResetVoteRequested.RemoveDynamic(this, &UCPP_ClearComponent_GimmickSolved::HandleGimmickResetVoteRequested);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 기믹을 활성화하고, 활성화 즉시 Solved가 된 경우(발판 위에 이미 인원 충족 등) 바로 클리어를 반영하는 함수
void UCPP_ClearComponent_GimmickSolved::ActivateGimmick()
{
	if (!TargetGimmick)
	{
		return;
	}

	TargetGimmick->Activate();

	if (TargetGimmick->IsSolved())
	{
		MarkClearSatisfied();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 소유 Zone이 서버 권한을 가지고 있는지 확인하는 함수
//! \return 서버 권한이 있으면 true
bool UCPP_ClearComponent_GimmickSolved::HasZoneAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

////////////////////////////
//! \author 준혁
//! \brief TriggerActor의 InteractableComponent를 얻는 함수
//! \return InteractableComponent, 없으면 nullptr
UInteractableComponent* UCPP_ClearComponent_GimmickSolved::GetTriggerInteractable() const
{
	return IsValid(TriggerActor) ? TriggerActor->FindComponentByClass<UInteractableComponent>() : nullptr;
}

////////////////////////////
//! \author 준혁
//! \brief TriggerActor의 상호작용 가능 상태를 설정하는 함수
//! \param bEnabled 상호작용 가능 여부
void UCPP_ClearComponent_GimmickSolved::SetTriggerInteractionEnabled(bool bEnabled) const
{
	if (UInteractableComponent* TriggerInteractable = GetTriggerInteractable())
	{
		TriggerInteractable->SetInteractionEnabled(bEnabled);
	}
}
