////////////////////////////
//! \file InteractableComponent.cpp
//! \brief 상호작용 대상 컴포넌트의 서버 권위 상태 관리(활성 Gate·점유·사용 기록·원자적 승인)를 구현한다.
//! \editor 준혁 - 상호작용 상태 관리 설계(AI_Docs/InteractionStateManagementDesign.md) 적용
#include "InteractableComponent.h"

#include "GAS/MyPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

////////////////////////////
//! \author HanUl
//! \editor 준혁 - bInteractionEnabled 복제를 위해 켰던 컴포넌트 복제를 상태·기록 복제로 확장
//! \brief 상호작용 대상 컴포넌트를 생성한다. 상태 판정과 이벤트 중계만 하므로 Tick을 사용하지 않는다.
//! \param 없음
//! \return 없음
UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	DefaultInteractionText = NSLOCTEXT("Interaction", "DefaultInteractionText", "상호작용");
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 상호작용 세션의 해제 정책을 설정하는 함수
// InReleaseMode : 새로 적용할 상호작용 해제 정책
void UInteractableComponent::SetReleaseMode(EInteractionReleaseMode InReleaseMode)
{
	ReleaseMode = InReleaseMode;
}

////////////////////////////
//! \author 준혁
//! \brief 가이드 UI에 표시할 유효 옵션 목록을 반환하는 함수. 등록 옵션이 없으면
//!        DefaultInteractionText로 기본 옵션 1개를 합성해 모든 상호작용 액터에서 가이드가 뜨게 한다.
//! \return 유효 옵션 목록 (항상 1개 이상)
TArray<FInteractionOption> UInteractableComponent::GetEffectiveInteractionOptions() const
{
	if (!InteractionOptions.IsEmpty())
	{
		return InteractionOptions;
	}

	TArray<FInteractionOption> DefaultOptions;
	FInteractionOption& DefaultOption = DefaultOptions.AddDefaulted_GetRef();
	DefaultOption.DisplayText = DefaultInteractionText;
	return DefaultOptions;
}

////////////////////////////
//! \author 준혁
//! \brief 복제 프로퍼티를 등록한다. 상태와 사용자 기록만 클라이언트 프롬프트 판정용으로 복제한다.
//! \param OutLifetimeProps 복제 프로퍼티 목록
//! \return 없음
void UInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInteractableComponent, CurrentState);
	DOREPLIFETIME(UInteractableComponent, UsedUserIds);
	DOREPLIFETIME(UInteractableComponent, UsedOptionIndices);
	DOREPLIFETIME(UInteractableComponent, OptionUseRecords);
	DOREPLIFETIME(UInteractableComponent, InteractionCooldownRecords);
}

////////////////////////////
//! \author 준혁
//! \brief 서버에서 초기 Gate를 적용하고, 미지원 정책 조합과 소유 액터 복제 누락을 검증하는 함수
void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasOwnerAuthority())
	{
		// Shared + Manual은 미지원 조합이다. 자동 종료 정책으로 처리하고 배치 실수를 알린다.
		if (ConcurrencyMode == EInteractionConcurrencyMode::Shared && ReleaseMode == EInteractionReleaseMode::Manual)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Interactable] Shared + Manual은 미지원 조합 - OnInteractEnd로 동작한다. Owner: %s"),
				*GetNameSafe(GetOwner()));
		}

		// 복제 상태가 필요한데 소유 액터가 복제되지 않으면 클라이언트 프롬프트가 서버 상태와 어긋난다.
		if (GetOwner() && !GetOwner()->GetIsReplicated())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Interactable] 소유 액터가 복제되지 않음 - 클라이언트 프롬프트가 상태를 반영하지 못한다. Owner: %s"),
				*GetNameSafe(GetOwner()));
		}

		// 외부(Zone 어댑터 등)가 BeginPlay 순서상 먼저 SetInteractionEnabled를 호출했다면 그 값을 유지한다.
		if (!bGateExplicitlySet)
		{
			bInteractionGateOpen = bStartInteractionEnabled;
		}
		RecomputeState();
	}
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 단일 bool 판정을 복제 상태·사용 제한 기반 판정으로 확장
//! \brief 해당 Interactor가 지금 상호작용할 수 있는지 판정한다.
//!        클라이언트에서는 복제 상태 기반의 프롬프트 사전 판정이며 최종 승인이 아니다.
//! \param Interactor 상호작용을 시도하는 액터
//! \return 상호작용 가능 여부
bool UInteractableComponent::CanInteract(const AActor* Interactor) const
{
	if (!IsValid(Interactor) || !IsValid(GetOwner()))
	{
		return false;
	}

	if (CurrentState != EInteractableState::Ready)
	{
		return false;
	}

	const int32 UserId = ResolveInteractorUserId(Interactor);
	if (InteractionCooldown > 0.0f
		&& (UserId < 0 || IsUserInteractionCooldownActive(UserId)))
	{
		return false;
	}

	// OncePerPlayer는 공용 상태를 Consumed로 만들지 않고 기록된 사용자만 거절한다.
	if (UsageMode == EInteractionUsageMode::OncePerPlayer)
	{
		if (UserId >= 0 && UsedUserIds.Contains(UserId))
		{
			return false;
		}
	}

	// 등록된 옵션이 전부 소진된 액터는 후보에서 제외한다. (가이드/프롬프트 미표시)
	if (!InteractionOptions.IsEmpty())
	{
		bool bAnyOptionAvailable = false;
		for (int32 OptionIndex = 0; OptionIndex < InteractionOptions.Num(); ++OptionIndex)
		{
			if (IsOptionAvailable(OptionIndex, Interactor))
			{
				bAnyOptionAvailable = true;
				break;
			}
		}

		if (!bAnyOptionAvailable)
		{
			return false;
		}
	}

	return true;
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 선행 옵션(언락) 조건 판정 추가
//! \brief 해당 Interactor가 지금 이 옵션을 선택할 수 있는지(잠기지 않았고 소진되지 않았는지) 판정하는 함수.
//!        복제된 사용 기록 기반이라 클라이언트 가이드 UI 필터링에도 사용된다.
//! \param OptionIndex 판정할 옵션 인덱스
//! \param Interactor 상호작용을 시도하는 액터 (플레이어별 판정에 사용)
//! \return 선택 가능 여부
bool UInteractableComponent::IsOptionAvailable(int32 OptionIndex, const AActor* Interactor) const
{
	// 옵션 미등록 액터는 합성 기본 옵션(0번)만 존재하며 잠금/소진 개념이 없다.
	if (InteractionOptions.IsEmpty())
	{
		return OptionIndex == 0;
	}

	if (!InteractionOptions.IsValidIndex(OptionIndex))
	{
		return false;
	}

	const int32 UserId = ResolveInteractorUserId(Interactor);

	// 선행 옵션이 아직 사용되지 않은 옵션은 잠겨 있다.
	if (!IsOptionPrerequisiteMet(OptionIndex, UserId))
	{
		return false;
	}

	switch (InteractionOptions[OptionIndex].UsageMode)
	{
	case EInteractionUsageMode::OnceGlobal:
		return !UsedOptionIndices.Contains(OptionIndex);
	case EInteractionUsageMode::OncePerPlayer:
		return UserId < 0 || !HasUserUsedOption(OptionIndex, UserId);
	case EInteractionUsageMode::Unlimited:
	default:
		return true;
	}
}

////////////////////////////
//! \author 준혁
//! \brief 이 옵션의 선행 옵션 조건이 충족됐는지 반환하는 함수.
//!        미지정(-1)이나 잘못된 인덱스(범위 밖/자기 자신)는 잠금 없이 취급한다.
//! \param OptionIndex 판정할 옵션 인덱스
//! \param UserId 상호작용자의 인증 사용자 ID (플레이어별 선행 조건 판정에 사용)
//! \return 선행 조건 충족 여부
bool UInteractableComponent::IsOptionPrerequisiteMet(int32 OptionIndex, int32 UserId) const
{
	const FInteractionOption& Option = InteractionOptions[OptionIndex];
	const int32 PrereqIndex = Option.PrerequisiteOptionIndex;

	if (PrereqIndex == INDEX_NONE || PrereqIndex == OptionIndex || !InteractionOptions.IsValidIndex(PrereqIndex))
	{
		return true;
	}

	if (Option.bPrerequisitePerPlayer)
	{
		return UserId >= 0 && HasUserUsedOption(PrereqIndex, UserId);
	}

	return UsedOptionIndices.Contains(PrereqIndex);
}

////////////////////////////
//! \author 준혁
//! \brief 소유 액터가 등록한 옵션 목록을 저장하고 선행 옵션 인덱스의 유효성을 검증하는 함수.
//!        잘못된 선행 인덱스(범위 밖/자기 자신)는 배치 오류이므로 경고를 남긴다(잠금 없이 동작).
//! \param InOptions 등록할 옵션 목록
void UInteractableComponent::SetInteractionOptions(const TArray<FInteractionOption>& InOptions)
{
	InteractionOptions = InOptions;

	for (int32 OptionIndex = 0; OptionIndex < InteractionOptions.Num(); ++OptionIndex)
	{
		const int32 PrereqIndex = InteractionOptions[OptionIndex].PrerequisiteOptionIndex;
		if (PrereqIndex != INDEX_NONE && (PrereqIndex == OptionIndex || !InteractionOptions.IsValidIndex(PrereqIndex)))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Interactable] 옵션 %d의 선행 옵션 인덱스 %d가 잘못됨(범위 밖/자기 자신) - 잠금 없이 동작한다. Owner: %s"),
				OptionIndex, PrereqIndex, *GetNameSafe(GetOwner()));
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 해당 사용자가 이 옵션을 사용한 기록이 있는지 반환하는 함수
//! \param OptionIndex 옵션 인덱스
//! \param UserId 인증 사용자 ID
//! \return 사용 기록 존재 여부
bool UInteractableComponent::HasUserUsedOption(int32 OptionIndex, int32 UserId) const
{
	return OptionUseRecords.ContainsByPredicate([OptionIndex, UserId](const FInteractionOptionUseRecord& Record)
	{
		return Record.OptionIndex == OptionIndex && Record.UserId == UserId;
	});
}

////////////////////////////
//! \author 준혁
//! \editor 준혁
//! \brief [서버] 상호작용 시작을 원자적으로 승인하는 함수. 검사와 상태 변경(점유 획득·사용 기록)을
//!        한 함수 안에서 끝내 같은 프레임에 도착한 두 독점 요청 중 하나만 점유를 얻는다.
//! \param Interactor 요청한 플레이어 액터
//! \param OutContext 승인 성공 시 채워지는 시작 Context
//! \param OutRejectReason 실패 사유 (성공 시 None)
//! \param OptionIndex 요청한 상호작용 옵션 인덱스. 옵션 목록이 비어 있으면 0만 허용된다.
//! \return 승인 여부
bool UInteractableComponent::TryBeginInteraction(AActor* Interactor, FInteractionStartContext& OutContext, EInteractionRejectReason& OutRejectReason, int32 OptionIndex)
{
	OutRejectReason = EInteractionRejectReason::None;

	if (!HasOwnerAuthority() || !IsValid(Interactor))
	{
		OutRejectReason = EInteractionRejectReason::InvalidInteractor;
		return false;
	}

	// 옵션 인덱스는 클라이언트 입력이므로 서버에서 범위를 검증한다.
	const bool bValidOption = InteractionOptions.IsEmpty() ? (OptionIndex == 0) : InteractionOptions.IsValidIndex(OptionIndex);
	if (!bValidOption)
	{
		OutRejectReason = EInteractionRejectReason::InvalidOption;
		return false;
	}

	// 접속 종료 등으로 종료 이벤트를 못 받은 무효 점유/활성 참조를 먼저 정리한다.
	PruneInvalidReferences();

	const int32 UserId = ResolveInteractorUserId(Interactor);
	PruneExpiredInteractionCooldowns();

	if (!bInteractionGateOpen)
	{
		OutRejectReason = EInteractionRejectReason::Disabled;
		return false;
	}

	if (ConcurrencyMode == EInteractionConcurrencyMode::Exclusive && ExclusiveOccupant.IsValid())
	{
		OutRejectReason = EInteractionRejectReason::Busy;
		return false;
	}

	// 같은 플레이어의 중복 시작(연타/레이턴시)은 점유와 무관하게 거절한다.
	if (IsInteractorActive(Interactor))
	{
		OutRejectReason = EInteractionRejectReason::Busy;
		return false;
	}

	if (InteractionCooldown > 0.0f)
	{
		if (UserId < 0)
		{
			OutRejectReason = EInteractionRejectReason::MissingUserId;
			return false;
		}

		if (IsUserInteractionCooldownActive(UserId))
		{
			OutRejectReason = EInteractionRejectReason::Cooldown;
			return false;
		}
	}

	if (UsageMode == EInteractionUsageMode::OnceGlobal && bAnyInteractionOccurred)
	{
		OutRejectReason = EInteractionRejectReason::ConsumedGlobal;
		return false;
	}

	if (UsageMode == EInteractionUsageMode::OncePerPlayer)
	{
		if (UserId < 0)
		{
			OutRejectReason = EInteractionRejectReason::MissingUserId;
			return false;
		}

		if (UsedUserIds.Contains(UserId))
		{
			OutRejectReason = EInteractionRejectReason::ConsumedForPlayer;
			return false;
		}
	}

	// 옵션 단위 잠금/사용 제한: 잠기거나 소진된 옵션 요청은 거절한다. (클라 가이드는 숨기지만 레이턴시로 도착할 수 있다)
	if (!InteractionOptions.IsEmpty())
	{
		if (!IsOptionPrerequisiteMet(OptionIndex, UserId))
		{
			OutRejectReason = EInteractionRejectReason::OptionLocked;
			return false;
		}

		const EInteractionUsageMode OptionUsageMode = InteractionOptions[OptionIndex].UsageMode;

		if (OptionUsageMode == EInteractionUsageMode::OnceGlobal && UsedOptionIndices.Contains(OptionIndex))
		{
			OutRejectReason = EInteractionRejectReason::ConsumedGlobal;
			return false;
		}

		if (OptionUsageMode == EInteractionUsageMode::OncePerPlayer)
		{
			if (UserId < 0)
			{
				OutRejectReason = EInteractionRejectReason::MissingUserId;
				return false;
			}

			if (HasUserUsedOption(OptionIndex, UserId))
			{
				OutRejectReason = EInteractionRejectReason::ConsumedForPlayer;
				return false;
			}
		}
	}

	// 최초 여부는 기록 갱신 전에 계산한다.
	const bool bFirstGlobal = !bAnyInteractionOccurred;
	const bool bFirstForUser = (UserId >= 0) && !UsedUserIds.Contains(UserId);

	// 옵션별 최초 여부도 함께 계산한다. (다른 옵션을 먼저 썼어도 이 옵션이 처음이면 최초로 판정)
	const bool bFirstGlobalForOption = !UsedOptionIndices.Contains(OptionIndex);
	const bool bFirstForUserOption = (UserId >= 0) && !HasUserUsedOption(OptionIndex, UserId);

	// 점유 획득
	if (ConcurrencyMode == EInteractionConcurrencyMode::Exclusive)
	{
		ExclusiveOccupant = Interactor;
	}

	// 사용 기록은 승인 시점에 추가하고 중단 시 되돌리지 않는다. (최초 이벤트 중복 방지)
	bAnyInteractionOccurred = true;
	if (UserId >= 0)
	{
		UsedUserIds.AddUnique(UserId);
	}

	bool bOptionUsageChanged = false;
	if (bFirstGlobalForOption)
	{
		UsedOptionIndices.Add(OptionIndex);
		bOptionUsageChanged = true;
	}
	if (bFirstForUserOption)
	{
		FInteractionOptionUseRecord& Record = OptionUseRecords.AddDefaulted_GetRef();
		Record.OptionIndex = OptionIndex;
		Record.UserId = UserId;
		bOptionUsageChanged = true;
	}

	ActiveInteractors.Add(Interactor);

	OutContext.Interactor = Interactor;
	OutContext.InteractorUserId = UserId;
	OutContext.bFirstGlobalInteraction = bFirstGlobal;
	OutContext.bFirstForInteractor = bFirstForUser;
	OutContext.SelectedOptionIndex = OptionIndex;
	OutContext.bFirstGlobalForOption = bFirstGlobalForOption;
	OutContext.bFirstForInteractorForOption = bFirstForUserOption;

	RecomputeState();

	// 서버 측 리스너(리슨 서버 로컬 UI 포함)에게 옵션 소진 변화를 즉시 알린다. 원격 클라는 OnRep으로 받는다.
	if (bOptionUsageChanged)
	{
		OnInteractionOptionUsageChanged.Broadcast();
	}

	OnInteractionStarted.Broadcast(OutContext);

	return true;
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 일반 UI/Dialogue 종료를 처리하는 함수. OnInteractEnd 모드면 점유도 해제한다. 중복 요청은 멱등.
//! \param Interactor 상호작용을 종료한 플레이어 액터
//! \return 없음
void UInteractableComponent::EndInteraction(AActor* Interactor)
{
	if (!HasOwnerAuthority() || !Interactor)
	{
		return;
	}

	if (!IsInteractorActive(Interactor))
	{
		return;
	}

	RemoveActiveInteractor(Interactor);

	// Manual 모드는 세션 종료 이벤트만 처리하고 점유(Busy)를 유지한다. 해제는 CompleteInteraction이 담당한다.
	if (GetEffectiveReleaseMode() != EInteractionReleaseMode::Manual && ExclusiveOccupant.Get() == Interactor)
	{
		ExclusiveOccupant.Reset();
	}

	RecomputeState();
	StartInteractionCooldown(Interactor);
	OnInteractionEnded.Broadcast(Interactor);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] Manual 액터의 정상적인 콘텐츠 완료 지점에서 점유를 해제하는 함수.
//!        요청 사용자와 현재 점유자가 일치할 때만 해제하며 중복 호출은 무시한다.
//! \param Interactor 완료를 요청한 플레이어 액터
//! \return 없음
void UInteractableComponent::CompleteInteraction(AActor* Interactor)
{
	if (!HasOwnerAuthority() || !Interactor)
	{
		return;
	}

	if (GetEffectiveReleaseMode() != EInteractionReleaseMode::Manual)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interactable] CompleteInteraction은 Manual 모드 전용 - 무시한다. Owner: %s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (ExclusiveOccupant.Get() != Interactor)
	{
		return;
	}

	ExclusiveOccupant.Reset();
	RecomputeState();
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 접속 종료·Pawn 파괴·데이터 오류·연출 실패·Zone 강제 리셋용 비정상 중단 함수.
//!        ReleaseMode와 관계없이 점유와 활성 참조를 정리해 영구 Busy를 방지한다.
//!        최초/사용 기록은 되돌리지 않는다.
//! \param Interactor 중단할 플레이어 액터
//! \return 없음
void UInteractableComponent::AbortInteraction(AActor* Interactor)
{
	if (!HasOwnerAuthority() || !Interactor)
	{
		return;
	}

	const bool bWasActive = IsInteractorActive(Interactor);
	RemoveActiveInteractor(Interactor);

	if (ExclusiveOccupant.Get() == Interactor)
	{
		ExclusiveOccupant.Reset();
	}

	RecomputeState();

	// 시작을 벌인 액터가 정리(세션 제거 등)를 할 수 있도록 종료 이벤트로 통지한다.
	if (bWasActive)
	{
		StartInteractionCooldown(Interactor);
		OnInteractionEnded.Broadcast(Interactor);
	}
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 외부 활성화 Gate를 켜고 끄는 함수. 새로운 요청만 즉시 차단하며 진행 중 UI/연출은 강제로 닫지 않는다.
//! \param bEnabled 상호작용 가능 여부
//! \return 없음
void UInteractableComponent::SetInteractionEnabled(bool bEnabled)
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	bInteractionGateOpen = bEnabled;
	bGateExplicitlySet = true;
	RecomputeState();
}

////////////////////////////
//! \author 준혁
//! \brief [서버] Zone 재시작 등에서 점유·활성 참조·전체/사용자 사용 기록을 초기 상태로 되돌리는 함수
void UInteractableComponent::ResetInteractionState()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	ActiveInteractors.Reset();
	ExclusiveOccupant.Reset();
	bAnyInteractionOccurred = false;
	UsedUserIds.Reset();
	UsedOptionIndices.Reset();
	OptionUseRecords.Reset();
	InteractionCooldownRecords.Reset();
	RecomputeState();
	OnInteractionOptionUsageChanged.Broadcast();
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 해당 액터가 현재 승인된 활성 상호작용자인지 반환하는 함수. 상점 구매 RPC 검증 등에 사용된다.
//! \param Interactor 검사할 플레이어 액터
//! \return 활성 상호작용자면 true
bool UInteractableComponent::IsInteractorActive(const AActor* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& ActiveInteractor : ActiveInteractors)
	{
		if (ActiveInteractor.Get() == Interactor)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 서버 Context를 그대로 전달하도록 시그니처 변경
//! \brief 상호작용한 플레이어의 클라이언트에서 로컬 시작 이벤트를 발화한다.
//! \param Context 서버가 승인한 시작 Context
//! \return 없음
void UInteractableComponent::HandleLocalInteractBegin(const FInteractionStartContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("Interaction Begin (Local) - Target: %s, Interactor: %s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Context.Interactor));

	OnLocalInteractionStarted.Broadcast(Context);
}

////////////////////////////
//! \author HanUl
//! \brief 상호작용한 플레이어의 클라이언트에서 종료 로컬 이벤트를 발화한다.
//! \param Interactor 상호작용을 종료한 액터
//! \return 없음
void UInteractableComponent::HandleLocalInteractEnd(AActor* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("Interaction End (Local) - Target: %s, Interactor: %s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Interactor));

	OnLocalInteractionEnded.Broadcast(Interactor);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버 쿨타임 복제를 기다리는 동안 상호작용 가이드가 다시 나타나지 않도록 로컬 쿨타임을 선반영하는 함수
// Interactor : 상호작용을 종료하는 플레이어 액터
// Return Value : 로컬 쿨타임을 선반영했으면 true
bool UInteractableComponent::PredictLocalInteractionCooldown(const AActor* Interactor)
{
	if (InteractionCooldown <= 0.0f || !IsValid(Interactor))
	{
		return false;
	}

	const int32 UserId = ResolveInteractorUserId(Interactor);
	if (UserId < 0)
	{
		return false;
	}

	PredictedInteractionCooldownEndTimes.Add(UserId, GetInteractionServerTimeSeconds() + InteractionCooldown);
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief Interactor의 인증 사용자 ID를 해석하는 함수. Pawn 포인터가 아닌 인증 ID를 쓰는 이유는
//!        사망·리스폰·재접속 후에도 같은 던전 세션에서 플레이어별 기록을 유지하기 위해서다.
//!        인증이 없는 PIE 테스트에서는 PlayerId로 폴백한다(로비 파티 패널과 동일한 관례).
//!        실서버는 항상 인증되므로 폴백이 쓰이지 않으며, 폴백 ID는 재접속 시 유지되지 않는다.
//! \param Interactor 상호작용 플레이어 액터
//! \return 인증 사용자 ID, 없으면 INDEX_NONE
int32 UInteractableComponent::ResolveInteractorUserId(const AActor* Interactor) const
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

//////////////////////////////////////////////////////////////////////
// - Codex -
// 해당 사용자의 개인 상호작용 쿨타임이 진행 중인지 확인하는 함수
// UserId : 확인할 인증 사용자 ID
// 반환값 : 쿨타임 진행 중이면 true
bool UInteractableComponent::IsUserInteractionCooldownActive(int32 UserId) const
{
	if (InteractionCooldown <= 0.0f || UserId < 0)
	{
		return false;
	}

	const double CurrentServerTime = GetInteractionServerTimeSeconds();
	const double* PredictedCooldownEndTime = PredictedInteractionCooldownEndTimes.Find(UserId);
	if (PredictedCooldownEndTime && *PredictedCooldownEndTime > CurrentServerTime)
	{
		return true;
	}

	const FInteractionCooldownRecord* Record = InteractionCooldownRecords.FindByPredicate(
		[UserId](const FInteractionCooldownRecord& Entry)
		{
			return Entry.UserId == UserId;
		});

	return Record && Record->CooldownEndServerTime > CurrentServerTime;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 상호작용을 종료한 사용자의 개인 쿨타임을 서버에서 시작하는 함수
// Interactor : 상호작용을 종료한 플레이어 액터
void UInteractableComponent::StartInteractionCooldown(const AActor* Interactor)
{
	if (!HasOwnerAuthority() || InteractionCooldown <= 0.0f || !IsValid(Interactor))
	{
		return;
	}

	const int32 UserId = ResolveInteractorUserId(Interactor);
	if (UserId < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Interactable] 개인 쿨타임 시작 실패 - 사용자 ID 없음. Owner: %s, Interactor: %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Interactor));
		return;
	}

	PruneExpiredInteractionCooldowns();

	FInteractionCooldownRecord* Record = InteractionCooldownRecords.FindByPredicate(
		[UserId](const FInteractionCooldownRecord& Entry)
		{
			return Entry.UserId == UserId;
		});

	if (!Record)
	{
		Record = &InteractionCooldownRecords.AddDefaulted_GetRef();
		Record->UserId = UserId;
	}

	Record->CooldownEndServerTime = GetInteractionServerTimeSeconds() + InteractionCooldown;

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버에서 만료된 개인 상호작용 쿨타임 기록을 제거하는 함수
void UInteractableComponent::PruneExpiredInteractionCooldowns()
{
	if (!HasOwnerAuthority())
	{
		return;
	}

	const double CurrentServerTime = GetInteractionServerTimeSeconds();
	const int32 RemovedCount = InteractionCooldownRecords.RemoveAll(
		[CurrentServerTime](const FInteractionCooldownRecord& Entry)
		{
			return Entry.CooldownEndServerTime <= CurrentServerTime;
		});

	if (RemovedCount > 0)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 서버와 클라이언트가 쿨타임 종료 시각 비교에 사용할 동기화된 서버 시간을 반환하는 함수
// 반환값 : 현재 서버 기준 시간(초)
double UInteractableComponent::GetInteractionServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}

	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

////////////////////////////
//! \author 준혁
//! \brief 소유 액터가 서버 권한을 가지고 있는지 확인하는 함수
//! \return 서버 권한이 있으면 true
bool UInteractableComponent::HasOwnerAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

////////////////////////////
//! \author 준혁
//! \brief 실제 적용되는 해제 정책을 반환하는 함수. 미지원 조합(Shared + Manual)은 OnInteractEnd로 처리한다.
//! \return 유효한 해제 정책
EInteractionReleaseMode UInteractableComponent::GetEffectiveReleaseMode() const
{
	if (ReleaseMode == EInteractionReleaseMode::Immediate)
	{
		return EInteractionReleaseMode::Immediate;
	}

	if (ConcurrencyMode == EInteractionConcurrencyMode::Shared)
	{
		return EInteractionReleaseMode::OnInteractEnd;
	}

	return ReleaseMode;
}

////////////////////////////
//! \author 준혁
//! \brief 접속 종료 등으로 종료 이벤트를 못 받은 무효 점유/활성 참조를 정리하는 함수
void UInteractableComponent::PruneInvalidReferences()
{
	ActiveInteractors.RemoveAll([](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid();
	});

	if (!ExclusiveOccupant.IsValid())
	{
		ExclusiveOccupant.Reset();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 활성 상호작용자 목록에서 해당 액터를 제거하는 함수
//! \param Interactor 제거할 플레이어 액터
void UInteractableComponent::RemoveActiveInteractor(const AActor* Interactor)
{
	ActiveInteractors.RemoveAll([Interactor](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Interactor;
	});
}

////////////////////////////
//! \author 준혁
//! \brief 실행 상태를 우선순위(Gate 꺼짐 > 독점 점유 > 전체 소진 > 가능)에 따라 재계산하는 함수.
//!        상태가 바뀌면 서버에서 즉시 상태 변경 이벤트를 발화한다. (클라이언트는 OnRep에서 발화)
void UInteractableComponent::RecomputeState()
{
	EInteractableState NewState = EInteractableState::Ready;

	if (!bInteractionGateOpen)
	{
		NewState = EInteractableState::Disabled;
	}
	else if (ConcurrencyMode == EInteractionConcurrencyMode::Exclusive && ExclusiveOccupant.IsValid())
	{
		NewState = EInteractableState::Busy;
	}
	else if (UsageMode == EInteractionUsageMode::OnceGlobal && bAnyInteractionOccurred)
	{
		NewState = EInteractableState::Consumed;
	}

	if (CurrentState == NewState)
	{
		return;
	}

	CurrentState = NewState;
	OnInteractableStateChanged.Broadcast(CurrentState);
}

////////////////////////////
//! \author 준혁
//! \brief 복제된 실행 상태 변경을 클라이언트에서 통지하는 함수. 프롬프트/연출 갱신용.
void UInteractableComponent::OnRep_CurrentState()
{
	OnInteractableStateChanged.Broadcast(CurrentState);
}

////////////////////////////
//! \author 준혁
//! \brief 복제된 옵션 사용 기록 변경을 클라이언트에서 통지하는 함수. 소진된 옵션을 가이드 UI에서 숨기는 갱신용.
void UInteractableComponent::OnRep_OptionUsage()
{
	OnInteractionOptionUsageChanged.Broadcast();
}
