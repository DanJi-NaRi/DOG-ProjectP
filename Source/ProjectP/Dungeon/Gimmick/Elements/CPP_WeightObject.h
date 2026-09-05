////////////////////////////
//! \file CPP_WeightObject.h
//! \brief 저울 기믹에서 사용하는 전용 무게추 오브젝트 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Interactable/Components/InteractableComponent.h"
#include "CPP_WeightObject.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UWeightCarryComponent;
class ACPP_BalanceScaleElement;

////////////////////////////
//! \class ACPP_WeightObject
//! \brief 상호작용한 플레이어 앞에 부착되어 운반되고, 일반 해제 시 맵 최초 배치 위치로 복귀하는 전용 무게추다.
//!        저울은 물리 Mass가 아니라 이 클래스의 WeightValue만 판정에 사용한다.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_WeightObject : public AActor
{
    GENERATED_BODY()

public:
    ACPP_WeightObject();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Gimmick|Weight")
    int32 GetWeightValue() const { return WeightValue; }

    UFUNCTION(BlueprintPure, Category = "Gimmick|Weight")
    bool IsBeingCarried() const { return CurrentCarrier != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Gimmick|Weight")
    AActor* GetCurrentCarrier() const { return CurrentCarrier; }

    //! 로컬 상호작용 UI가 이 무게추를 선택했는지에 따라 WeightMesh의 스텐실 강조를 전환한다.
    void SetInteractionFocusHighlighted(bool bHighlighted);

protected:
    virtual void BeginPlay() override;

    //! 무게추 운반 상태가 바뀌었을 때 서버와 클라이언트에서 실행할 블루프린트 연출 이벤트다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Weight|FX")
    void OnCarryStateChanged(bool bIsCarried, AActor* Carrier);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Weight")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Weight")
    TObjectPtr<UStaticMeshComponent> WeightMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Weight")
    TObjectPtr<UInteractableComponent> InteractableComponent;

    //! 플레이어 RootComponent를 기준으로 무게추가 떠 있을 상대 위치다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gimmick|Weight|Carry")
    FVector CarryOffset = FVector(150.0f, 0.0f, 80.0f);

    //! 플레이어 RootComponent를 기준으로 운반 중 적용할 상대 회전이다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gimmick|Weight|Carry")
    FRotator CarryRotation = FRotator::ZeroRotator;

private:
    friend class UWeightCarryComponent;
    friend class ACPP_BalanceScaleElement;

    //! 저울 합산에 사용하는 양의 정수 무게다. 물리 Mass와 무관하다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Weight", meta = (ClampMin = "1", UIMin = "1", AllowPrivateAccess = "true"))
    int32 WeightValue = 1;

    //! 현재 운반자다. 서버가 결정하고 클라이언트 연출 갱신을 위해 복제한다.
    UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier)
    TObjectPtr<AActor> CurrentCarrier;

    //! 맵에 처음 배치된 월드 Transform이다. 일반 해제 시 항상 이 위치로 복귀한다.
    FTransform InitialWorldTransform;

    //! 현재 무게추가 점유 중인 저울이다. 서버 슬롯 해제에만 사용한다.
    UPROPERTY(Transient)
    TObjectPtr<ACPP_BalanceScaleElement> CurrentScale;

    //! 시작 배치 슬롯에 고정되어 운반과 슬롯 제거가 금지된 무게추인지 나타낸다.
    bool bLockedToScale = false;

    //! 상호작용 UI 포커스가 해제될 때 복구할 WeightMesh의 최초 스텐실 값이다.
    int32 InitialCustomDepthStencilValue = 0;

    //! 로컬 상호작용 UI에서 현재 선택된 무게추인지 나타낸다.
    bool bInteractionFocusHighlighted = false;

    ECollisionEnabled::Type InitialMeshCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

    bool bInitialMeshSimulatesPhysics = false;

    UFUNCTION()
    void HandleInteractionStarted(const FInteractionStartContext& Context);

    UFUNCTION()
    void OnRep_CurrentCarrier();

    void BeginCarry(AActor* Interactor);
    bool SnapToScale(ACPP_BalanceScaleElement* ScaleElement, USceneComponent* SnapSlot, AActor* Interactor);
    bool LockToInitialScaleSlot(ACPP_BalanceScaleElement* ScaleElement, USceneComponent* SnapSlot);
    bool ReleaseFromCarrier(AActor* Interactor);
    void ReturnToInitialTransform();
    void ApplyCarryStateFX();
    void RefreshCustomDepthStencilValue();
};
