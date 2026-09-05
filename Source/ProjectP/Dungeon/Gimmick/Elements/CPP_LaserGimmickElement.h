#pragma once

#include "CoreMinimal.h"
#include "../CPP_GimmickElementBase.h"
#include "CPP_LaserGimmickElement.generated.h"

class ALaserReflectionActor;

UCLASS()
class PROJECTP_API ACPP_LaserGimmickElement : public ACPP_GimmickElementBase
{
    GENERATED_BODY()

public:
    ACPP_LaserGimmickElement();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual bool IsSatisfied() const override;
    virtual void ResetElement() override;
    virtual void SetGimmickInteractionEnabled(bool bEnabled) override;

    UFUNCTION(BlueprintPure, Category = "Gimmick|Laser")
    float GetClearHoldTime() const;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnRep_LaserEnabled();

    void ApplyLaserEnabledState() const;
    void RefreshClearState();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Laser", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ALaserReflectionActor> LaserActor;

    UPROPERTY(ReplicatedUsing = OnRep_LaserEnabled)
    bool bLaserEnabled = false;

    bool bLastObservedClear = false;
    float ClearStartServerTime = -1.0f;
};
