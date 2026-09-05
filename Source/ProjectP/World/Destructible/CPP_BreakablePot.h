#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "CPP_BreakablePot.generated.h"

class UAbilitySystemComponent;
class UCPP_BreakableAttributeSet;
class UGeometryCollectionComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

UCLASS()
class PROJECTP_API ACPP_BreakablePot : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACPP_BreakablePot();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Breakable Pot")
	bool IsBroken() const { return bBroken; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void OnRep_Broken();

	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void BreakPot();
	void ApplyBrokenVisualState();
	void ApplyBreakForce();
	void RefreshFadeState();
	void BeginFadeOut();
	void UpdateFadeFromServerTime();
	void DestroyPotOnServer();
	double GetSynchronizedServerTime() const;
	float ResolveBreakImpulseStrength(float DamageAmount) const;

	UPROPERTY(VisibleAnywhere, Category = "Breakable Pot|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Breakable Pot|Components")
	TObjectPtr<UStaticMeshComponent> IntactMesh;

	UPROPERTY(VisibleAnywhere, Category = "Breakable Pot|Components")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollection;

	UPROPERTY(VisibleAnywhere, Category = "Breakable Pot|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Breakable Pot|GAS")
	TObjectPtr<UCPP_BreakableAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|GAS", meta = (ClampMin = "1.0"))
	float InitialHealth = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos", meta = (ClampMin = "0.0"))
	float BreakImpulseRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos|Damage Tiers", meta = (ClampMin = "0.0"))
	float MediumDamageThreshold = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos|Damage Tiers", meta = (ClampMin = "0.0"))
	float HighDamageThreshold = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos|Damage Tiers", meta = (ClampMin = "0.0"))
	float LowDamageImpulseStrength = 75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos|Damage Tiers", meta = (ClampMin = "0.0", DisplayName = "Medium Damage Impulse Strength"))
	float BreakImpulseStrength = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Chaos|Damage Tiers", meta = (ClampMin = "0.0"))
	float HighDamageImpulseStrength = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Cleanup", meta = (ClampMin = "0.0"))
	float BrokenPieceLifetime = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Breakable Pot|Cleanup", meta = (ClampMin = "0.0"))
	float FadeDuration = 0.5f;

	FTimerHandle FadeDelayTimerHandle;
	FTimerHandle ServerDestroyTimerHandle;
	bool bFadingOut = false;

	UPROPERTY(Replicated)
	float ResolvedBreakImpulseStrength = 0.0f;

	UPROPERTY(Replicated)
	double BreakServerTime = -1.0;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;
};
