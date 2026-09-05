////////////////////////////
//! \file CPP_GimmickBase.cpp
//! \brief 기믹 상태머신 베이스 구현 파일이다.
//! \editor 준혁 - 활성/평가/클리어/되돌림 검증용 임시 로그 추가
//! \editor 준혁 - 클리어 시 요소 연출 고정(SolvedLock) 지시 추가
#include "CPP_GimmickBase.h"
#include "Streaming/MyStreamingPayloads.h"
#include "MyGameplayTags.h"
#include "CPP_GimmickCondition.h"
#include "CPP_GimmickReward.h"
#include "CPP_GimmickElementBase.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

ACPP_GimmickBase::ACPP_GimmickBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ACPP_GimmickBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_GimmickBase, CurrentState);
	DOREPLIFETIME(ACPP_GimmickBase, Progress);
}

void ACPP_GimmickBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoActivateForStandaloneTest)
	{
		Activate();
	}
}

void ACPP_GimmickBase::Activate()
{
	if (!HasAuthority() || CurrentState != EGimmickState::Inactive)
	{
		return;
	}

	SetElementInteractionsEnabled(true);
	SetState(EGimmickState::Active);

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[Gimmick] Activate - Id: %s"), *GimmickId.ToString());

	EvaluateOnServer();
}

void ACPP_GimmickBase::Deactivate()
{
	if (!HasAuthority() || CurrentState == EGimmickState::Inactive)
	{
		return;
	}

	SetElementInteractionsEnabled(false);

	if (CurrentState == EGimmickState::Solved)
	{
		SetElementsSolvedLock(false);
		RevertRewards();
	}

	SetProgress(0.0f);
	SetState(EGimmickState::Inactive);
}

void ACPP_GimmickBase::ResetGimmick()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bReactivateAfterReset = CurrentState != EGimmickState::Inactive;
	SetElementInteractionsEnabled(false);
	bResetInProgress = true;

	// 고정을 먼저 풀어야 ResetElement의 재동기화가 연출 상태까지 되돌린다.
	SetElementsSolvedLock(false);

	for (ACPP_GimmickElementBase* Element : Elements)
	{
		if (Element)
		{
			Element->ResetElement();
		}
	}

	if (CurrentState == EGimmickState::Solved)
	{
		RevertRewards();
	}

	OnReset();
	SetProgress(0.0f);

	if (bReactivateAfterReset)
	{
		SetState(EGimmickState::Active);
	}

	bResetInProgress = false;
	SetElementInteractionsEnabled(bReactivateAfterReset);

	if (bReactivateAfterReset)
	{
		EvaluateOnServer();
	}
}

void ACPP_GimmickBase::RegisterElement(ACPP_GimmickElementBase* Element)
{
	if (!HasAuthority() || !Element)
	{
		return;
	}

	Elements.AddUnique(Element);
	Element->SetGimmickInteractionEnabled(bElementInteractionsEnabled);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 등록된 모든 기믹 요소의 상호작용 상태를 일괄 변경하는 함수
// bEnabled : 요소 상호작용 활성 여부
void ACPP_GimmickBase::SetElementInteractionsEnabled(bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bElementInteractionsEnabled = bEnabled;

	for (ACPP_GimmickElementBase* Element : Elements)
	{
		if (Element)
		{
			Element->SetGimmickInteractionEnabled(bEnabled);
		}
	}
}

void ACPP_GimmickBase::NotifyElementChanged(ACPP_GimmickElementBase* /*Element*/)
{
	if (!HasAuthority() || bResetInProgress)
	{
		return;
	}

	EvaluateOnServer();
}

void ACPP_GimmickBase::EvaluateOnServer()
{
	if (!HasAuthority())
	{
		return;
	}

	// [임시 주석] 테스트 이후 삭제 예정 - Zone(또는 bAutoActivateForStandaloneTest)이 Activate를 안 부르면 여기서 막힘
	if (CurrentState == EGimmickState::Inactive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gimmick] 평가 스킵 - 아직 Activate 안 됨(Inactive). Id: %s (bAutoActivateForStandaloneTest 확인)"), *GimmickId.ToString());
		return;
	}

	SetProgress(ComputeProgress());

	const bool bMet = AreClearConditionsMet();

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[Gimmick] 평가 - Id: %s, 진행도: %.2f, 조건충족: %s"),
		*GimmickId.ToString(), Progress, bMet ? TEXT("YES") : TEXT("NO"));

	if (bMet && CurrentState == EGimmickState::Active)
	{
		EnterSolved();
	}
	else if (!bMet && !bLatchOnSolve && CurrentState == EGimmickState::Solved)
	{
		RevertToActive();
	}
}

bool ACPP_GimmickBase::AreClearConditionsMet() const
{
	// 조건이 하나도 없으면 자동 클리어를 막는다.
	if (ClearConditions.Num() == 0)
	{
		return false;
	}

	for (const UCPP_GimmickCondition* Condition : ClearConditions)
	{
		if (!Condition || !Condition->Evaluate(this))
		{
			return false;
		}
	}
	return true;
}

float ACPP_GimmickBase::ComputeProgress() const
{
	int32 Count = 0;
	float Sum = 0.0f;
	for (const UCPP_GimmickCondition* Condition : ClearConditions)
	{
		if (Condition)
		{
			Sum += Condition->GetProgress(this);
			++Count;
		}
	}
	return Count > 0 ? Sum / static_cast<float>(Count) : 0.0f;
}

void ACPP_GimmickBase::EnterSolved()
{
	SetState(EGimmickState::Solved);

	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Warning, TEXT("[Gimmick] ===== 클리어(Solved) ===== Id: %s, 보상 %d개 실행"),
		*GimmickId.ToString(), Rewards.Num());

	// 요소 연출을 클리어 시점 상태로 고정한 뒤 보상을 실행한다.
	SetElementsSolvedLock(true);
	ExecuteRewards();
}

void ACPP_GimmickBase::RevertToActive()
{
	// [임시 주석] 테스트 이후 삭제 예정
	UE_LOG(LogTemp, Log, TEXT("[Gimmick] 되돌림(Solved -> Active) - Id: %s"), *GimmickId.ToString());

	SetElementsSolvedLock(false);
	RevertRewards();
	SetState(EGimmickState::Active);
}

void ACPP_GimmickBase::SetElementsSolvedLock(bool bLocked)
{
	for (ACPP_GimmickElementBase* Element : Elements)
	{
		if (Element)
		{
			Element->SetSolvedLock(bLocked);
		}
	}
}

void ACPP_GimmickBase::ExecuteRewards()
{
	for (UCPP_GimmickReward* Reward : Rewards)
	{
		if (Reward)
		{
			Reward->Execute(this);
		}
	}
}

void ACPP_GimmickBase::RevertRewards()
{
	for (UCPP_GimmickReward* Reward : Rewards)
	{
		if (Reward)
		{
			Reward->Revert(this);
		}
	}
}

void ACPP_GimmickBase::SetState(EGimmickState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	const EGimmickState OldState = CurrentState;
	CurrentState = NewState;

	// 스트리밍 상태 조건이 "기믹이 켜져 있는 동안"을 잴 수 있게 켜짐·꺼짐을 알린다.
	if (HasAuthority())
	{
		const bool bWasActive = OldState == EGimmickState::Active;
		const bool bIsActive = NewState == EGimmickState::Active;
		if (bWasActive != bIsActive)
		{
			MyStreamingState::BroadcastState(
				this, MyGameplayTags::Streaming_State_Gimmick_Active, bIsActive);
		}
	}

	// 서버(및 리슨 서버 호스트)에서 즉시 반영. 원격 클라이언트는 OnRep_State가 처리한다.
	OnGimmickStateChanged.Broadcast(OldState, NewState);
	DispatchStateFX(OldState, NewState);
}

void ACPP_GimmickBase::SetProgress(float NewProgress)
{
	NewProgress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Progress, NewProgress))
	{
		return;
	}

	Progress = NewProgress;
	OnProgressChanged(Progress);
}

void ACPP_GimmickBase::DispatchStateFX(EGimmickState OldState, EGimmickState NewState)
{
	switch (NewState)
	{
	case EGimmickState::Active:
		if (OldState == EGimmickState::Solved)
		{
			OnRevert();
		}
		else
		{
			OnActivated();
		}
		break;

	case EGimmickState::Solved:
		OnSolved();
		break;

	case EGimmickState::Inactive:
		OnDeactivated();
		break;

	default:
		break;
	}
}

void ACPP_GimmickBase::OnRep_State(EGimmickState OldState)
{
	OnGimmickStateChanged.Broadcast(OldState, CurrentState);
	DispatchStateFX(OldState, CurrentState);
}

void ACPP_GimmickBase::OnRep_Progress()
{
	OnProgressChanged(Progress);
}
