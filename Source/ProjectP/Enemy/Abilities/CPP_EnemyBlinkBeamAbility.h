// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Enemy/Abilities/CPP_EnemyBeamAttackAbility.h"
#include "CPP_EnemyBlinkBeamAbility.generated.h"

class UCPP_AbilityTask_EnemyBlink;

////////////////////////////
//! \class UCPP_EnemyBlinkBeamAbility
//! \brief 점멸 눈빔(MON_PUN_001_PAT_04): 타겟 후방으로 순간이동(공용 UCPP_AbilityTask_EnemyBlink) 후, 조준 시간이
//!        단축된 눈빔을 발사한다. 눈빔 시퀀스는 부모 UCPP_EnemyBeamAttackAbility를 그대로 상속하고, 순간이동만 앞에 붙인다.
//! \note  조준 단축은 부모의 AimDuration을 Class Defaults에서 낮추는 것으로 처리(코드 없음). 순간이동 수치는 슬라임과 동일.
UCLASS()
class PROJECTP_API UCPP_EnemyBlinkBeamAbility : public UCPP_EnemyBeamAttackAbility
{
	GENERATED_BODY()

protected:
	//! \brief 발동 준비 후 훅 오버라이드: 순간이동을 먼저 수행하고, 재등장 시 부모의 빔 시퀀스를 시작한다.
	virtual void OnBeamActivated() override;

	//! \brief 소멸 유지 시간(초). 슬라임과 동일 기본값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkBeam", meta = (ClampMin = "0.0"))
	float VanishDuration = 0.8f;

	//! \brief 재등장 시 타겟 후방으로 벌릴 캡슐 표면 간 간격(cm).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkBeam", meta = (ClampMin = "0.0"))
	float ReappearBehindGap = 50.0f;

	//! \brief 재등장 직후 무적 시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkBeam", meta = (ClampMin = "0.0"))
	float InvincibleDuration = 0.15f;

	//! \brief 소멸/재등장 연출용 GameplayCue 태그(선택).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkBeam|Cue")
	FGameplayTag VanishCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|BlinkBeam|Cue")
	FGameplayTag ReappearCueTag;

private:
	UFUNCTION()
	void HandleBlinkFinished();

	UPROPERTY(Transient)
	TObjectPtr<UCPP_AbilityTask_EnemyBlink> ActiveBlinkTask;
};
