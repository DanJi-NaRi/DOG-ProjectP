////////////////////////////
//! \file CPP_PressurePlateElement.h
//! \brief 플레이어 점유를 감지하는 압력 발판 요소 액터 선언 파일이다.
//! \editor 준혁 - 점유 복제를 폰 포인터에서 bool로 변경(릴레번시 대응), 리셋/시작 시 오버랩 재스캔 추가
//! \editor 준혁 - 클리어 고정(SolvedLock) 시 점유 연출 동결 구현
#pragma once

#include "CoreMinimal.h"
#include "../CPP_GimmickElementBase.h"
#include "CPP_PressurePlateElement.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

////////////////////////////
//! \class ACPP_PressurePlateElement
//! \brief 트리거 구체 오버랩으로 플레이어 점유를 감지하고, 점유 변화를 소속 기믹에 통보한다.
UCLASS()
class PROJECTP_API ACPP_PressurePlateElement : public ACPP_GimmickElementBase
{
	GENERATED_BODY()

public:
	ACPP_PressurePlateElement();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsSatisfied() const override { return Occupant != nullptr; }
	virtual AActor* GetActivator() const override;
	virtual void ResetElement() override;

protected:
	virtual void BeginPlay() override;

	//! 고정 시 점유 연출(bIsOccupied)을 켠 채 동결, 해제 시 실제 오버랩 상태로 재동기화한다.
	virtual void OnSolvedLockChanged() override;

	UFUNCTION()
	void OnPlateBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnPlateEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_Occupied();

	//! 발판 점등/소등 연출(BP 구현).
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Plate")
	void OnOccupantChanged(bool bOccupied);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Plate")
	TObjectPtr<USphereComponent> TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick|Plate")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	//! 발판을 밟고 있는 대표 플레이어 폰(서버 전용, 조건 판정의 Activator). 클라에서는 항상 null이다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Gimmick|Plate")
	TObjectPtr<APawn> Occupant;

	//! 점유 여부(복제 -> 클라 점등). 폰 포인터 대신 bool을 복제해 점유 폰의 넷 릴레번시에 영향받지 않는다.
	UPROPERTY(ReplicatedUsing = OnRep_Occupied, BlueprintReadOnly, Category = "Gimmick|Plate")
	bool bIsOccupied = false;

private:
	void RefreshOccupant();

	//! 트리거에 실제로 겹쳐 있는 폰들로 목록을 재구성한다(서버). 리셋·시작 시점에 이미 밟고 있는 폰을 놓치지 않기 위함.
	void RescanOverlappingPawns();

	//! 서버에서만 유지하는, 현재 겹친 플레이어 폰 목록.
	UPROPERTY(Transient)
	TArray<TObjectPtr<APawn>> OverlappingPawns;
};
