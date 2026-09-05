// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "../Types/EPlayerControlMode.h"
#include "MyBasicControlComponent.generated.h"

class ACharacter;
class UInputAction;
class UInputComponent;
class UPlayerCameraComponent;
class UPlayerMovementComponent;
class USpringArmComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTP_API UMyBasicControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMyBasicControlComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitializeBasicControl(ACharacter* InOwnerCharacter, UPlayerMovementComponent* InPlayerMovementComponent, UPlayerCameraComponent* InPlayerCameraComponent, USpringArmComponent* InCameraBoom);
	void BindBasicInput(UInputComponent* PlayerInputComponent);

	//! \brief 조작이 있었다는 사실을 서버에 알린다. 잠수 판정이 이걸 쓴다.
	//! \details 이동·마우스는 프레임마다 들어오므로 시간으로 눌러서 보낸다.
	void ReportPlayerInput();

	void Move(const FInputActionValue& Value);
	void HandleJumpInput(const FInputActionValue& Value);

	UFUNCTION(Server, Unreliable)
	void ServerReportPlayerInput();

	//! 마지막으로 서버에 조작을 알린 시각이다. 아직 없으면 음수다.
	double LastPlayerInputReportSeconds = -1.0;
	void Zoom(const FInputActionValue& Value);
	void StartOrbit(const FInputActionValue& Value);
	void EndOrbit(const FInputActionValue& Value);
	void OrbitLook(const FInputActionValue& Value);
	void ToggleCameraLock(const FInputActionValue& Value);
	void HandleInteractionInput(const FInputActionValue& Value);
	void HandleOwnerLifeStateChanged(bool bDead);

	void SetPlayerControlMode(EPlayerControlMode NewMode) { ControlMode = NewMode; }
	EPlayerControlMode GetPlayerControlMode() const { return ControlMode; }
	bool IsOrbitMode() const;
	bool IsMouseAimMode() const;

	///////////////////////// Character Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;

	///////////////////////// Camera Control Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Zoom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_RMB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_MouseLook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_CamLock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interaction;

private:
	bool CacheOwnerReferences();
	bool IsOwnerDead() const;
	void EnterOrbit(float InPreOrbitFacingYaw, float InOrbitFacingCameraOffsetYaw);
	void ExitOrbit();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerMovementComponent> PlayerMovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCameraComponent> PlayerCameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	EPlayerControlMode ControlMode = EPlayerControlMode::MouseAim;

	UPROPERTY()
	float PreOrbitFacingYaw = 0.0f;

	UPROPERTY()
	float OrbitFacingCameraOffsetYaw = 0.0f;

	float SavedMouseX = 0.0f;
	float SavedMouseY = 0.0f;
	bool bHasSavedMousePos = false;
};
