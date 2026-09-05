////////////////////////////
//! \file WeightCarryComponent.h
//! \brief 플레이어의 무게추 운반 상태를 일반 상호작용 세션과 분리해 관리하는 컴포넌트 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dungeon/Gimmick/Elements/CPP_BalanceScaleElement.h"
#include "WeightCarryComponent.generated.h"

class ACPP_WeightObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeightCarryStateChangedSignature);

////////////////////////////
//! \class UWeightCarryComponent
//! \brief 플레이어 한 명이 운반할 수 있는 무게추 하나를 서버 권위로 관리한다.
//!        운반 상태는 일반 ActiveInteraction과 독립적이므로 무게추를 든 채 다른 액터와 상호작용할 수 있다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTP_API UWeightCarryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeightCarryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Player|WeightCarry")
    bool HasCarriedWeight() const { return CarriedWeightObject != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Player|WeightCarry")
    ACPP_WeightObject* GetCarriedWeightObject() const { return CarriedWeightObject; }

    UFUNCTION(BlueprintPure, Category = "Player|WeightCarry")
    FText GetReleaseInteractionText() const { return ReleaseInteractionText; }

    UFUNCTION(BlueprintPure, Category = "Player|WeightCarry")
    FText GetPlaceLeftInteractionText() const { return PlaceLeftInteractionText; }

    UFUNCTION(BlueprintPure, Category = "Player|WeightCarry")
    FText GetPlaceRightInteractionText() const { return PlaceRightInteractionText; }

    //! 서버가 승인한 무게추를 빈 운반 슬롯에 등록한다.
    bool TryBeginCarry(ACPP_WeightObject* WeightObject);

    //! 소유 플레이어가 현재 무게추를 최초 배치 위치로 돌려보내도록 요청한다.
    UFUNCTION(BlueprintCallable, Category = "Player|WeightCarry")
    void RequestReleaseCarriedWeight();

    //! 소유 플레이어가 현재 무게추를 저울의 지정 방향에 배치하도록 요청한다.
    UFUNCTION(BlueprintCallable, Category = "Player|WeightCarry")
    void RequestPlaceCarriedWeight(ACPP_BalanceScaleElement* ScaleElement, EBalanceScaleSide Side);

    //! Owner의 사망 상태를 반영하고, 서버에서 사망한 운반자의 무게추를 최초 위치로 복귀시킨다.
    void HandleOwnerLifeStateChanged(bool bDead);

    //! 기믹 종료 시 지정 무게추가 현재 운반 중이면 서버에서 최초 위치로 강제 복귀시킨다.
    void ForceReturnCarriedWeightForGimmick(ACPP_WeightObject* WeightObject);

    UPROPERTY(BlueprintAssignable, Category = "Player|WeightCarry")
    FWeightCarryStateChangedSignature OnWeightCarryStateChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    //! 통합 상호작용 가이드에서 운반 중인 무게추에 표시할 기본 해제 문구다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|WeightCarry")
    FText ReleaseInteractionText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|WeightCarry")
    FText PlaceLeftInteractionText;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|WeightCarry")
    FText PlaceRightInteractionText;

    //! 선택한 방향의 5개 슬롯이 모두 찼을 때 요청 플레이어에게 표시할 Notice다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|WeightCarry|Notice")
    FText SideFullNoticeText;

    //! 무게추 없이 저울 배치를 시도했을 때 요청 플레이어에게 표시할 Notice다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|WeightCarry|Notice")
    FText NoCarriedWeightNoticeText;

private:
    UFUNCTION(Server, Reliable)
    void ServerRequestReleaseCarriedWeight();

    UFUNCTION(Server, Reliable)
    void ServerRequestPlaceCarriedWeight(ACPP_BalanceScaleElement* ScaleElement, EBalanceScaleSide Side);

    UFUNCTION()
    void OnRep_CarriedWeightObject();

    void ReleaseCarriedWeightToInitialTransform();
    void PlaceCarriedWeightOnScale(ACPP_BalanceScaleElement* ScaleElement, EBalanceScaleSide Side);
    void SendSideFullNotice() const;
    void SendNoCarriedWeightNotice() const;

    //! 현재 운반 중인 무게추다. UI가 필요한 소유 클라이언트에만 복제한다.
    UPROPERTY(ReplicatedUsing = OnRep_CarriedWeightObject)
    TObjectPtr<ACPP_WeightObject> CarriedWeightObject;
};
