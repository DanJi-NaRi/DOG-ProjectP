////////////////////////////
//! \file WeightCarryComponent.cpp
//! \brief 플레이어의 서버 권위 무게추 운반과 최초 위치 복귀 요청을 구현한다.
#include "WeightCarryComponent.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include "Dungeon/DungeonPC.h"
#include "Dungeon/Gimmick/Elements/CPP_WeightObject.h"

////////////////////////////
//! \author Codex
//! \brief 무게추 운반 컴포넌트의 복제와 기본 놓기 문구를 초기화한다.
UWeightCarryComponent::UWeightCarryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    ReleaseInteractionText = NSLOCTEXT("WeightCarry", "ReleaseInteractionText", "놓기");
    PlaceLeftInteractionText = NSLOCTEXT("WeightCarry", "PlaceLeftInteractionText", "왼쪽에 놓기");
    PlaceRightInteractionText = NSLOCTEXT("WeightCarry", "PlaceRightInteractionText", "오른쪽에 놓기");
    SideFullNoticeText = NSLOCTEXT("WeightCarry", "SideFullNoticeText", "선택한 저울에는 더 이상 단지를 놓을 수 없습니다.");
    NoCarriedWeightNoticeText = NSLOCTEXT("WeightCarry", "NoCarriedWeightNoticeText", "무게추를 들어야 저울에 놓을 수 있습니다.");
}

////////////////////////////
//! \author Codex
//! \brief 컴포넌트 종료 시 서버가 운반 중인 무게추를 최초 위치로 복귀시킨다.
//! \param EndPlayReason 컴포넌트 종료 사유다.
void UWeightCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseCarriedWeightToInitialTransform();
    Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author Codex
//! \brief 소유 클라이언트 UI에 필요한 현재 무게추 포인터를 복제 등록한다.
//! \param OutLifetimeProps 복제할 프로퍼티 목록이다.
void UWeightCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UWeightCarryComponent, CarriedWeightObject, COND_OwnerOnly);
}

////////////////////////////
//! \author Codex
//! \brief 서버가 승인한 무게추를 플레이어의 빈 운반 슬롯에 등록하고 실제 부착을 요청한다.
//! \param WeightObject 운반을 시작할 무게추다.
//! \return 서버에서 운반 시작이 확정되면 true다.
bool UWeightCarryComponent::TryBeginCarry(ACPP_WeightObject* WeightObject)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValid(WeightObject) || CarriedWeightObject || WeightObject->IsBeingCarried())
    {
        return false;
    }

    WeightObject->BeginCarry(OwnerActor);
    if (WeightObject->GetCurrentCarrier() != OwnerActor)
    {
        return false;
    }

    CarriedWeightObject = WeightObject;
    OnWeightCarryStateChanged.Broadcast();
    OwnerActor->ForceNetUpdate();
    return true;
}

////////////////////////////
//! \author Codex
//! \brief 소유 플레이어의 현재 무게추 해제 요청을 서버 RPC로 전달한다. (Reliable)
void UWeightCarryComponent::RequestReleaseCarriedWeight()
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !CarriedWeightObject)
    {
        return;
    }

    if (OwnerPawn->HasAuthority())
    {
        ReleaseCarriedWeightToInitialTransform();
        return;
    }

    ServerRequestReleaseCarriedWeight();
}

////////////////////////////
//! \author Codex
//! \brief 소유 클라이언트가 요청한 무게추 해제를 서버에서 확정한다. (Reliable)
void UWeightCarryComponent::ServerRequestReleaseCarriedWeight_Implementation()
{
    ReleaseCarriedWeightToInitialTransform();
}

////////////////////////////
//! \author Codex
//! \brief 소유 플레이어의 저울 방향 배치 요청을 서버 RPC로 전달한다. (Reliable)
//! \param ScaleElement 배치를 요청할 저울이다.
//! \param Side 배치를 요청할 좌우 방향이다.
void UWeightCarryComponent::RequestPlaceCarriedWeight(ACPP_BalanceScaleElement* ScaleElement, EBalanceScaleSide Side)
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !IsValid(ScaleElement))
    {
        return;
    }

    if (OwnerPawn->HasAuthority())
    {
        PlaceCarriedWeightOnScale(ScaleElement, Side);
        return;
    }

    ServerRequestPlaceCarriedWeight(ScaleElement, Side);
}

////////////////////////////
//! \author Codex
//! \brief 클라이언트가 선택한 저울과 방향을 서버에서 재검증해 배치를 시도한다. (Reliable)
//! \param ScaleElement 클라이언트가 근처 저울로 선택한 액터다.
//! \param Side 클라이언트가 선택한 좌우 방향이다.
void UWeightCarryComponent::ServerRequestPlaceCarriedWeight_Implementation(
    ACPP_BalanceScaleElement* ScaleElement,
    EBalanceScaleSide Side)
{
    PlaceCarriedWeightOnScale(ScaleElement, Side);
}

////////////////////////////
//! \author Codex
//! \brief Owner 사망 시 서버가 운반 중인 무게추를 최초 배치 위치로 복귀시킨다.
//! \param bDead true이면 사망 상태다.
void UWeightCarryComponent::HandleOwnerLifeStateChanged(bool bDead)
{
    if (bDead)
    {
        ReleaseCarriedWeightToInitialTransform();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 종료 시 서버에서 지정 무게추의 운반 상태와 플레이어 보유 포인터를 함께 정리하는 함수
// WeightObject : 최초 위치로 강제 복귀시킬 무게추
void UWeightCarryComponent::ForceReturnCarriedWeightForGimmick(ACPP_WeightObject* WeightObject)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || CarriedWeightObject != WeightObject)
    {
        return;
    }

    ReleaseCarriedWeightToInitialTransform();
}

////////////////////////////
//! \author Codex
//! \brief 복제된 운반 무게추가 바뀌면 소유 클라이언트의 통합 상호작용 가이드 갱신 이벤트를 발화한다.
void UWeightCarryComponent::OnRep_CarriedWeightObject()
{
    OnWeightCarryStateChanged.Broadcast();
}

////////////////////////////
//! \author Codex
//! \brief 서버에서 현재 무게추를 운반 슬롯에서 제거하고 맵 최초 배치 위치로 복귀시킨다.
void UWeightCarryComponent::ReleaseCarriedWeightToInitialTransform()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !CarriedWeightObject)
    {
        return;
    }

    if (!IsValid(CarriedWeightObject))
    {
        CarriedWeightObject = nullptr;
        OnWeightCarryStateChanged.Broadcast();
        OwnerActor->ForceNetUpdate();
        return;
    }

    ACPP_WeightObject* WeightObject = CarriedWeightObject;
    if (!WeightObject->ReleaseFromCarrier(OwnerActor))
    {
        return;
    }

    CarriedWeightObject = nullptr;
    OnWeightCarryStateChanged.Broadcast();
    OwnerActor->ForceNetUpdate();
}

////////////////////////////
//! \author Codex
//! \brief 서버에서 현재 무게추를 저울의 첫 빈 슬롯에 배치하고 성공 시 운반 슬롯을 비운다.
//! \param ScaleElement 배치할 저울이다.
//! \param Side 배치할 좌우 방향이다.
void UWeightCarryComponent::PlaceCarriedWeightOnScale(ACPP_BalanceScaleElement* ScaleElement, EBalanceScaleSide Side)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValid(ScaleElement))
    {
        return;
    }

    if (!CarriedWeightObject)
    {
        SendNoCarriedWeightNotice();
        return;
    }

    const EBalanceScalePlacementResult PlacementResult = ScaleElement->TrySnapWeight(
        CarriedWeightObject,
        Side,
        OwnerActor);

    if (PlacementResult == EBalanceScalePlacementResult::SideFull)
    {
        SendSideFullNotice();
        return;
    }

    if (PlacementResult != EBalanceScalePlacementResult::Success)
    {
        return;
    }

    CarriedWeightObject = nullptr;
    OnWeightCarryStateChanged.Broadcast();
    OwnerActor->ForceNetUpdate();
}

////////////////////////////
//! \author Codex
//! \brief 슬롯 부족으로 거절된 플레이어의 소유 클라이언트에 일반 Notice를 전송한다.
void UWeightCarryComponent::SendSideFullNotice() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    ADungeonPC* DungeonPC = OwnerPawn ? Cast<ADungeonPC>(OwnerPawn->GetController()) : nullptr;
    if (DungeonPC)
    {
        DungeonPC->SendNoticeToClient(SideFullNoticeText, 0.0f);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 무게추 없이 저울 배치를 시도한 플레이어의 소유 클라이언트에 Notice를 전송하는 함수
void UWeightCarryComponent::SendNoCarriedWeightNotice() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    ADungeonPC* DungeonPC = OwnerPawn ? Cast<ADungeonPC>(OwnerPawn->GetController()) : nullptr;
    if (DungeonPC)
    {
        DungeonPC->SendNoticeToClient(NoCarriedWeightNoticeText, 0.0f);
    }
}
