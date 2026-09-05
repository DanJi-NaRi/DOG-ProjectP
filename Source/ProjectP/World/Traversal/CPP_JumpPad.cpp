////////////////////////////
//! \file CPP_JumpPad.cpp
//! \brief 점프대 범위 진입·이탈에 따라 플레이어의 일반 점프 높이를 변경하고 복구한다.
#include "CPP_JumpPad.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/PlayerCharacterBase.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어 감지 범위와 발판 메시를 가진 점프대 액터를 생성하는 함수
ACPP_JumpPad::ACPP_JumpPad()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
    PadMesh->SetupAttachment(SceneRoot);
    PadMesh->SetCollisionProfileName(TEXT("BlockAll"));

    ActivationBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationBox"));
    ActivationBox->SetupAttachment(SceneRoot);
    ActivationBox->InitBoxExtent(FVector(100.0f, 100.0f, 60.0f));
    ActivationBox->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
    ActivationBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ActivationBox->SetCollisionObjectType(ECC_WorldDynamic);
    ActivationBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    ActivationBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ActivationBox->SetGenerateOverlapEvents(true);
}

void ACPP_JumpPad::BeginPlay()
{
    Super::BeginPlay();

    ActivationBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACPP_JumpPad::OnActivationBoxBeginOverlap);
    ActivationBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ACPP_JumpPad::OnActivationBoxEndOverlap);

    ApplyToCurrentOverlaps();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 점프대가 종료될 때 적용 중인 플레이어 점프값을 모두 원래 값으로 복구하는 함수
// EndPlayReason : 점프대 액터가 종료되는 이유
void ACPP_JumpPad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestoreAllJumpValues();

    Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어가 감지 범위에 들어오면 목표 높이에 필요한 강화 점프값을 적용하는 함수
// OverlappedComponent : 오버랩을 감지한 점프대 박스 컴포넌트
// OtherActor : 감지 범위에 들어온 액터
// OtherComponent : 감지 범위에 들어온 액터의 컴포넌트
// OtherBodyIndex : 오버랩된 바디 인덱스
// bFromSweep : 스윕 이동으로 발생한 오버랩인지 여부
// SweepResult : 스윕 오버랩 충돌 결과
void ACPP_JumpPad::OnActivationBoxBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OtherActor);
    if (!PlayerCharacter)
    {
        return;
    }

    ApplyEnhancedJump(PlayerCharacter);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어가 감지 범위에서 완전히 벗어나면 진입 전 점프값으로 복구하는 함수
// OverlappedComponent : 오버랩 종료를 감지한 점프대 박스 컴포넌트
// OtherActor : 감지 범위에서 벗어난 액터
// OtherComponent : 감지 범위에서 벗어난 액터의 컴포넌트
// OtherBodyIndex : 오버랩이 종료된 바디 인덱스
void ACPP_JumpPad::OnActivationBoxEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;

    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OtherActor);
    if (!PlayerCharacter || ActivationBox->IsOverlappingActor(PlayerCharacter))
    {
        return;
    }

    RestoreOriginalJump(PlayerCharacter);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어의 기존 JumpZVelocity를 저장하고 점프대 목표 높이에 필요한 값으로 변경하는 함수
// Character : 강화 점프값을 적용할 플레이어 캐릭터
void ACPP_JumpPad::ApplyEnhancedJump(ACharacter* Character)
{
    if (!IsValid(Character))
    {
        return;
    }

    UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
    if (!MovementComponent)
    {
        return;
    }

    const TWeakObjectPtr<ACharacter> CharacterKey(Character);
    if (!OriginalJumpZVelocities.Contains(CharacterKey))
    {
        OriginalJumpZVelocities.Add(CharacterKey, MovementComponent->JumpZVelocity);
    }

    MovementComponent->JumpZVelocity = CalculateRequiredJumpZVelocity(MovementComponent);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 플레이어에게 저장된 진입 전 JumpZVelocity를 복구하고 저장 항목을 제거하는 함수
// Character : 일반 점프값으로 복구할 플레이어 캐릭터
void ACPP_JumpPad::RestoreOriginalJump(ACharacter* Character)
{
    if (!IsValid(Character))
    {
        return;
    }

    const TWeakObjectPtr<ACharacter> CharacterKey(Character);
    const float* OriginalJumpZVelocity = OriginalJumpZVelocities.Find(CharacterKey);
    if (!OriginalJumpZVelocity)
    {
        return;
    }

    if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
    {
        MovementComponent->JumpZVelocity = *OriginalJumpZVelocity;
    }

    OriginalJumpZVelocities.Remove(CharacterKey);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// BeginPlay 전에 이미 점프대 범위와 겹친 플레이어에게 강화 점프값을 적용하는 함수
void ACPP_JumpPad::ApplyToCurrentOverlaps()
{
    if (!ActivationBox)
    {
        return;
    }

    TArray<AActor*> OverlappingActors;
    ActivationBox->GetOverlappingActors(OverlappingActors, APlayerCharacterBase::StaticClass());
    for (AActor* OverlappingActor : OverlappingActors)
    {
        if (APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OverlappingActor))
        {
            ApplyEnhancedJump(PlayerCharacter);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 점프대가 추적 중인 모든 유효 플레이어의 JumpZVelocity를 원래 값으로 복구하는 함수
void ACPP_JumpPad::RestoreAllJumpValues()
{
    for (const TPair<TWeakObjectPtr<ACharacter>, float>& Pair : OriginalJumpZVelocities)
    {
        ACharacter* Character = Pair.Key.Get();
        if (!IsValid(Character))
        {
            continue;
        }

        if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
        {
            MovementComponent->JumpZVelocity = Pair.Value;
        }
    }

    OriginalJumpZVelocities.Reset();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 플레이어 중력에서 목표 높이에 도달하는 데 필요한 초기 Z 속도를 계산하는 함수
// MovementComponent : 중력값과 현재 일반 점프값을 제공하는 캐릭터 이동 컴포넌트
// Return Value : 목표 높이에 필요한 양의 Z 속도, 계산할 수 없으면 현재 JumpZVelocity
float ACPP_JumpPad::CalculateRequiredJumpZVelocity(const UCharacterMovementComponent* MovementComponent) const
{
    if (!MovementComponent)
    {
        return 0.0f;
    }

    const float GravityMagnitude = FMath::Abs(MovementComponent->GetGravityZ());
    const float SafeLaunchHeight = FMath::Max(LaunchHeight, 0.0f);
    if (GravityMagnitude <= KINDA_SMALL_NUMBER || SafeLaunchHeight <= KINDA_SMALL_NUMBER)
    {
        return MovementComponent->JumpZVelocity;
    }

    return FMath::Sqrt(2.0f * GravityMagnitude * SafeLaunchHeight);
}
