// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PlayerCameraComponent.generated.h"

class ACameraActor;
class UCameraShakeBase;
class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class APlayerController;
struct FInputActionValue;

////////////////////////////
//! \struct FInteractionViewParams
//! \author HanUl
//! \brief 상호작용 연출 카메라의 배치와 전환을 정의하는 파라미터다.
//!        카메라는 상호작용 대상의 로컬 축을 기준으로 배치되며 대상만 바라본다.
//!        상호작용 대상(상점 NPC 등)이 자기 연출값을 들고 있다가 EnterInteractionView에 전달한다.
USTRUCT(BlueprintType)
struct FInteractionViewParams
{
	GENERATED_BODY()

	//! \brief 대상의 정면 방향으로 떨어지는 카메라 거리(cm)이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView", meta = (ClampMin = "0.0"))
	float FrontDistance = 250.0f;

	//! \brief 카메라와 시선축의 대상 기준 좌우 평행 오프셋(cm)이다.
	//!        양수면 대상이 화면 왼쪽에, 음수면 화면 오른쪽에 배치된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView")
	float SideOffset = 0.0f;

	//! \brief 대상의 루트 위치를 기준으로 한 카메라 높이(cm)이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView")
	float Height = 160.0f;

	//! \brief 시선점 높이 보정(cm)이다. 액터 루트 대신 상체/얼굴 높이를 바라보게 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView")
	float LookAtHeightOffset = 140.0f;

	//! \brief 상호작용 뷰의 FOV이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView", meta = (ClampMin = "20.0", ClampMax = "120.0"))
	float Fov = 55.0f;

	//! \brief 진입 블렌드 시간(초)이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|InteractionView", meta = (ClampMin = "0.0"))
	float BlendTime = 0.5f;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTP_API UPlayerCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerCameraComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Component Tick 사용
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void InitializeCameraBoom(USpringArmComponent* InCameraBoom);
	void InitializeFollowCamera(UCameraComponent* InFollowCamera);
	void HandleZoom(const FInputActionValue& Value);
	void HandleOrbitLook(const FInputActionValue& Value, bool bIsOrbiting);
	bool ToggleOrbitRotationLock();
	bool IsOrbitRotationLocked() const { return bOrbitRotationLocked; }
	void HandleOrbitStartCursor(APlayerController* PC, float& OutSavedMouseX, float& OutSavedMouseY, bool& bOutHasSavedMousePos);
	void HandleOrbitEndCursor(APlayerController* PC, float SavedMouseX, float SavedMouseY, bool bHasSavedMousePos);
	void HandleOwnerLifeStateChanged(bool bDead);

	//! \brief 공격자 본인에게 전달된 적중 피드백 태그에 대응하는 카메라 셰이크를 로컬에서 재생한다.
	UFUNCTION(BlueprintCallable, Category = "Camera|CombatFeedback")
	void PlayAttackerHitCameraFeedback(FGameplayTag CameraFeedbackTag);

	//! \brief 대상의 정면에 카메라를 배치하고 대상만 보는 상호작용 뷰로 전환한다. 로컬 전용.
	UFUNCTION(BlueprintCallable, Category = "Camera|InteractionView")
	void EnterInteractionView(AActor* FocusTarget, const FInteractionViewParams& Params);

	//! \brief 상호작용 뷰를 종료하고 원래 게임플레이 카메라로 복귀한다. 로컬 전용.
	UFUNCTION(BlueprintCallable, Category = "Camera|InteractionView")
	void ExitInteractionView(float BlendTime = 0.5f);

	UFUNCTION(BlueprintPure, Category = "Camera|InteractionView")
	bool IsInteractionViewActive() const { return bInteractionViewActive; }

	float GetOrbitYawSpeed() const { return OrbitYawSpeed; }
	float GetOrbitPitchSpeed() const { return OrbitPitchSpeed; }
	float GetMinPitch() const { return MinPitch; }
	float GetMaxPitch() const { return MaxPitch; }

private:
	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY()
	TObjectPtr<UCameraComponent>    FollowCamera;

	TWeakObjectPtr<USceneComponent> GameplayCameraBoomParent;
	FName GameplayCameraBoomSocketName = NAME_None;
	FVector GameplayCameraBoomRelativeLocation = FVector::ZeroVector;
	FVector GameplayCameraBoomRelativeScale = FVector::OneVector;
	bool bCameraDetachedForDeath = false;
 
	// Length
	float TargetArmLength;
	float MinArmLength = 600.0f;
	float MaxArmLength = 2400.0f;

	float ZoomStep = -100.0f;
	float LerpSpeed = 5.0f;

	//Angle
	float OrbitYawSpeed = 2.0f;
	float OrbitPitchSpeed = 2.0f;
	bool bOrbitRotationLocked = false;
	//Uproperty
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", 
		meta = (AllowPrivateAccess = "true", ClampMin = "-90.0", ClampMax = "-60.0", UIMin = "-90.0", UIMax = "-60.0"))
	float MinPitch = -65.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", 
		meta = (AllowPrivateAccess = "true", ClampMin = "-30.0", ClampMax = "-5.0", UIMin = "-30.0", UIMax = "-5.0"))
	float MaxPitch = -10.0f;

	// FOV
	float TargetFov;
	
	// Min - Max 지정 후 자동 보간.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", 
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "45.0"))
	float MinFov = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera",
		meta = (AllowPrivateAccess = "true", ClampMin = "-30.0", ClampMax = "-5.0", UIMin = "45.0", UIMax = "90.0"))
	float MaxFov = 60.0f;

	// Combat Feedback
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCameraShakeBase> BasicAttackHitShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCameraShakeBase> SkillHitShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UCameraShakeBase> UltimateHitShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BasicAttackHitShakeScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SkillHitShakeScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float UltimateHitShakeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BasicAttackHitMinInterval = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SkillHitMinInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|CombatFeedback",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float UltimateHitMinInterval = 0.2f;

	bool ResolveAttackerHitCameraFeedback(
		FGameplayTag CameraFeedbackTag,
		TSubclassOf<UCameraShakeBase>& OutShakeClass,
		float& OutScale,
		float& OutMinInterval
	) const;

	TMap<FGameplayTag, double> LastCameraFeedbackTimeByTag;

	// Interaction View
	APlayerController* GetOwningPlayerController() const;

	//! \brief 상호작용 뷰에 재사용하는 로컬 전용 카메라 액터이다.
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> InteractionViewCamera;

	//! \brief 상호작용 뷰 진입 직전의 뷰 타겟이다. 종료 시 복귀 대상으로 사용한다.
	TWeakObjectPtr<AActor> PreviousViewTarget;

	bool bInteractionViewActive = false;
};
 
