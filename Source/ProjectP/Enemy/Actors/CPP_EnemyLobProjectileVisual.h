#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_EnemyLobProjectileVisual.generated.h"

class USceneComponent;

USTRUCT()
struct FEnemyLobProjectileVisualData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY()
	float PeakHeight = 0.0f;

	UPROPERTY()
	float Duration = 0.0f;

	UPROPERTY()
	float ServerStartTime = 0.0f;
};

////////////////////////////
//! \class ACPP_EnemyLobProjectileVisual
//! \brief 서버 시각을 기준으로 시작점과 도착점 사이의 포물선 경로를 재생하는 무충돌 연출 액터.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_EnemyLobProjectileVisual : public AActor
{
	GENERATED_BODY()

public:
	ACPP_EnemyLobProjectileVisual();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeLobVisual(
		const FVector& InStartLocation,
		const FVector& InEndLocation,
		float InPeakHeight,
		float InDuration,
		float InServerStartTime
	);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|LobProjectile")
	TObjectPtr<USceneComponent> SceneRoot;

private:
	UFUNCTION()
	void OnRep_VisualData();

	void ApplyVisualAtServerTime();
	float GetSynchronizedServerTime() const;

	UPROPERTY(ReplicatedUsing = OnRep_VisualData)
	FEnemyLobProjectileVisualData VisualData;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|LobProjectile", meta = (ClampMin = "0.0"))
	float NetworkDestroyGraceDuration = 0.5f;

	bool bHasVisualData = false;
};
