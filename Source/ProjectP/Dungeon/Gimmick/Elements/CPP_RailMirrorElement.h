#pragma once

#include "CoreMinimal.h"
#include "../CPP_GimmickElementBase.h"
#include "../../Interactable/Components/InteractableComponent.h"
#include "CPP_RailMirrorElement.generated.h"

class APawn;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class PROJECTP_API ACPP_RailMirrorElement : public ACPP_GimmickElementBase
{
    GENERATED_BODY()

public:
    ACPP_RailMirrorElement();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void ResetElement() override;
    virtual void SetGimmickInteractionEnabled(bool bEnabled) override;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void OnSolvedLockChanged() override;

private:
    static constexpr int32 ClockwiseOptionIndex = 0;
    static constexpr int32 CounterClockwiseOptionIndex = 1;

    UFUNCTION()
    void HandleInteractionStarted(const FInteractionStartContext& Context);

    UFUNCTION()
    void OnRep_CurrentRailOffset();

    UFUNCTION()
    void OnRep_CurrentRotationOffset();

    void ApplyRailOffset();
    void ApplyMirrorRotation();
    void BuildInteractionOptions();
    void StartMirrorRotationAnimation();
    void UpdateMirrorRotationAnimation(float DeltaSeconds);
    void UpdateRailVisuals();
    float CalculateDesiredRailSpeed() const;
    float CalculatePusherIntentSpeed(const APawn* Pusher, const FVector& RailAxis) const;
    bool TryRotateMirror(float DirectionSign, AActor* Interactor);
    void SendRotationLimitNotice(AActor* Interactor, bool bClockwise) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RailLengthMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RailStartWidthMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> RailEndWidthMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> CartRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> CartCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> CartMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> MirrorPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> MirrorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> PushDetectionVolume;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInteractableComponent> InteractableComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rail", meta = (AllowPrivateAccess = "true", Units = "cm"))
    float MinRailOffset = -500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rail", meta = (AllowPrivateAccess = "true", Units = "cm"))
    float MaxRailOffset = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rail", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMesh> RailWidthMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
    float MaxRailMoveSpeed = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rail", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
    float MinimumPushIntentSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rotation", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "360.0", Units = "deg"))
    float RotationStepDegrees = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rotation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
    float MaxRotationRange = 360.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|RailMirror|Rotation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
    float RotationAnimationDuration = 0.25f;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentRailOffset, BlueprintReadOnly, Category = "Gimmick|RailMirror|State", meta = (AllowPrivateAccess = "true"))
    float CurrentRailOffset = 0.0f;

    UPROPERTY(ReplicatedUsing = OnRep_CurrentRotationOffset, BlueprintReadOnly, Category = "Gimmick|RailMirror|State", meta = (AllowPrivateAccess = "true"))
    float CurrentRotationOffset = 0.0f;

    FVector InitialCartRelativeLocation = FVector::ZeroVector;
    FRotator InitialMirrorRelativeRotation = FRotator::ZeroRotator;
    float DisplayedRotationOffset = 0.0f;
    float RotationAnimationStartOffset = 0.0f;
    float RotationAnimationElapsed = 0.0f;
    bool bGimmickInteractionEnabled = false;
    bool bRotationAnimating = false;
};
