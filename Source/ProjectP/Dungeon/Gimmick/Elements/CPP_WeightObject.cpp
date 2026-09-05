////////////////////////////
//! \file CPP_WeightObject.cpp
//! \brief 저울 기믹에서 사용하는 전용 무게추 오브젝트 구현 파일이다.
#include "CPP_WeightObject.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include "CPP_BalanceScaleElement.h"
#include "Player/Components/WeightCarryComponent.h"

////////////////////////////
//! \author Codex
//! \brief 무게추의 메시, 상호작용 컴포넌트, 네트워크 이동 복제를 초기화한다.
ACPP_WeightObject::ACPP_WeightObject()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    WeightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeightMesh"));
    WeightMesh->SetupAttachment(SceneRoot);

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

////////////////////////////
//! \author Codex
//! \brief 최초 배치 Transform과 메시 물리 상태를 저장하고 서버 상호작용 이벤트를 연결한다.
void ACPP_WeightObject::BeginPlay()
{
    Super::BeginPlay();

    InitialWorldTransform = GetActorTransform();
    InitialMeshCollisionEnabled = WeightMesh->GetCollisionEnabled();
    bInitialMeshSimulatesPhysics = WeightMesh->IsSimulatingPhysics();
    InitialCustomDepthStencilValue = WeightMesh->CustomDepthStencilValue;

    if (HasAuthority())
    {
        InteractableComponent->OnInteractionStarted.AddUniqueDynamic(this, &ACPP_WeightObject::HandleInteractionStarted);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로컬 상호작용 UI가 선택한 무게추 하나의 Custom Depth Stencil 값을 강조하거나 최초 값으로 복구하는 함수
// bHighlighted : 선택 강조 적용 여부
void ACPP_WeightObject::SetInteractionFocusHighlighted(bool bHighlighted)
{
    bInteractionFocusHighlighted = bHighlighted;
    RefreshCustomDepthStencilValue();
}

////////////////////////////
//! \author Codex
//! \brief 클라이언트 운반 연출에 필요한 현재 운반자 포인터를 복제 등록한다.
//! \param OutLifetimeProps 복제할 프로퍼티 목록이다.
void ACPP_WeightObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_WeightObject, CurrentCarrier);
}

////////////////////////////
//! \author Codex
//! \brief 서버가 승인한 상호작용자를 무게추 운반자로 설정한다.
//! \param Context 서버가 확정한 상호작용자 정보다.
void ACPP_WeightObject::HandleInteractionStarted(const FInteractionStartContext& Context)
{
    if (!HasAuthority() || !IsValid(Context.Interactor) || IsBeingCarried() || bLockedToScale)
    {
        return;
    }

    if (UWeightCarryComponent* WeightCarryComponent = Context.Interactor->FindComponentByClass<UWeightCarryComponent>())
    {
        WeightCarryComponent->TryBeginCarry(this);
    }
}

////////////////////////////
//! \author Codex
//! \brief 지정된 현재 운반자가 놓기를 실행하면 무게추를 맵 최초 위치로 복귀시킨다.
//! \param Interactor 무게추를 놓는 플레이어 액터다.
//! \return 지정 액터가 실제 운반자여서 복귀가 완료되면 true다.
bool ACPP_WeightObject::ReleaseFromCarrier(AActor* Interactor)
{
    if (!HasAuthority() || Interactor != CurrentCarrier)
    {
        return false;
    }

    ReturnToInitialTransform();
    return !IsBeingCarried();
}

////////////////////////////
//! \author Codex
//! \brief 무게추의 물리·충돌을 끄고 승인된 플레이어 앞 상대 위치에 부착한다.
//! \param Interactor 무게추를 운반할 플레이어 액터다.
void ACPP_WeightObject::BeginCarry(AActor* Interactor)
{
    if (!HasAuthority() || !IsValid(Interactor) || !Interactor->GetRootComponent() || bLockedToScale)
    {
        return;
    }

    if (CurrentScale)
    {
        CurrentScale->RemoveWeight(this);
        CurrentScale = nullptr;
    }

    CurrentCarrier = Interactor;

    WeightMesh->SetSimulatePhysics(false);
    WeightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AttachToComponent(Interactor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    SetActorRelativeLocation(CarryOffset, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRelativeRotation(CarryRotation, false, nullptr, ETeleportType::TeleportPhysics);

    // Shared 정책으로 잘못 배치되어도 다른 플레이어의 동시 운반 승인을 막는 서버 Gate 안전장치다.
    InteractableComponent->SetInteractionEnabled(false);

    ApplyCarryStateFX();
    ForceNetUpdate();
}

////////////////////////////
//! \author Codex
//! \brief 운반 중인 무게추를 저울의 고정 슬롯에 부착하고 다시 상호작용 가능 상태로 전환한다.
//! \param ScaleElement 무게추를 점유자로 관리할 저울이다.
//! \param SnapSlot 무게추가 부착될 고정 슬롯 컴포넌트다.
//! \param Interactor 현재 무게추를 운반 중인 플레이어 액터다.
//! \return 서버 검증과 부착이 모두 성공하면 true다.
bool ACPP_WeightObject::SnapToScale(ACPP_BalanceScaleElement* ScaleElement, USceneComponent* SnapSlot, AActor* Interactor)
{
    if (!HasAuthority() || !IsValid(ScaleElement) || !IsValid(SnapSlot) ||
        !IsValid(Interactor) || CurrentCarrier != Interactor || CurrentScale || bLockedToScale)
    {
        return false;
    }

    if (!AttachToComponent(SnapSlot, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        return false;
    }

    SetActorRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);

    WeightMesh->SetSimulatePhysics(false);
    WeightMesh->SetCollisionEnabled(InitialMeshCollisionEnabled);

    CurrentCarrier = nullptr;
    CurrentScale = ScaleElement;
    InteractableComponent->SetInteractionEnabled(true);

    ApplyCarryStateFX();
    ForceNetUpdate();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레벨에서 시작 배치로 지정된 무게추를 저울 슬롯에 부착하고 운반 불가 상태로 잠그는 함수
// ScaleElement : 시작 무게추를 점유자로 관리할 저울
// SnapSlot : 시작 무게추가 고정될 좌우 슬롯
// Return Value : 서버에서 슬롯 고정과 잠금이 완료되었으면 true
bool ACPP_WeightObject::LockToInitialScaleSlot(
    ACPP_BalanceScaleElement* ScaleElement,
    USceneComponent* SnapSlot)
{
    if (!HasAuthority() || !IsValid(ScaleElement) || !IsValid(SnapSlot) || CurrentCarrier ||
        (CurrentScale && CurrentScale != ScaleElement))
    {
        return false;
    }

    WeightMesh->SetSimulatePhysics(false);
    if (!AttachToComponent(SnapSlot, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        return false;
    }

    SetActorRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
    SetActorRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);

    CurrentCarrier = nullptr;
    CurrentScale = ScaleElement;
    bLockedToScale = true;
    InteractableComponent->SetInteractionEnabled(false);

    ApplyCarryStateFX();
    ForceNetUpdate();
    return true;
}

////////////////////////////
//! \author Codex
//! \brief 운반 부착을 해제하고 무게추를 맵 최초 배치 Transform과 물리 상태로 복구한다.
void ACPP_WeightObject::ReturnToInitialTransform()
{
    if (!HasAuthority() || bLockedToScale)
    {
        return;
    }

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorTransform(InitialWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);

    WeightMesh->SetCollisionEnabled(InitialMeshCollisionEnabled);
    WeightMesh->SetSimulatePhysics(bInitialMeshSimulatesPhysics);

    CurrentCarrier = nullptr;
    CurrentScale = nullptr;
    InteractableComponent->SetInteractionEnabled(true);

    ApplyCarryStateFX();
    ForceNetUpdate();
}

////////////////////////////
//! \author Codex
//! \brief 복제된 운반자 변경을 원격 클라이언트의 블루프린트 연출에 전달한다.
void ACPP_WeightObject::OnRep_CurrentCarrier()
{
    ApplyCarryStateFX();
}

////////////////////////////
//! \author Codex
//! \brief 현재 운반자 유무를 블루프린트 운반 상태 연출 이벤트로 변환한다.
void ACPP_WeightObject::ApplyCarryStateFX()
{
    RefreshCustomDepthStencilValue();
    OnCarryStateChanged(IsBeingCarried(), CurrentCarrier);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로컬 UI 포커스 또는 로컬 플레이어의 운반 상태가 유지되는 동안 WeightMesh의 Custom Depth Stencil 값을 2로 적용하는 함수
void ACPP_WeightObject::RefreshCustomDepthStencilValue()
{
    if (!WeightMesh)
    {
        return;
    }

    const APawn* CarrierPawn = Cast<APawn>(CurrentCarrier);
    const bool bCarriedByLocalPlayer = CarrierPawn && CarrierPawn->IsLocallyControlled();
    WeightMesh->SetCustomDepthStencilValue(
        bInteractionFocusHighlighted || bCarriedByLocalPlayer
            ? 2
            : InitialCustomDepthStencilValue);
}
