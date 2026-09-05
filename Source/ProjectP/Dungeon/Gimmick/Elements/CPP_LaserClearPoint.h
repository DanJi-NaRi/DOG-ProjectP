#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_LaserClearPoint.generated.h"

class ACPP_LaserGimmickElement;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PROJECTP_API ACPP_LaserClearPoint : public AActor
{
    GENERATED_BODY()

public:
    ACPP_LaserClearPoint();

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure, Category = "Gimmick|Laser Clear Point")
    float GetChargeProgress() const;

    UFUNCTION(BlueprintPure, Category = "Gimmick|Laser Clear Point")
    bool IsLaserContactActive() const;

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Laser Clear Point|FX")
    void OnLaserContactStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Laser Clear Point|FX")
    void OnLaserContactEnded();

    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Laser Clear Point|FX")
    void OnChargeProgressChanged(float NormalizedProgress);

    UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Laser Clear Point|FX")
    void OnChargeCompleted();

private:
    float GetTargetChargeProgress() const;
    void InitializeChargeMaterial();
    void RefreshVisualState(float DeltaSeconds);
    void UpdateChargeMaterialParameters();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> VFXRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ChargeVisualMesh;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|Setup", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ACPP_LaserGimmickElement> LaserElement;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|FX", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
    int32 ChargeMaterialSlotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|FX", meta = (AllowPrivateAccess = "true"))
    FName ChargeProgressParameterName = TEXT("ChargeProgress");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|FX", meta = (AllowPrivateAccess = "true"))
    FName LaserContactParameterName = TEXT("LaserContact");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Laser Clear Point|FX", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.1", DisplayName = "Charge Drain Speed (Progress Per Second)"))
    float ChargeDrainSpeed = 2.0f;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ChargeMaterialInstance;

    float DisplayedChargeProgress = 0.0f;
    float LastChargeProgress = -1.0f;
    bool bLastLaserContactActive = false;
    bool bChargeCompleted = false;
};
