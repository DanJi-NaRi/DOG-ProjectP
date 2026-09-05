////////////////////////////
//! \file CPP_GimmickElementBase.h
//! \brief 기믹을 구성하는 요소(발판·반사판·저울판 등)의 기반 액터 선언 파일이다.
//! \editor 준혁 - 클리어 시 요소 연출을 고정하는 SolvedLock 공통 API 추가
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_GimmickElementBase.generated.h"

class ACPP_GimmickBase;

////////////////////////////
//! \class ACPP_GimmickElementBase
//! \brief "자기 상태만 아는" 기믹 요소의 베이스. 소속 기믹에 자동 등록되고, 상태가 바뀌면 소속 기믹에 통보한다.
//!        클리어 판정 자체는 몰라도 되며, 판정은 소속 기믹의 조건 블록이 GetElements()를 훑어 처리한다.
UCLASS(Abstract)
class PROJECTP_API ACPP_GimmickElementBase : public AActor
{
	GENERATED_BODY()

public:
	ACPP_GimmickElementBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//! 이 요소가 충족 상태인가? (예: 발판 점유중) 서브클래스가 구현한다.
	virtual bool IsSatisfied() const;

	//! 이 요소를 활성화시킨 액터(예: 발판 위 플레이어). 없으면 nullptr.
	virtual AActor* GetActivator() const;

	//! Zone/기믹 초기화 시 호출. 원위치 복귀 등 서브클래스별 리셋을 수행한다(서버).
	virtual void ResetElement();

    //! 기믹 생명주기에 맞춰 이 요소가 제공하는 상호작용만 활성화하거나 비활성화한다(서버).
    virtual void SetGimmickInteractionEnabled(bool bEnabled);

	//! 기믹 클리어 시 요소 연출을 클리어 시점 상태로 고정/해제한다(서버, 기믹 베이스가 호출).
	void SetSolvedLock(bool bLocked);

	UFUNCTION(BlueprintPure, Category = "Gimmick|Element")
	bool IsSolvedLocked() const { return bSolvedLocked; }

	ACPP_GimmickBase* GetOwnerGimmick() const { return OwnerGimmick; }

protected:
	virtual void BeginPlay() override;

	//! BeginPlay 시점의 기본 상태를 저장한다. 미래 요소는 추가 상태가 있으면 오버라이드하고 Super를 호출한다.
	virtual void CaptureInitialState();

	//! 고정/해제 시 서브클래스별 처리(서버). 연출을 구동하는 복제 변수의 동결·재동기화를 수행한다.
	virtual void OnSolvedLockChanged();

	//! 고정/해제 연출 훅(서버와 클라 양쪽에서 호출, BP 구현). 연출이 별도 복제 변수로 안 묶인 요소용.
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Element")
	void OnSolvedLockChangedFX(bool bLocked);

	UFUNCTION()
	void OnRep_SolvedLocked();

	//! 자기 상태 변화를 소속 기믹에 통보(서버). 서브클래스가 상태 변경 지점에서 호출한다.
	void MarkStateDirty();

	//! 이 요소가 속한 기믹. 레벨에서 인스턴스별로 지정한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Element")
	TObjectPtr<ACPP_GimmickBase> OwnerGimmick;

	//! 클리어 고정 여부(복제 -> 클라 고정 연출).
	UPROPERTY(ReplicatedUsing = OnRep_SolvedLocked, BlueprintReadOnly, Category = "Gimmick|Element")
	bool bSolvedLocked = false;

private:
	//! 공통 요소가 BeginPlay에서 기억한 최초 월드 Transform이다.
	FTransform InitialActorTransform;

	bool bInitialStateCaptured = false;
};
