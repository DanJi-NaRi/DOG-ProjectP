#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "CPP_RitualObjective.generated.h"

class UAbilitySystemComponent;
class UCPP_RitualObjectiveAttributeSet;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnRitualGaugeChangedSignature,
	float, CurrentGauge,
	float, MaxGauge);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRitualObjectiveEnteredSignature, AActor*);
DECLARE_MULTICAST_DELEGATE(FOnRitualGaugeFullSignature);

////////////////////////////
//! \class ACPP_RitualObjective
//! \brief 의식 흡수 범위와 0~100 게이지를 서버 권한으로 관리하는 고정 목표 Actor.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_RitualObjective : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_RitualObjective();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Ritual Objective|Gauge")
	float GetCurrentGauge() const { return CurrentGauge; }

	UFUNCTION(BlueprintPure, Category = "Ritual Objective|Gauge")
	float GetMaxGauge() const { return 100.0f; }

	UFUNCTION(BlueprintPure, Category = "Ritual Objective|Gauge")
	float GetGaugeRatio() const { return CurrentGauge / GetMaxGauge(); }

	UFUNCTION(BlueprintPure, Category = "Ritual Objective|State")
	bool IsAbsorptionEnabled() const { return bAbsorptionEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Ritual Objective|State")
	void SetAbsorptionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Ritual Objective|State")
	void ResetObjective();

	bool ApplyAbsorption(AActor* AbsorbedActor);

	UPROPERTY(BlueprintAssignable, Category = "Ritual Objective|Event")
	FOnRitualGaugeChangedSignature OnGaugeChanged;

	FOnRitualObjectiveEnteredSignature OnObjectiveEntered;
	FOnRitualGaugeFullSignature OnGaugeFull;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual Objective|Components")
	TObjectPtr<USphereComponent> AbsorptionRange;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual Objective|MyGAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual Objective|MyGAS")
	TObjectPtr<UCPP_RitualObjectiveAttributeSet> AttributeSet;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Ritual Objective|Gauge", meta = (ClampMin = "0.1", ClampMax = "100.0"))
	float GaugeIncreasePerAbsorption = 10.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ritual Objective|Event")
	void BP_OnAbsorbed(FVector AbsorbedLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Ritual Objective|Event")
	void BP_OnGaugeFull();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ritual Objective|Event")
	void BP_OnObjectiveReset();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ritual Objective|Event")
	void BP_OnAbsorptionEnabledChanged(bool bEnabled);

private:
	void InitializeAbilitySystem();

	UFUNCTION()
	void HandleAbsorptionRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAbsorptionEffects(FVector AbsorbedLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayGaugeFullEffects();

	UFUNCTION()
	void OnRep_CurrentGauge();

	UFUNCTION()
	void OnRep_AbsorptionEnabled();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentGauge, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ritual Objective|Gauge", meta = (AllowPrivateAccess = "true"))
	float CurrentGauge = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_AbsorptionEnabled)
	bool bAbsorptionEnabled = false;
};
