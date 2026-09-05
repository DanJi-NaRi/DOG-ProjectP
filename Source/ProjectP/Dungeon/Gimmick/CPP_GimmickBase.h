////////////////////////////
//! \file CPP_GimmickBase.h
//! \brief 기믹 상태머신 베이스 액터 선언 파일이다. 클리어 판정·보상·되돌림을 담당하며, 라이프사이클(활성/초기화)은 Zone이 구동한다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_GimmickTypes.h"
#include "CPP_GimmickBase.generated.h"

class UCPP_GimmickCondition;
class UCPP_GimmickReward;
class ACPP_GimmickElementBase;
class USceneComponent;

////////////////////////////
//! \class ACPP_GimmickBase
//! \brief 조립형 조건/보상 블록과 요소 액터를 집계해 클리어를 판정하는 기믹 상태머신.
//!        Zone은 Activate()/Deactivate()/ResetGimmick()을 호출하고 OnGimmickStateChanged를 구독한다(Zone 연동 seam).
UCLASS(Abstract, Blueprintable)
class PROJECTP_API ACPP_GimmickBase : public AActor
{
	GENERATED_BODY()

public:
	ACPP_GimmickBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== Zone-facing API (서버 권위) =====
	//! 기믹을 활성화한다(Inactive -> Active). Zone 진입 시 호출.
	UFUNCTION(BlueprintCallable, Category = "Gimmick")
	void Activate();

	//! 기믹을 비활성화한다(-> Inactive). 클리어 상태였다면 보상을 되돌린다.
	UFUNCTION(BlueprintCallable, Category = "Gimmick")
	void Deactivate();

	//! 요소를 원위치시키고 재도전 가능 상태로 되돌린다. Zone 진행/재시작 시 호출.
	UFUNCTION(BlueprintCallable, Category = "Gimmick")
	void ResetGimmick();

    //! Zone Clear처럼 기믹의 Solved 상태와 보상은 유지하면서 요소 상호작용만 일괄 전환한다.
    void SetElementInteractionsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	bool IsSolved() const { return CurrentState == EGimmickState::Solved; }

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	EGimmickState GetGimmickState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	float GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	FName GetGimmickId() const { return GimmickId; }

	// ===== Element -> Base (서버) =====
	//! 요소가 BeginPlay에서 자기를 등록한다.
	void RegisterElement(ACPP_GimmickElementBase* Element);

	//! 요소 상태가 변하면 호출되어 재평가를 트리거한다(틱 아님, 이벤트 구동).
	void NotifyElementChanged(ACPP_GimmickElementBase* Element);

	const TArray<TObjectPtr<ACPP_GimmickElementBase>>& GetElements() const { return Elements; }

	//! Zone/방 매니저가 구독하는 상태 변경 델리게이트.
	UPROPERTY(BlueprintAssignable, Category = "Gimmick")
	FGimmickStateChangedSignature OnGimmickStateChanged;

protected:
	virtual void BeginPlay() override;

	// ===== 연출 훅 (BP에서 구현) =====
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnDeactivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnProgressChanged(float InProgress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnSolved();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnRevert();

	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|FX")
	void OnReset();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gimmick")
	TObjectPtr<USceneComponent> SceneRoot;

	//! 식별용(Zone이 특정 기믹을 참조할 때 사용, 선택).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Setup")
	FName GimmickId;

	//! 클리어 판정 블록들(모두 충족해야 클리어). 에디터에서 조립.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Gimmick|Setup")
	TArray<TObjectPtr<UCPP_GimmickCondition>> ClearConditions;

	//! 클리어 시 실행할 결과 블록들. 에디터에서 조립.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Gimmick|Setup")
	TArray<TObjectPtr<UCPP_GimmickReward>> Rewards;

	//! true면 한 번 클리어 시 고정(완료형), false면 조건이 깨질 때 되돌림(지속형).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Setup")
	bool bLatchOnSolve = false;

	//! Zone 없이 단독(PIE) 검증용: 서버 BeginPlay에서 자동 Activate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Debug")
	bool bAutoActivateForStandaloneTest = false;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Gimmick")
	EGimmickState CurrentState = EGimmickState::Inactive;

	UPROPERTY(ReplicatedUsing = OnRep_Progress, BlueprintReadOnly, Category = "Gimmick")
	float Progress = 0.0f;

	//! 런타임에 등록된 요소들(복제하지 않음, 서버 판정용).
	UPROPERTY(Transient)
	TArray<TObjectPtr<ACPP_GimmickElementBase>> Elements;

private:
	void EvaluateOnServer();
	void SetElementsSolvedLock(bool bLocked);
	void SetState(EGimmickState NewState);
	void SetProgress(float NewProgress);
	void EnterSolved();
	void RevertToActive();
	void ExecuteRewards();
	void RevertRewards();
	bool AreClearConditionsMet() const;
	float ComputeProgress() const;
	void DispatchStateFX(EGimmickState OldState, EGimmickState NewState);

	UFUNCTION()
	void OnRep_State(EGimmickState OldState);

	UFUNCTION()
	void OnRep_Progress();

    bool bElementInteractionsEnabled = false;
	bool bResetInProgress = false;
};
