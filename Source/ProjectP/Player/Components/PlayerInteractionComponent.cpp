////////////////////////////
//! \file PlayerInteractionComponent.cpp
//! \brief 플레이어의 상호작용 감지, 후보 선정, 서버 상호작용 요청을 구현한다.
#include "PlayerInteractionComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "../PlayerCharacterBase.h"
#include "../../Dungeon/Gimmick/Elements/CPP_BalanceScaleElement.h"
#include "../../Dungeon/Gimmick/Elements/CPP_WeightObject.h"
#include "../../Dungeon/Interactable/Components/InteractableComponent.h"
#include "PlayerMovementComponent.h"
#include "WeightCarryComponent.h"

////////////////////////////
//! \author HanUl
//! \brief 상호작용 컴포넌트를 생성한다. 감지는 주기 타이머로 처리하므로 Tick을 사용하지 않는다.
//! \param 없음
//! \return 없음
UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	NearbyBalanceScaleSide = EBalanceScaleSide::Left;

	// Server/Client RPC 라우팅을 위해 컴포넌트 복제를 켠다.
	SetIsReplicatedByDefault(true);
}

////////////////////////////
//! \author HanUl
//! \brief 상호작용 대상 감지 주기 타이머를 시작한다.
//! \param 없음
//! \return 없음
void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwnerActor = GetOwner())
	{
		WeightCarryComponent = OwnerActor->FindComponentByClass<UWeightCarryComponent>();
		if (WeightCarryComponent)
		{
			WeightCarryComponent->OnWeightCarryStateChanged.AddUniqueDynamic(this, &UPlayerInteractionComponent::HandleWeightCarryStateChanged);
		}
	}

	// 로컬 컨트롤 여부는 Possess 시점에 따라 바뀔 수 있으므로 타이머는 항상 돌리고 갱신 함수에서 거른다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DetectionTimerHandle,
			this,
			&UPlayerInteractionComponent::UpdateInteractionCandidates,
			FMath::Max(DetectionInterval, 0.01f),
			true
		);
	}
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 일반 종료가 아닌 중단(Abort) 경로로 정리하도록 변경 (수동 점유 액터의 영구 Busy 방지)
//! \brief 감지 타이머를 정리한다.
//! \param EndPlayReason 종료 사유
//! \return 없음
void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 진행 중 파괴(사망, 접속 종료 등) 시 대상이 종료 이벤트를 못 받는 것을 막는 서버 측 안전장치다.
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority() && ActiveInteraction)
	{
		ActiveInteraction->AbortInteraction(OwnerActor);
		ActiveInteraction = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectionTimerHandle);
	}

	for (UInteractableComponent* Candidate : InteractionCandidates)
	{
		if (Candidate)
		{
			Candidate->OnInteractionOptionUsageChanged.RemoveDynamic(this, &UPlayerInteractionComponent::HandleCandidateOptionUsageChanged);
		}
	}
	SetCurrentCandidate(nullptr);
	InteractionCandidates.Reset();
	InteractionEntries.Reset();
	NearbyBalanceScale = nullptr;

	if (WeightCarryComponent)
	{
		WeightCarryComponent->OnWeightCarryStateChanged.RemoveDynamic(this, &UPlayerInteractionComponent::HandleWeightCarryStateChanged);
		WeightCarryComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author HanUl
//! \brief 상호작용 입력을 처리한다. 진행 중이면 종료 요청, 아니면 현재 후보에게 시작을 요청한다(토글).
//! \param 없음
//! \return 없음
void UPlayerInteractionComponent::TryInteract()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || bOwnerDead)
	{
		return;
	}

	// 진행 중인 상호작용이 있으면 같은 입력으로 종료한다.
	if (ActiveInteraction)
	{
		if (ActiveInteraction->PredictLocalInteractionCooldown(GetOwner()))
		{
			RebuildInteractionEntries();
		}

		ServerRequestEndInteract();
		return;
	}

	// 대화 등 전용 UI가 떠 있는 동안에는 새 상호작용 시작을 막는다. (종료는 위에서 이미 허용됨)
	if (bInteractionBlocked)
	{
		return;
	}

	if (!InteractionEntries.IsValidIndex(SelectedInteractionEntryIndex))
	{
		return;
	}

	// 최종 판정은 서버가 한다. 리슨 서버 호스트는 RPC가 로컬 실행으로 이어진다.
	const FPlayerInteractionEntry& SelectedEntry = InteractionEntries[SelectedInteractionEntryIndex];
	if (SelectedEntry.EntryType == EPlayerInteractionEntryType::ReleaseCarriedWeight)
	{
		if (WeightCarryComponent)
		{
			WeightCarryComponent->RequestReleaseCarriedWeight();
		}
		return;
	}

	if (SelectedEntry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightLeft ||
		SelectedEntry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightRight)
	{
		if (WeightCarryComponent)
		{
			ACPP_BalanceScaleElement* ScaleElement = Cast<ACPP_BalanceScaleElement>(SelectedEntry.ActionTarget);
			const EBalanceScaleSide Side = SelectedEntry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightLeft
				? EBalanceScaleSide::Left
				: EBalanceScaleSide::Right;
			WeightCarryComponent->RequestPlaceCarriedWeight(ScaleElement, Side);
		}
		return;
	}

	if (!SelectedEntry.Interactable || !SelectedEntry.Interactable->GetOwner())
	{
		return;
	}

	ServerRequestInteract(SelectedEntry.Interactable->GetOwner(), SelectedEntry.OptionIndex);
}

////////////////////////////
//! \author HanUl
//! \brief Owner의 사망 상태를 반영하고 사망 시 후보 UI와 서버 상호작용 점유를 중단한다.
//! \param bDead true이면 사망 상태
//! \return 없음
void UPlayerInteractionComponent::HandleOwnerLifeStateChanged(bool bDead)
{
	bOwnerDead = bDead;
	if (!bOwnerDead)
	{
		LocallyEndedInteractionFromDeath.Reset();
		return;
	}

	SetInteractionCandidates({});
	SetNearbyBalanceScale(nullptr, EBalanceScaleSide::Left);
	RebuildInteractionEntries();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !ActiveInteraction)
	{
		return;
	}

	AActor* TargetActor = ActiveInteraction->GetOwner();
	if (OwnerActor->HasAuthority())
	{
		ActiveInteraction->AbortInteraction(OwnerActor);
		ActiveInteraction = nullptr;
		ClientNotifyInteractEnd(TargetActor);
		return;
	}

	ActiveInteraction = nullptr;
	if (IsValid(TargetActor))
	{
		if (UInteractableComponent* Interactable = TargetActor->FindComponentByClass<UInteractableComponent>())
		{
			LocallyEndedInteractionFromDeath = Interactable;
			Interactable->HandleLocalInteractEnd(OwnerActor);
		}
	}
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 옵션 미등록 액터도 기본 옵션 1개로 가이드가 뜨므로 판정을 유효 옵션 기준으로 변경
//! \brief 상호작용 가이드가 표시 중인지 반환하는 함수. 후보가 있으면 유효 옵션이 항상 1개 이상이라 true다.
//!        true면 마우스 휠이 줌 대신 옵션 선택으로 동작한다.
//! \return 가이드 표시 여부
bool UPlayerInteractionComponent::HasInteractionOptions() const
{
	return !InteractionEntries.IsEmpty();
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 소진된 일회성 옵션은 선택 불가하도록 가용성 검사 추가
//! \brief 선택 항목을 지정 통합 인덱스로 옮기고 변경 이벤트를 발화하는 함수. 가이드 UI의 호버/클릭이 호출한다.
//! \param NewIndex 새로 선택할 통합 상호작용 항목 인덱스
void UPlayerInteractionComponent::SetSelectedInteractionEntry(int32 NewIndex)
{
	if (!IsInteractionEntryAvailable(NewIndex))
	{
		return;
	}

	if (SelectedInteractionEntryIndex == NewIndex)
	{
		return;
	}

	SelectedInteractionEntryIndex = NewIndex;
	SetCurrentCandidate(InteractionEntries[SelectedInteractionEntryIndex].Interactable);
	OnInteractionOptionSelectionChanged.Broadcast(SelectedInteractionEntryIndex);
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 소진된 일회성 옵션을 건너뛰고 다음 선택 가능한 옵션으로 이동하도록 변경
//! \brief 선택 옵션을 위/아래로 한 칸 옮기는 함수(순환). 마우스 휠 입력이 호출한다.
//! \param Delta 이동 방향(-1: 위, +1: 아래)
void UPlayerInteractionComponent::StepSelectedInteractionEntry(int32 Delta)
{
	const int32 EntryCount = InteractionEntries.Num();
	if (EntryCount <= 0 || Delta == 0)
	{
		return;
	}

	const int32 Direction = (Delta > 0) ? 1 : -1;
	int32 Index = (SelectedInteractionEntryIndex != INDEX_NONE) ? SelectedInteractionEntryIndex : 0;

	// 사용할 수 없게 된 항목은 건너뛴다. 최대 한 바퀴만 돌고, 선택 가능한 항목이 없으면 이동하지 않는다.
	for (int32 Step = 0; Step < EntryCount; ++Step)
	{
		Index = ((Index + Direction) % EntryCount + EntryCount) % EntryCount;
		if (IsInteractionEntryAvailable(Index))
		{
			SetSelectedInteractionEntry(Index);
			return;
		}
	}
}

////////////////////////////
//! \author 준혁
//! \brief 통합 목록에서 선택 가능한 첫 항목 인덱스를 찾는 함수
//! \return 첫 가용 항목 인덱스, 없으면 INDEX_NONE
int32 UPlayerInteractionComponent::FindFirstAvailableInteractionEntry() const
{
	for (int32 EntryIndex = 0; EntryIndex < InteractionEntries.Num(); ++EntryIndex)
	{
		if (IsInteractionEntryAvailable(EntryIndex))
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

////////////////////////////
//! \author 준혁
//! \brief 통합 목록의 지정 항목이 현재 선택 가능한지 검사하는 함수
//! \param EntryIndex 검사할 통합 상호작용 항목 인덱스
//! \return 대상과 실제 옵션이 모두 유효하면 true
bool UPlayerInteractionComponent::IsInteractionEntryAvailable(int32 EntryIndex) const
{
	if (!InteractionEntries.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FPlayerInteractionEntry& Entry = InteractionEntries[EntryIndex];
	if (Entry.EntryType == EPlayerInteractionEntryType::ReleaseCarriedWeight)
	{
		return WeightCarryComponent && WeightCarryComponent->HasCarriedWeight();
	}

	if (Entry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightLeft ||
		Entry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightRight)
	{
		const ACPP_BalanceScaleElement* ScaleElement = Cast<ACPP_BalanceScaleElement>(Entry.ActionTarget);
		const EBalanceScaleSide EntrySide = Entry.EntryType == EPlayerInteractionEntryType::PlaceCarriedWeightLeft
			? EBalanceScaleSide::Left
			: EBalanceScaleSide::Right;
		return WeightCarryComponent && IsValid(ScaleElement) && ScaleElement->IsWeightPlacementEnabled() &&
			Entry.ActionTarget.Get() == NearbyBalanceScale.Get() && EntrySide == NearbyBalanceScaleSide;
	}

	return Entry.Interactable && Entry.Interactable->IsOptionAvailable(Entry.OptionIndex, GetOwner());
}

////////////////////////////
//! \author 준혁
//! \brief 후보의 옵션 사용 기록이 바뀌면(파티원이 일회성 옵션을 소진하는 경우 포함) 선택을 보정하고
//!        후보 변경 이벤트를 재발화해 가이드 UI를 다시 그리게 하는 함수.
void UPlayerInteractionComponent::HandleCandidateOptionUsageChanged()
{
	RebuildInteractionEntries();
}

////////////////////////////
//! \author Codex
//! \brief 운반 무게추가 바뀌면 항상 표시해야 하는 놓기 항목을 포함해 통합 목록을 다시 만든다.
void UPlayerInteractionComponent::HandleWeightCarryStateChanged()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		EBalanceScaleSide NewBalanceScaleSide = EBalanceScaleSide::Left;
		ACPP_BalanceScaleElement* NewBalanceScale = FindNearestBalanceScale(NewBalanceScaleSide);
		if (NearbyBalanceScale != NewBalanceScale ||
			(IsValid(NewBalanceScale) && NearbyBalanceScaleSide != NewBalanceScaleSide))
		{
			SetNearbyBalanceScale(NewBalanceScale, NewBalanceScaleSide);
			return;
		}
	}

	RebuildInteractionEntries();
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - CanInteract 사전 검사 + HandleInteractBegin 분리 호출을 원자적 TryBeginInteraction으로 교체.
//!         승인과 상태 변경이 한 서버 함수에서 끝나 동시 요청 중 하나만 독점 점유를 얻는다.
//! \brief 클라이언트가 요청한 상호작용 대상을 서버에서 재검증하고, 통과 시 상호작용 시작을 확정한다.
//! \param TargetActor 클라이언트가 후보로 선정했던 상호작용 대상 액터
//! \param OptionIndex 클라이언트가 선택한 상호작용 옵션 인덱스 (TryBeginInteraction이 범위 검증)
//! \return 없음
void UPlayerInteractionComponent::ServerRequestInteract_Implementation(AActor* TargetActor, int32 OptionIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !IsValid(TargetActor))
	{
		return;
	}

	// 이미 진행 중이면 새 시작을 받지 않는다(연타/레이턴시로 인한 중복 시작 방지).
	if (ActiveInteraction)
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction request rejected - already interacting. Interactor: %s, Active: %s, Requested: %s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(ActiveInteraction->GetOwner()),
			*GetNameSafe(TargetActor));
		return;
	}

	UInteractableComponent* Interactable = TargetActor->FindComponentByClass<UInteractableComponent>();
	if (!Interactable)
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction request rejected - no InteractableComponent. Interactor: %s, Target: %s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor));
		return;
	}

	// 재접속 대기 중인 플레이어는 인게임 영향에서 제외된다.
	const APlayerCharacterBase* OwnerPlayer = Cast<APlayerCharacterBase>(OwnerActor);
	if (OwnerPlayer && (OwnerPlayer->IsReconnectInactive() || OwnerPlayer->IsDead()))
	{
		return;
	}

	// 거리 재검증: 클라이언트 감지는 콜리전 바운드 기준 오버랩이므로 서버도 바운드 기준 거리로 판정한다.
	const float MaxDistance = DetectionRadius * FMath::Max(ServerDistanceToleranceScale, 1.0f);
	const float DistSquared = TargetActor->GetComponentsBoundingBox().ComputeSquaredDistanceToPoint(OwnerActor->GetActorLocation());
	if (DistSquared > FMath::Square(MaxDistance))
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction request rejected - out of range. Interactor: %s, Target: %s, Dist: %.1f, Max: %.1f"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor),
			FMath::Sqrt(DistSquared),
			MaxDistance);
		return;
	}

	// 상태·점유·사용 제한·옵션 인덱스 검사와 승인 처리는 대상 컴포넌트가 원자적으로 수행한다.
	FInteractionStartContext Context;
	EInteractionRejectReason RejectReason = EInteractionRejectReason::None;
	if (!Interactable->TryBeginInteraction(OwnerActor, Context, RejectReason, OptionIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction request rejected - %s. Interactor: %s, Target: %s"),
			*UEnum::GetValueAsString(RejectReason),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor));
		return;
	}

	// 승인 성공 후에만 진행 상태를 지정한다.
	// 무게추는 승인 이벤트에서 별도 운반 컴포넌트로 소유권을 넘기고 세션은 즉시 정리한다.
	// Immediate 정책 액터도 승인 이벤트에서 동작을 끝내므로 동일하게 세션을 즉시 정리한다.
	if (Cast<ACPP_WeightObject>(TargetActor) ||
		Interactable->GetReleaseMode() == EInteractionReleaseMode::Immediate)
	{
		Interactable->EndInteraction(OwnerActor);
		return;
	}

	ActiveInteraction = Interactable;
	ClientNotifyInteractBegin(TargetActor, Context);
}

////////////////////////////
//! \author HanUl
//! \brief 진행 중인 상호작용을 서버에서 종료 확정하고 소유 클라이언트에 통지한다.
//! \param 없음
//! \return 없음
void UPlayerInteractionComponent::ServerRequestEndInteract_Implementation()
{
	if (!ActiveInteraction)
	{
		return;
	}

	AActor* TargetActor = ActiveInteraction->GetOwner();
	ActiveInteraction->EndInteraction(GetOwner());
	ActiveInteraction = nullptr;

	ClientNotifyInteractEnd(TargetActor);
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 서버 승인 Context를 그대로 로컬 이벤트에 전달하도록 변경 (클라는 최초 여부를 재계산하지 않는다)
//! \brief 서버에서 확정된 상호작용 시작을 상호작용한 플레이어의 클라이언트에 알려 로컬 이벤트를 발화시킨다.
//! \param TargetActor 상호작용이 시작된 대상 액터
//! \param Context 서버가 승인한 시작 Context
//! \return 없음
void UPlayerInteractionComponent::ClientNotifyInteractBegin_Implementation(AActor* TargetActor, FInteractionStartContext Context)
{
	if (bOwnerDead || !IsValid(TargetActor))
	{
		return;
	}

	if (UInteractableComponent* Interactable = TargetActor->FindComponentByClass<UInteractableComponent>())
	{
		// 소유 클라이언트에서도 진행 상태를 세팅해 다음 입력이 종료로 라우팅되게 한다.
		ActiveInteraction = Interactable;

		// 스킬 조준과 같은 경로로 대상을 바라보게 회전한다(컨트롤러 회전 → 이동 복제로 전파).
		AActor* OwnerActor = GetOwner();
		const FVector ToTarget = (TargetActor->GetActorLocation() - OwnerActor->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			if (UPlayerMovementComponent* MovementComponent = OwnerActor->FindComponentByClass<UPlayerMovementComponent>())
			{
				MovementComponent->RequestSkillFacingYaw(ToTarget.Rotation().Yaw, FacingInterpSpeed, 1.0f);
			}
		}

		Interactable->HandleLocalInteractBegin(Context);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 확정된 상호작용 종료를 소유 클라이언트에 알려 로컬 종료 이벤트를 발화시킨다.
//! \param TargetActor 상호작용이 종료된 대상 액터
//! \return 없음
void UPlayerInteractionComponent::ClientNotifyInteractEnd_Implementation(AActor* TargetActor)
{
	ActiveInteraction = nullptr;

	if (!IsValid(TargetActor))
	{
		return;
	}

	if (UInteractableComponent* Interactable = TargetActor->FindComponentByClass<UInteractableComponent>())
	{
		if (LocallyEndedInteractionFromDeath.Get() == Interactable)
		{
			LocallyEndedInteractionFromDeath.Reset();
			return;
		}

		Interactable->HandleLocalInteractEnd(GetOwner());
	}
}

////////////////////////////
//! \author HanUl
//! \brief 로컬 플레이어 주변을 감지해 모든 상호작용 후보를 갱신한다. 감지 주기 타이머로 호출된다.
//! \param 없음
//! \return 없음
void UPlayerInteractionComponent::UpdateInteractionCandidates()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || bOwnerDead)
	{
		SetNearbyBalanceScale(nullptr, EBalanceScaleSide::Left);
		SetInteractionCandidates({});
		return;
	}

	EBalanceScaleSide NewBalanceScaleSide = EBalanceScaleSide::Left;
	ACPP_BalanceScaleElement* NewBalanceScale = FindNearestBalanceScale(NewBalanceScaleSide);
	SetNearbyBalanceScale(NewBalanceScale, NewBalanceScaleSide);
	SetInteractionCandidates(FindInteractables());
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 가장 가까운 단일 후보 대신 반경 안의 모든 후보를 거리순으로 반환하도록 변경
//! \brief 감지 반경 안의 모든 상호작용 가능 대상을 중복 없이 찾아 거리순으로 반환한다.
//! \param 없음
//! \return 거리순으로 정렬된 상호작용 대상 컴포넌트 목록
TArray<UInteractableComponent*> UPlayerInteractionComponent::FindInteractables() const
{
	TArray<UInteractableComponent*> FoundInteractables;

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return FoundInteractables;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerInteractionDetect), false, OwnerActor);
	World->OverlapMultiByObjectType(
		Overlaps,
		OwnerLocation,
		FQuat::Identity,
		FCollisionObjectQueryParams::AllObjects,
		FCollisionShape::MakeSphere(DetectionRadius),
		QueryParams
	);

	TSet<UInteractableComponent*> UniqueInteractables;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor || OverlapActor == OwnerActor)
		{
			continue;
		}

		UInteractableComponent* Interactable = OverlapActor->FindComponentByClass<UInteractableComponent>();
		if (!Interactable || !Interactable->CanInteract(OwnerActor))
		{
			continue;
		}

		UniqueInteractables.Add(Interactable);
	}

	FoundInteractables = UniqueInteractables.Array();
	FoundInteractables.Sort([OwnerLocation](const UInteractableComponent& Left, const UInteractableComponent& Right)
	{
		const AActor* LeftOwner = Left.GetOwner();
		const AActor* RightOwner = Right.GetOwner();
		const float LeftDistance = LeftOwner ? FVector::DistSquared(OwnerLocation, LeftOwner->GetActorLocation()) : TNumericLimits<float>::Max();
		const float RightDistance = RightOwner ? FVector::DistSquared(OwnerLocation, RightOwner->GetActorLocation()) : TNumericLimits<float>::Max();

		if (!FMath::IsNearlyEqual(LeftDistance, RightDistance))
		{
			return LeftDistance < RightDistance;
		}

		return Left.GetUniqueID() < Right.GetUniqueID();
	});

	return FoundInteractables;
}

////////////////////////////
//! \author Codex
//! \brief 운반 여부와 무관하게 로컬 플레이어가 좌우 배치 범위에 들어간 활성 저울과 가장 가까운 방향을 찾는다.
//! \param OutSide 가장 가까운 배치 범위의 좌우 방향이다.
//! \return 가장 가까운 배치 가능 저울이며 없으면 nullptr다.
ACPP_BalanceScaleElement* UPlayerInteractionComponent::FindNearestBalanceScale(EBalanceScaleSide& OutSide) const
{
	OutSide = EBalanceScaleSide::Left;

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !WeightCarryComponent)
	{
		return nullptr;
	}

	ACPP_BalanceScaleElement* NearestScale = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ACPP_BalanceScaleElement> It(World); It; ++It)
	{
		ACPP_BalanceScaleElement* ScaleElement = *It;
		if (!IsValid(ScaleElement) || !ScaleElement->IsWeightPlacementEnabled())
		{
			continue;
		}

		const EBalanceScaleSide Sides[] =
		{
			EBalanceScaleSide::Left,
			EBalanceScaleSide::Right,
		};

		for (const EBalanceScaleSide Side : Sides)
		{
			if (!ScaleElement->IsActorInsidePlacementRange(OwnerActor, Side))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(
				OwnerActor->GetActorLocation(),
				ScaleElement->GetPlacementRangeCenter(Side));
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestScale = ScaleElement;
				OutSide = Side;
			}
		}
	}

	return NearestScale;
}

////////////////////////////
//! \author Codex
//! \brief 현재 배치 대상 저울과 좌우 방향을 갱신하고 달라진 경우 운반 선택지 목록을 다시 만든다.
//! \param NewBalanceScale 새로 감지된 가장 가까운 저울이며 없으면 nullptr다.
//! \param NewSide 새로 감지된 배치 범위의 좌우 방향이다.
void UPlayerInteractionComponent::SetNearbyBalanceScale(
	ACPP_BalanceScaleElement* NewBalanceScale,
	EBalanceScaleSide NewSide)
{
	if (NearbyBalanceScale == NewBalanceScale &&
		(!IsValid(NewBalanceScale) || NearbyBalanceScaleSide == NewSide))
	{
		return;
	}

	NearbyBalanceScale = NewBalanceScale;
	NearbyBalanceScaleSide = NewSide;
	RebuildInteractionEntries();
}

////////////////////////////
//! \author 준혁
//! \brief 감지된 전체 후보를 갱신하고 각 후보의 옵션 변경 이벤트를 다시 바인딩하는 함수
//! \param NewCandidates 새로 감지된 거리순 상호작용 후보 목록
//! \return 없음
void UPlayerInteractionComponent::SetInteractionCandidates(const TArray<UInteractableComponent*>& NewCandidates)
{
	bool bCandidatesChanged = InteractionCandidates.Num() != NewCandidates.Num();
	if (!bCandidatesChanged)
	{
		for (int32 CandidateIndex = 0; CandidateIndex < NewCandidates.Num(); ++CandidateIndex)
		{
			if (InteractionCandidates[CandidateIndex] != NewCandidates[CandidateIndex])
			{
				bCandidatesChanged = true;
				break;
			}
		}
	}

	if (!bCandidatesChanged)
	{
		return;
	}

	for (UInteractableComponent* Candidate : InteractionCandidates)
	{
		if (Candidate)
		{
			Candidate->OnInteractionOptionUsageChanged.RemoveDynamic(this, &UPlayerInteractionComponent::HandleCandidateOptionUsageChanged);
		}
	}

	InteractionCandidates.Reset(NewCandidates.Num());
	for (UInteractableComponent* Candidate : NewCandidates)
	{
		if (Candidate)
		{
			InteractionCandidates.Add(Candidate);
			Candidate->OnInteractionOptionUsageChanged.AddUniqueDynamic(this, &UPlayerInteractionComponent::HandleCandidateOptionUsageChanged);
		}
	}

	RebuildInteractionEntries();
}

////////////////////////////
//! \author 준혁
//! \brief 모든 후보의 사용 가능한 옵션을 하나의 거리순 가이드 항목 목록으로 다시 만드는 함수
//! \return 없음
void UPlayerInteractionComponent::RebuildInteractionEntries()
{
	EPlayerInteractionEntryType PreviousEntryType = EPlayerInteractionEntryType::Interactable;
	UInteractableComponent* PreviousInteractable = nullptr;
	AActor* PreviousActionTarget = nullptr;
	int32 PreviousOptionIndex = INDEX_NONE;
	if (InteractionEntries.IsValidIndex(SelectedInteractionEntryIndex))
	{
		PreviousEntryType = InteractionEntries[SelectedInteractionEntryIndex].EntryType;
		PreviousInteractable = InteractionEntries[SelectedInteractionEntryIndex].Interactable;
		PreviousActionTarget = InteractionEntries[SelectedInteractionEntryIndex].ActionTarget;
		PreviousOptionIndex = InteractionEntries[SelectedInteractionEntryIndex].OptionIndex;
	}

	InteractionEntries.Reset();
	if (!bOwnerDead && WeightCarryComponent)
	{
		if (NearbyBalanceScale && NearbyBalanceScale->IsWeightPlacementEnabled())
		{
			FPlayerInteractionEntry& PlacementEntry = InteractionEntries.AddDefaulted_GetRef();
			PlacementEntry.EntryType = NearbyBalanceScaleSide == EBalanceScaleSide::Left
				? EPlayerInteractionEntryType::PlaceCarriedWeightLeft
				: EPlayerInteractionEntryType::PlaceCarriedWeightRight;
			PlacementEntry.ActionTarget = NearbyBalanceScale;
			PlacementEntry.DisplayText = NearbyBalanceScaleSide == EBalanceScaleSide::Left
				? WeightCarryComponent->GetPlaceLeftInteractionText()
				: WeightCarryComponent->GetPlaceRightInteractionText();
		}
		else if (WeightCarryComponent->HasCarriedWeight())
		{
			FPlayerInteractionEntry& ReleaseEntry = InteractionEntries.AddDefaulted_GetRef();
			ReleaseEntry.EntryType = EPlayerInteractionEntryType::ReleaseCarriedWeight;
			ReleaseEntry.DisplayText = WeightCarryComponent->GetReleaseInteractionText();
		}
	}

	if (bOwnerDead)
	{
		SelectedInteractionEntryIndex = INDEX_NONE;
		SetCurrentCandidate(nullptr);
		OnInteractionEntriesChanged.Broadcast();
		OnInteractionOptionSelectionChanged.Broadcast(INDEX_NONE);
		return;
	}

	for (UInteractableComponent* Candidate : InteractionCandidates)
	{
		// 무게추 운반 중에는 일반 상호작용 후보를 모두 숨기고 놓기/저울 배치 항목만 유지한다.
		if (WeightCarryComponent && WeightCarryComponent->HasCarriedWeight())
		{
			break;
		}

		if (!Candidate || !Candidate->CanInteract(GetOwner()))
		{
			continue;
		}

		const TArray<FInteractionOption> Options = Candidate->GetEffectiveInteractionOptions();
		for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
		{
			if (!Candidate->IsOptionAvailable(OptionIndex, GetOwner()))
			{
				continue;
			}

			FPlayerInteractionEntry& NewEntry = InteractionEntries.AddDefaulted_GetRef();
			NewEntry.Interactable = Candidate;
			NewEntry.OptionIndex = OptionIndex;
			NewEntry.DisplayText = Options[OptionIndex].DisplayText;
		}
	}

	SelectedInteractionEntryIndex = INDEX_NONE;
	for (int32 EntryIndex = 0; EntryIndex < InteractionEntries.Num(); ++EntryIndex)
	{
		const FPlayerInteractionEntry& Entry = InteractionEntries[EntryIndex];
		if (Entry.EntryType == PreviousEntryType && Entry.Interactable == PreviousInteractable &&
			Entry.ActionTarget == PreviousActionTarget && Entry.OptionIndex == PreviousOptionIndex)
		{
			SelectedInteractionEntryIndex = EntryIndex;
			break;
		}
	}

	if (SelectedInteractionEntryIndex == INDEX_NONE)
	{
		SelectedInteractionEntryIndex = FindFirstAvailableInteractionEntry();
	}

	UInteractableComponent* NewCurrentCandidate = InteractionEntries.IsValidIndex(SelectedInteractionEntryIndex)
		? InteractionEntries[SelectedInteractionEntryIndex].Interactable.Get()
		: nullptr;
	SetCurrentCandidate(NewCurrentCandidate);

	OnInteractionEntriesChanged.Broadcast();
	OnInteractionOptionSelectionChanged.Broadcast(SelectedInteractionEntryIndex);
}

////////////////////////////
//! \author 준혁
//! \brief 현재 선택 항목이 속한 후보를 갱신하고 대상이 바뀐 경우 기존 후보 변경 이벤트를 발화하는 함수
//! \param NewCandidate 새로 선택된 항목의 상호작용 컴포넌트, 없으면 nullptr
//! \return 없음
void UPlayerInteractionComponent::SetCurrentCandidate(UInteractableComponent* NewCandidate)
{
	if (CurrentCandidate == NewCandidate)
	{
		return;
	}

	if (IsValid(CurrentCandidate))
	{
		if (ACPP_WeightObject* PreviousWeightObject = Cast<ACPP_WeightObject>(CurrentCandidate->GetOwner()))
		{
			PreviousWeightObject->SetInteractionFocusHighlighted(false);
		}
	}

	CurrentCandidate = NewCandidate;

	if (IsValid(CurrentCandidate))
	{
		if (ACPP_WeightObject* NewWeightObject = Cast<ACPP_WeightObject>(CurrentCandidate->GetOwner()))
		{
			NewWeightObject->SetInteractionFocusHighlighted(true);
		}
	}

	OnInteractionCandidateChanged.Broadcast(NewCandidate ? NewCandidate->GetOwner() : nullptr);
}
