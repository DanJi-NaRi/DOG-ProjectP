#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "CPP_BreachObjective.generated.h"

class UAbilitySystemComponent;
class UCPP_BreachObjectiveAttributeSet;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBreachObjectiveHitSignature, AActor*);

////////////////////////////
//! \class ACPP_BreachObjective
//! \brief Enemy의 고정 공격 대상이 되어 실제 피해 한 건을 돌파 적중 이벤트로 변환하는 GAS 기반 목표 지점.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_BreachObjective : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_BreachObjective();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Breach Objective|State")
	bool CanReceiveDamage() const { return bDamageEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Breach Objective|State")
	void SetDamageEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Breach Objective|State")
	void ResetObjective();

	FOnBreachObjectiveHitSignature OnBreachObjectiveHit;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach Objective|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach Objective|MyGAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach Objective|MyGAS")
	TObjectPtr<UCPP_BreachObjectiveAttributeSet> AttributeSet;

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Objective|Event")
	void BP_OnObjectiveHit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Objective|Event")
	void BP_OnObjectiveReset();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Objective|Event")
	void BP_OnDamageEnabledChanged(bool bEnabled);

private:
	friend class UCPP_BreachObjectiveAttributeSet;

	void InitializeAbilitySystem();
	void HandleIncomingDamage(AActor* SourceActor);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayObjectiveHitEffects();

	UFUNCTION()
	void OnRep_DamageEnabled();

	UPROPERTY(ReplicatedUsing = OnRep_DamageEnabled)
	bool bDamageEnabled = false;
};
