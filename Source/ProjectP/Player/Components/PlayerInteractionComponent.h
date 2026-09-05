////////////////////////////
//! \file PlayerInteractionComponent.h
//! \brief 플레이어의 상호작용 감지, 후보 선정, 서버 상호작용 요청 컴포넌트 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Dungeon/Interactable/Components/InteractableComponent.h"
#include "PlayerInteractionComponent.generated.h"

class UWeightCarryComponent;
class ACPP_BalanceScaleElement;
enum class EBalanceScaleSide : uint8;

UENUM(BlueprintType)
enum class EPlayerInteractionEntryType : uint8
{
	Interactable,
	ReleaseCarriedWeight,
	PlaceCarriedWeightLeft,
	PlaceCarriedWeightRight,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionCandidateChangedSignature, AActor*, NewCandidateActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractionEntriesChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionOptionSelectionChangedSignature, int32, NewSelectedIndex);

////////////////////////////
//! \struct FPlayerInteractionEntry
//! \brief 상호작용 가이드에 표시되는 한 항목과 실제 실행 대상/옵션을 함께 보관한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FPlayerInteractionEntry
{
	GENERATED_BODY()

public:
	//! \brief 통합 항목이 실행할 동작 종류다.
	UPROPERTY(BlueprintReadOnly, Category = "Player|Interaction")
	EPlayerInteractionEntryType EntryType = EPlayerInteractionEntryType::Interactable;

	//! \brief 항목을 실행할 상호작용 컴포넌트이다.
	UPROPERTY(BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<UInteractableComponent> Interactable = nullptr;

	//! \brief 일반 상호작용 컴포넌트가 없는 운반 배치 항목의 실행 대상 액터다.
	UPROPERTY(BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<AActor> ActionTarget = nullptr;

	//! \brief 대상 상호작용 컴포넌트 내부의 실제 옵션 인덱스이다.
	UPROPERTY(BlueprintReadOnly, Category = "Player|Interaction")
	int32 OptionIndex = INDEX_NONE;

	//! \brief 가이드 버튼에 표시할 텍스트이다.
	UPROPERTY(BlueprintReadOnly, Category = "Player|Interaction")
	FText DisplayText;
};

////////////////////////////
//! \class UPlayerInteractionComponent
//! \author HanUl
//! \brief 로컬 플레이어 주변의 InteractableComponent를 주기적으로 감지해 거리순 통합 항목 목록을 만들고,
//!        상호작용 입력 시 서버 검증을 거쳐 대상의 상호작용 시작 이벤트를 발화시키는 컴포넌트다.
//! \note 감지와 후보 선정은 로컬 플레이어에서만 동작한다. 상호작용 시작 확정은 서버가 거리와
//!       CanInteract를 재검증한 뒤에만 이뤄지며, 클라이언트 후보를 그대로 신뢰하지 않는다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTP_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	UFUNCTION(BlueprintPure, Category = "Player|Interaction")
	UInteractableComponent* GetCurrentCandidate() const { return CurrentCandidate; }

	//! \brief 현재 가이드에 표시할 전체 상호작용 항목 목록을 반환한다.
	const TArray<FPlayerInteractionEntry>& GetInteractionEntries() const { return InteractionEntries; }

	//! \brief 현재 확정된 상호작용이 진행 중인지 여부를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Player|Interaction")
	bool IsInteracting() const { return ActiveInteraction != nullptr; }

	//! \brief 상호작용 입력 처리. 진행 중이면 종료를, 아니면 현재 후보에게 시작을 요청한다(토글).
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction")
	void TryInteract();

	//! \brief Owner의 사망 상태를 반영한다. 사망 시 후보와 진행 중 상호작용을 즉시 정리한다.
	void HandleOwnerLifeStateChanged(bool bDead);

	//! \brief 새 상호작용 시작을 차단/해제한다. 대화 등 전용 UI가 떠 있는 동안 사용한다.
	//!        진행 중인 상호작용의 종료는 차단 중에도 허용된다.
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction")
	void SetInteractionBlocked(bool bBlocked) { bInteractionBlocked = bBlocked; }

	//! \brief 현재 후보가 상호작용 옵션 목록을 가지고 있는지 반환한다.
	//!        true면 상호작용 가이드 UI가 표시 중이며, 마우스 휠은 줌이 아니라 옵션 선택 이동으로 동작해야 한다.
	UFUNCTION(BlueprintPure, Category = "Player|Interaction|Options")
	bool HasInteractionOptions() const;

	//! \brief 현재 선택된 통합 상호작용 항목 인덱스를 반환한다. 항목이 없으면 INDEX_NONE.
	UFUNCTION(BlueprintPure, Category = "Player|Interaction|Options")
	int32 GetSelectedInteractionEntryIndex() const { return SelectedInteractionEntryIndex; }

	//! \brief 기존 블루프린트 호환용 함수. 반환값은 통합 상호작용 항목 인덱스이다.
	UFUNCTION(BlueprintPure, Category = "Player|Interaction|Options")
	int32 GetSelectedOptionIndex() const { return GetSelectedInteractionEntryIndex(); }

	//! \brief 선택 항목을 지정한 통합 상호작용 항목 인덱스로 옮긴다.
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction|Options")
	void SetSelectedInteractionEntry(int32 NewIndex);

	//! \brief 기존 블루프린트 호환용 함수. 통합 상호작용 항목 인덱스를 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction|Options")
	void SetSelectedOption(int32 NewIndex) { SetSelectedInteractionEntry(NewIndex); }

	//! \brief 선택 항목을 위/아래로 한 칸 옮긴다(순환).
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction|Options")
	void StepSelectedInteractionEntry(int32 Delta);

	//! \brief 기존 블루프린트 호환용 함수. 통합 상호작용 항목을 위/아래로 한 칸 옮긴다.
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction|Options")
	void StepSelectedOption(int32 Delta) { StepSelectedInteractionEntry(Delta); }

	//! \brief 상호작용 후보가 바뀔 때 발화한다. 후보가 사라지면 nullptr로 발화한다. (프롬프트 UI 연동용)
	UPROPERTY(BlueprintAssignable, Category = "Player|Interaction")
	FInteractionCandidateChangedSignature OnInteractionCandidateChanged;

	//! \brief 주변 후보 또는 후보의 사용 가능한 옵션이 바뀌어 통합 항목 목록을 다시 그려야 할 때 발화한다.
	UPROPERTY(BlueprintAssignable, Category = "Player|Interaction|Options")
	FInteractionEntriesChangedSignature OnInteractionEntriesChanged;

	//! \brief 선택된 통합 상호작용 항목이 바뀔 때 발화한다. 항목이 없어지면 INDEX_NONE으로 발화한다. (기존 이름 호환 유지)
	UPROPERTY(BlueprintAssignable, Category = "Player|Interaction|Options")
	FInteractionOptionSelectionChangedSignature OnInteractionOptionSelectionChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//! \brief 상호작용 대상 감지 반경(cm)이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Interaction", meta = (ClampMin = "0.0"))
	float DetectionRadius = 250.0f;

	//! \brief 감지 갱신 주기(초)이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Interaction", meta = (ClampMin = "0.01"))
	float DetectionInterval = 0.15f;

	//! \brief 서버 거리 재검증 시 감지 반경에 곱하는 여유 배율이다(이동/레이턴시 오차 보정).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Interaction", meta = (ClampMin = "1.0"))
	float ServerDistanceToleranceScale = 1.5f;

	//! \brief 상호작용 시작 시 대상을 바라보는 회전의 보간 속도이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player|Interaction", meta = (ClampMin = "0.0"))
	float FacingInterpSpeed = 10.0f;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* TargetActor, int32 OptionIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestEndInteract();

	UFUNCTION(Client, Reliable)
	void ClientNotifyInteractBegin(AActor* TargetActor, FInteractionStartContext Context);

	UFUNCTION(Client, Reliable)
	void ClientNotifyInteractEnd(AActor* TargetActor);

	void UpdateInteractionCandidates();
	TArray<UInteractableComponent*> FindInteractables() const;
	void SetInteractionCandidates(const TArray<UInteractableComponent*>& NewCandidates);
	ACPP_BalanceScaleElement* FindNearestBalanceScale(EBalanceScaleSide& OutSide) const;
	void SetNearbyBalanceScale(ACPP_BalanceScaleElement* NewBalanceScale, EBalanceScaleSide NewSide);
	void RebuildInteractionEntries();
	void SetCurrentCandidate(UInteractableComponent* NewCandidate);

	//! \brief 통합 목록에서 선택 가능한 첫 항목 인덱스를 찾는다. 없으면 INDEX_NONE.
	int32 FindFirstAvailableInteractionEntry() const;

	//! \brief 통합 목록의 지정 항목이 현재 선택 가능한지 반환한다.
	bool IsInteractionEntryAvailable(int32 EntryIndex) const;

	//! \brief 후보의 옵션 사용 기록이 바뀌면(누군가 일회성 옵션 소진) 선택을 보정하고 가이드 UI 갱신을 유발한다.
	UFUNCTION()
	void HandleCandidateOptionUsageChanged();

	UFUNCTION()
	void HandleWeightCarryStateChanged();

	//! \brief Owner의 지속 운반 상태를 관리하는 별도 컴포넌트다.
	UPROPERTY(Transient)
	TObjectPtr<UWeightCarryComponent> WeightCarryComponent;

	//! \brief 운반 중인 로컬 플레이어가 현재 배치 범위 안에 있는 가장 가까운 저울이다.
	UPROPERTY(Transient)
	TObjectPtr<ACPP_BalanceScaleElement> NearbyBalanceScale;

	//! \brief 현재 감지된 저울에서 플레이어가 들어가 있는 좌우 배치 방향이다.
	EBalanceScaleSide NearbyBalanceScaleSide;

	//! \brief 로컬 감지 반경 안에 있는 모든 상호작용 후보이다. 거리순으로 정렬된다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInteractableComponent>> InteractionCandidates;

	//! \brief 모든 후보의 사용 가능한 옵션을 거리순으로 펼친 가이드 항목 목록이다.
	UPROPERTY(Transient)
	TArray<FPlayerInteractionEntry> InteractionEntries;

	//! \brief 현재 선택된 통합 항목이 속한 상호작용 후보이다. 기존 단일 후보 참조와의 호환을 위해 유지한다.
	UPROPERTY(Transient)
	TObjectPtr<UInteractableComponent> CurrentCandidate;

	//! \brief 확정되어 진행 중인 상호작용 대상이다. 서버는 요청 확정 시, 소유 클라이언트는 통지 수신 시 세팅된다.
	UPROPERTY(Transient)
	TObjectPtr<UInteractableComponent> ActiveInteraction;

	TWeakObjectPtr<UInteractableComponent> LocallyEndedInteractionFromDeath;

	//! \brief true면 새 상호작용 시작이 차단된다. (대화 UI 표시 중 등)
	bool bInteractionBlocked = false;

	bool bOwnerDead = false;

	//! \brief 현재 선택된 통합 상호작용 항목 인덱스. 항목이 없으면 INDEX_NONE.
	int32 SelectedInteractionEntryIndex = INDEX_NONE;

	FTimerHandle DetectionTimerHandle;
};
