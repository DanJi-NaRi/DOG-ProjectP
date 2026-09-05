#include "CPP_RailMirrorElement.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dungeon/DungeonPC.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 직선 레일 이동과 단계별 거울 회전에 필요한 컴포넌트를 구성하는 생성자
ACPP_RailMirrorElement::ACPP_RailMirrorElement()
{
    PrimaryActorTick.bCanEverTick = true;
    SetNetUpdateFrequency(30.0f);
    SetMinNetUpdateFrequency(10.0f);
    SetReplicateMovement(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    RailLengthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailLengthMesh"));
    RailLengthMesh->SetupAttachment(SceneRoot);
    RailLengthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RailStartWidthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailStartWidthMesh"));
    RailStartWidthMesh->SetupAttachment(SceneRoot);
    RailStartWidthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RailEndWidthMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailEndWidthMesh"));
    RailEndWidthMesh->SetupAttachment(SceneRoot);
    RailEndWidthMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CartRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CartRoot"));
    CartRoot->SetupAttachment(SceneRoot);

    CartCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CartCollision"));
    CartCollision->SetupAttachment(CartRoot);
    CartCollision->SetBoxExtent(FVector(60.0f, 60.0f, 80.0f));
    CartCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CartCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

    CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
    CartMesh->SetupAttachment(CartRoot);
    CartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    MirrorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MirrorPivot"));
    MirrorPivot->SetupAttachment(CartRoot);

    MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh"));
    MirrorMesh->SetupAttachment(MirrorPivot);
    MirrorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MirrorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    MirrorMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MirrorMesh->ComponentTags.Add(TEXT("Mirror"));

    PushDetectionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PushDetectionVolume"));
    PushDetectionVolume->SetupAttachment(CartRoot);
    PushDetectionVolume->SetBoxExtent(FVector(85.0f, 85.0f, 100.0f));
    PushDetectionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PushDetectionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    PushDetectionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PushDetectionVolume->SetGenerateOverlapEvents(true);

    InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
    InteractableComponent->SetReleaseMode(EInteractionReleaseMode::Immediate);
}

void ACPP_RailMirrorElement::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    UpdateRailVisuals();
    BuildInteractionOptions();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레일 위치와 거울 회전 상태를 클라이언트에 복제하도록 등록하는 함수
// OutLifetimeProps : 복제할 프로퍼티 목록
void ACPP_RailMirrorElement::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACPP_RailMirrorElement, CurrentRailOffset);
    DOREPLIFETIME(ACPP_RailMirrorElement, CurrentRotationOffset);
}

void ACPP_RailMirrorElement::BeginPlay()
{
    Super::BeginPlay();

    InitialCartRelativeLocation = CartRoot->GetRelativeLocation();
    InitialMirrorRelativeRotation = MirrorPivot->GetRelativeRotation();
    DisplayedRotationOffset = CurrentRotationOffset;
    bRotationAnimating = false;

    ApplyRailOffset();
    ApplyMirrorRotation();
    BuildInteractionOptions();

    if (HasAuthority())
    {
        InteractableComponent->OnInteractionStarted.AddUniqueDynamic(
            this,
            &ACPP_RailMirrorElement::HandleInteractionStarted);
        InteractableComponent->SetInteractionEnabled(false);
    }
}

void ACPP_RailMirrorElement::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateMirrorRotationAnimation(DeltaSeconds);

    if (!HasAuthority() || !bGimmickInteractionEnabled || IsSolvedLocked())
    {
        return;
    }

    const float DesiredRailSpeed = CalculateDesiredRailSpeed();
    if (FMath::IsNearlyZero(DesiredRailSpeed))
    {
        return;
    }

    const float LowerRailOffset = FMath::Min(MinRailOffset, MaxRailOffset);
    const float UpperRailOffset = FMath::Max(MinRailOffset, MaxRailOffset);
    const float NewRailOffset = FMath::Clamp(
        CurrentRailOffset + DesiredRailSpeed * DeltaSeconds,
        LowerRailOffset,
        UpperRailOffset);

    if (FMath::IsNearlyEqual(CurrentRailOffset, NewRailOffset))
    {
        return;
    }

    CurrentRailOffset = NewRailOffset;
    ApplyRailOffset();
    ForceNetUpdate();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레일 위치와 거울 회전 상태를 최초 배치 상태로 되돌리는 함수
void ACPP_RailMirrorElement::ResetElement()
{
    if (!HasAuthority())
    {
        return;
    }

    Super::ResetElement();

    CurrentRailOffset = 0.0f;
    CurrentRotationOffset = 0.0f;
    DisplayedRotationOffset = 0.0f;
    RotationAnimationStartOffset = 0.0f;
    RotationAnimationElapsed = 0.0f;
    bRotationAnimating = false;
    ApplyRailOffset();
    ApplyMirrorRotation();

    InteractableComponent->ResetInteractionState();
    InteractableComponent->SetInteractionEnabled(
        bGimmickInteractionEnabled && !IsSolvedLocked());
    ForceNetUpdate();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 공통 기믹 상태에 맞춰 몸으로 밀기와 회전 상호작용을 활성화하거나 비활성화하는 함수
// bEnabled : 반사판 기믹 상호작용 활성화 여부
void ACPP_RailMirrorElement::SetGimmickInteractionEnabled(bool bEnabled)
{
    if (!HasAuthority())
    {
        return;
    }

    bGimmickInteractionEnabled = bEnabled;
    InteractableComponent->SetInteractionEnabled(bEnabled && !IsSolvedLocked());
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 기믹 클리어 고정 상태에 맞춰 반사판 이동과 회전 상호작용을 잠그거나 해제하는 함수
void ACPP_RailMirrorElement::OnSolvedLockChanged()
{
    if (!HasAuthority())
    {
        return;
    }

    InteractableComponent->SetInteractionEnabled(
        bGimmickInteractionEnabled && !IsSolvedLocked());
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 선택한 상호작용 옵션에 따라 거울을 시계 또는 반시계 방향으로 회전시키는 함수
// Context : 서버에서 승인된 상호작용 대상과 옵션 정보
void ACPP_RailMirrorElement::HandleInteractionStarted(
    const FInteractionStartContext& Context)
{
    if (!HasAuthority() || !bGimmickInteractionEnabled || IsSolvedLocked())
    {
        return;
    }

    if (Context.SelectedOptionIndex == ClockwiseOptionIndex)
    {
        TryRotateMirror(1.0f, Context.Interactor);
    }
    else if (Context.SelectedOptionIndex == CounterClockwiseOptionIndex)
    {
        TryRotateMirror(-1.0f, Context.Interactor);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 복제된 레일 위치를 클라이언트의 카트 컴포넌트에 적용하는 함수
void ACPP_RailMirrorElement::OnRep_CurrentRailOffset()
{
    ApplyRailOffset();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 복제된 회전 각도를 클라이언트의 거울 피벗에 적용하는 함수
void ACPP_RailMirrorElement::OnRep_CurrentRotationOffset()
{
    StartMirrorRotationAnimation();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 레일 오프셋을 초기 카트 위치의 로컬 X축에 적용하는 함수
void ACPP_RailMirrorElement::ApplyRailOffset()
{
    CartRoot->SetRelativeLocation(
        InitialCartRelativeLocation + FVector(CurrentRailOffset, 0.0f, 0.0f));
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 회전 오프셋을 초기 거울 회전에 더해 세로축 회전을 적용하는 함수
void ACPP_RailMirrorElement::ApplyMirrorRotation()
{
    FRotator NewRotation = InitialMirrorRelativeRotation;
    NewRotation.Yaw += DisplayedRotationOffset;
    MirrorPivot->SetRelativeRotation(NewRotation);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 에디터에서 설정한 회전 단계를 반영해 시계·반시계 회전 UI 문구를 구성하는 함수
void ACPP_RailMirrorElement::BuildInteractionOptions()
{
    FNumberFormattingOptions NumberFormat;
    NumberFormat.SetMinimumFractionalDigits(0);
    NumberFormat.SetMaximumFractionalDigits(2);

    const FText StepText = FText::AsNumber(RotationStepDegrees, &NumberFormat);

    TArray<FInteractionOption> Options;
    Options.SetNum(2);
    Options[ClockwiseOptionIndex].DisplayText = FText::Format(
        NSLOCTEXT("RailMirror", "RotateClockwise", "시계 방향 {0}° 회전"),
        StepText);
    Options[CounterClockwiseOptionIndex].DisplayText = FText::Format(
        NSLOCTEXT("RailMirror", "RotateCounterClockwise", "반시계 방향 {0}° 회전"),
        StepText);

    InteractableComponent->SetInteractionOptions(Options);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 표시 각도를 시작점으로 삼아 복제된 목표 각도까지의 회전 보간을 시작하는 함수
void ACPP_RailMirrorElement::StartMirrorRotationAnimation()
{
    RotationAnimationStartOffset = DisplayedRotationOffset;
    RotationAnimationElapsed = 0.0f;

    if (RotationAnimationDuration <= UE_KINDA_SMALL_NUMBER ||
        FMath::IsNearlyEqual(DisplayedRotationOffset, CurrentRotationOffset))
    {
        DisplayedRotationOffset = CurrentRotationOffset;
        bRotationAnimating = false;
        ApplyMirrorRotation();
        return;
    }

    bRotationAnimating = true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 설정된 시간 비율과 Ease In/Out 곡선으로 이번 프레임의 거울 표시 각도를 갱신하는 함수
// DeltaSeconds : 이전 프레임부터 흐른 시간
void ACPP_RailMirrorElement::UpdateMirrorRotationAnimation(float DeltaSeconds)
{
    if (!bRotationAnimating)
    {
        return;
    }

    const float SafeDuration = FMath::Max(RotationAnimationDuration, UE_KINDA_SMALL_NUMBER);
    RotationAnimationElapsed += DeltaSeconds;

    const float LinearAlpha = FMath::Clamp(RotationAnimationElapsed / SafeDuration, 0.0f, 1.0f);
    const float EasedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, LinearAlpha, 2.0f);
    DisplayedRotationOffset = FMath::Lerp(
        RotationAnimationStartOffset,
        CurrentRotationOffset,
        EasedAlpha);
    ApplyMirrorRotation();

    if (LinearAlpha >= 1.0f)
    {
        DisplayedRotationOffset = CurrentRotationOffset;
        bRotationAnimating = false;
        ApplyMirrorRotation();
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 레일 범위에 맞춰 길이 메쉬를 늘리고 폭 메쉬를 양 끝에 원본 크기로 배치하는 함수
void ACPP_RailMirrorElement::UpdateRailVisuals()
{
    if (!RailLengthMesh || !RailStartWidthMesh || !RailEndWidthMesh || !CartRoot)
    {
        return;
    }

    const float CartInitialX = CartRoot->GetRelativeLocation().X;
    const float LowerRailX = CartInitialX + FMath::Min(MinRailOffset, MaxRailOffset);
    const float UpperRailX = CartInitialX + FMath::Max(MinRailOffset, MaxRailOffset);
    const float RailCenterX = (LowerRailX + UpperRailX) * 0.5f;
    const float RailLength = UpperRailX - LowerRailX;

    RailStartWidthMesh->SetStaticMesh(RailWidthMeshAsset);
    RailEndWidthMesh->SetStaticMesh(RailWidthMeshAsset);

    FVector StartLocation = RailStartWidthMesh->GetRelativeLocation();
    StartLocation.X = LowerRailX;
    RailStartWidthMesh->SetRelativeLocation(StartLocation);

    FVector EndLocation = RailEndWidthMesh->GetRelativeLocation();
    EndLocation.X = UpperRailX;
    RailEndWidthMesh->SetRelativeLocation(EndLocation);

    const UStaticMesh* LengthStaticMesh = RailLengthMesh->GetStaticMesh();
    if (!LengthStaticMesh)
    {
        return;
    }

    const FBoxSphereBounds MeshBounds = LengthStaticMesh->GetBounds();
    const float MeshLength = MeshBounds.BoxExtent.X * 2.0f;
    if (MeshLength <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    FVector LengthScale = RailLengthMesh->GetRelativeScale3D();
    LengthScale.X = RailLength / MeshLength;
    RailLengthMesh->SetRelativeScale3D(LengthScale);

    FVector LengthLocation = RailLengthMesh->GetRelativeLocation();
    LengthLocation.X = RailCenterX - MeshBounds.Origin.X * LengthScale.X;
    RailLengthMesh->SetRelativeLocation(LengthLocation);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 좌우에서 실제로 밀고 있는 플레이어 수를 비교해 이번 프레임의 카트 이동속도를 계산하는 함수
// Return Value : 레일 로컬 X축 기준 부호를 포함한 카트 이동속도
float ACPP_RailMirrorElement::CalculateDesiredRailSpeed() const
{
    TArray<AActor*> OverlappingActors;
    PushDetectionVolume->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

    const FVector RailAxis = SceneRoot->GetForwardVector().GetSafeNormal();
    const FVector CartLocation = CartRoot->GetComponentLocation();

    int32 PositivePusherCount = 0;
    int32 NegativePusherCount = 0;
    float PositiveSpeedSum = 0.0f;
    float NegativeSpeedSum = 0.0f;

    for (AActor* OverlappingActor : OverlappingActors)
    {
        const APawn* Pusher = Cast<APawn>(OverlappingActor);
        if (!Pusher || !Pusher->IsPlayerControlled())
        {
            continue;
        }

        const float IntentSpeed = CalculatePusherIntentSpeed(Pusher, RailAxis);
        const float PusherSide = FVector::DotProduct(
            Pusher->GetActorLocation() - CartLocation,
            RailAxis);

        if (PusherSide < -UE_KINDA_SMALL_NUMBER &&
            IntentSpeed > MinimumPushIntentSpeed)
        {
            ++PositivePusherCount;
            PositiveSpeedSum += IntentSpeed;
        }
        else if (PusherSide > UE_KINDA_SMALL_NUMBER &&
            IntentSpeed < -MinimumPushIntentSpeed)
        {
            ++NegativePusherCount;
            NegativeSpeedSum += IntentSpeed;
        }
    }

    if (PositivePusherCount == NegativePusherCount)
    {
        return 0.0f;
    }

    const float WinningSideAverageSpeed = PositivePusherCount > NegativePusherCount
        ? PositiveSpeedSum / static_cast<float>(PositivePusherCount)
        : NegativeSpeedSum / static_cast<float>(NegativePusherCount);

    return FMath::Clamp(
        WinningSideAverageSpeed,
        -FMath::Max(MaxRailMoveSpeed, 0.0f),
        FMath::Max(MaxRailMoveSpeed, 0.0f));
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어의 실제 속도와 최근 이동 입력으로 레일 방향 밀기 의도 속도를 계산하는 함수
// Pusher : 반사판 밀기 감지 범위 안의 플레이어 Pawn
// RailAxis : 월드 공간의 레일 이동축
// Return Value : 레일축에 투영한 부호 포함 밀기 의도 속도
float ACPP_RailMirrorElement::CalculatePusherIntentSpeed(
    const APawn* Pusher,
    const FVector& RailAxis) const
{
    if (!Pusher)
    {
        return 0.0f;
    }

    const UPawnMovementComponent* MovementComponent = Pusher->GetMovementComponent();
    FVector MovementIntent = Pusher->GetLastMovementInputVector();
    float MovementIntentScale = FMath::Clamp(MovementIntent.Size(), 0.0f, 1.0f);

    if (const UCharacterMovementComponent* CharacterMovement =
        Cast<UCharacterMovementComponent>(MovementComponent))
    {
        const FVector CurrentAcceleration = CharacterMovement->GetCurrentAcceleration();
        if (!CurrentAcceleration.IsNearlyZero())
        {
            MovementIntent = CurrentAcceleration.GetSafeNormal();
            MovementIntentScale = CharacterMovement->GetMaxAcceleration() > UE_KINDA_SMALL_NUMBER
                ? FMath::Clamp(
                    CurrentAcceleration.Size() / CharacterMovement->GetMaxAcceleration(),
                    0.0f,
                    1.0f)
                : 1.0f;
        }
    }

    const float RailInput = FVector::DotProduct(MovementIntent, RailAxis);
    if (FMath::IsNearlyZero(RailInput))
    {
        return FVector::DotProduct(Pusher->GetVelocity(), RailAxis);
    }

    const float IntendedMaxSpeed = MovementComponent
        ? MovementComponent->GetMaxSpeed()
        : Pusher->GetVelocity().Size();
    return RailInput * MovementIntentScale * IntendedMaxSpeed;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 설정된 단계와 최대 범위를 검증한 뒤 거울 회전을 서버 상태에 적용하는 함수
// DirectionSign : 시계 방향이면 1, 반시계 방향이면 -1
// Interactor : 회전을 요청한 플레이어 액터
// Return Value : 회전이 적용되었으면 true, 범위를 벗어나면 false
bool ACPP_RailMirrorElement::TryRotateMirror(
    float DirectionSign,
    AActor* Interactor)
{
    const float SafeRotationStep = FMath::Clamp(RotationStepDegrees, 0.1f, 360.0f);
    const float SafeRotationRange = FMath::Clamp(MaxRotationRange, 0.0f, 360.0f);
    const bool bUnlimitedRotation = SafeRotationRange >= 360.0f - UE_KINDA_SMALL_NUMBER;
    const float TargetRotationOffset =
        CurrentRotationOffset + FMath::Sign(DirectionSign) * SafeRotationStep;

    if (!bUnlimitedRotation)
    {
        const float HalfRotationRange = SafeRotationRange * 0.5f;
        if (TargetRotationOffset < -HalfRotationRange - UE_KINDA_SMALL_NUMBER ||
            TargetRotationOffset > HalfRotationRange + UE_KINDA_SMALL_NUMBER)
        {
            SendRotationLimitNotice(Interactor, DirectionSign > 0.0f);
            return false;
        }
    }

    CurrentRotationOffset = TargetRotationOffset;
    StartMirrorRotationAnimation();
    ForceNetUpdate();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 회전 한계에 도달했음을 요청한 플레이어에게만 Notice로 전달하는 함수
// Interactor : 회전을 요청한 플레이어 Pawn 또는 Controller
// bClockwise : 시계 방향 한계이면 true, 반시계 방향 한계이면 false
void ACPP_RailMirrorElement::SendRotationLimitNotice(
    AActor* Interactor,
    bool bClockwise) const
{
    const APawn* InteractorPawn = Cast<APawn>(Interactor);
    ADungeonPC* DungeonPC = InteractorPawn
        ? Cast<ADungeonPC>(InteractorPawn->GetController())
        : Cast<ADungeonPC>(Interactor);
    if (!DungeonPC)
    {
        return;
    }

    DungeonPC->SendNoticeToClient(
        bClockwise
            ? FText::FromString(TEXT("시계 방향 회전 한계에 도달했습니다."))
            : FText::FromString(TEXT("반시계 방향 회전 한계에 도달했습니다.")),
        0.0f);
}
