// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "MyGameplayTags.h"
#include "Player/PlayerCharacterBase.h"
#include "Streaming/MyStreamingPayloads.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "../../GAS/MyAbilitySystemLibrary.h"
#include "../../MyGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "../PlayerCharacterBase.h"
#include "../../Streaming/MyStreamingCombatMessageLibrary.h"

UPlayerMovementComponent::UPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

////////////////////////////
//! \author HanUl
//! \brief 이동 컴포넌트 틱에서 누적된 회전 요청을 단일 정책으로 처리한다.
//! \param DeltaTime 이전 프레임 이후 경과 시간
//! \param TickType 현재 틱 종류
//! \param ThisTickFunction 현재 틱 함수 정보
//! \return 없음
void UPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFacing(DeltaTime);
}

void UPlayerMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if(!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(GetOwner());
	}

	if (OwnerCharacter)
	{
		SetFacingYawImmediate(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw));
	}

	StartStreamingMoveWatch();
}

////////////////////////////
// Author : HanUl
// About : 이동 처리에 필요한 소유 캐릭터와 카메라 참조를 연결한다.
// Parameters : InOwnerCharacter - 이동을 적용할 캐릭터, InFollowCamera - 카메라 컴포넌트, InCameraBoom - 스프링암 컴포넌트
// return : 없음
void UPlayerMovementComponent::InitializeMovement(ACharacter* InOwnerCharacter, UCameraComponent* InFollowCamera, USpringArmComponent* InCameraBoom)
{
	OwnerCharacter = InOwnerCharacter;
	FollowCamera = InFollowCamera;
	CameraBoom = InCameraBoom;
}

////////////////////////////
// Author : HanUl
// About : 카메라 기준으로 이동 입력을 처리하고 현재 이동 방향 Yaw를 계산한다.
// Parameters : Value - Enhanced Input 이동 입력, OutMoveYaw - 계산된 이동 방향 Yaw
// return : 유효한 이동 방향이 계산되었는지 여부
bool UPlayerMovementComponent::HandleMove(const FInputActionValue& Value, float& OutMoveYaw)
{
	if (!OwnerCharacter || IsMovementInputBlocked())
	{
		return false;
	}

	const FVector2D MoveInput = Value.Get<FVector2D>();
	AController* Controller = OwnerCharacter->GetController();

	if (!Controller || MoveInput.IsNearlyZero())
	{
		bHasLastMoveInputDirection = false;
		return false;
	}

	FRotator YawRotation = FRotator::ZeroRotator;
	if (FollowCamera)
	{
		const FRotator CameraRotation = FollowCamera->GetComponentRotation();
		YawRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
	}
	else if (CameraBoom)
	{
		const FRotator CameraRotation = CameraBoom->GetComponentRotation();
		YawRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
	}
	else
	{
		const FRotator ControlRotation = Controller->GetControlRotation();
		YawRotation = FRotator(0.0f, ControlRotation.Yaw, 0.0f);
	}

	const FVector ForwardVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightVector = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	const FVector MoveDirection = (ForwardVector * MoveInput.Y) + (RightVector * MoveInput.X);
	if (MoveDirection.IsNearlyZero())
	{
		bHasLastMoveInputDirection = false;
		return false;
	}

	LastMoveInputDirection = MoveDirection.GetSafeNormal2D();
	LastMoveInputTime = OwnerCharacter->GetWorld() ? OwnerCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
	bHasLastMoveInputDirection = true;
	OutMoveYaw = LastMoveInputDirection.Rotation().Yaw;

	RequestMovementFacingYaw(OutMoveYaw);

	OwnerCharacter->AddMovementInput(ForwardVector, MoveInput.Y);
	OwnerCharacter->AddMovementInput(RightVector, MoveInput.X);
	return true;
}


////////////////////////////
//! \author HanUl
//! \brief 지정한 시간 안에 입력된 최근 이동 방향을 반환한다.
//! \param MaxAgeSeconds 유효하게 볼 최대 입력 경과 시간
//! \param OutDirection 최근 이동 입력 방향
//! \return 유효한 최근 이동 입력 방향이 있으면 true
bool UPlayerMovementComponent::GetLastMoveInputDirection(float MaxAgeSeconds, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;
	if (!OwnerCharacter || !bHasLastMoveInputDirection)
	{
		return false;
	}

	const UWorld* World = OwnerCharacter->GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : LastMoveInputTime;
	if (CurrentTime - LastMoveInputTime > MaxAgeSeconds)
	{
		return false;
	}

	OutDirection = LastMoveInputDirection;
	return !OutDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief 소유 캐릭터의 방향을 즉시 지정한 Yaw로 맞추고 대기 중인 회전 요청을 정리한다.
//! \param NewYaw 적용할 월드 기준 Yaw
//! \return 없음
void UPlayerMovementComponent::SetFacingYawImmediate(float NewYaw)
{
	if (!CanApplyFacing() || bFacingYawLocked)
	{
		return;
	}

	bHasMovementFacingRequest = false;
	bHasSkillFacingRequest = false;
	ApplyFacingYawToController(NewYaw);
}

////////////////////////////
//! \author HanUl
//! \brief 이동 또는 속도 기반 회전 요청을 등록한다.
//! \param NewYaw 이동 방향 월드 기준 Yaw
//! \return 없음
void UPlayerMovementComponent::RequestMovementFacingYaw(float NewYaw)
{
	if (!CanApplyFacing() || bFacingYawLocked)
	{
		return;
	}

	if (bHasSkillFacingRequest && !CanMovementOverrideSkillFacing())
	{
		return;
	}

	CancelSkillFacingRequest();
	MovementFacingYaw = FRotator::NormalizeAxis(NewYaw);
	bHasMovementFacingRequest = true;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 조준 기반 회전 요청을 등록한다.
//! \param NewYaw 스킬 조준 방향 월드 기준 Yaw
//! \param InterpSpeed 회전 보간 속도
//! \param ToleranceDegrees 목표 도달 허용 각도
//! \return 없음
void UPlayerMovementComponent::RequestSkillFacingYaw(float NewYaw, float InterpSpeed, float ToleranceDegrees)
{
	if (!CanApplyFacing() || bFacingYawLocked)
	{
		return;
	}

	SkillFacingYaw = FRotator::NormalizeAxis(NewYaw);
	SkillFacingInterpSpeed = FMath::Max(0.0f, InterpSpeed);
	SkillFacingToleranceDegrees = FMath::Max(0.0f, ToleranceDegrees);
	SkillFacingRequestTime = GetOwnerWorldTimeSeconds();
	bHasSkillFacingRequest = true;
	bHasMovementFacingRequest = false;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 스킬 조준 회전 요청을 취소한다.
//! \param 없음
//! \return 없음
void UPlayerMovementComponent::CancelSkillFacingRequest()
{
	bHasSkillFacingRequest = false;
}

////////////////////////////
//! \author HanUl
//! \brief 이동·스킬에서 누적된 모든 Character 회전 요청을 취소한다.
//! \param 없음
//! \return 없음
void UPlayerMovementComponent::CancelAllFacingRequests()
{
	bHasMovementFacingRequest = false;
	bHasSkillFacingRequest = false;
}

////////////////////////////
//! \author HanUl
//! \brief 지정한 Yaw로 캐릭터 방향을 즉시 고정하고 이동·스킬 회전 요청이 덮어쓰지 못하게 한다.
//! \param NewYaw 고정할 월드 기준 Yaw
//! \return 없음
void UPlayerMovementComponent::LockFacingYaw(float NewYaw)
{
	if (!CanApplyFacing())
	{
		return;
	}

	LockedFacingYaw = FRotator::NormalizeAxis(NewYaw);
	bFacingYawLocked = true;
	bHasMovementFacingRequest = false;
	bHasSkillFacingRequest = false;
	ApplyFacingYawToController(LockedFacingYaw);

	if (OwnerCharacter->HasAuthority())
	{
		OwnerCharacter->ForceNetUpdate();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 고정된 캐릭터 방향을 해제해 이후 이동·스킬 입력의 회전 요청을 다시 허용한다.
//! \param 없음
//! \return 없음
void UPlayerMovementComponent::UnlockFacingYaw()
{
	bFacingYawLocked = false;
}



////////////////////////////
//! \author HanUl
//! \brief 점프 입력이 눌렸을 때 소유 캐릭터 점프를 수행한다.
//! \param Value Enhanced Input 점프 입력
//! \return 없음
void UPlayerMovementComponent::HandleJump(const FInputActionValue& Value)
{
	if (!OwnerCharacter || IsMovementInputBlocked())
	{
		return;
	}

	const bool bPressed = Value.Get<bool>();
	if (bPressed)
	{
		OwnerCharacter->Jump();
		// 점프는 어빌리티가 아니라 입력이라 스스로 사실을 남기지 않는다.
		// 스트리밍이 셀 수 있도록 서버에 한 번 보고한다.
		ServerReportJump(WasMovingOnJump());
	}
}

////////////////////////////
//! \author 장효제
//! \brief 점프 순간 이동 입력이 있었는지 판정한다.
//! \return 최근 이동 입력이 살아 있으면 true다.
bool UPlayerMovementComponent::WasMovingOnJump() const
{
	// 입력이 끊긴 직후의 점프까지 이동 중으로 보지 않도록 짧은 유효 시간만 인정한다.
	constexpr float MoveInputGraceSeconds = 0.2f;
	FVector Unused;
	return GetLastMoveInputDirection(MoveInputGraceSeconds, Unused);
}

////////////////////////////
//! \author 장효제
//! \brief 서버에서 점프 사실을 스킬 사용 사실로 발행한다.
//! \param bWasMoving 점프 순간 이동 입력이 있었는지다.
//! \return 없음
void UPlayerMovementComponent::ServerReportJump_Implementation(const bool bWasMoving)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 계층 태그라 Skill.Common.Jump 조건은 둘 다 세고, InPlace 조건은 제자리 점프만 센다.
	const FGameplayTag JumpTag = bWasMoving
		? MyGameplayTags::Skill_Common_Jump_Moving.GetTag()
		: MyGameplayTags::Skill_Common_Jump_InPlace.GetTag();
	UMyStreamingCombatMessageLibrary::BroadcastSkillUsed(this, Owner, JumpTag);
}

////////////////////////////
//! \author HanUl
//! \brief GAS 상태 태그를 확인해 이동과 점프 입력이 차단된 상태인지 반환한다.
//! \param 없음
//! \return 이동 또는 점프 입력 차단 태그가 있으면 true
bool UPlayerMovementComponent::IsMovementInputBlocked() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OwnerCharacter.Get());
	if (PlayerCharacter && PlayerCharacter->IsDead())
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(OwnerCharacter);
	return ASC
		&& (ASC->HasMatchingGameplayTag(MyGameplayTags::State_Player_Dead)
			|| ASC->HasMatchingGameplayTag(MyGameplayTags::State_Skill_BlockMoveInput));
}

////////////////////////////
//! \author HanUl
//! \brief GAS 상태 태그를 확인해 다른 스킬 입력이 차단된 상태인지 반환한다.
//! \param 없음
//! \return 스킬 입력 차단 태그가 있으면 true
bool UPlayerMovementComponent::IsSkillInputBlocked() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(OwnerCharacter);
	return ASC
		&& (ASC->HasMatchingGameplayTag(MyGameplayTags::State_Player_Dead)
			|| ASC->HasMatchingGameplayTag(MyGameplayTags::State_Skill_BlockSkillInput));
}

////////////////////////////
//! \author HanUl
//! \brief 현재 로컬 또는 권한 인스턴스에서 회전을 적용할 수 있는지 확인한다.
//! \param 없음
//! \return 회전 적용 가능 여부
bool UPlayerMovementComponent::CanApplyFacing() const
{
	return OwnerCharacter && (OwnerCharacter->IsLocallyControlled() || OwnerCharacter->HasAuthority());
}

////////////////////////////
//! \author HanUl
//! \brief 이동 회전 요청이 현재 스킬 회전 요청을 덮어쓸 수 있는지 확인한다.
//! \param 없음
//! \return 이동 회전으로 전환 가능하면 true
bool UPlayerMovementComponent::CanMovementOverrideSkillFacing() const
{
	if (!bHasSkillFacingRequest)
	{
		return true;
	}

	if (IsSkillInputBlocked())
	{
		return false;
	}

	return GetOwnerWorldTimeSeconds() - SkillFacingRequestTime >= SkillFacingMovementOverrideDelay;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 우선순위에 맞는 회전 요청을 적용한다.
//! \param DeltaTime 이전 프레임 이후 경과 시간
//! \return 없음
void UPlayerMovementComponent::UpdateFacing(float DeltaTime)
{
	if (!CanApplyFacing())
	{
		bHasMovementFacingRequest = false;
		return;
	}

	if (bFacingYawLocked)
	{
		ApplyFacingYawToController(LockedFacingYaw);
		return;
	}

	if (bHasSkillFacingRequest)
	{
		const bool bReached = ApplyFacingYaw(SkillFacingYaw, SkillFacingInterpSpeed, SkillFacingToleranceDegrees, true, DeltaTime);
		if (bReached)
		{
			bHasSkillFacingRequest = false;
		}
		return;
	}

	if (bHasMovementFacingRequest)
	{
		ApplyFacingYaw(MovementFacingYaw, MovementFacingInterpSpeed, 0.0f, false, DeltaTime);
		bHasMovementFacingRequest = false;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 지정한 목표 Yaw를 보간해 Controller와 Character에 적용한다.
//! \param TargetYaw 목표 월드 기준 Yaw
//! \param InterpSpeed 회전 보간 속도
//! \param ToleranceDegrees 목표 도달 허용 각도
//! \param bClearOnReached 목표 도달 판정을 사용할지 여부
//! \param DeltaTime 이전 프레임 이후 경과 시간
//! \return 목표 각도에 도달했으면 true
bool UPlayerMovementComponent::ApplyFacingYaw(float TargetYaw, float InterpSpeed, float ToleranceDegrees, bool bClearOnReached, float DeltaTime)
{
	if (!CanApplyFacing())
	{
		return false;
	}

	AController* Controller = OwnerCharacter->GetController();
	if (!Controller)
	{
		return false;
	}

	TargetYaw = FRotator::NormalizeAxis(TargetYaw);
	float NextYaw = TargetYaw;
	if (InterpSpeed > 0.0f && DeltaTime > 0.0f)
	{
		const float CurrentYaw = FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw);
		const FRotator CurrentRotation(0.0f, CurrentYaw, 0.0f);
		const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
		const FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, InterpSpeed);
		NextYaw = FRotator::NormalizeAxis(SmoothedRotation.Yaw);
	}

	const float RemainingYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(NextYaw, TargetYaw));
	const bool bReached = bClearOnReached && RemainingYawDelta <= ToleranceDegrees;
	if (bReached)
	{
		NextYaw = TargetYaw;
	}

	ApplyFacingYawToController(NextYaw);

	if (OwnerCharacter->HasAuthority())
	{
		OwnerCharacter->ForceNetUpdate();
	}

	return bReached;
}

////////////////////////////
//! \author HanUl
//! \brief Controller Yaw와 Character facing을 지정한 월드 Yaw로 적용한다.
//! \param NewYaw 적용할 월드 기준 Yaw
//! \return 없음
void UPlayerMovementComponent::ApplyFacingYawToController(float NewYaw) const
{
	if (!OwnerCharacter)
	{
		return;
	}

	AController* Controller = OwnerCharacter->GetController();
	if (!Controller)
	{
		return;
	}

	FRotator NewControlRotation = Controller->GetControlRotation();
	NewControlRotation.Yaw = FRotator::NormalizeAxis(NewYaw);
	Controller->SetControlRotation(NewControlRotation);
	OwnerCharacter->FaceRotation(NewControlRotation, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 소유 캐릭터 월드의 현재 시간을 반환한다.
//! \param 없음
//! \return 월드 시간이 있으면 현재 시간, 없으면 0
float UPlayerMovementComponent::GetOwnerWorldTimeSeconds() const
{
	const UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0f;
}

////////////////////////////
//! \author 장효제
//! \brief 이동·정지 상태를 지켜보기 시작한다. 서버에서만 돈다.
//! \details 이동에는 전이 지점이 없다. 속도는 이어서 변하기 때문이다.
//!          그래서 주기로 보고, 문턱을 넘는 순간만 상태 변화로 삼는다.
void UPlayerMovementComponent::StartStreamingMoveWatch()
{
	UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		StreamingMoveWatchHandle,
		this,
		&UPlayerMovementComponent::UpdateStreamingMoveState,
		MyStreamingMoveWatch::IntervalSeconds,
		true);
}

////////////////////////////
//! \author 장효제
//! \brief 지금 속도를 보고 이동·정지 상태를 갱신한다.
//! \details 문턱을 하나만 두면 그 언저리에서 상태가 쉴 새 없이 뒤집힌다.
//!          켜지는 문턱을 끄는 문턱보다 높게 두어 사이 구간에서는 그대로 둔다.
//!          죽은 동안은 멈춰 있어도 정지로 세지 않는다. 조작할 수 없는 시간이라
//!          "움직이지 않았다"는 말이 성립하지 않는다.
void UPlayerMovementComponent::UpdateStreamingMoveState()
{
	if (!OwnerCharacter)
	{
		return;
	}

	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OwnerCharacter);
	if (PlayerCharacter && PlayerCharacter->IsDead())
	{
		// 상태를 끄고 나간다. 켜진 채로 두면 죽은 동안 시간이 계속 재진다.
		if (bStreamingMoveStateBroadcast)
		{
			MyStreamingState::BroadcastState(
				OwnerCharacter, MyGameplayTags::Streaming_State_Player_Moving, false);
			MyStreamingState::BroadcastState(
				OwnerCharacter, MyGameplayTags::Streaming_State_Player_Idle, false);
			bStreamingMoveStateBroadcast = false;
		}
		return;
	}

	const float Speed = OwnerCharacter->GetVelocity().Size2D();
	const bool bNowMoving = MyStreamingMoveWatch::ResolveMoving(Speed, bStreamingMoving);
	if (bStreamingMoveStateBroadcast && bNowMoving == bStreamingMoving)
	{
		return;
	}

	BroadcastStreamingMoveState(bNowMoving);
}

////////////////////////////
//! \author 장효제
//! \brief 이동·정지 상태를 짝으로 알린다.
//! \details 둘은 반대말이라 한쪽을 켜면 반드시 다른 쪽을 끈다.
//!          꺼짐을 빠뜨리면 스트리밍이 두 상태를 모두 켜진 것으로 본다.
//! \param bNowMoving 지금 이동 중이면 true다.
void UPlayerMovementComponent::BroadcastStreamingMoveState(const bool bNowMoving)
{
	bStreamingMoving = bNowMoving;
	bStreamingMoveStateBroadcast = true;

	MyStreamingState::BroadcastState(
		OwnerCharacter, MyGameplayTags::Streaming_State_Player_Moving, bNowMoving);
	MyStreamingState::BroadcastState(
		OwnerCharacter, MyGameplayTags::Streaming_State_Player_Idle, !bNowMoving);
}
