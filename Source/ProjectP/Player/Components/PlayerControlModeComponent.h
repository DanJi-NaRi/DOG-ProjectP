// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Types/EPlayerControlMode.h"
#include "PlayerControlModeComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTP_API UPlayerControlModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	// Sets default values for this component's properties
	UPlayerControlModeComponent();

protected:

	// Called when the game starts
	virtual void BeginPlay() override;

public:	

	void SetPlayerControlMode(EPlayerControlMode NewMode) { ControlMode = NewMode; }
	EPlayerControlMode GetPlayerControlMode() const { return ControlMode; }

	bool IsOrbitMode() const;
	bool IsMouseAimMode() const;

	void EnterOrbit(float InPreOrbitFacingYaw, float InOrbitFacingCameraOffsetYaw);
	void ExitOrbit();

	float GetPreOrbitFacingYaw() const { return PreOrbitFacingYaw; }
	float GetOrbitFacingCameraOffsetYaw() const { return OrbitFacingCameraOffsetYaw; }

private:

	UPROPERTY()
	EPlayerControlMode ControlMode = EPlayerControlMode::MouseAim;

	UPROPERTY()
	float PreOrbitFacingYaw = 0.0f;

	UPROPERTY()
	float OrbitFacingCameraOffsetYaw = 0.0f;
		
};
