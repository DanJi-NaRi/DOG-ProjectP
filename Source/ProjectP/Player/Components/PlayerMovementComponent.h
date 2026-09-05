// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerMovementComponent.generated.h"

class ACharacter;
class UCameraComponent;
class USpringArmComponent;
struct FInputActionValue;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTP_API UPlayerMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPlayerMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	void InitializeMovement(ACharacter* InOwnerCharacter, UCameraComponent* InFollowCamera, USpringArmComponent* InCameraBoom);
	bool HandleMove(const FInputActionValue& Value, float& OutMoveYaw);
	void HandleJump(const FInputActionValue& Value);
	bool GetLastMoveInputDirection(float MaxAgeSeconds, FVector& OutDirection) const;
	void SetFacingYawImmediate(float NewYaw);
	void RequestMovementFacingYaw(float NewYaw);
	void RequestSkillFacingYaw(float NewYaw, float InterpSpeed, float ToleranceDegrees);
	void CancelSkillFacingRequest();
	void CancelAllFacingRequests();
	void LockFacingYaw(float NewYaw);
	void UnlockFacingYaw();

private:
	//! \brief 이동·정지 상태를 서버에서 지켜본다.
	//! \details 이동은 사실이 아니라 상태다. 켜짐과 꺼짐이 짝을 이루고
	//!          그 사이의 시간을 스트리밍이 잰다. 전이 지점이 없어 서버가
	//!          주기로 속도를 본다. 문턱을 둘로 나눠 경계에서 깜빡이지 않게 한다.
	void StartStreamingMoveWatch();
	void UpdateStreamingMoveState();
	void BroadcastStreamingMoveState(bool bNowMoving);

	//! \brief 점프 사실을 서버에 보고한다. 사실 확정과 발행은 서버가 한다.
	UFUNCTION(Server, Reliable)
	void ServerReportJump(bool bWasMoving);

	//! \brief 점프 순간 이동 입력이 있었는지 판정한다.
	bool WasMovingOnJump() const;

	bool IsMovementInputBlocked() const;
	bool IsSkillInputBlocked() const;
	bool CanApplyFacing() const;
	bool CanMovementOverrideSkillFacing() const;
	void UpdateFacing(float DeltaTime);
	bool ApplyFacingYaw(float TargetYaw, float InterpSpeed, float ToleranceDegrees, bool bClearOnReached, float DeltaTime);
	void ApplyFacingYawToController(float NewYaw) const;
	float GetOwnerWorldTimeSeconds() const;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	//! 이동·정지 상태를 지켜보는 타이머다. 서버에서만 돈다.
	FTimerHandle StreamingMoveWatchHandle;
	//! 지금 이동 상태로 보고 있는지다.
	bool bStreamingMoving = false;
	//! 한 번이라도 상태를 알린 적이 있는지다. 첫 판정은 반드시 알린다.
	bool bStreamingMoveStateBroadcast = false;

	UPROPERTY()
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MovementFacingInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SkillFacingMovementOverrideDelay = 0.12f;

	FVector LastMoveInputDirection = FVector::ZeroVector;
	float LastMoveInputTime = -1000.0f;
	bool bHasLastMoveInputDirection = false;

	float MovementFacingYaw = 0.0f;
	bool bHasMovementFacingRequest = false;

	float SkillFacingYaw = 0.0f;
	float SkillFacingInterpSpeed = 18.0f;
	float SkillFacingToleranceDegrees = 1.0f;
	float SkillFacingRequestTime = -1000.0f;
	bool bHasSkillFacingRequest = false;

	float LockedFacingYaw = 0.0f;
	bool bFacingYawLocked = false;
};
