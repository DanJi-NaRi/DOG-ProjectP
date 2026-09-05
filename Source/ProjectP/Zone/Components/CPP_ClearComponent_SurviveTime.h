// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClearComponent.h"
#include "CPP_ClearComponent_SurviveTime.generated.h"

////////////////////////////
//! \class UCPP_ClearComponent_SurviveTime
//! \brief 시간 버티기 클리어 조건 어댑터. Zone Active 시 타이머를 시작하고 Duration 경과 시 클리어를 통보한다.
//!        Survival 스폰 모드(무한 스폰)와 조합해 "제한 시간 동안 버티기" 방을 구성한다.
UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Clear Component (Survive Time)"))
class PROJECTP_API UCPP_ClearComponent_SurviveTime : public UClearComponent
{
	GENERATED_BODY()

public:
	//! Zone Active 시: 생존 타이머를 시작한다.
	virtual void ActivateClearCondition_Implementation() override;

	//! 감시 비활성화 시: 타이머를 정지한다.
	virtual void DeactivateClearCondition_Implementation() override;

	//! Zone 재시작 등 리셋 시: 타이머를 정지하고 상태를 초기화한다.
	virtual void ResetClearCondition_Implementation() override;

	//! 남은 생존 시간(초). 비활성 상태면 0. (HUD 등 연출용, 서버 값)
	UFUNCTION(BlueprintPure, Category = "Zone|Clear")
	float GetRemainingTime() const;

protected:
	// 버텨야 하는 시간(초). 경과 시 Zone 클리어.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Clear", meta = (ClampMin = "0.0"))
	float Duration = 60.0f;

private:
	bool HasZoneAuthority() const;

	void HandleSurviveTimeExpired();

	FTimerHandle SurviveTimerHandle;
};
