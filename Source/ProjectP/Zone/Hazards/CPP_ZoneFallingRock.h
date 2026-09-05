#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_ZoneFallingRock.generated.h"

class ACPP_BossTelegraphActor;
class UAbilitySystemComponent;
class UGameplayEffect;
class USceneComponent;
class USphereComponent;

////////////////////////////
//! \class ACPP_ZoneFallingRock
//! \brief Zone 생존 기믹에서 경고 텔레그래프를 표시한 뒤 범위 안의 플레이어에게 고정 피해를 한 번 적용한다.
//!        서버가 수명과 피해 판정을 담당하고, 액터와 텔레그래프는 클라이언트에 복제된다.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_ZoneFallingRock : public AActor
{
	GENERATED_BODY()

public:
	ACPP_ZoneFallingRock();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock")
	TObjectPtr<USphereComponent> DamageArea;

private:
	void SpawnWarningTelegraph();
	void ClearWarningTelegraph();
	void HandleWarningFinished();
	void ApplyFixedDamageToOverlappingPlayers();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float DamageRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float WarningDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FixedDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ACPP_BossTelegraphActor> TelegraphActorClass;

	UPROPERTY(VisibleAnywhere, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<ACPP_BossTelegraphActor> ActiveTelegraphActor;

	FTimerHandle WarningTimerHandle;
	bool bWarningResolved = false;
};
