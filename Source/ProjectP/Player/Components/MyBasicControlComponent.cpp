#include "MyBasicControlComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "../../Streaming/MyStreamingPayloads.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "PlayerCameraComponent.h"
#include "../PlayerCharacterBase.h"
#include "PlayerInteractionComponent.h"
#include "PlayerMovementComponent.h"
#include "MyPlayerController.h"


////////////////////////////
//! \author HanUl
//! \brief 기본 조작 입력 바인딩을 담당하는 컴포넌트를 생성한다.
//! \param 없음
//! \return 없음
UMyBasicControlComponent::UMyBasicControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 시작 시점에 필요한 기본 초기화를 수행한다.
//! \param 없음
//! \return 없음
void UMyBasicControlComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwnerReferences();

	if (OwnerCharacter)
	{
		if (PlayerMovementComponent)
		{
			PlayerMovementComponent->SetFacingYawImmediate(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw));
		}
	}
}


////////////////////////////
//! \author HanUl
//! \brief 오비트 모드에서 이동 방향에 맞춰 캐릭터 회전을 갱신한다.
//! \param DeltaTime 프레임 델타 타임
//! \param TickType 현재 Tick 종류
//! \param ThisTickFunction 현재 Tick 함수
//! \return 없음
void UMyBasicControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CacheOwnerReferences() || !OwnerCharacter->IsLocallyControlled() || IsOwnerDead())
	{
		return;
	}

	if (!IsOrbitMode())
	{
		return;
	}

	const FVector Velocity2D(OwnerCharacter->GetVelocity().X, OwnerCharacter->GetVelocity().Y, 0.0f);
	if (!Velocity2D.IsNearlyZero())
	{
		if (PlayerMovementComponent)
		{
			PlayerMovementComponent->RequestMovementFacingYaw(Velocity2D.Rotation().Yaw);
		}
	}
}


////////////////////////////
//! \author HanUl
//! \brief 기본 조작 처리에 필요한 Character와 세부 실행 컴포넌트 참조를 연결한다.
//! \param InOwnerCharacter 입력을 적용할 캐릭터
//! \param InPlayerMovementComponent 이동 입력 실행 컴포넌트
//! \param InPlayerCameraComponent 카메라 입력 실행 컴포넌트
//! \param InCameraBoom 카메라 기준 Yaw를 계산할 SpringArm 컴포넌트
//! \return 없음
void UMyBasicControlComponent::InitializeBasicControl(ACharacter* InOwnerCharacter, UPlayerMovementComponent* InPlayerMovementComponent, UPlayerCameraComponent* InPlayerCameraComponent, USpringArmComponent* InCameraBoom)
{
	OwnerCharacter = InOwnerCharacter;
	PlayerMovementComponent = InPlayerMovementComponent;
	PlayerCameraComponent = InPlayerCameraComponent;
	CameraBoom = InCameraBoom;

	if (OwnerCharacter)
	{
		if (PlayerMovementComponent)
		{
			PlayerMovementComponent->SetFacingYawImmediate(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw));
		}
	}
}


////////////////////////////
//! \author HanUl
//! \brief 기본 이동과 카메라 제어 입력을 소유 Character의 처리 함수에 바인딩한다.
//! \param PlayerInputComponent 입력 바인딩 대상 컴포넌트
//! \return 없음
void UMyBasicControlComponent::BindBasicInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	if (!CacheOwnerReferences())
	{
		return;
	}

	// 어떤 조작이든 잠수 판정을 깨운다. 조작 종류는 가리지 않는다.
	for (const UInputAction* Action : {IA_Move, IA_Jump, IA_Zoom, IA_RMB, IA_MouseLook,
		IA_CamLock, IA_Interaction})
	{
		if (Action)
		{
			EIC->BindAction(Action, ETriggerEvent::Triggered, this,
				&UMyBasicControlComponent::ReportPlayerInput);
		}
	}

	if (IA_Move)
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &UMyBasicControlComponent::Move);
	}

	if (IA_Jump)
	{
		EIC->BindAction(IA_Jump, ETriggerEvent::Triggered, this, &UMyBasicControlComponent::HandleJumpInput);
	}

	if (IA_Zoom)
	{
		EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &UMyBasicControlComponent::Zoom);
	}

	if (IA_RMB)
	{
		EIC->BindAction(IA_RMB, ETriggerEvent::Started, this, &UMyBasicControlComponent::StartOrbit);
		EIC->BindAction(IA_RMB, ETriggerEvent::Completed, this, &UMyBasicControlComponent::EndOrbit);
	}

	if (IA_MouseLook)
	{
		EIC->BindAction(IA_MouseLook, ETriggerEvent::Triggered, this, &UMyBasicControlComponent::OrbitLook);
	}

	if (IA_CamLock)
	{
		EIC->BindAction(IA_CamLock, ETriggerEvent::Started, this, &UMyBasicControlComponent::ToggleCameraLock);
	}

	if (IA_Interaction) {
		EIC->BindAction(IA_Interaction, ETriggerEvent::Started, this, &UMyBasicControlComponent::HandleInteractionInput);
	}
}


////////////////////////////
//! \author HanUl
//! \brief 입력된 2D 이동값을 이동 컴포넌트에 전달하고 계산된 이동 방향으로 캐릭터를 회전한다.
//! \param Value Enhanced Input의 2D 축 입력값
//! \return 없음
void UMyBasicControlComponent::Move(const FInputActionValue& Value)
{
	if (!CacheOwnerReferences() || !PlayerMovementComponent)
	{
		return;
	}

	float MoveYaw = 0.0f;
	PlayerMovementComponent->HandleMove(Value, MoveYaw);
}


////////////////////////////
//! \author HanUl
//! \brief 점프 입력을 이동 컴포넌트에 전달한다.
//! \param Value Enhanced Input의 점프 입력
//! \return 없음
void UMyBasicControlComponent::HandleJumpInput(const FInputActionValue& Value)
{
	if (CacheOwnerReferences() && PlayerMovementComponent)
	{
		PlayerMovementComponent->HandleJump(Value);
	}
}


////////////////////////////
//! \author HanUl
//! \editor 준혁 - 상호작용 가이드 표시 중에는 휠을 줌 대신 옵션 선택 이동으로 라우팅
//! \brief 줌 입력을 처리한다. 상호작용 가이드(옵션 목록 보유 후보)가 떠 있으면
//!        옵션 개수와 무관하게 줌을 막고 휠을 옵션 선택 이동으로 사용한다.
//! \param Value Enhanced Input의 줌 입력
//! \return 없음
void UMyBasicControlComponent::Zoom(const FInputActionValue& Value)
{
	if (!CacheOwnerReferences() || IsOwnerDead())
	{
		return;
	}

	if (UPlayerInteractionComponent* PlayerInteractionComponent = OwnerCharacter->FindComponentByClass<UPlayerInteractionComponent>())
	{
		if (PlayerInteractionComponent->HasInteractionOptions())
		{
			// 휠 업(+) = 위 옵션, 휠 다운(-) = 아래 옵션. (대화 선택지와 동일한 방향 규칙)
			PlayerInteractionComponent->StepSelectedOption(Value.Get<float>() > 0.0f ? -1 : 1);
			return;
		}
	}

	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->HandleZoom(Value);
	}
}


////////////////////////////
//! \author HanUl
//! \brief 우클릭 입력 시작 시 오비트 모드와 카메라 커서 처리를 시작한다.
//! \param Value Enhanced Input의 우클릭 입력
//! \return 없음
void UMyBasicControlComponent::StartOrbit(const FInputActionValue& Value)
{
	const bool bPressed = Value.Get<bool>();
	if (!bPressed || !CacheOwnerReferences())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (const AMyPlayerController* MyPC = Cast<AMyPlayerController>(PC);
		MyPC && MyPC->IsDeathSpectating())
	{
		return;
	}

	if (PlayerCameraComponent && PlayerCameraComponent->IsOrbitRotationLocked())
	{
		return;
	}

	if (PlayerCameraComponent && PC)
	{
		PlayerCameraComponent->HandleOrbitStartCursor(PC, SavedMouseX, SavedMouseY, bHasSavedMousePos);
	}

	const float NewPreOrbitFacingYaw = OwnerCharacter->GetActorRotation().Yaw;
	float NewOrbitFacingCameraOffsetYaw = 0.0f;
	if (CameraBoom)
	{
		const float CameraYaw = CameraBoom->GetComponentRotation().Yaw;
		NewOrbitFacingCameraOffsetYaw = FMath::FindDeltaAngleDegrees(CameraYaw, NewPreOrbitFacingYaw);
	}

	EnterOrbit(NewPreOrbitFacingYaw, NewOrbitFacingCameraOffsetYaw);
}


////////////////////////////
//! \author HanUl
//! \brief 우클릭 입력 종료 시 오비트 모드와 카메라 커서 처리를 종료한다.
//! \param Value Enhanced Input의 우클릭 입력
//! \return 없음
void UMyBasicControlComponent::EndOrbit(const FInputActionValue& Value)
{
	const bool bPressed = Value.Get<bool>();
	if (bPressed || !CacheOwnerReferences() || !IsOrbitMode())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (const AMyPlayerController* MyPC = Cast<AMyPlayerController>(PC);
		MyPC && MyPC->IsDeathSpectating())
	{
		return;
	}

	if (PlayerCameraComponent && PC)
	{
		PlayerCameraComponent->HandleOrbitEndCursor(PC, SavedMouseX, SavedMouseY, bHasSavedMousePos);
	}

	ExitOrbit();
}


////////////////////////////
//! \author HanUl
//! \brief 우클릭 중 마우스 입력을 카메라 컴포넌트에 전달한다.
//! \param Value Enhanced Input의 마우스룩 입력
//! \return 없음
void UMyBasicControlComponent::OrbitLook(const FInputActionValue& Value)
{
	const AMyPlayerController* MyPC = CacheOwnerReferences()
		? Cast<AMyPlayerController>(OwnerCharacter->GetController())
		: nullptr;
	if (MyPC && MyPC->IsDeathSpectating())
	{
		return;
	}

	if (CacheOwnerReferences() && PlayerCameraComponent)
	{
		PlayerCameraComponent->HandleOrbitLook(Value, IsOrbitMode());
	}
}

////////////////////////////
//! \author HanUl
//! \brief 카메라 각도 잠금을 토글하고 잠금 시 진행 중인 Orbit과 커서 캡처를 정리한다.
//! \param Value Enhanced Input의 카메라 잠금 입력
//! \return 없음
void UMyBasicControlComponent::ToggleCameraLock(const FInputActionValue& Value)
{
	(void)Value;

	if (!CacheOwnerReferences() || IsOwnerDead() || !PlayerCameraComponent)
	{
		return;
	}

	const bool bLocked = PlayerCameraComponent->ToggleOrbitRotationLock();
	if (!bLocked || !IsOrbitMode())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (PC)
	{
		PlayerCameraComponent->HandleOrbitEndCursor(
			PC,
			SavedMouseX,
			SavedMouseY,
			bHasSavedMousePos);
	}

	bHasSavedMousePos = false;
	ExitOrbit();
}

////////////////////////////
//! \author HanUl
//! \brief 사망 진입 시 기존 Pawn 카메라의 Orbit 상태와 커서를 정리한다.
//! \param bDead true이면 사망 상태
//! \return 없음
void UMyBasicControlComponent::HandleOwnerLifeStateChanged(bool bDead)
{
	if (!bDead || !CacheOwnerReferences())
	{
		return;
	}

	if (IsOrbitMode())
	{
		APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
		if (PlayerCameraComponent && PC)
		{
			PlayerCameraComponent->HandleOrbitEndCursor(
				PC,
				SavedMouseX,
				SavedMouseY,
				bHasSavedMousePos);
		}
	}

	bHasSavedMousePos = false;
	ExitOrbit();
}


////////////////////////////
//! \author HanUl
//! \brief 상호작용 입력을 상호작용 컴포넌트에 전달한다.
//! \param Value Enhanced Input의 상호작용 입력
//! \return 없음
void UMyBasicControlComponent::HandleInteractionInput(const FInputActionValue& Value)
{
	if (!CacheOwnerReferences() || IsOwnerDead())
	{
		return;
	}

	if (UPlayerInteractionComponent* PlayerInteractionComponent = OwnerCharacter->FindComponentByClass<UPlayerInteractionComponent>())
	{
		PlayerInteractionComponent->TryInteract();
	}
}


////////////////////////////
//! \author HanUl
//! \brief 현재 조작 모드가 오비트 모드인지 확인한다.
//! \return 오비트 모드 여부
bool UMyBasicControlComponent::IsOrbitMode() const
{
	return ControlMode == EPlayerControlMode::Orbit;
}


////////////////////////////
//! \author HanUl
//! \brief 현재 조작 모드가 마우스 조준 모드인지 확인한다.
//! \return 마우스 조준 모드 여부
bool UMyBasicControlComponent::IsMouseAimMode() const
{
	return ControlMode == EPlayerControlMode::MouseAim;
}


////////////////////////////
//! \author HanUl
//! \brief Owner와 기본 조작 실행 컴포넌트 참조를 지연 캐시한다.
//! \return 기본 조작 처리가 가능한 Owner 캐시 여부
bool UMyBasicControlComponent::CacheOwnerReferences()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(OwnerActor);
	}

	if (!PlayerMovementComponent)
	{
		PlayerMovementComponent = OwnerActor->FindComponentByClass<UPlayerMovementComponent>();
	}

	if (!PlayerCameraComponent)
	{
		PlayerCameraComponent = OwnerActor->FindComponentByClass<UPlayerCameraComponent>();
	}

	if (!CameraBoom)
	{
		CameraBoom = OwnerActor->FindComponentByClass<USpringArmComponent>();
	}

	return OwnerCharacter != nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 소유 Character가 사망 상태인지 확인한다. Orbit 입력은 이 판정과 무관하게 유지된다.
//! \param 없음
//! \return 사망 상태이면 true
bool UMyBasicControlComponent::IsOwnerDead() const
{
	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OwnerCharacter.Get());
	return PlayerCharacter && PlayerCharacter->IsDead();
}


////////////////////////////
//! \author HanUl
//! \brief Orbit 모드 진입 시 조준 기준 각도를 저장하고 모드를 전환한다.
//! \param InPreOrbitFacingYaw Orbit 진입 직전 캐릭터 방향
//! \param InOrbitFacingCameraOffsetYaw 카메라와 캐릭터 상대 Yaw
//! \return 없음
void UMyBasicControlComponent::EnterOrbit(float InPreOrbitFacingYaw, float InOrbitFacingCameraOffsetYaw)
{
	PreOrbitFacingYaw = InPreOrbitFacingYaw;
	OrbitFacingCameraOffsetYaw = InOrbitFacingCameraOffsetYaw;
	ControlMode = EPlayerControlMode::Orbit;
}


////////////////////////////
//! \author HanUl
//! \brief Orbit 모드를 종료하고 MouseAim 모드로 복귀한다.
//! \param 없음
//! \return 없음
void UMyBasicControlComponent::ExitOrbit()
{
	ControlMode = EPlayerControlMode::MouseAim;
}

////////////////////////////
//! \author 장효제
//! \brief 조작이 있었다는 사실을 서버에 알린다.
//! \details 이동과 마우스는 프레임마다 들어온다. 그대로 보내면 3인 기준 초당
//!          수백 건이 되므로 시간으로 눌러서 보낸다. 잠수 판정이 수십 초 단위라
//!          이 해상도로 충분하다.
//! \return 없음
void UMyBasicControlComponent::ReportPlayerInput()
{
	const AActor* Owner = GetOwner();
	const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (!MyStreamingPlayerInput::ShouldReport(Now, LastPlayerInputReportSeconds))
	{
		return;
	}

	LastPlayerInputReportSeconds = Now;
	ServerReportPlayerInput();
}

////////////////////////////
//! \author 장효제
//! \brief 서버에서 조작 사실을 발행한다.
//! \return 없음
void UMyBasicControlComponent::ServerReportPlayerInput_Implementation()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	int32 UserIndex = INDEX_NONE;
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			UserIndex = PlayerState->GetPlayerId();
		}
	}

	MyStreamingPlayerInput::BroadcastPlayerInput(this, UserIndex);
}
