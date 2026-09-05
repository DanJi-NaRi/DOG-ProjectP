#include "Player/Spectator/PlayerSpectatorCameraActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

////////////////////////////
//! \author HanUl
//! \brief 로컬 관전용 루트, SpringArm, Camera를 생성하고 복제를 비활성화한다.
//! \param 없음
//! \return 없음
APlayerSpectatorCameraActor::APlayerSpectatorCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicates(false);
	SetActorEnableCollision(false);

	SpectatorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpectatorRoot"));
	SetRootComponent(SpectatorRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SpectatorRoot);
	CameraBoom->TargetArmLength = 2000.0f;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetUsingAbsoluteRotation(true);

	SpectatorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectatorCamera"));
	SpectatorCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	SpectatorCamera->bUsePawnControlRotation = false;
	SpectatorCamera->FieldOfView = 70.0f;
}

////////////////////////////
//! \author HanUl
//! \brief 관전 중 카메라 Zoom을 보간하고 생존 팀원이 지정된 경우 피벗을 대상 위치로 이동한다.
//! \param DeltaSeconds 프레임 델타 시간
//! \return 없음
void APlayerSpectatorCameraActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = FMath::FInterpTo(
			CameraBoom->TargetArmLength,
			TargetArmLength,
			DeltaSeconds,
			ZoomInterpSpeed);
	}

	if (SpectatorCamera)
	{
		SpectatorCamera->SetFieldOfView(FMath::FInterpTo(
			SpectatorCamera->FieldOfView,
			TargetFieldOfView,
			DeltaSeconds,
			ZoomInterpSpeed));
	}

	AActor* TargetActor = FollowTarget.Get();
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector DesiredLocation = TargetActor->GetActorLocation() + TargetOffset;
	const FVector NewLocation = FollowInterpSpeed > 0.0f
		? FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaSeconds, FollowInterpSpeed)
		: DesiredLocation;
	SetActorLocation(NewLocation);
}

////////////////////////////
//! \author HanUl
//! \brief PlayerController의 ViewTarget으로 사용될 현재 카메라 정보를 제공한다.
//! \param DeltaTime 프레임 델타 시간
//! \param OutResult 출력할 카메라 시점 정보
//! \return 없음
void APlayerSpectatorCameraActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (SpectatorCamera)
	{
		SpectatorCamera->GetCameraView(DeltaTime, OutResult);
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

////////////////////////////
//! \author HanUl
//! \brief 기존 플레이 카메라와 동일한 화면에서 시작하도록 관전 피벗과 렌즈 값을 초기화한다.
//! \param CameraLocation 현재 플레이 카메라 월드 위치
//! \param CameraRotation 현재 플레이 카메라 월드 회전
//! \param ArmLength 사용할 SpringArm 길이
//! \param FieldOfView 사용할 수직 시야각
//! \return 없음
void APlayerSpectatorCameraActor::InitializeFromView(
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float ArmLength,
	float FieldOfView)
{
	if (!CameraBoom || !SpectatorCamera)
	{
		return;
	}

	const float SafeArmLength = FMath::Max(ArmLength, 0.0f);
	const FVector PivotLocation = CameraLocation + (CameraRotation.Vector() * SafeArmLength);

	SetActorLocation(PivotLocation);
	CameraBoom->TargetArmLength = SafeArmLength;
	TargetArmLength = SafeArmLength;
	CameraBoom->SetWorldRotation(CameraRotation);
	const float SafeFieldOfView = FMath::Clamp(FieldOfView, 5.0f, 170.0f);
	SpectatorCamera->SetFieldOfView(SafeFieldOfView);
	TargetFieldOfView = SafeFieldOfView;
}

////////////////////////////
//! \author HanUl
//! \brief 카메라가 위치를 추적할 생존 팀원을 지정한다.
//! \param NewFollowTarget 새 추적 대상 Actor
//! \return 없음
void APlayerSpectatorCameraActor::SetFollowTarget(AActor* NewFollowTarget)
{
	FollowTarget = NewFollowTarget;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 추적 대상을 해제하고 마지막 카메라 피벗 위치에 머문다.
//! \param 없음
//! \return 없음
void APlayerSpectatorCameraActor::ClearFollowTarget()
{
	FollowTarget.Reset();
}

////////////////////////////
//! \author HanUl
//! \brief 관전 활성 상태에 맞춰 추적과 Zoom 보간에 사용하는 Actor Tick을 전환한다.
//! \param bActive true이면 관전 Tick 활성화
//! \return 없음
void APlayerSpectatorCameraActor::SetSpectatorActive(bool bActive)
{
	SetActorTickEnabled(bActive);
}

////////////////////////////
//! \author HanUl
//! \brief 휠 입력으로 관전 카메라의 목표 ArmLength와 FOV를 독립적으로 조정한다.
//! \param ZoomInput 휠 위는 양수, 아래는 음수
//! \param ArmLengthStep 휠 한 단계당 ArmLength 변화량
//! \param MinArmLength 최소 ArmLength
//! \param MaxArmLength 최대 ArmLength
//! \param FieldOfViewStep 휠 한 단계당 FOV 변화량
//! \param MinFieldOfView 최소 FOV
//! \param MaxFieldOfView 최대 FOV
//! \return 없음
void APlayerSpectatorCameraActor::AdjustZoom(
	float ZoomInput,
	float ArmLengthStep,
	float MinArmLength,
	float MaxArmLength,
	float FieldOfViewStep,
	float MinFieldOfView,
	float MaxFieldOfView)
{
	const float SafeMinArmLength = FMath::Min(MinArmLength, MaxArmLength);
	const float SafeMaxArmLength = FMath::Max(MinArmLength, MaxArmLength);
	const float SafeMinFieldOfView = FMath::Min(MinFieldOfView, MaxFieldOfView);
	const float SafeMaxFieldOfView = FMath::Max(MinFieldOfView, MaxFieldOfView);

	TargetArmLength = FMath::Clamp(
		TargetArmLength + (ZoomInput * ArmLengthStep),
		SafeMinArmLength,
		SafeMaxArmLength);
	TargetFieldOfView = FMath::Clamp(
		TargetFieldOfView + (ZoomInput * FieldOfViewStep),
		SafeMinFieldOfView,
		SafeMaxFieldOfView);
}

////////////////////////////
//! \author HanUl
//! \brief 우클릭 Orbit 중 전달된 마우스 이동량으로 관전 SpringArm 회전을 변경한다.
//! \param MouseDeltaX 마우스 가로 이동량
//! \param MouseDeltaY 마우스 세로 이동량
//! \param YawSpeed Yaw 입력 배율
//! \param PitchSpeed Pitch 입력 배율
//! \param MinPitch 허용할 최소 Pitch
//! \param MaxPitch 허용할 최대 Pitch
//! \return 없음
void APlayerSpectatorCameraActor::AddOrbitInput(
	float MouseDeltaX,
	float MouseDeltaY,
	float YawSpeed,
	float PitchSpeed,
	float MinPitch,
	float MaxPitch)
{
	if (!CameraBoom)
	{
		return;
	}

	FRotator NewRotation = CameraBoom->GetComponentRotation();
	NewRotation.Yaw += MouseDeltaX * YawSpeed;
	NewRotation.Pitch = FMath::Clamp(
		NewRotation.Pitch + (MouseDeltaY * PitchSpeed),
		MinPitch,
		MaxPitch);
	CameraBoom->SetWorldRotation(NewRotation);
}

////////////////////////////
//! \author HanUl
//! \brief 팀원 추적 높이 오프셋과 위치 보간 속도를 설정한다.
//! \param InTargetOffset 대상 Actor 위치에 더할 월드 오프셋
//! \param InFollowInterpSpeed 위치 보간 속도
//! \return 없음
void APlayerSpectatorCameraActor::ConfigureFollow(const FVector& InTargetOffset, float InFollowInterpSpeed)
{
	TargetOffset = InTargetOffset;
	FollowInterpSpeed = FMath::Max(InFollowInterpSpeed, 0.0f);
}
