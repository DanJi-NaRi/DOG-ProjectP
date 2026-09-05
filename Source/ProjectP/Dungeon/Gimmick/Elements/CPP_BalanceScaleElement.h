////////////////////////////
//! \file CPP_BalanceScaleElement.h
//! \brief 좌우 각 5개의 고정 스냅 슬롯을 제공하는 저울 기믹 요소 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "../CPP_GimmickElementBase.h"
#include "CPP_BalanceScaleElement.generated.h"

class USceneComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class ACPP_WeightObject;

//! \enum EBalanceScaleSide 저울에서 무게추를 배치할 좌우 방향이다.
UENUM(BlueprintType)
enum class EBalanceScaleSide : uint8
{
    Left UMETA(DisplayName = "왼쪽"),
    Right UMETA(DisplayName = "오른쪽"),
};

//! \enum EBalanceScalePlacementResult 서버의 무게추 슬롯 배치 판정 결과다.
UENUM(BlueprintType)
enum class EBalanceScalePlacementResult : uint8
{
    Success,
    InvalidRequest,
    OutOfRange,
    SideFull,
};

//! \enum EBalancePanHeightState 저울 한쪽 모델의 수직 높이 상태다.
UENUM(BlueprintType)
enum class EBalancePanHeightState : uint8
{
    Down UMETA(DisplayName = "내려감"),
    SlightlyDown UMETA(DisplayName = "조금 내려감"),
    Level UMETA(DisplayName = "수평"),
    SlightlyUp UMETA(DisplayName = "조금 올라감"),
    Up UMETA(DisplayName = "올라감"),
};

//! \struct FBalanceScaleReplicatedState 좌우 총무게와 대응 높이 상태를 원자적으로 복제하는 데이터다.
USTRUCT(BlueprintType)
struct PROJECTP_API FBalanceScaleReplicatedState
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    int32 LeftTotalWeight = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    int32 RightTotalWeight = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    EBalancePanHeightState LeftHeightState = EBalancePanHeightState::Level;

    UPROPERTY(BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    EBalancePanHeightState RightHeightState = EBalancePanHeightState::Level;
};

////////////////////////////
//! \class ACPP_BalanceScaleElement
//! \brief 저울 프레임과 중심축, 좌우 접시, 방향별 배치 판정 범위, 좌우 각 5개의 스냅 위치를 제공한다.
//!        서버에서 슬롯 점유와 배치·회수, 무게 합산 및 좌우 5단계 높이 상태를 관리한다.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_BalanceScaleElement : public ACPP_GimmickElementBase
{
    GENERATED_BODY()

public:
    ACPP_BalanceScaleElement();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void ResetElement() override;

    //! 지정 액터가 저울의 지정 방향 배치 가능 범위 안에 있는지 반환한다.
    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Interaction")
    bool IsActorInsidePlacementRange(const AActor* Actor, EBalanceScaleSide Side) const;

    //! 지정 방향 배치 범위의 월드 중심 위치를 반환한다.
    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Interaction")
    FVector GetPlacementRangeCenter(EBalanceScaleSide Side) const;

    //! 지정 방향과 인덱스에 해당하는 고정 스냅 위치를 반환한다.
    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Slot")
    USceneComponent* GetSnapSlot(EBalanceScaleSide Side, int32 SlotIndex) const;

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Slot")
    int32 GetSlotCountPerSide() const { return SlotsPerSide; }

    //! 지정 방향에 현재 점유된 슬롯 수를 반환한다.
    int32 GetOccupiedSlotCount(EBalanceScaleSide Side) const;

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Weight")
    int32 GetLeftTotalWeight() const { return BalanceState.LeftTotalWeight; }

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Weight")
    int32 GetRightTotalWeight() const { return BalanceState.RightTotalWeight; }

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Height")
    EBalancePanHeightState GetLeftHeightState() const { return BalanceState.LeftHeightState; }

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Height")
    EBalancePanHeightState GetRightHeightState() const { return BalanceState.RightHeightState; }

    //! 서버에서 운반 중인 무게추를 지정 방향의 첫 빈 슬롯에 배치한다.
    EBalanceScalePlacementResult TrySnapWeight(
        ACPP_WeightObject* WeightObject,
        EBalanceScaleSide Side,
        AActor* RequestingActor);

    //! 서버에서 저울 위 무게추의 기존 슬롯 점유를 해제한다.
    bool RemoveWeight(ACPP_WeightObject* WeightObject);

    virtual void SetGimmickInteractionEnabled(bool bEnabled) override;

    UFUNCTION(BlueprintPure, Category = "Gimmick|BalanceScale|Interaction")
    bool IsWeightPlacementEnabled() const { return bGimmickInteractionEnabled; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    //! 무게 합산 결과가 바뀐 뒤 서버와 클라이언트에서 실행할 추가 블루프린트 연출 이벤트다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|BalanceScale|Height")
    void OnPanHeightStateChanged(
        EBalancePanHeightState LeftState,
        EBalancePanHeightState RightState,
        int32 LeftTotalWeight,
        int32 RightTotalWeight);

    //! 액터의 이동·회전 기준이 되는 루트다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    TObjectPtr<USceneComponent> SceneRoot;

    //! 움직이지 않는 저울 받침대 메시다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    TObjectPtr<UStaticMeshComponent> FrameMesh;

    //! 몸통과 좌우 접시의 공통 기준점이다. 좌우 높이 변화는 각 PanRoot의 Z 이동으로 적용한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    TObjectPtr<USceneComponent> BeamPivot;

    //! 중심축을 시각화할 저울대 메시다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale")
    TObjectPtr<UStaticMeshComponent> BeamMesh;

    //! 왼쪽 접시와 왼쪽 슬롯의 공통 기준점이다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Left")
    TObjectPtr<USceneComponent> LeftPanRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Left")
    TObjectPtr<UStaticMeshComponent> LeftPanMesh;

    //! 플레이어가 왼쪽 배치 선택지를 볼 수 있는 박스 판정 범위다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Left")
    TObjectPtr<UBoxComponent> LeftPlacementRange;

    //! 오른쪽 접시와 오른쪽 슬롯의 공통 기준점이다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Right")
    TObjectPtr<USceneComponent> RightPanRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Right")
    TObjectPtr<UStaticMeshComponent> RightPanMesh;

    //! 플레이어가 오른쪽 배치 선택지를 볼 수 있는 박스 판정 범위다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Right")
    TObjectPtr<UBoxComponent> RightPlacementRange;

    //! 이 저울 기믹에 속하며 활성 상태를 함께 적용할 무게추 액터들이다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Interaction")
    TArray<TObjectPtr<ACPP_WeightObject>> WeightObjects;

    //! 게임 시작부터 왼쪽 슬롯에 배열 순서대로 고정할 레벨 배치 무게추들이다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Initial Placement")
    TArray<TObjectPtr<ACPP_WeightObject>> InitialLeftWeightObjects;

    //! 게임 시작부터 오른쪽 슬롯에 배열 순서대로 고정할 레벨 배치 무게추들이다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Initial Placement")
    TArray<TObjectPtr<ACPP_WeightObject>> InitialRightWeightObjects;

    //! 시작 배치 무게추의 Materials Element 0에 적용할 비활성 머테리얼이다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Initial Placement")
    TObjectPtr<UMaterialInterface> InitialPlacementOffMaterial;

    //! 블루프린트에서 위치를 조정할 수 있는 왼쪽 고정 스냅 슬롯 5개다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Left")
    TArray<TObjectPtr<USceneComponent>> LeftSnapSlots;

    //! 블루프린트에서 위치를 조정할 수 있는 오른쪽 고정 스냅 슬롯 5개다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Right")
    TArray<TObjectPtr<USceneComponent>> RightSnapSlots;

    //! 무게 차이가 이 값 이상이면 조금 상태가 아니라 완전히 내려감/올라감 상태를 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Height", meta = (ClampMin = "1", UIMin = "1"))
    int32 FullHeightWeightDifference = 5;

    //! 완전히 내려감/올라감 상태에서 최초 위치에 더할 Z 높이 절댓값이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Height", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float FullHeightOffset = 50.0f;

    //! 현재 높이에서 새 목표 높이까지 선형 이동하는 시간이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|BalanceScale|Height", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
    float PanMoveDuration = 0.5f;

private:
    TArray<TObjectPtr<ACPP_WeightObject>>& GetSlotOccupants(EBalanceScaleSide Side);
    const TArray<TObjectPtr<ACPP_WeightObject>>& GetSlotOccupants(EBalanceScaleSide Side) const;
    int32 FindFirstEmptySlotIndex(EBalanceScaleSide Side);
    void PruneInvalidSlotOccupants();
    void CollectEffectiveWeightObjects(
        EBalanceScaleSide Side,
        TSet<const ACPP_WeightObject*>& OutWeightObjects) const;
    int32 CalculateTotalWeight(EBalanceScaleSide Side) const;
    void RefreshBalanceState();
    void ApplyBalanceState();
    float CalculateHeightOffset(int32 SideTotalWeight, int32 OtherSideTotalWeight) const;
    void ApplyInitialPlacementMaterial();
    void InitializeLockedWeightPlacements();
    void PlaceInitialLockedWeights(
        const TArray<TObjectPtr<ACPP_WeightObject>>& InitialWeights,
        EBalanceScaleSide Side);

    UFUNCTION()
    void OnRep_BalanceState();

    //! 서버에서만 관리하는 왼쪽 슬롯 점유 무게추 목록이다.
    UPROPERTY(Transient)
    TArray<TObjectPtr<ACPP_WeightObject>> LeftSlotOccupants;

    //! 서버에서만 관리하는 오른쪽 슬롯 점유 무게추 목록이다.
    UPROPERTY(Transient)
    TArray<TObjectPtr<ACPP_WeightObject>> RightSlotOccupants;

    //! 서버가 계산하고 클라이언트가 동일한 높이를 적용하도록 복제하는 좌우 무게·높이 묶음이다.
    UPROPERTY(ReplicatedUsing = OnRep_BalanceState)
    FBalanceScaleReplicatedState BalanceState;

    //! 클라이언트 UI와 서버 배치 요청이 함께 사용하는 기믹 상호작용 Gate다.
    UPROPERTY(Replicated)
    bool bGimmickInteractionEnabled = false;

    FVector InitialLeftPanRelativeLocation = FVector::ZeroVector;
    FVector InitialRightPanRelativeLocation = FVector::ZeroVector;
    FVector PanTransitionStartLeftLocation = FVector::ZeroVector;
    FVector PanTransitionStartRightLocation = FVector::ZeroVector;
    FVector PanTransitionTargetLeftLocation = FVector::ZeroVector;
    FVector PanTransitionTargetRightLocation = FVector::ZeroVector;
    float PanTransitionElapsedTime = 0.0f;
    bool bInitialPanLocationsCached = false;
    bool bPanTransitionActive = false;

    static constexpr int32 SlotsPerSide = 5;
};
