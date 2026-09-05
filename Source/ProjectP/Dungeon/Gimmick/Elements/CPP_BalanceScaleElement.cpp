////////////////////////////
//! \file CPP_BalanceScaleElement.cpp
//! \brief 저울 기믹 요소의 컴포넌트 골격과 스냅 슬롯 조회를 구현한다.
#include "CPP_BalanceScaleElement.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#include "CPP_WeightObject.h"
#include "Player/Components/WeightCarryComponent.h"

namespace
{
    const FVector DefaultSlotOffsets[] =
    {
        FVector(0.0f, 0.0f, 0.0f),
        FVector(60.0f, 0.0f, 0.0f),
        FVector(-60.0f, 0.0f, 0.0f),
        FVector(0.0f, 60.0f, 0.0f),
        FVector(0.0f, -60.0f, 0.0f),
    };
}

////////////////////////////
//! \author Codex
//! \brief 저울 프레임, 중심축, 좌우 접시, 근처 판정 범위와 좌우 각 5개의 고정 슬롯을 생성한다.
ACPP_BalanceScaleElement::ACPP_BalanceScaleElement()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> InitialPlacementOffMaterialFinder(
        TEXT("/Game/내꺼에셋모음/카노푸스/항아리텍스쳐/항아리OFF.항아리OFF"));
    if (InitialPlacementOffMaterialFinder.Succeeded())
    {
        InitialPlacementOffMaterial = InitialPlacementOffMaterialFinder.Object;
    }

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
    FrameMesh->SetupAttachment(SceneRoot);

    BeamPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BeamPivot"));
    BeamPivot->SetupAttachment(SceneRoot);
    BeamPivot->SetRelativeLocation(FVector::ZeroVector);

    BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
    BeamMesh->SetupAttachment(BeamPivot);

    LeftPanRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftPanRoot"));
    LeftPanRoot->SetupAttachment(BeamPivot);
    LeftPanRoot->SetRelativeLocation(FVector::ZeroVector);

    LeftPanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftPanMesh"));
    LeftPanMesh->SetupAttachment(LeftPanRoot);

    LeftPlacementRange = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftPlacementRange"));
    LeftPlacementRange->SetupAttachment(LeftPanRoot);
    LeftPlacementRange->InitBoxExtent(FVector(200.0f));
    LeftPlacementRange->SetCollisionProfileName(TEXT("Trigger"));
    LeftPlacementRange->SetGenerateOverlapEvents(true);

    RightPanRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RightPanRoot"));
    RightPanRoot->SetupAttachment(BeamPivot);
    RightPanRoot->SetRelativeLocation(FVector::ZeroVector);

    RightPanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPanMesh"));
    RightPanMesh->SetupAttachment(RightPanRoot);

    RightPlacementRange = CreateDefaultSubobject<UBoxComponent>(TEXT("RightPlacementRange"));
    RightPlacementRange->SetupAttachment(RightPanRoot);
    RightPlacementRange->InitBoxExtent(FVector(200.0f));
    RightPlacementRange->SetCollisionProfileName(TEXT("Trigger"));
    RightPlacementRange->SetGenerateOverlapEvents(true);

    LeftSnapSlots.Reserve(SlotsPerSide);
    RightSnapSlots.Reserve(SlotsPerSide);
    LeftSlotOccupants.SetNum(SlotsPerSide);
    RightSlotOccupants.SetNum(SlotsPerSide);

    for (int32 SlotIndex = 0; SlotIndex < SlotsPerSide; ++SlotIndex)
    {
        const FName LeftSlotName(*FString::Printf(TEXT("LeftSnapSlot_%d"), SlotIndex + 1));
        USceneComponent* LeftSlot = CreateDefaultSubobject<USceneComponent>(LeftSlotName);
        LeftSlot->SetupAttachment(LeftPanRoot);
        LeftSlot->SetRelativeLocation(DefaultSlotOffsets[SlotIndex]);
        LeftSnapSlots.Add(LeftSlot);

        const FName RightSlotName(*FString::Printf(TEXT("RightSnapSlot_%d"), SlotIndex + 1));
        USceneComponent* RightSlot = CreateDefaultSubobject<USceneComponent>(RightSlotName);
        RightSlot->SetupAttachment(RightPanRoot);
        RightSlot->SetRelativeLocation(DefaultSlotOffsets[SlotIndex]);
        RightSnapSlots.Add(RightSlot);
    }
}

////////////////////////////
//! \author Codex
//! \brief BP에서 조정된 좌우 접시의 최초 상대 위치를 저장하고 현재 높이 상태를 적용한다.
void ACPP_BalanceScaleElement::BeginPlay()
{
    Super::BeginPlay();

    InitialLeftPanRelativeLocation = LeftPanRoot->GetRelativeLocation();
    InitialRightPanRelativeLocation = RightPanRoot->GetRelativeLocation();
    bInitialPanLocationsCached = true;

    ApplyInitialPlacementMaterial();

    if (HasAuthority())
    {
        InitializeLockedWeightPlacements();
    }

    ApplyBalanceState();
    OnPanHeightStateChanged(
        BalanceState.LeftHeightState,
        BalanceState.RightHeightState,
        BalanceState.LeftTotalWeight,
        BalanceState.RightTotalWeight);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 저울의 모든 무게추와 슬롯 점유, 합산 무게, 접시 높이를 최초 상태로 복원하는 함수
void ACPP_BalanceScaleElement::ResetElement()
{
    if (!HasAuthority())
    {
        return;
    }

    Super::ResetElement();

    for (ACPP_WeightObject* WeightObject : WeightObjects)
    {
        if (!IsValid(WeightObject))
        {
            continue;
        }

        if (WeightObject->bLockedToScale)
        {
            continue;
        }

        if (WeightObject->IsBeingCarried())
        {
            AActor* Carrier = WeightObject->GetCurrentCarrier();
            UWeightCarryComponent* CarryComponent = Carrier
                ? Carrier->FindComponentByClass<UWeightCarryComponent>()
                : nullptr;

            if (CarryComponent)
            {
                CarryComponent->ForceReturnCarriedWeightForGimmick(WeightObject);
            }
        }

        if (WeightObject->CurrentScale && WeightObject->CurrentScale != this)
        {
            WeightObject->CurrentScale->RemoveWeight(WeightObject);
        }

        WeightObject->ReturnToInitialTransform();
        WeightObject->InteractableComponent->ResetInteractionState();
    }

    InitializeLockedWeightPlacements();
}

void ACPP_BalanceScaleElement::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bPanTransitionActive || !LeftPanRoot || !RightPanRoot)
    {
        bPanTransitionActive = false;
        SetActorTickEnabled(false);
        return;
    }

    PanTransitionElapsedTime += DeltaSeconds;
    const float MoveDuration = FMath::Max(PanMoveDuration, KINDA_SMALL_NUMBER);
    const float MoveAlpha = FMath::Clamp(PanTransitionElapsedTime / MoveDuration, 0.0f, 1.0f);

    LeftPanRoot->SetRelativeLocation(FMath::Lerp(
        PanTransitionStartLeftLocation,
        PanTransitionTargetLeftLocation,
        MoveAlpha));
    RightPanRoot->SetRelativeLocation(FMath::Lerp(
        PanTransitionStartRightLocation,
        PanTransitionTargetRightLocation,
        MoveAlpha));

    if (MoveAlpha >= 1.0f)
    {
        LeftPanRoot->SetRelativeLocation(PanTransitionTargetLeftLocation);
        RightPanRoot->SetRelativeLocation(PanTransitionTargetRightLocation);
        bPanTransitionActive = false;
        SetActorTickEnabled(false);
    }
}

////////////////////////////
//! \author Codex
//! \brief 좌우 총무게와 높이 상태를 하나의 복제 구조체로 등록한다.
//! \param OutLifetimeProps 복제할 프로퍼티 목록이다.
void ACPP_BalanceScaleElement::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_BalanceScaleElement, BalanceState);
    DOREPLIFETIME(ACPP_BalanceScaleElement, bGimmickInteractionEnabled);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 생명주기에 따라 저울 배치와 소속 무게추의 상호작용 Gate를 변경하는 함수
// bEnabled : 저울과 무게추 상호작용 활성 여부
void ACPP_BalanceScaleElement::SetGimmickInteractionEnabled(bool bEnabled)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bStateChanged = bGimmickInteractionEnabled != bEnabled;
    bGimmickInteractionEnabled = bEnabled;

    for (ACPP_WeightObject* WeightObject : WeightObjects)
    {
        if (!IsValid(WeightObject))
        {
            continue;
        }

        if (!bEnabled && WeightObject->IsBeingCarried())
        {
            AActor* Carrier = WeightObject->GetCurrentCarrier();
            UWeightCarryComponent* CarryComponent = Carrier
                ? Carrier->FindComponentByClass<UWeightCarryComponent>()
                : nullptr;

            if (CarryComponent)
            {
                CarryComponent->ForceReturnCarriedWeightForGimmick(WeightObject);
            }
            else
            {
                WeightObject->ReturnToInitialTransform();
            }
        }

        WeightObject->InteractableComponent->SetInteractionEnabled(
            bEnabled && !WeightObject->IsBeingCarried() && !WeightObject->bLockedToScale);
    }

    if (bStateChanged)
    {
        ForceNetUpdate();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정 액터가 저울의 지정 방향 박스 판정 범위와 겹쳐 있는지 확인하는 함수
// Actor : 배치 범위를 검사할 액터
// Side : 검사할 저울 방향
// 반환값 : 유효한 액터가 지정 방향의 PlacementRange와 겹쳐 있으면 true
bool ACPP_BalanceScaleElement::IsActorInsidePlacementRange(
    const AActor* Actor,
    EBalanceScaleSide Side) const
{
    const UBoxComponent* PlacementRange = Side == EBalanceScaleSide::Left
        ? LeftPlacementRange.Get()
        : RightPlacementRange.Get();

    return IsValid(Actor) && PlacementRange && PlacementRange->IsOverlappingActor(Actor);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정 방향 박스 판정 범위의 월드 중심 위치를 반환하는 함수
// Side : 조회할 저울 방향
// 반환값 : 유효한 판정 범위의 월드 중심 위치, 판정 범위가 없으면 저울 액터의 월드 위치
FVector ACPP_BalanceScaleElement::GetPlacementRangeCenter(EBalanceScaleSide Side) const
{
    const UBoxComponent* PlacementRange = Side == EBalanceScaleSide::Left
        ? LeftPlacementRange.Get()
        : RightPlacementRange.Get();

    return PlacementRange ? PlacementRange->GetComponentLocation() : GetActorLocation();
}

////////////////////////////
//! \author Codex
//! \brief 지정한 좌우 방향의 고정 스냅 슬롯을 인덱스로 조회한다.
//! \param Side 조회할 저울 방향이다.
//! \param SlotIndex 조회할 슬롯 인덱스다.
//! \return 유효한 슬롯 컴포넌트이며, 범위를 벗어나면 nullptr다.
USceneComponent* ACPP_BalanceScaleElement::GetSnapSlot(EBalanceScaleSide Side, int32 SlotIndex) const
{
    const TArray<TObjectPtr<USceneComponent>>& Slots = Side == EBalanceScaleSide::Left
        ? LeftSnapSlots
        : RightSnapSlots;

    return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

////////////////////////////
//! \author Codex
//! \brief 지정 방향에서 현재 유효한 무게추가 점유 중인 슬롯 수를 계산한다.
//! \param Side 확인할 저울 방향이다.
//! \return 점유 중인 슬롯 수다.
int32 ACPP_BalanceScaleElement::GetOccupiedSlotCount(EBalanceScaleSide Side) const
{
    TSet<const ACPP_WeightObject*> EffectiveWeightObjects;
    CollectEffectiveWeightObjects(Side, EffectiveWeightObjects);
    return EffectiveWeightObjects.Num();
}

////////////////////////////
//! \author Codex
//! \brief 서버에서 운반 중인 무게추를 지정 방향의 첫 번째 빈 슬롯에 원자적으로 스냅한다.
//! \param WeightObject 배치할 무게추다.
//! \param Side 배치할 저울 방향이다.
//! \param RequestingActor 무게추를 운반 중인 요청 플레이어다.
//! \return 배치 성공 또는 서버 거절 사유다.
EBalanceScalePlacementResult ACPP_BalanceScaleElement::TrySnapWeight(
    ACPP_WeightObject* WeightObject,
    EBalanceScaleSide Side,
    AActor* RequestingActor)
{
    if (!HasAuthority() || !bGimmickInteractionEnabled ||
        !IsValid(WeightObject) || !IsValid(RequestingActor) ||
        WeightObject->GetCurrentCarrier() != RequestingActor)
    {
        return EBalanceScalePlacementResult::InvalidRequest;
    }

    if (!IsActorInsidePlacementRange(RequestingActor, Side))
    {
        return EBalanceScalePlacementResult::OutOfRange;
    }

    const int32 EmptySlotIndex = FindFirstEmptySlotIndex(Side);
    if (EmptySlotIndex == INDEX_NONE)
    {
        return EBalanceScalePlacementResult::SideFull;
    }

    USceneComponent* SnapSlot = GetSnapSlot(Side, EmptySlotIndex);
    if (!SnapSlot || !WeightObject->SnapToScale(this, SnapSlot, RequestingActor))
    {
        return EBalanceScalePlacementResult::InvalidRequest;
    }

    TArray<TObjectPtr<ACPP_WeightObject>>& Occupants = GetSlotOccupants(Side);
    Occupants[EmptySlotIndex] = WeightObject;

    // 슬롯 점유가 확정되는 순간에만 무게·높이 갱신과 기믹 조건 재평가를 수행한다.
    RefreshBalanceState();
    MarkStateDirty();
    return EBalanceScalePlacementResult::Success;
}

////////////////////////////
//! \author Codex
//! \brief 서버에서 지정 무게추가 점유하던 좌우 슬롯을 찾아 비우고 기믹 상태 변경을 알린다.
//! \param WeightObject 슬롯에서 제거할 무게추다.
//! \return 실제 점유 슬롯을 찾아 비웠으면 true다.
bool ACPP_BalanceScaleElement::RemoveWeight(ACPP_WeightObject* WeightObject)
{
    if (!HasAuthority() || !IsValid(WeightObject) || WeightObject->bLockedToScale)
    {
        return false;
    }

    for (TArray<TObjectPtr<ACPP_WeightObject>>* Occupants : { &LeftSlotOccupants, &RightSlotOccupants })
    {
        const int32 SlotIndex = Occupants->IndexOfByKey(WeightObject);
        if (SlotIndex != INDEX_NONE)
        {
            (*Occupants)[SlotIndex] = nullptr;

            // 슬롯에서 빠지는 순간에만 무게·높이 갱신과 기믹 조건 재평가를 수행한다.
            RefreshBalanceState();
            MarkStateDirty();
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Initial Placement에 등록된 무게추의 Materials Element 0을 비활성 머테리얼로 변경하는 함수
void ACPP_BalanceScaleElement::ApplyInitialPlacementMaterial()
{
    if (!InitialPlacementOffMaterial)
    {
        return;
    }

    const auto ApplyMaterialToWeights = [this](const TArray<TObjectPtr<ACPP_WeightObject>>& InitialWeights)
    {
        for (ACPP_WeightObject* WeightObject : InitialWeights)
        {
            if (IsValid(WeightObject) && WeightObject->WeightMesh)
            {
                WeightObject->WeightMesh->SetMaterial(0, InitialPlacementOffMaterial);
            }
        }
    };

    ApplyMaterialToWeights(InitialLeftWeightObjects);
    ApplyMaterialToWeights(InitialRightWeightObjects);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 인스턴스에 지정된 좌우 시작 무게추를 슬롯에 고정하고 초기 무게 상태를 계산하는 함수
void ACPP_BalanceScaleElement::InitializeLockedWeightPlacements()
{
    if (!HasAuthority())
    {
        return;
    }

    LeftSlotOccupants.Init(nullptr, SlotsPerSide);
    RightSlotOccupants.Init(nullptr, SlotsPerSide);

    PlaceInitialLockedWeights(InitialLeftWeightObjects, EBalanceScaleSide::Left);
    PlaceInitialLockedWeights(InitialRightWeightObjects, EBalanceScaleSide::Right);

    RefreshBalanceState();
    MarkStateDirty();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정 방향의 시작 무게추 배열을 같은 인덱스의 슬롯에 배치하고 영구 잠금하는 함수
// InitialWeights : 레벨 인스턴스에서 지정한 시작 무게추 배열
// Side : 시작 무게추를 고정할 저울 방향
void ACPP_BalanceScaleElement::PlaceInitialLockedWeights(
    const TArray<TObjectPtr<ACPP_WeightObject>>& InitialWeights,
    EBalanceScaleSide Side)
{
    const int32 PlacementCount = FMath::Min(InitialWeights.Num(), SlotsPerSide);
    if (InitialWeights.Num() > SlotsPerSide)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BalanceScale] 시작 무게추가 슬롯 수를 초과했습니다. Scale: %s, Side: %s, Count: %d"),
            *GetNameSafe(this),
            Side == EBalanceScaleSide::Left ? TEXT("Left") : TEXT("Right"),
            InitialWeights.Num());
    }

    TArray<TObjectPtr<ACPP_WeightObject>>& Occupants = GetSlotOccupants(Side);
    for (int32 SlotIndex = 0; SlotIndex < PlacementCount; ++SlotIndex)
    {
        ACPP_WeightObject* WeightObject = InitialWeights[SlotIndex];
        USceneComponent* SnapSlot = GetSnapSlot(Side, SlotIndex);
        if (!IsValid(WeightObject) || !SnapSlot)
        {
            continue;
        }

        if (LeftSlotOccupants.Contains(WeightObject) || RightSlotOccupants.Contains(WeightObject))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BalanceScale] 동일한 시작 무게추가 중복 지정되었습니다. Scale: %s, Weight: %s"),
                *GetNameSafe(this), *GetNameSafe(WeightObject));
            continue;
        }

        WeightObjects.AddUnique(WeightObject);
        if (!WeightObject->LockToInitialScaleSlot(this, SnapSlot))
        {
            UE_LOG(LogTemp, Warning, TEXT("[BalanceScale] 시작 무게추를 슬롯에 고정하지 못했습니다. Scale: %s, Weight: %s, Slot: %d"),
                *GetNameSafe(this), *GetNameSafe(WeightObject), SlotIndex);
            continue;
        }

        Occupants[SlotIndex] = WeightObject;
    }
}

////////////////////////////
//! \author Codex
//! \brief 지정 방향의 서버 슬롯 점유 배열을 반환한다.
//! \param Side 조회할 저울 방향이다.
//! \return 수정 가능한 슬롯 점유 배열 참조다.
TArray<TObjectPtr<ACPP_WeightObject>>& ACPP_BalanceScaleElement::GetSlotOccupants(EBalanceScaleSide Side)
{
    return Side == EBalanceScaleSide::Left ? LeftSlotOccupants : RightSlotOccupants;
}

////////////////////////////
//! \author Codex
//! \brief 지정 방향의 서버 슬롯 점유 배열을 읽기 전용으로 반환한다.
//! \param Side 조회할 저울 방향이다.
//! \return 읽기 전용 슬롯 점유 배열 참조다.
const TArray<TObjectPtr<ACPP_WeightObject>>& ACPP_BalanceScaleElement::GetSlotOccupants(EBalanceScaleSide Side) const
{
    return Side == EBalanceScaleSide::Left ? LeftSlotOccupants : RightSlotOccupants;
}

////////////////////////////
//! \author Codex
//! \brief 무효 점유자를 정리한 뒤 지정 방향의 첫 번째 빈 슬롯을 찾는다.
//! \param Side 빈 슬롯을 찾을 저울 방향이다.
//! \return 첫 빈 슬롯 인덱스이며 모두 차 있으면 INDEX_NONE이다.
int32 ACPP_BalanceScaleElement::FindFirstEmptySlotIndex(EBalanceScaleSide Side)
{
    PruneInvalidSlotOccupants();
    return GetSlotOccupants(Side).IndexOfByPredicate([](const TObjectPtr<ACPP_WeightObject>& WeightObject)
    {
        return !IsValid(WeightObject);
    });
}

////////////////////////////
//! \author Codex
//! \brief 파괴된 무게추가 남긴 서버 슬롯 점유 참조를 빈 슬롯으로 정리한다.
void ACPP_BalanceScaleElement::PruneInvalidSlotOccupants()
{
    for (TArray<TObjectPtr<ACPP_WeightObject>>* Occupants : { &LeftSlotOccupants, &RightSlotOccupants })
    {
        for (TObjectPtr<ACPP_WeightObject>& WeightObject : *Occupants)
        {
            if (!IsValid(WeightObject))
            {
                WeightObject = nullptr;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정 방향의 시작 고정 무게추와 플레이어가 올린 무게추를 중복 없이 하나의 판정 집합으로 수집하는 함수
// Side : 무게추를 집계할 저울 방향
// OutWeightObjects : 클리어 조건과 총무게 계산에 사용할 유효 무게추 집합
void ACPP_BalanceScaleElement::CollectEffectiveWeightObjects(
    EBalanceScaleSide Side,
    TSet<const ACPP_WeightObject*>& OutWeightObjects) const
{
    OutWeightObjects.Reset();

    for (const TObjectPtr<ACPP_WeightObject>& WeightObject : GetSlotOccupants(Side))
    {
        if (IsValid(WeightObject) && !WeightObject->bLockedToScale)
        {
            OutWeightObjects.Add(WeightObject.Get());
        }
    }

    const TArray<TObjectPtr<ACPP_WeightObject>>& InitialWeights =
        Side == EBalanceScaleSide::Left
            ? InitialLeftWeightObjects
            : InitialRightWeightObjects;
    for (const TObjectPtr<ACPP_WeightObject>& WeightObject : InitialWeights)
    {
        if (!IsValid(WeightObject))
        {
            continue;
        }

        // 같은 액터가 양쪽에 잘못 지정된 경우 실제 시작 배치와 동일하게 왼쪽 설정을 우선한다.
        if (Side == EBalanceScaleSide::Right && InitialLeftWeightObjects.Contains(WeightObject))
        {
            continue;
        }

        OutWeightObjects.Add(WeightObject.Get());
    }
}

////////////////////////////
//! \author Codex
//! \brief 지정 방향 슬롯에 배치된 유효 무게추의 WeightValue 총합을 계산한다.
//! \param Side 합산할 저울 방향이다.
//! \return 해당 방향의 양의 정수 무게 총합이다.
int32 ACPP_BalanceScaleElement::CalculateTotalWeight(EBalanceScaleSide Side) const
{
    int32 TotalWeight = 0;
    TSet<const ACPP_WeightObject*> EffectiveWeightObjects;
    CollectEffectiveWeightObjects(Side, EffectiveWeightObjects);
    for (const ACPP_WeightObject* WeightObject : EffectiveWeightObjects)
    {
        if (IsValid(WeightObject))
        {
            TotalWeight += WeightObject->GetWeightValue();
        }
    }

    return TotalWeight;
}

////////////////////////////
//! \author Codex
//! \brief 서버에서 좌우 총무게 차이를 5단계 높이 상태로 변환하고 좌우 모델 위치와 복제 상태를 갱신한다.
void ACPP_BalanceScaleElement::RefreshBalanceState()
{
    if (!HasAuthority())
    {
        return;
    }

    FBalanceScaleReplicatedState NewState;
    NewState.LeftTotalWeight = CalculateTotalWeight(EBalanceScaleSide::Left);
    NewState.RightTotalWeight = CalculateTotalWeight(EBalanceScaleSide::Right);

    const int32 WeightDifference = NewState.LeftTotalWeight - NewState.RightTotalWeight;
    if (WeightDifference != 0)
    {
        const bool bLeftIsHeavier = WeightDifference > 0;
        const bool bUseFullHeight = FMath::Abs(WeightDifference) >= FMath::Max(FullHeightWeightDifference, 1);

        if (bLeftIsHeavier)
        {
            NewState.LeftHeightState = bUseFullHeight
                ? EBalancePanHeightState::Down
                : EBalancePanHeightState::SlightlyDown;
            NewState.RightHeightState = bUseFullHeight
                ? EBalancePanHeightState::Up
                : EBalancePanHeightState::SlightlyUp;
        }
        else
        {
            NewState.LeftHeightState = bUseFullHeight
                ? EBalancePanHeightState::Up
                : EBalancePanHeightState::SlightlyUp;
            NewState.RightHeightState = bUseFullHeight
                ? EBalancePanHeightState::Down
                : EBalancePanHeightState::SlightlyDown;
        }
    }

    const bool bStateChanged =
        BalanceState.LeftTotalWeight != NewState.LeftTotalWeight ||
        BalanceState.RightTotalWeight != NewState.RightTotalWeight ||
        BalanceState.LeftHeightState != NewState.LeftHeightState ||
        BalanceState.RightHeightState != NewState.RightHeightState;
    if (!bStateChanged)
    {
        return;
    }

    BalanceState = NewState;
    ApplyBalanceState();
    OnPanHeightStateChanged(
        BalanceState.LeftHeightState,
        BalanceState.RightHeightState,
        BalanceState.LeftTotalWeight,
        BalanceState.RightTotalWeight);
    ForceNetUpdate();
}

////////////////////////////
//! \author Codex
//! \brief 현재 복제 무게 차이로 선형 계산한 목표 위치를 만들고 좌우 접시의 시간 보간을 시작한다.
void ACPP_BalanceScaleElement::ApplyBalanceState()
{
    if (!bInitialPanLocationsCached || !LeftPanRoot || !RightPanRoot)
    {
        return;
    }

    PanTransitionStartLeftLocation = LeftPanRoot->GetRelativeLocation();
    PanTransitionStartRightLocation = RightPanRoot->GetRelativeLocation();

    PanTransitionTargetLeftLocation = InitialLeftPanRelativeLocation;
    PanTransitionTargetLeftLocation.Z += CalculateHeightOffset(
        BalanceState.LeftTotalWeight,
        BalanceState.RightTotalWeight);

    PanTransitionTargetRightLocation = InitialRightPanRelativeLocation;
    PanTransitionTargetRightLocation.Z += CalculateHeightOffset(
        BalanceState.RightTotalWeight,
        BalanceState.LeftTotalWeight);

    PanTransitionElapsedTime = 0.0f;

    const bool bLeftAlreadyAtTarget = PanTransitionStartLeftLocation.Equals(PanTransitionTargetLeftLocation);
    const bool bRightAlreadyAtTarget = PanTransitionStartRightLocation.Equals(PanTransitionTargetRightLocation);
    if (PanMoveDuration <= KINDA_SMALL_NUMBER || (bLeftAlreadyAtTarget && bRightAlreadyAtTarget))
    {
        LeftPanRoot->SetRelativeLocation(PanTransitionTargetLeftLocation);
        RightPanRoot->SetRelativeLocation(PanTransitionTargetRightLocation);
        bPanTransitionActive = false;
        SetActorTickEnabled(false);
        return;
    }

    bPanTransitionActive = true;
    SetActorTickEnabled(true);
}

////////////////////////////
//! \author Codex
//! \brief 한쪽과 반대쪽의 무게 차이를 최대 기준으로 정규화하여 부호 있는 Z 오프셋을 계산한다.
//! \param SideTotalWeight 높이를 계산할 한쪽의 총무게다.
//! \param OtherSideTotalWeight 반대쪽의 총무게다.
//! \return 해당 쪽이 무거우면 음수, 가벼우면 양수이며 최대 높이로 제한된 Z 오프셋이다.
float ACPP_BalanceScaleElement::CalculateHeightOffset(
    int32 SideTotalWeight,
    int32 OtherSideTotalWeight) const
{
    const int32 SafeFullHeightDifference = FMath::Max(FullHeightWeightDifference, 1);
    const float SignedWeightDifference = static_cast<float>(SideTotalWeight - OtherSideTotalWeight);
    const float HeightRatio = FMath::Clamp(
        SignedWeightDifference / static_cast<float>(SafeFullHeightDifference),
        -1.0f,
        1.0f);

    return -HeightRatio * FMath::Max(FullHeightOffset, 0.0f);
}

////////////////////////////
//! \author Codex
//! \brief 복제된 좌우 무게·높이 상태를 클라이언트 모델 위치와 블루프린트 연출에 반영한다.
void ACPP_BalanceScaleElement::OnRep_BalanceState()
{
    ApplyBalanceState();
    OnPanHeightStateChanged(
        BalanceState.LeftHeightState,
        BalanceState.RightHeightState,
        BalanceState.LeftTotalWeight,
        BalanceState.RightTotalWeight);
}
