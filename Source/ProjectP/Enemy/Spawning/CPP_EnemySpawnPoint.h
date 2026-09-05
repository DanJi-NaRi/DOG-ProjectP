// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_EnemySpawnPoint.generated.h"

class UArrowComponent;
class UBoxComponent;

////////////////////////////
//! \class ACPP_EnemySpawnPoint
//! \brief 레벨에 배치하는 스폰 위치 마커. SpawnPointTag로 그룹화되어 웨이브가 위치를 선택한다.
UCLASS()
class PROJECTP_API ACPP_EnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ACPP_EnemySpawnPoint();

	// 이 스폰 지점의 그룹 태그. WaveData의 SpawnPointTag와 매칭된다.
	FName GetSpawnPointTag() const { return SpawnPointTag; }

	// 이 지점을 중심으로 적이 스폰될 수 있는 XY 하프익스텐트(cm). (X, Y) 각각 절반 크기. 0이면 정확히 이 위치.
	FVector2D GetSpawnExtent() const { return SpawnExtent; }

protected:
	// 웨이브가 위치를 선택할 때 사용하는 그룹 태그. (예: "North", "Center")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FName SpawnPointTag;

	// 스폰 산포 사각형의 XY 하프익스텐트(cm). 이 사각형(스폰 지점 회전 반영) 안 랜덤 위치에 스폰되며, 네비메시 위로 보정된다. (0,0)이면 정확히 이 지점.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0.0", UIMin = "0.0"))
	FVector2D SpawnExtent = FVector2D::ZeroVector;

#if WITH_EDITORONLY_DATA
	// 에디터에서 위치/방향을 시각화하기 위한 화살표.
	UPROPERTY(VisibleAnywhere, Category = "Spawn")
	TObjectPtr<UArrowComponent> ArrowComponent;

	// 에디터에서 SpawnExtent를 사각형 와이어프레임으로 시각화. (게임 중 숨김/무충돌)
	UPROPERTY(VisibleAnywhere, Category = "Spawn")
	TObjectPtr<UBoxComponent> ExtentVisual;

	// SpawnExtent 값을 ExtentVisual 크기에 반영한다.
	void SyncExtentVisual();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
