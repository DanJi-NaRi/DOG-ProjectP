#include "CPP_LaserClearPoint.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dungeon/Gimmick/CPP_GimmickBase.h"
#include "Dungeon/Gimmick/Elements/CPP_LaserGimmickElement.h"
#include "Materials/MaterialInstanceDynamic.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레이저 클리어 판정 표면과 향후 연출 부착 지점을 구성하는 생성자
ACPP_LaserClearPoint::ACPP_LaserClearPoint()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    TargetMesh->SetupAttachment(SceneRoot);
    TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TargetMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    TargetMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    TargetMesh->ComponentTags.Add(TEXT("Clear"));

    VFXRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VFXRoot"));
    VFXRoot->SetupAttachment(TargetMesh);

    ChargeVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeVisualMesh"));
    ChargeVisualMesh->SetupAttachment(VFXRoot);
    ChargeVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ChargeVisualMesh->SetCastShadow(false);
}

void ACPP_LaserClearPoint::BeginPlay()
{
    Super::BeginPlay();

    InitializeChargeMaterial();
    RefreshVisualState(0.0f);
}

void ACPP_LaserClearPoint::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    RefreshVisualState(DeltaSeconds);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 충전 중에는 판정값을 따르고 이탈 후에는 선형 감소하는 화면 표시 진행률을 반환하는 함수
// Return Value : 0부터 1까지의 연출용 충전 진행률
float ACPP_LaserClearPoint::GetChargeProgress() const
{
    return DisplayedChargeProgress;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 레이저가 현재 Clear 표면에 도달한 상태인지 반환하는 함수
// Return Value : 레이저가 활성화되어 Clear 목표에 닿아 있으면 true
bool ACPP_LaserClearPoint::IsLaserContactActive() const
{
    return IsValid(LaserElement) && LaserElement->IsSatisfied();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 연결된 기믹이 서버 판정으로 계산한 현재 충전 목표 진행률을 반환하는 함수
// Return Value : 0부터 1까지의 판정 기준 진행률
float ACPP_LaserClearPoint::GetTargetChargeProgress() const
{
    if (!IsValid(LaserElement))
    {
        return 0.0f;
    }

    const ACPP_GimmickBase* OwnerGimmick = LaserElement->GetOwnerGimmick();
    return OwnerGimmick
        ? FMath::Clamp(OwnerGimmick->GetProgress(), 0.0f, 1.0f)
        : 0.0f;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정된 충전 연출 메쉬의 머티리얼을 런타임 동적 머티리얼로 준비하는 함수
void ACPP_LaserClearPoint::InitializeChargeMaterial()
{
    if (!ChargeVisualMesh)
    {
        return;
    }

    ChargeMaterialInstance = ChargeVisualMesh->CreateDynamicMaterialInstance(
        FMath::Max(ChargeMaterialSlotIndex, 0));
    UpdateChargeMaterialParameters();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 접촉과 진행률 변화를 감지해 블루프린트 연출 이벤트로 전달하는 함수
// DeltaSeconds : 이전 프레임부터 흐른 시간
void ACPP_LaserClearPoint::RefreshVisualState(float DeltaSeconds)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const bool bCurrentLaserContactActive = IsLaserContactActive();
    if (bCurrentLaserContactActive)
    {
        DisplayedChargeProgress = GetTargetChargeProgress();
    }
    else
    {
        DisplayedChargeProgress = FMath::Max(
            0.0f,
            DisplayedChargeProgress - FMath::Max(ChargeDrainSpeed, 0.01f) * DeltaSeconds);
    }

    const float CurrentChargeProgress = GetChargeProgress();
    bool bVisualParametersChanged = false;

    if (bCurrentLaserContactActive != bLastLaserContactActive)
    {
        bLastLaserContactActive = bCurrentLaserContactActive;
        bVisualParametersChanged = true;

        if (bCurrentLaserContactActive)
        {
            OnLaserContactStarted();
        }
        else
        {
            bChargeCompleted = false;
            OnLaserContactEnded();
        }
    }

    if (!FMath::IsNearlyEqual(CurrentChargeProgress, LastChargeProgress))
    {
        LastChargeProgress = CurrentChargeProgress;
        bVisualParametersChanged = true;
        OnChargeProgressChanged(CurrentChargeProgress);
    }

    if (bVisualParametersChanged)
    {
        UpdateChargeMaterialParameters();
    }

    if (!bChargeCompleted && CurrentChargeProgress >= 1.0f - UE_KINDA_SMALL_NUMBER)
    {
        bChargeCompleted = true;
        OnChargeCompleted();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 접촉 여부와 충전 진행률을 팀 연출용 머티리얼 Scalar Parameter에 전달하는 함수
void ACPP_LaserClearPoint::UpdateChargeMaterialParameters()
{
    if (!ChargeMaterialInstance)
    {
        return;
    }

    if (!ChargeProgressParameterName.IsNone())
    {
        ChargeMaterialInstance->SetScalarParameterValue(
            ChargeProgressParameterName,
            FMath::Clamp(LastChargeProgress, 0.0f, 1.0f));
    }

    if (!LaserContactParameterName.IsNone())
    {
        ChargeMaterialInstance->SetScalarParameterValue(
            LaserContactParameterName,
            bLastLaserContactActive ? 1.0f : 0.0f);
    }
}
