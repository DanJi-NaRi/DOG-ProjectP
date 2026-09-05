#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "CPP_DefenseObjective.generated.h"

class UAbilitySystemComponent;
class UCPP_DefenseObjectiveAttributeSet;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnDefenseObjectiveHealthChangedSignature,
	float, CurrentHealth,
	float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDefenseObjectiveDestroyedSignature);

////////////////////////////
//! \class ACPP_DefenseObjective
//! \brief Battle Zone에서 Enemy의 고정 타겟이 되는 GAS 기반 단일 방어 거점.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_DefenseObjective : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_DefenseObjective();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Defense Objective|Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Defense Objective|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Defense Objective|Health")
	float GetHealthRatio() const;

	UFUNCTION(BlueprintPure, Category = "Defense Objective|State")
	bool IsObjectiveDestroyed() const { return bObjectiveDestroyed; }

	UFUNCTION(BlueprintPure, Category = "Defense Objective|State")
	bool CanReceiveDamage() const { return bDamageEnabled && !bObjectiveDestroyed; }

	UFUNCTION(BlueprintCallable, Category = "Defense Objective|State")
	void SetDamageEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Defense Objective|State")
	void ResetObjective();

	UPROPERTY(BlueprintAssignable, Category = "Defense Objective|Event")
	FOnDefenseObjectiveHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Defense Objective|Event")
	FOnDefenseObjectiveDestroyedSignature OnObjectiveDestroyed;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defense Objective|MyGAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defense Objective|MyGAS")
	TObjectPtr<UCPP_DefenseObjectiveAttributeSet> AttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Objective|Health", meta = (ClampMin = "1.0"))
	float InitialMaxHealth = 1000.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Defense Objective|Event")
	void BP_OnObjectiveDestroyed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Defense Objective|Event")
	void BP_OnObjectiveReset();

	UFUNCTION(BlueprintImplementableEvent, Category = "Defense Objective|Event")
	void BP_OnDamageEnabledChanged(bool bEnabled);

private:
	void InitializeAbilitySystem();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleObjectiveDestroyed();

	UFUNCTION()
	void OnRep_ObjectiveDestroyed();

	UFUNCTION()
	void OnRep_DamageEnabled();

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveDestroyed)
	bool bObjectiveDestroyed = false;

	UPROPERTY(ReplicatedUsing = OnRep_DamageEnabled)
	bool bDamageEnabled = false;
};
