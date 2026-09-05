////////////////////////////
//! \file CPP_JumpPad.h
//! \brief 점프대 범위 안에서 플레이어의 일반 점프 높이를 강화하는 액터 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_JumpPad.generated.h"

class ACharacter;
class UBoxComponent;
class UCharacterMovementComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

////////////////////////////
//! \class ACPP_JumpPad
//! \brief 플레이어가 범위 안에서 점프 입력을 사용하면 지정 높이까지 점프하도록 JumpZVelocity를 임시 변경한다.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_JumpPad : public AActor
{
    GENERATED_BODY()

public:
    ACPP_JumpPad();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION()
    void OnActivationBoxBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnActivationBoxEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    //! 점프대 액터의 위치와 컴포넌트 부착 기준이 되는 루트다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
    TObjectPtr<USceneComponent> SceneRoot;

    //! 플레이어가 밟을 점프대 외형과 물리 충돌을 담당하는 메시다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
    TObjectPtr<UStaticMeshComponent> PadMesh;

    //! 강화 점프가 활성화되는 플레이어 감지 범위다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
    TObjectPtr<UBoxComponent> ActivationBox;

    //! 점프대 표면을 기준으로 플레이어가 도달할 목표 최고 높이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump Pad", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float LaunchHeight = 1400.0f;

private:
    void ApplyEnhancedJump(ACharacter* Character);
    void RestoreOriginalJump(ACharacter* Character);
    void ApplyToCurrentOverlaps();
    void RestoreAllJumpValues();
    float CalculateRequiredJumpZVelocity(const UCharacterMovementComponent* MovementComponent) const;

    //! 점프대 진입 전 플레이어별 JumpZVelocity다. 이탈 시 정확한 기존값으로 복구한다.
    TMap<TWeakObjectPtr<ACharacter>, float> OriginalJumpZVelocities;
};
