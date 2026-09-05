#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "../../GAS/MyAbilitySet.h"
#include "CPP_DumbPlayer.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UMyAttributeSet;

UCLASS()
class PROJECTP_API ACPP_DumbPlayer : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_DumbPlayer();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Dumb Player|Attack")
	bool ActivateBasicAttack(AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category = "Dumb Player|Leader")
	AActor* GetLeaderActor() const;

	UFUNCTION(BlueprintCallable, Category = "Dumb Player|Leader")
	void SetLeaderActor(AActor* NewLeaderActor);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Dumb Player|Leader")
	TObjectPtr<AActor> LeaderActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dumb Player|Movement", meta = (ClampMin = "0.0"))
	float FollowStartDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dumb Player|Movement", meta = (ClampMin = "0.0"))
	float FollowStopDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dumb Player|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 600.0f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dumb Player|MyGAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dumb Player|MyGAS")
	TObjectPtr<UMyAttributeSet> MyAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dumb Player|MyGAS")
	TObjectPtr<UMyAbilitySet> DefaultAbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dumb Player|MyGAS")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

private:
	void InitializeAbilitySystem();
	void ApplyDefaultAttributes();
	void GrantDefaultAbilities();

	UPROPERTY(Transient)
	FMyAbilitySetGrantedHandles GrantedAbilityHandles;
};
