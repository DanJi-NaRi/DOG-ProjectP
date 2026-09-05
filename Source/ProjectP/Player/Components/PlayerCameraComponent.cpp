// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MyGameplayTags.h"

UPlayerCameraComponent::UPlayerCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPlayerCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);

	if (CameraBoom) {
		TargetArmLength = CameraBoom->TargetArmLength;
	}

	if (FollowCamera) {
		TargetFov = FollowCamera->FieldOfView;
	}


}

////////////////////////////
//! \author HanUl
//! \brief 상호작용 뷰용 카메라 액터를 정리한다.
//! \param EndPlayReason 종료 사유
//! \return 없음
void UPlayerCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LastCameraFeedbackTimeByTag.Reset();

	if (InteractionViewCamera)
	{
		InteractionViewCamera->Destroy();
		InteractionViewCamera = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UPlayerCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraBoom || !FollowCamera) {
		//UE_LOG(LogTemp, Log, TEXT("Disable Tick Check"));
		return;
	}

	// Smoothly interpolate arm length and FOV
	//UE_LOG(LogTemp, Log, TEXT("Tick Check"));

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetArmLength,
		DeltaTime,
		LerpSpeed
	);

	const float NewFov = FMath::FInterpTo(
		FollowCamera->FieldOfView,
		TargetFov,
		DeltaTime,
		LerpSpeed 
	);

	FollowCamera->SetFieldOfView(NewFov);
	
}

////////////////////////////
//! \author HanUl
//! \brief 공격자 적중 피드백 태그에 대응하는 CameraShake를 소유 플레이어의 현재 카메라에 로컬 재생한다.
//! \param CameraFeedbackTag Basic/Skill/Ultimate 공격자 적중 피드백 태그
//! \return 없음
void UPlayerCameraComponent::PlayAttackerHitCameraFeedback(FGameplayTag CameraFeedbackTag)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !PlayerController || !PlayerController->PlayerCameraManager || !World)
	{
		return;
	}

	// 상점 등 별도 ViewTarget을 사용하는 연출 중에는 전투 카메라 피드백을 재생하지 않는다.
	if (bInteractionViewActive)
	{
		return;
	}

	TSubclassOf<UCameraShakeBase> ShakeClass;
	float ShakeScale = 0.0f;
	float MinInterval = 0.0f;
	if (!ResolveAttackerHitCameraFeedback(CameraFeedbackTag, ShakeClass, ShakeScale, MinInterval)
		|| !ShakeClass
		|| ShakeScale <= 0.0f)
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (const double* LastPlayTime = LastCameraFeedbackTimeByTag.Find(CameraFeedbackTag))
	{
		if (CurrentTime - *LastPlayTime < static_cast<double>(FMath::Max(MinInterval, 0.0f)))
		{
			return;
		}
	}

	LastCameraFeedbackTimeByTag.Add(CameraFeedbackTag, CurrentTime);
	PlayerController->PlayerCameraManager->StartCameraShake(
		ShakeClass,
		ShakeScale,
		ECameraShakePlaySpace::CameraLocal
	);
}

////////////////////////////
//! \author HanUl
//! \brief 공격자 적중 피드백 태그를 에디터 설정 CameraShake 클래스와 스케일, 최소 간격으로 해석한다.
//! \param CameraFeedbackTag 해석할 공격자 적중 피드백 태그
//! \param OutShakeClass 재생할 CameraShake 클래스
//! \param OutScale 재생 스케일
//! \param OutMinInterval 같은 등급의 최소 재생 간격
//! \return 지원하는 피드백 태그이면 true
bool UPlayerCameraComponent::ResolveAttackerHitCameraFeedback(
	FGameplayTag CameraFeedbackTag,
	TSubclassOf<UCameraShakeBase>& OutShakeClass,
	float& OutScale,
	float& OutMinInterval
) const
{
	OutShakeClass = nullptr;
	OutScale = 0.0f;
	OutMinInterval = 0.0f;

	if (CameraFeedbackTag.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Basic))
	{
		OutShakeClass = BasicAttackHitShakeClass;
		OutScale = BasicAttackHitShakeScale;
		OutMinInterval = BasicAttackHitMinInterval;
		return true;
	}

	if (CameraFeedbackTag.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Skill))
	{
		OutShakeClass = SkillHitShakeClass;
		OutScale = SkillHitShakeScale;
		OutMinInterval = SkillHitMinInterval;
		return true;
	}

	if (CameraFeedbackTag.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Ultimate))
	{
		OutShakeClass = UltimateHitShakeClass;
		OutScale = UltimateHitShakeScale;
		OutMinInterval = UltimateHitMinInterval;
		return true;
	}

	return false;
}

////////////////////////////
// Author : HanUl
// About : 카메라 붐 참조를 컴포넌트에 연결한다.
// Parameters : InCameraBoom - 캐릭터 소유 SpringArm 컴포넌트
// return : 없음
void UPlayerCameraComponent::InitializeCameraBoom(USpringArmComponent* InCameraBoom)
{
	
	CameraBoom = InCameraBoom;
	if (CameraBoom)
	{
		TargetArmLength = CameraBoom->TargetArmLength;
		GameplayCameraBoomParent = CameraBoom->GetAttachParent();
		GameplayCameraBoomSocketName = CameraBoom->GetAttachSocketName();
		GameplayCameraBoomRelativeLocation = CameraBoom->GetRelativeLocation();
		GameplayCameraBoomRelativeScale = CameraBoom->GetRelativeScale3D();
	}

}

void UPlayerCameraComponent::InitializeFollowCamera(UCameraComponent* InFollowCamera)
{

	FollowCamera = InFollowCamera;
	if (FollowCamera)
	{
		TargetFov = FollowCamera->FieldOfView;
	}

}

////////////////////////////
// Author : HanUl
// About : 줌 입력으로 카메라 붐 길이 및 FOV 를 조정한다. 보간을 위해 실제 적용은 Tick에서 처리.
// Parameters : Value - Enhanced Input의 줌 입력(float)
// return : 없음
void UPlayerCameraComponent::HandleZoom(const FInputActionValue& Value)
{
	// 상호작용 뷰 중에는 게임플레이 카메라 상태를 바꾸지 않는다(복귀 시 튐 방지).
	if (bInteractionViewActive || !CameraBoom || !FollowCamera)
	{
		return;
	}

	const float ZoomValue = Value.Get<float>();
	
	//UE_LOG(LogTemp, Log, TEXT("Zoom Input: %f"), ZoomValue);

	// Lerp
	
	TargetArmLength = FMath::Clamp(
		TargetArmLength + (ZoomValue * ZoomStep),
		MinArmLength,
		MaxArmLength
	);

	//CameraBoom->TargetArmLength = NewArmLength; // Lerp를 위해 Tick에서 처리
	
	// fov
	TargetFov = FMath::GetMappedRangeValueClamped(
		FVector2D(MinArmLength, MaxArmLength),
		FVector2D(MinFov, MaxFov),
		TargetArmLength
	);

	//FollowCamera->SetFieldOfView(NewFov); // Lerp를 위해 Tick에서 처리

	
}

////////////////////////////
// Author : HanUl
// About : Orbit 모드일 때 마우스 룩 입력으로 카메라 회전을 갱신한다.
// Parameters : Value - Enhanced Input의 마우스룩 입력(FVector2D), bIsOrbiting - Orbit 상태 여부
// return : 없음
void UPlayerCameraComponent::HandleOrbitLook(const FInputActionValue& Value, bool bIsOrbiting)
{
	// 상호작용 뷰 중에는 게임플레이 카메라 상태를 바꾸지 않는다(복귀 시 튐 방지).
	if (bInteractionViewActive || bOrbitRotationLocked || !bIsOrbiting || !CameraBoom)
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>();
	if (LookInput.IsNearlyZero())
	{
		return;
	}

	FRotator CurrentRotation = CameraBoom->GetRelativeRotation();
	CurrentRotation.Yaw += LookInput.X * OrbitYawSpeed;
	CurrentRotation.Pitch = FMath::Clamp(CurrentRotation.Pitch + (LookInput.Y * OrbitPitchSpeed), MinPitch, MaxPitch);
	CameraBoom->SetRelativeRotation(CurrentRotation);
}

////////////////////////////
//! \author HanUl
//! \brief 현재 카메라 각도를 유지한 채 Orbit 회전 입력 잠금 상태를 토글한다.
//! \param 없음
//! \return 토글 후 Orbit 회전 입력이 잠겨 있으면 true
bool UPlayerCameraComponent::ToggleOrbitRotationLock()
{
	bOrbitRotationLocked = !bOrbitRotationLocked;
	return bOrbitRotationLocked;
}

////////////////////////////
// Author : HanUl
// About : Orbit 시작 시 커서를 숨기고 현재 마우스 좌표를 저장한다.
// Parameters : PC - 로컬 PlayerController, OutSavedMouseX/Y - 저장 좌표, bOutHasSavedMousePos - 저장 성공 여부
// return : 없음
void UPlayerCameraComponent::HandleOrbitStartCursor(APlayerController* PC, float& OutSavedMouseX, float& OutSavedMouseY, bool& bOutHasSavedMousePos)
{
	if (!PC)
	{
		bOutHasSavedMousePos = false;
		return;
	}

	bOutHasSavedMousePos = PC->GetMousePosition(OutSavedMouseX, OutSavedMouseY);
	PC->bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	PC->SetInputMode(InputMode);
}

////////////////////////////
// Author : HanUl
// About : Orbit 종료 시 커서를 복원하고 필요하면 저장 좌표로 커서를 되돌린다.
// Parameters : PC - 로컬 PlayerController, SavedMouseX/Y - 저장 좌표, bHasSavedMousePos - 저장 성공 여부
// return : 없음
void UPlayerCameraComponent::HandleOrbitEndCursor(APlayerController* PC, float SavedMouseX, float SavedMouseY, bool bHasSavedMousePos)
{
	if (!PC)
	{
		return;
	}

	PC->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	if (bHasSavedMousePos)
	{
		PC->SetMouseLocation(FMath::RoundToInt(SavedMouseX), FMath::RoundToInt(SavedMouseY));
	}
}

////////////////////////////
//! \author HanUl
//! \brief 로컬 플레이어 사망 시 CameraBoom을 월드에 고정해 시체 낙하와 분리하고 Alive 복귀 시 다시 Character에 부착한다.
//! \param bDead true이면 사망 상태
//! \return 없음
void UPlayerCameraComponent::HandleOwnerLifeStateChanged(bool bDead)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !CameraBoom)
	{
		return;
	}

	if (bDead)
	{
		if (bCameraDetachedForDeath)
		{
			return;
		}

		if (bInteractionViewActive)
		{
			ExitInteractionView(0.0f);
		}

		CameraBoom->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		bCameraDetachedForDeath = true;
		return;
	}

	if (!bCameraDetachedForDeath)
	{
		return;
	}

	USceneComponent* CameraParent = GameplayCameraBoomParent.Get();
	if (!CameraParent)
	{
		return;
	}

	const FRotator DetachedWorldRotation = CameraBoom->GetComponentRotation();
	CameraBoom->AttachToComponent(
		CameraParent,
		FAttachmentTransformRules::KeepWorldTransform,
		GameplayCameraBoomSocketName);
	CameraBoom->SetRelativeLocation(GameplayCameraBoomRelativeLocation);
	CameraBoom->SetRelativeScale3D(GameplayCameraBoomRelativeScale);
	CameraBoom->SetWorldRotation(DetachedWorldRotation);
	bCameraDetachedForDeath = false;
}

////////////////////////////
//! \author HanUl
//! \brief 대상의 정면에 카메라를 배치해 대상만 바라보는 상호작용 뷰로 블렌드 전환한다.
//! \param FocusTarget 프레임에 담을 상호작용 대상 액터
//! \param Params 카메라 배치와 블렌드 파라미터
//! \return 없음
void UPlayerCameraComponent::EnterInteractionView(AActor* FocusTarget, const FInteractionViewParams& Params)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !FocusTarget || !World)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	const FVector TargetLocation = FocusTarget->GetActorLocation();

	// 대상의 로컬 전방/오른쪽 축을 기준으로 카메라를 배치해 플레이어의 접근 방향과 무관한 구도를 만든다.
	// SideOffset은 카메라와 시선축에 함께 적용해 정면 방향을 유지하면서 화면 구도만 좌우로 옮긴다.
	FVector TargetForward = FocusTarget->GetActorForwardVector().GetSafeNormal2D();
	if (TargetForward.IsNearlyZero())
	{
		TargetForward = FVector::ForwardVector;
	}
	const FVector TargetRight = FVector::CrossProduct(FVector::UpVector, TargetForward);

	const FVector CameraLocation = TargetLocation
		+ TargetForward * Params.FrontDistance
		- TargetRight * Params.SideOffset
		+ FVector::UpVector * Params.Height;

	const FVector LookAtLocation = TargetLocation
		- TargetRight * Params.SideOffset
		+ FVector::UpVector * Params.LookAtHeightOffset;

	const FRotator CameraRotation = (LookAtLocation - CameraLocation).Rotation();

	// 로컬 전용 카메라 액터를 재사용한다(복제되지 않음).
	if (!InteractionViewCamera)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InteractionViewCamera = World->SpawnActor<ACameraActor>(CameraLocation, CameraRotation, SpawnParams);
	}
	else
	{
		InteractionViewCamera->SetActorLocationAndRotation(CameraLocation, CameraRotation);
	}

	if (!InteractionViewCamera)
	{
		return;
	}

	if (UCameraComponent* ViewCameraComponent = InteractionViewCamera->GetCameraComponent())
	{
		ViewCameraComponent->SetFieldOfView(Params.Fov);
		ViewCameraComponent->bConstrainAspectRatio = false;
	}

	// 최초 진입 시에만 복귀 대상을 저장한다(연출 중 재진입해도 원래 카메라로 복귀).
	if (!bInteractionViewActive)
	{
		PreviousViewTarget = PC->GetViewTarget();
	}

	PC->SetViewTargetWithBlend(InteractionViewCamera, Params.BlendTime, VTBlend_EaseInOut, 2.0f);
	bInteractionViewActive = true;
}

////////////////////////////
//! \author HanUl
//! \brief 상호작용 뷰를 종료하고 진입 전 뷰 타겟으로 블렌드 복귀한다.
//! \param BlendTime 복귀 블렌드 시간(초)
//! \return 없음
void UPlayerCameraComponent::ExitInteractionView(float BlendTime)
{
	if (!bInteractionViewActive)
	{
		return;
	}
	bInteractionViewActive = false;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	AActor* ReturnTarget = PreviousViewTarget.IsValid() ? PreviousViewTarget.Get() : GetOwner();
	PreviousViewTarget = nullptr;

	PC->SetViewTargetWithBlend(ReturnTarget, FMath::Max(BlendTime, 0.0f), VTBlend_EaseInOut, 2.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 소유 Pawn의 로컬 PlayerController를 반환한다.
//! \return 로컬 PlayerController, 없으면 nullptr
APlayerController* UPlayerCameraComponent::GetOwningPlayerController() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
}
