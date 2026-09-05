#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClearComponent.generated.h"

class AZoneBase;
class UClearComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClearConditionSatisfiedSignature, UClearComponent*, ClearComponent);

UCLASS(Blueprintable, ClassGroup=(Zone), meta=(BlueprintSpawnableComponent))
class PROJECTP_API UClearComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UClearComponent();

	UFUNCTION(BlueprintCallable, Category = "Zone|Clear")
	void InitializeClearComponent(AZoneBase* InOwnerZone);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Zone|Clear")
	void ActivateClearCondition();
	virtual void ActivateClearCondition_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Zone|Clear")
	void DeactivateClearCondition();
	virtual void DeactivateClearCondition_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Zone|Clear")
	void ResetClearCondition();
	virtual void ResetClearCondition_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Zone|Clear")
	void MarkClearSatisfied();

	UFUNCTION(BlueprintPure, Category = "Zone|Clear")
	bool IsClearSatisfied() const { return bClearSatisfied; }

	UFUNCTION(BlueprintPure, Category = "Zone|Clear")
	bool IsClearConditionActive() const { return bClearConditionActive; }

	UPROPERTY(BlueprintAssignable, Category = "Zone|Clear")
	FOnClearConditionSatisfiedSignature OnClearConditionSatisfied;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Zone|Clear")
	TObjectPtr<AZoneBase> OwnerZone;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear")
	bool bClearConditionActive = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear")
	bool bClearSatisfied = false;

private:
	bool HasOwnerAuthority() const;
	void BroadcastClearSatisfied();

	bool bClearSatisfiedBroadcasted = false;
};
