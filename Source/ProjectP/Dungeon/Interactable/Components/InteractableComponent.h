////////////////////////////
//! \file InteractableComponent.h
//! \brief 상호작용 가능 액터에 부착하는 범용 상호작용 대상 컴포넌트 선언 파일이다.
//! \editor 준혁 - 상호작용 상태 관리 설계(AI_Docs/InteractionStateManagementDesign.md)에 따라
//!         단일 bInteractionEnabled 판정을 상태·정책 모델(상태머신+동시성/해제/사용 제한)로 확장
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

//! \enum EInteractableState 상호작용 액터의 실행 상태. 복제되어 클라이언트 프롬프트 판정에 사용된다.
UENUM(BlueprintType)
enum class EInteractableState : uint8
{
	Disabled UMETA(DisplayName = "비활성"),  //! 외부 조건(Zone 등)이 충족되지 않아 사용 불가
	Ready    UMETA(DisplayName = "가능"),    //! 새로운 상호작용 승인 가능
	Busy     UMETA(DisplayName = "점유 중"), //! Exclusive 액터를 누군가 점유 중
	Consumed UMETA(DisplayName = "소진"),    //! OnceGlobal 사용 기록 때문에 전체 사용 불가
};

//! \enum EInteractionConcurrencyMode 동시 사용 정책
UENUM(BlueprintType)
enum class EInteractionConcurrencyMode : uint8
{
	Shared    UMETA(DisplayName = "동시 사용"),  //! 여러 플레이어 동시 사용 허용
	Exclusive UMETA(DisplayName = "독점"),       //! 한 명이 점유하면 나머지 플레이어 차단
};

//! \enum EInteractionReleaseMode 점유 해제 정책
UENUM(BlueprintType)
enum class EInteractionReleaseMode : uint8
{
	OnInteractEnd UMETA(DisplayName = "종료 시 자동"), //! UI/Dialogue 종료 시 자동 해제
	Manual        UMETA(DisplayName = "수동 완료"),    //! 액터가 CompleteInteraction()을 호출할 때 해제
	Immediate     UMETA(DisplayName = "시작 즉시 완료"), //! 승인 이벤트 실행 직후 세션을 즉시 정리
};

//! \enum EInteractionUsageMode 사용 제한 정책
UENUM(BlueprintType)
enum class EInteractionUsageMode : uint8
{
	Unlimited     UMETA(DisplayName = "무제한"),
	OnceGlobal    UMETA(DisplayName = "전체 1회"),      //! 액터 전체에서 한 번
	OncePerPlayer UMETA(DisplayName = "플레이어별 1회"), //! 인증 사용자마다 한 번
};

//! \enum EInteractionRejectReason 서버 승인 실패 사유. 서버 로그, UI 피드백, 자동화 테스트에서 사용한다.
UENUM(BlueprintType)
enum class EInteractionRejectReason : uint8
{
	None,
	Disabled,
	Busy,
	Cooldown,
	ConsumedGlobal,
	ConsumedForPlayer,
	InvalidInteractor,
	MissingUserId,
	InvalidOption, //! 요청한 상호작용 옵션 인덱스가 옵션 목록 범위 밖
	OptionLocked,  //! 선행 옵션이 아직 사용되지 않아 잠겨 있는 옵션
};

//! \struct FInteractionOption 상호작용 가이드 UI에 표시되는 상호작용 옵션 하나.
//!         컴포넌트는 표시 정보만 알고, 각 인덱스가 무슨 동작인지는 소유 액터가 해석한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FInteractionOption
{
	GENERATED_BODY()

	//! 가이드 UI에 표시할 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayText;

	//! 이 옵션의 사용 제한. 전체 1회/플레이어별 1회 옵션은 소진되면 가이드 UI에서 사라진다.
	//! (액터 단위 UsageMode와 별개로 옵션 단위로 판정한다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	EInteractionUsageMode UsageMode = EInteractionUsageMode::Unlimited;

	//! 이 옵션이 표시되기 위한 선행 옵션 인덱스(-1=없음). 선행 옵션이 한 번 사용된 후에만 가이드에 나타난다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "-1"))
	int32 PrerequisiteOptionIndex = INDEX_NONE;

	//! true면 선행 옵션을 직접 사용한 플레이어에게만 표시, false면 파티 누구든 사용하면 전원에게 표시된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bPrerequisitePerPlayer = false;
};

//! \struct FInteractionOptionUseRecord 옵션×사용자 사용 기록 한 건.
//!         클라이언트가 소진된 옵션을 가이드에서 숨길 수 있도록 복제된다. (파티 3명×옵션 몇 개라 배열 복제로 충분)
USTRUCT()
struct FInteractionOptionUseRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 OptionIndex = INDEX_NONE;

	UPROPERTY()
	int32 UserId = INDEX_NONE;
};

//! \struct FInteractionCooldownRecord 사용자별 상호작용 쿨타임 종료 시각 한 건.
//!         서버 시간을 기준으로 판정해 다른 사용자의 상호작용에는 영향을 주지 않는다.
USTRUCT()
struct FInteractionCooldownRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 UserId = INDEX_NONE;

	UPROPERTY()
	double CooldownEndServerTime = 0.0;
};

//! \struct FInteractionStartContext 서버가 승인한 상호작용 시작 정보.
//!         Shared 모드에서는 여러 요청이 동시에 존재할 수 있어 컴포넌트 임시 변수로 두면
//!         마지막 요청 정보가 덮어써질 수 있으므로, 이벤트마다 값으로 전달한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FInteractionStartContext
{
	GENERATED_BODY()

	//! 승인된 플레이어 액터
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	//! 인증 사용자 ID (AMyPlayerState::GetUserIndex, 없으면 -1)
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 InteractorUserId = INDEX_NONE;

	//! 액터 전체 최초 승인 여부
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bFirstGlobalInteraction = false;

	//! 해당 사용자 최초 승인 여부
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bFirstForInteractor = false;

	//! 승인된 상호작용 옵션 인덱스. 옵션 목록이 비어 있는 액터는 항상 0이다.
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 SelectedOptionIndex = 0;

	//! 이 옵션이 액터 전체에서 처음 승인됐는지. (액터 단위 bFirstGlobalInteraction과 달리 옵션별 판정)
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bFirstGlobalForOption = false;

	//! 이 옵션이 해당 사용자에게 처음 승인됐는지. (액터 단위 bFirstForInteractor와 달리 옵션별 판정)
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bFirstForInteractorForOption = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionStartedSignature, const FInteractionStartContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionEndedSignature, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractableStateChangedSignature, EInteractableState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractionOptionUsageChangedSignature);

////////////////////////////
//! \class UInteractableComponent
//! \author HanUl
//! \editor 준혁 - 서버 권위 상태 관리자(활성 Gate·점유·사용 기록·원자적 승인)로 확장
//! \brief 상호작용 액터의 범용 서버 권위 상태 관리자다. 활성/비활성, 동시 사용, 점유 해제,
//!        사용 제한, 최초 판정, 승인된 시작·종료 이벤트 중계, 프롬프트용 최소 상태 복제를 담당한다.
//! \note 실제 상호작용 내용(카메라 연출, UI, 기믹 동작 등)은 소유 액터가
//!       OnInteractionStarted(서버) / OnLocalInteractionStarted(상호작용자 로컬)에 바인딩해 구현한다.
//!       Dialogue, 파티, 상점 UI, 기믹 클래스는 알지 않는다.
//!       복제 상태가 필요한 소유 액터는 bReplicates = true여야 한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTP_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//! \brief 해당 Interactor가 지금 상호작용할 수 있는지 판정한다.
	//!        클라이언트에서는 복제 상태 기반의 프롬프트 사전 판정이며 최종 승인이 아니다.
	UFUNCTION(BlueprintPure, Category = "Interaction")
	virtual bool CanInteract(const AActor* Interactor) const;

	//! \brief [서버] 상호작용 시작을 원자적으로 승인한다. 검사와 상태 변경(점유 획득·사용 기록)을
	//!        한 함수 안에서 끝내므로 동시에 도착한 두 독점 요청 중 하나만 점유를 얻는다.
	//! \param Interactor 요청한 플레이어 액터
	//! \param OutContext 승인 성공 시 채워지는 시작 Context
	//! \param OutRejectReason 실패 사유 (성공 시 None)
	//! \param OptionIndex 요청한 상호작용 옵션 인덱스. 옵션 목록이 비어 있으면 0만 허용된다.
	//! \return 승인 여부
	bool TryBeginInteraction(AActor* Interactor, FInteractionStartContext& OutContext, EInteractionRejectReason& OutRejectReason, int32 OptionIndex = 0);

	//! \brief 소유 액터가 자신의 상호작용 옵션 목록을 등록한다. BeginPlay에서 서버/클라 양쪽 모두 호출한다.
	//!        (에디터 배치 데이터 기반이라 복제하지 않고 양쪽에서 동일하게 구성한다)
	//!        잘못된 선행 옵션 인덱스(범위 밖/자기 자신)는 경고를 남기고 잠금 없이 동작한다.
	UFUNCTION(BlueprintCallable, Category = "Interaction|Options")
	void SetInteractionOptions(const TArray<FInteractionOption>& InOptions);

	//! \brief 소유 액터가 등록한 상호작용 옵션 목록을 반환한다. (등록 전이면 빈 배열)
	UFUNCTION(BlueprintPure, Category = "Interaction|Options")
	const TArray<FInteractionOption>& GetInteractionOptions() const { return InteractionOptions; }

	UFUNCTION(BlueprintPure, Category = "Interaction|Options")
	int32 GetInteractionOptionCount() const { return InteractionOptions.Num(); }

	//! \brief 가이드 UI에 표시할 유효 옵션 목록을 반환한다. 등록 옵션이 없으면 DefaultInteractionText로
	//!        기본 옵션 1개를 합성한다 — 상호작용 가능한 모든 액터 근처에서 가이드 UI가 항상 뜨도록 보장한다.
	UFUNCTION(BlueprintPure, Category = "Interaction|Options")
	TArray<FInteractionOption> GetEffectiveInteractionOptions() const;

	//! \brief 유효 옵션 개수를 반환한다. 항상 1 이상이다. (소진 여부와 무관한 전체 개수)
	UFUNCTION(BlueprintPure, Category = "Interaction|Options")
	int32 GetEffectiveOptionCount() const { return FMath::Max(InteractionOptions.Num(), 1); }

	//! \brief 해당 Interactor가 지금 이 옵션을 선택할 수 있는지(소진되지 않았는지) 판정한다.
	//!        복제된 사용 기록 기반이라 클라이언트 가이드 UI 필터링에도 사용할 수 있다.
	UFUNCTION(BlueprintPure, Category = "Interaction|Options")
	bool IsOptionAvailable(int32 OptionIndex, const AActor* Interactor) const;

	//! \brief [서버] 일반 UI/Dialogue 종료 요청. OnInteractEnd 모드면 점유도 해제한다. 중복 요청은 멱등.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndInteraction(AActor* Interactor);

	//! \brief [서버] Manual 액터의 정상적인 콘텐츠 완료 지점. 요청 사용자가 현재 점유자일 때만 점유를 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CompleteInteraction(AActor* Interactor);

	//! \brief [서버] 접속 종료·Pawn 파괴·데이터 오류·연출 실패·Zone 강제 리셋용 비정상 중단.
	//!        ReleaseMode와 관계없이 점유와 활성 참조를 정리해 영구 Busy를 방지한다.
	//!        최초/사용 기록은 되돌리지 않는다(최초 이벤트 중복 방지).
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void AbortInteraction(AActor* Interactor);

	//! \brief [서버] 외부 활성화 Gate를 켜고 끈다. 새로운 요청만 즉시 차단하며 진행 중 UI/연출은 강제로 닫지 않는다.
	//!        진행 중 작업도 취소해야 할 때는 호출자가 AbortInteraction()을 명시적으로 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	//! \brief [서버] Zone 재시작 등에서 점유·활성 참조·전체/사용자 사용 기록을 초기 상태로 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ResetInteractionState();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	EInteractableState GetInteractableState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	EInteractionConcurrencyMode GetConcurrencyMode() const { return ConcurrencyMode; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	EInteractionReleaseMode GetReleaseMode() const { return ReleaseMode; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetReleaseMode(EInteractionReleaseMode InReleaseMode);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	EInteractionUsageMode GetUsageMode() const { return UsageMode; }

	//! \brief [서버] 해당 액터가 현재 승인된 활성 상호작용자인지 반환한다. (상점 구매 RPC 검증 등)
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractorActive(const AActor* Interactor) const;

	//! \brief 해당 인증 사용자가 이 액터와 상호작용한 기록이 있는지 반환한다. (복제 배열 기반, 클라에서도 판정 가능)
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool HasUserInteracted(int32 UserId) const { return UserId >= 0 && UsedUserIds.Contains(UserId); }

	//! \brief 상호작용한 플레이어의 클라이언트에서 로컬 시작 이벤트를 중계한다. 서버 Context를 그대로 전달한다.
	void HandleLocalInteractBegin(const FInteractionStartContext& Context);

	//! \brief 상호작용한 플레이어의 클라이언트에서 로컬 종료 이벤트를 중계한다.
	void HandleLocalInteractEnd(AActor* Interactor);

	//! \brief 서버의 쿨타임 복제를 기다리는 동안 가이드가 다시 나타나지 않도록 로컬 쿨타임을 선반영한다.
	//! \param Interactor 상호작용을 종료하는 플레이어 액터
	//! \return 로컬 쿨타임을 선반영했으면 true
	bool PredictLocalInteractionCooldown(const AActor* Interactor);

	//! \brief [서버] 승인된 상호작용 시작이 확정됐을 때 발화한다. 월드에 영향을 주는 동작(상점 목록, 오벨리스크 응답 등)을 여기에 바인딩한다.
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionStartedSignature OnInteractionStarted;

	//! \brief [서버] 상호작용 종료(일반 종료·비정상 중단 공통)가 확정됐을 때 발화한다. 시작 시 벌인 월드 동작의 정리를 여기에 바인딩한다.
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionEndedSignature OnInteractionEnded;

	//! \brief 상호작용한 플레이어의 클라이언트에서 발화한다. 로컬 연출(카메라, UI 등)을 여기에 바인딩한다.
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionStartedSignature OnLocalInteractionStarted;

	//! \brief 상호작용한 플레이어의 클라이언트에서 종료 시 발화한다. 로컬 연출 복귀(카메라, UI 닫기)를 여기에 바인딩한다.
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionEndedSignature OnLocalInteractionEnded;

	//! \brief 실행 상태가 바뀔 때 발화한다(서버 즉시, 클라이언트 OnRep). 프롬프트/연출 갱신용.
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractableStateChangedSignature OnInteractableStateChanged;

	//! \brief 옵션 사용 기록이 바뀔 때 발화한다(서버 즉시, 클라이언트 OnRep). 소진된 옵션을 가이드 UI에서 숨기는 갱신용.
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Options")
	FInteractionOptionUsageChangedSignature OnInteractionOptionUsageChanged;

protected:
	virtual void BeginPlay() override;

	//! \brief [테스트 주입 seam] Interactor의 인증 사용자 ID를 해석한다. 기본 구현은 AMyPlayerState::GetUserIndex.
	//!        production 코드는 오버라이드하지 않으며 자동화 테스트 프로브만 오버라이드한다.
	virtual int32 ResolveInteractorUserId(const AActor* Interactor) const;

	//! \brief 동시 사용 정책
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Policy")
	EInteractionConcurrencyMode ConcurrencyMode = EInteractionConcurrencyMode::Shared;

	//! \brief 점유 해제 정책. Shared + Manual은 미지원 조합이며 OnInteractEnd로 동작하고 경고를 남긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Policy")
	EInteractionReleaseMode ReleaseMode = EInteractionReleaseMode::OnInteractEnd;

	//! \brief 사용 제한 정책
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Policy")
	EInteractionUsageMode UsageMode = EInteractionUsageMode::Unlimited;

	//! \brief 시작 시 상호작용 가능 여부. Zone이 개방 시점을 제어하는 액터(오벨리스크 등)는 false로 배치한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Policy")
	bool bStartInteractionEnabled = true;

	//! \brief 상호작용 종료 후 같은 플레이어만 다시 시작할 수 없는 시간(초). 0이면 쿨타임을 적용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Policy", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float InteractionCooldown = 0.0f;

	//! \brief 옵션을 등록하지 않는 액터(상점, 텔레포트 등)가 가이드 UI에 표시할 기본 옵션 이름.
	//!        인스턴스별로 지정한다(상점="상점" 등). 소유 액터가 SetInteractionOptions로 옵션을 등록하면 무시된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Options")
	FText DefaultInteractionText;

	//! \brief 상호작용 옵션 목록. 소유 액터가 BeginPlay에서 SetInteractionOptions로 채운다(런타임 전용).
	//!        에디터에 노출하지 않는다 — 소유 액터가 덮어쓰므로 입력 지점이 두 곳이 되면 혼란만 준다.
	//!        비어 있으면 가이드 UI에는 DefaultInteractionText 기본 옵션 1개가 표시되고 승인은 0번만 허용된다.
	UPROPERTY(Transient)
	TArray<FInteractionOption> InteractionOptions;

private:
	//! \brief 실행 상태. 복제되어 클라이언트 프롬프트 판정과 상태 연출에 사용된다.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
	EInteractableState CurrentState = EInteractableState::Ready;

	//! \brief [서버] 외부 활성화 Gate. BeginPlay에서 bStartInteractionEnabled로 초기화된다.
	bool bInteractionGateOpen = true;

	//! \brief [서버] SetInteractionEnabled로 Gate가 명시적으로 설정됐는지. 액터 간 BeginPlay 순서가 보장되지 않으므로
	//!        Zone 어댑터가 먼저 잠근 Gate를 이 컴포넌트의 BeginPlay 초기값이 덮어쓰지 않게 한다.
	bool bGateExplicitlySet = false;

	//! \brief [서버] 액터 전체에서 상호작용이 한 번이라도 승인됐는지. 최초 전체 판정과 OnceGlobal에 사용된다.
	bool bAnyInteractionOccurred = false;

	//! \brief 상호작용을 승인받은 인증 사용자 ID 기록. 파티가 최대 3명이므로 일반 배열 복제로 충분하다.
	//!        클라이언트의 OncePerPlayer 프롬프트 판정에 사용된다. 사망·리스폰·재접속과 무관하게 유지된다.
	UPROPERTY(Replicated)
	TArray<int32> UsedUserIds;

	//! \brief 사용된 적 있는 옵션 인덱스 기록. 옵션별 전체 최초 판정과 전체 1회 옵션 소진 판정에 사용한다.
	//!        클라이언트가 소진 옵션을 가이드에서 숨길 수 있도록 복제된다.
	UPROPERTY(ReplicatedUsing = OnRep_OptionUsage)
	TArray<int32> UsedOptionIndices;

	//! \brief 옵션×사용자 사용 기록. 옵션별 사용자 최초 판정과 플레이어별 1회 옵션 소진 판정에 사용한다. 복제된다.
	UPROPERTY(ReplicatedUsing = OnRep_OptionUsage)
	TArray<FInteractionOptionUseRecord> OptionUseRecords;

	//! \brief 사용자별 상호작용 쿨타임 종료 시각. 해당 사용자 클라이언트의 후보 사전 판정에 사용되도록 복제한다.
	UPROPERTY(Replicated)
	TArray<FInteractionCooldownRecord> InteractionCooldownRecords;

	//! \brief 서버 쿨타임 복제 전 가이드 재노출을 막는 사용자별 로컬 예측 종료 시각. 서버 권한 판정에는 사용하지 않는다.
	TMap<int32, double> PredictedInteractionCooldownEndTimes;

	//! \brief [서버] 현재 승인되어 진행 중인 상호작용자 목록. 복제하지 않는다.
	TArray<TWeakObjectPtr<AActor>> ActiveInteractors;

	//! \brief [서버] Exclusive 점유자. 복제하지 않는다.
	TWeakObjectPtr<AActor> ExclusiveOccupant;

	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_OptionUsage();

	//! \brief 해당 사용자가 이 옵션을 사용한 기록이 있는지 반환한다.
	bool HasUserUsedOption(int32 OptionIndex, int32 UserId) const;

	//! \brief 이 옵션의 선행 옵션 조건이 충족됐는지 반환한다. 잘못된 선행 인덱스는 잠금 없이 취급한다.
	bool IsOptionPrerequisiteMet(int32 OptionIndex, int32 UserId) const;

	//! \brief 해당 사용자의 개인 상호작용 쿨타임이 진행 중인지 반환한다.
	bool IsUserInteractionCooldownActive(int32 UserId) const;

	//! \brief 상호작용을 종료한 사용자의 개인 쿨타임을 시작한다.
	void StartInteractionCooldown(const AActor* Interactor);

	//! \brief 서버에서 만료된 개인 쿨타임 기록을 제거한다.
	void PruneExpiredInteractionCooldowns();

	//! \brief 서버와 클라이언트가 공통으로 비교할 동기화된 서버 시간을 반환한다.
	double GetInteractionServerTimeSeconds() const;

	bool HasOwnerAuthority() const;
	EInteractionReleaseMode GetEffectiveReleaseMode() const;
	void PruneInvalidReferences();
	void RemoveActiveInteractor(const AActor* Interactor);
	void RecomputeState();
};
