#include "CPP_LaserGimmickElement.h"

#include "Engine/World.h"
#include "LaserReflectionActor.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기존 레이저 액터를 공통 기믹 요소로 연결하기 위한 생성자
ACPP_LaserGimmickElement::ACPP_LaserGimmickElement()
{
    PrimaryActorTick.bCanEverTick = true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이저 활성화 상태를 클라이언트에 복제하도록 등록하는 함수
// OutLifetimeProps : 복제할 프로퍼티 목록
void ACPP_LaserGimmickElement::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_LaserGimmickElement, bLaserEnabled);
}

void ACPP_LaserGimmickElement::BeginPlay()
{
    Super::BeginPlay();

    ApplyLaserEnabledState();

    if (HasAuthority())
    {
        RefreshClearState();
    }
}

void ACPP_LaserGimmickElement::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority() && bLaserEnabled)
    {
        RefreshClearState();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 레이저가 Clear 목표에 도달했는지 반환하는 함수
// Return Value : 레이저가 활성화되어 있고 IsClear가 true이면 true
bool ACPP_LaserGimmickElement::IsSatisfied() const
{
    return bLaserEnabled && IsValid(LaserActor) && LaserActor->IsClear;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이저가 Clear 목표에 연속으로 닿아 있는 시간을 반환하는 함수
// Return Value : 서버에서 측정한 연속 접촉 시간, 접촉 중이 아니면 0
float ACPP_LaserGimmickElement::GetClearHoldTime() const
{
    const UWorld* World = GetWorld();
    if (!World || !IsSatisfied() || ClearStartServerTime < 0.0f)
    {
        return 0.0f;
    }

    return FMath::Max(0.0f, World->GetTimeSeconds() - ClearStartServerTime);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이저의 클리어 상태를 초기화하는 함수
void ACPP_LaserGimmickElement::ResetElement()
{
    if (!HasAuthority())
    {
        return;
    }

    Super::ResetElement();

    bLastObservedClear = false;
    ClearStartServerTime = -1.0f;

    if (IsValid(LaserActor))
    {
        LaserActor->IsClear = false;

        if (bLaserEnabled)
        {
            LaserActor->UpdateLaserTrace();
        }
    }

    RefreshClearState();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 공통 기믹 상태에 맞춰 레이저의 동작과 표시를 활성화하거나 비활성화하는 함수
// bEnabled : 레이저 기믹 활성화 여부
void ACPP_LaserGimmickElement::SetGimmickInteractionEnabled(bool bEnabled)
{
    if (!HasAuthority())
    {
        return;
    }

    bLaserEnabled = bEnabled;
    ApplyLaserEnabledState();
    RefreshClearState();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 복제된 레이저 활성화 상태를 클라이언트의 레이저 액터에 반영하는 함수
void ACPP_LaserGimmickElement::OnRep_LaserEnabled()
{
    ApplyLaserEnabledState();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 활성화 상태에 따라 레이저 액터의 Tick과 화면 표시 상태를 적용하는 함수
void ACPP_LaserGimmickElement::ApplyLaserEnabledState() const
{
    if (!IsValid(LaserActor))
    {
        return;
    }

    LaserActor->SetActorTickEnabled(bLaserEnabled);
    LaserActor->SetActorHiddenInGame(!bLaserEnabled);

    if (bLaserEnabled)
    {
        LaserActor->UpdateLaserTrace();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이저의 IsClear 변화를 감지하고 소유 기믹에 조건 재평가를 요청하는 함수
void ACPP_LaserGimmickElement::RefreshClearState()
{
    const bool bCurrentClear = IsSatisfied();
    if (bCurrentClear != bLastObservedClear)
    {
        bLastObservedClear = bCurrentClear;
        ClearStartServerTime = bCurrentClear && GetWorld()
            ? GetWorld()->GetTimeSeconds()
            : -1.0f;
        MarkStateDirty();
        return;
    }

    if (bCurrentClear && !IsSolvedLocked())
    {
        MarkStateDirty();
    }
}
