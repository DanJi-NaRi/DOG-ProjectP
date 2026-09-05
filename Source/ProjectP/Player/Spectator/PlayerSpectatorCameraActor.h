#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerSpectatorCameraActor.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
struct FMinimalViewInfo;

////////////////////////////
//! \class APlayerSpectatorCameraActor
//! \brief 사망한 로컬 플레이어의 고정 시점과 생존 팀원 추적 시점을 담당하는 비복제 카메라 Actor이다.
UCLASS(NotBlueprintable, Transient)
class PROJECTP_API APlayerSpectatorCameraActor : public AActor
{
	GENERATED_BODY()

public:
	APlayerSpectatorCameraActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	void InitializeFromView(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float ArmLength,
		float FieldOfView);
	void SetFollowTarget(AActor* NewFollowTarget);
	void ClearFollowTarget();
	void SetSpectatorActive(bool bActive);
	void AdjustZoom(
		float ZoomInput,
		float ArmLengthStep,
		float MinArmLength,
		float MaxArmLength,
		float FieldOfViewStep,
		float MinFieldOfView,
		float MaxFieldOfView);
	void AddOrbitInput(
		float MouseDeltaX,
		float MouseDeltaY,
		float YawSpeed,
		float PitchSpeed,
		float MinPitch,
		float MaxPitch);
	void ConfigureFollow(const FVector& InTargetOffset, float InFollowInterpSpeed);

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> SpectatorRoot;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> SpectatorCamera;

	TWeakObjectPtr<AActor> FollowTarget;
	FVector TargetOffset = FVector(0.0f, 0.0f, 100.0f);
	float FollowInterpSpeed = 5.0f;
	float TargetArmLength = 2000.0f;
	float TargetFieldOfView = 70.0f;
	float ZoomInterpSpeed = 5.0f;
};
