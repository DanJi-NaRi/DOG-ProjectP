// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy/Abilities/CPP_EnemyAttackPatternData.h"
#include "CPP_EnemyAreaBase.generated.h"

class ACPP_EnemyTelegraphActor;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;

UCLASS()
class PROJECTP_API ACPP_EnemyAreaBase : public AActor
{
	GENERATED_BODY()

public:
	ACPP_EnemyAreaBase();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Area")
	void InitializeArea(
		UAbilitySystemComponent* InSourceASC,
		TSubclassOf<UGameplayEffect> InHitGameplayEffect,
		EEnemyAreaDamageType InDamageType,
		float InAreaRadius,
		float InAreaHalfHeight,
		float InWarningDuration,
		float InActiveDuration,
		float InDamageInterval,
		float InDamageCoefficient,
		TSubclassOf<ACPP_EnemyTelegraphActor> InTelegraphActorClass,
		FGameplayTag InImpactCueTag
	);

	float GetWarningServerStartTime() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Area")
	TObjectPtr<USceneComponent> SceneRoot;

	//! \brief 설치자(적)의 ASC. 피해 GameplayEffect Spec을 이 ASC로 생성해 Source가 설치자가 되게 한다.
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	TSubclassOf<UGameplayEffect> HitGameplayEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	EEnemyAreaDamageType DamageType = EEnemyAreaDamageType::Single;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float AreaRadius = 300.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float AreaHalfHeight = 150.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float WarningDuration = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float ActiveDuration = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float DamageInterval = 0.5f;

	//! \brief 스킬 피해 계수. 공격력 곱셈은 ExecutionCalculation이 캡처한 Source AttackPower로 수행한다.
	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Area")
	float DamageCoefficient = 0.0f;

private:
	void ActivateArea();
	void ApplyAreaDamage();
	void HandleActiveDurationFinished();
	void SpawnWarningTelegraph();
	void DestroyWarningTelegraph();
	void CollectPlayerTargets(TArray<AActor*>& OutTargets) const;
	bool ApplyDamageToTarget(AActor* TargetActor);
	void ExecuteImpactCue() const;

	FTimerHandle WarningTimerHandle;
	FTimerHandle DamageTimerHandle;
	FTimerHandle ActiveDurationTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<ACPP_EnemyTelegraphActor> ActiveTelegraphActor;

	TSubclassOf<ACPP_EnemyTelegraphActor> TelegraphActorClass;
	FGameplayTag ImpactCueTag;
	float WarningServerStartTime = 0.0f;
};
