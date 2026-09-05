#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BossRockWarningActor.generated.h"

class ACPP_BossTelegraphActor;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;
class USphereComponent;

UCLASS()
class PROJECTP_API ACPP_BossRockWarningActor : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BossRockWarningActor();

	void Initialize(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageGameplayEffect);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning")
	TObjectPtr<USphereComponent> WarningArea;

private:
	void SpawnWarningTelegraph();
	void ClearWarningTelegraph();
	void HandleWarningFinished();
	void ApplyDamageToOverlappingPawns();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WarningRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|RockWarning", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ACPP_BossTelegraphActor> TelegraphActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WarningDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DamageCoefficient = 1.5f;

	//! \brief 바위 피해 판정 1회에 함께 적용할 저주 게이지 수치.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|RockWarning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float CurseGaugeAmount = 0.0f;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	UPROPERTY(Transient)
	TObjectPtr<ACPP_BossTelegraphActor> ActiveTelegraphActor;

	FTimerHandle WarningTimerHandle;
};
