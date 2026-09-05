#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPP_ZoneFallingRockComponent.generated.h"

class ACPP_ZoneFallingRock;
class AZoneBase;

////////////////////////////
//! \class UCPP_ZoneFallingRockComponent
//! \brief Zone 생존 기믹 동안 생존 플레이어 주변과 Zone 전체에 낙석을 주기적으로 생성하고 정리한다.
UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Zone Falling Rock Component"))
class PROJECTP_API UCPP_ZoneFallingRockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPP_ZoneFallingRockComponent();

	UFUNCTION(BlueprintCallable, Category = "Zone|FallingRock")
	void StartFallingRocks();

	UFUNCTION(BlueprintCallable, Category = "Zone|FallingRock")
	void StopFallingRocks();

	UFUNCTION(BlueprintPure, Category = "Zone|FallingRock")
	bool IsFallingRockActive() const { return bFallingRockActive; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasZoneAuthority() const;
	void SpawnRockBatch();
	void GatherLivingPlayers(TArray<AActor*>& OutLivingPlayers) const;
	bool TryFindPlayerRockLocation(const AActor* PlayerActor, FVector& OutLocation) const;
	bool TryFindGlobalRockLocation(FVector& OutLocation) const;
	bool TryProjectCandidateToZone(const FVector& Candidate, FVector& OutLocation) const;
	ACPP_ZoneFallingRock* SpawnRockAtLocation(const FVector& SpawnLocation);
	void DestroyActiveRocks();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ACPP_ZoneFallingRock> FallingRockClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 RockCountPerPlayer = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 GlobalRockCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float SpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float PlayerSpawnRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxSpawnAttempts = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|FallingRock|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	FVector NavigationProjectionExtent = FVector(100.0f, 100.0f, 300.0f);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACPP_ZoneFallingRock>> ActiveRocks;

	FTimerHandle SpawnTimerHandle;
	bool bFallingRockActive = false;
};
