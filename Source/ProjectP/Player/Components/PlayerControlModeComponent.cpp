// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerControlModeComponent.h"

// Sets default values for this component's properties
UPlayerControlModeComponent::UPlayerControlModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UPlayerControlModeComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UPlayerControlModeComponent::IsOrbitMode() const
{

	return ControlMode == EPlayerControlMode::Orbit;
}

bool UPlayerControlModeComponent::IsMouseAimMode() const
{
	return ControlMode == EPlayerControlMode::MouseAim;
}

////////////////////////////
// Author : HanUl
// About : Orbit 모드 진입 시 조준 기준 각도를 저장하고 모드를 전환한다.
// Parameters : InPreOrbitFacingYaw - Orbit 진입 직전 캐릭터 방향, InOrbitFacingCameraOffsetYaw - 카메라와 캐릭터 상대 Yaw
// return : 없음
void UPlayerControlModeComponent::EnterOrbit(float InPreOrbitFacingYaw, float InOrbitFacingCameraOffsetYaw)
{
	PreOrbitFacingYaw = InPreOrbitFacingYaw;
	OrbitFacingCameraOffsetYaw = InOrbitFacingCameraOffsetYaw;
	ControlMode = EPlayerControlMode::Orbit;
}

////////////////////////////
// Author : HanUl
// About : Orbit 모드를 종료하고 MouseAim 모드로 복귀한다.
// Parameters : 없음
// return : 없음
void UPlayerControlModeComponent::ExitOrbit()
{
	ControlMode = EPlayerControlMode::MouseAim;
}


