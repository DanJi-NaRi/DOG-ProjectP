// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Types/ZoneDataTypes.h"
#include "GameFramework/Actor.h"
#include "ZoneBase.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UClearComponent;
class APlayerState;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnZoneClearedSignature, AZoneBase* /*ClearedZone*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnZonePlayerEnteredSignature, AZoneBase* /*Zone*/, APlayerState* /*EnteredPlayer*/);

UCLASS(Blueprintable)
class PROJECTP_API AZoneBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AZoneBase();
	//get/set
	EZoneState GetZoneState() const { return ZoneState; }
	EZoneType GetZoneType() const { return ZoneType; }
	const UBoxComponent* GetZoneBoundary() const { return ZoneBoundary; }

	// 월드 위치가 이 Zone의 경계(ZoneBoundary) 안에 있는지 판정한다. (치트의 현재 존 탐색 등)
	bool ContainsLocation(const FVector& WorldLocation) const;
	bool HasEncounterFailed() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Boundary", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> ZoneBoundary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Boundary", meta = (ClampMin = "0.0"))
	FVector BoundaryExtent = FVector(500.0f, 500.0f, 200.0f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Objects|Doors", meta = (DisplayName = "Entrance"))
	TObjectPtr<AActor> Entrance;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Objects|Doors", meta = (DisplayName = "Exit"))
	TObjectPtr<AActor> Exit;

	// 문들(출구, 입구)
	// 프리스포너들.
	// 스포너들.
	// 기타 오브젝트들.
	// 상점 등.

public:	
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ChangeState(EZoneState NewState);

	// 상태별 랩핑 함수 
	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ExecutePreparingState();

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ExecuteReadyState();

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ExecuteClearState();

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ExecuteActiveState();

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void ExecuteEnteringState();

	UFUNCTION(BlueprintCallable, Category = "Zone")
	void NotifyClearConditionSatisfied(UClearComponent* SatisfiedClearCondition);

#if !UE_BUILD_SHIPPING
	//! 비Shipping 개발 검증 전용. Clear 조건을 우회해 실제 Zone Clear 델리게이트 경로를 실행한다.
	bool CheatForceClear();
#endif

	// ZoneManager 구독용 (서버 전용)
	FOnZoneClearedSignature OnZoneCleared;
	FOnZonePlayerEnteredSignature OnZonePlayerEntered;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	EZoneType ZoneType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	EZoneState ZoneState;

	// 구조에 따라서 Array로 
	UPROPERTY()
	TObjectPtr<UClearComponent> ClearCondition;
	

private:
	UFUNCTION()
	void HandleZoneBoundaryBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor, // Player로 변경
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	// 문 등 수신 Actor에게는 Open/Close 두 가지 신호만 전달. 상태 해석은 Zone이 담당.
	void SendOpenSignal(AActor* TargetActor);
	void SendCloseSignal(AActor* TargetActor);

	void CollectClearConditions();

	UFUNCTION()
	void HandleClearConditionSatisfied(UClearComponent* SatisfiedClearCondition);
	void FinalizeClearConditionSatisfied(UClearComponent* SatisfiedClearCondition);
	bool bClearFinalizationPending = false;

	// Door Control
	void OpenEntrance();
	void CloseEntrance();
	void OpenExit();
	void CloseExit();

	void ActivatePreSpawners(); // Preparing 상태 미리 생생기
	void ActivateSpawners();	// Active 상태 전투 중 생성기

};
