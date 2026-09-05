// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPlayerController.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "Whisper/WhisperSpeechSubsystem.h"
#include "Widget/MyUIManagerSubsystem.h"
#include "GAS/MyPlayerState.h"
#include "Messenger/CPP_MessengerWidget.h"
#include "MyGameplayTags.h"
#include "Streaming/MyStreamingChatTypes.h"
#include "GameInstance/SubSystems/Cheat/CPP_CheatCommandSubsystem.h"
#include "GameInstance/SubSystems/NetSub/AccountSessionSubsystem.h"
#include "Item/MyInventoryComponent.h"
#include "Widget/God/MyGodPageWidget.h"
#include "Widget/Inventory/MyInventoryWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Player/Components/PlayerCameraComponent.h"
#include "Player/PlayerCharacterBase.h"
#include "Player/Spectator/PlayerSpectatorCameraActor.h"
#include "Widget/HUD/Nameplate/MyDamageFloatingLayerWidget.h"
#include "Widget/HUD/Nameplate/MyDamageNumberStackWidget.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버 이동(ClientTravel) 직전 클라이언트에서 사용 중인 캐릭터 ID를 GameInstance에 보관하는 함수
// 던전 종료 후 로비 복귀 시 캐릭터 재선택 없이 이 값으로 복원한다.
// PendingURL : 이동할 서버 주소
// TravelType : 이동 방식
// bIsSeamlessTravel : 심리스 트래블 여부
void AMyPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);

	const AMyPlayerState* MyPS = GetPlayerState<AMyPlayerState>();
	const int32 SelectedCharacterId = MyPS ? MyPS->GetSelectedCharacterId() : -1;
	if (SelectedCharacterId == -1)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UAccountSessionSubsystem* AccountSessionSubsystem = GameInstance ? GameInstance->GetSubsystem<UAccountSessionSubsystem>() : nullptr;
	if (AccountSessionSubsystem)
	{
		AccountSessionSubsystem->SetLastUsedCharacterId(SelectedCharacterId);
	}
}

void AMyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	// 해당 Local, Client의 PlayerController인지 확인
	if (IsLocalPlayerController())
	{
		// 메신저 위젯 생성 (MessengerWidgetClass가 지정된 PC에서만 생성)
		if (MessengerWidgetClass)
		{
			Messenger = CreateWidget<UCPP_MessengerWidget>(this, MessengerWidgetClass);
			if (Messenger)
			{
				Messenger->AddToViewport(MessengerViewportZOrder);
				// 채팅 패널과 동일: 루트는 클릭을 통과시키고 자식 위젯만 입력을 받는다
				Messenger->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UWhisperSpeechSubsystem* WhisperSubsystem = GameInstance->GetSubsystem<UWhisperSpeechSubsystem>())
			{
				WhisperSubsystem->OnTranscriptionCompleted.RemoveDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionCompleted);
				WhisperSubsystem->OnTranscriptionCompleted.AddDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionCompleted);
				WhisperSubsystem->OnTranscriptionFailed.RemoveDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionFailed);
				WhisperSubsystem->OnTranscriptionFailed.AddDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionFailed);
			}
		}

		if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (IMC_Default)
				{
					InputSystem->AddMappingContext(IMC_Default, 0);
				}
			}
		}

		InitializeDebugUIVerticalSlice();
	}
}

void AMyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpectatorFollowDelayTimerHandle);
	GetWorldTimerManager().ClearTimer(SpectatorTargetRefreshTimerHandle);
	RestoreSpectatorOrbitCursor();

	if (SpectatorCameraActor)
	{
		SpectatorCameraActor->Destroy();
		SpectatorCameraActor = nullptr;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWhisperSpeechSubsystem* WhisperSubsystem = GameInstance->GetSubsystem<UWhisperSpeechSubsystem>())
		{
			WhisperSubsystem->OnTranscriptionCompleted.RemoveDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionCompleted);
			WhisperSubsystem->OnTranscriptionFailed.RemoveDynamic(this, &AMyPlayerController::HandleWhisperTranscriptionFailed);
		}
	}

	// 메신저 위젯 정리 (로비↔던전 이동 시 채팅이 섞이지 않도록)
	// 단독 생성분(뷰포트에 직접 추가된 경우)만 제거하고, HUD에 임베드된 위젯은 HUD가 소유하므로 건드리지 않는다.
	if (Messenger)
	{
		if (Messenger->IsInViewport())
		{
			Messenger->RemoveFromParent();
		}
		Messenger = nullptr;
	}

	if (DamageFloatingLayer)
	{
		DamageFloatingLayer->ClearDamageNumbers();
		DamageFloatingLayer->RemoveFromParent();
		DamageFloatingLayer = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 몬스터 생명주기와 독립적으로 데미지 숫자를 재생할 로컬 UI 레이어를 생성하는 함수
void AMyPlayerController::InitializeDamageFloatingLayer()
{
	if (!IsLocalPlayerController() || DamageFloatingLayer)
	{
		return;
	}

	DamageFloatingLayer = CreateWidget<UMyDamageFloatingLayerWidget>(
		this,
		UMyDamageFloatingLayerWidget::StaticClass());
	if (!DamageFloatingLayer)
	{
		return;
	}

	// 전투 HUD보다 위에서 숫자를 표시하되 입력을 사용하지 않는 로컬 전용 레이어다.
	DamageFloatingLayer->AddToPlayerScreen(10);
	DamageFloatingLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

////////////////////////////
//! \author 준혁
//! \author 장효제
//! \brief 인벤토리(I), 신 페이지(G), 채팅 및 퀵슬롯 키 입력을 PlayerController에 바인딩한다.
//! \details 장효제: 신 페이지를 기존 인벤토리와 동일한 직접 키 바인딩 방식으로 연결했다.
void AMyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent && bEnableUITestVerticalSlice)
	{
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ThisClass::HandleDebugUITestMenuPressed);
	}

	// UI 테스트 슬라이스가 켜져 있으면 I 키를 디버그 메뉴가 사용하므로 인벤토리 바인딩은 건너뛴다
	if (InputComponent && !bEnableUITestVerticalSlice)
	{
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &ThisClass::HandleToggleInventoryPressed);
	}

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ThisClass::HandleToggleGodPagePressed);

		// 채팅창이 비활성(포커스 없음) 상태일 때 Enter로 채팅 입력을 활성화한다.
		// 채팅창이 포커스를 갖고 있으면 UI가 Enter를 소비하므로 이 바인딩은 호출되지 않는다.
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::HandleChatFocusKeyPressed);

		// 아이템 퀵슬롯 사용 키 (1~4). 채팅 입력 중에는 UI가 키를 소비하므로 호출되지 않는다.
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::HandleUseQuickSlot1Pressed);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::HandleUseQuickSlot2Pressed);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::HandleUseQuickSlot3Pressed);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ThisClass::HandleUseQuickSlot4Pressed);

		// 사망 관전 입력은 Pawn이 파괴된 뒤에도 유지되어야 하므로 PlayerController가 소유한다.
		FInputKeyBinding& SpectatorNextBinding = InputComponent->BindKey(
			EKeys::PageUp,
			IE_Pressed,
			this,
			&ThisClass::HandleSpectatorNextPressed);
		SpectatorNextBinding.bConsumeInput = false;

		FInputKeyBinding& SpectatorZoomInBinding = InputComponent->BindKey(
			EKeys::MouseScrollUp,
			IE_Pressed,
			this,
			&ThisClass::HandleSpectatorZoomIn);
		SpectatorZoomInBinding.bConsumeInput = false;

		FInputKeyBinding& SpectatorZoomOutBinding = InputComponent->BindKey(
			EKeys::MouseScrollDown,
			IE_Pressed,
			this,
			&ThisClass::HandleSpectatorZoomOut);
		SpectatorZoomOutBinding.bConsumeInput = false;

		FInputKeyBinding& SpectatorOrbitPressedBinding = InputComponent->BindKey(
			EKeys::RightMouseButton,
			IE_Pressed,
			this,
			&ThisClass::HandleSpectatorOrbitPressed);
		SpectatorOrbitPressedBinding.bConsumeInput = false;

		FInputKeyBinding& SpectatorOrbitReleasedBinding = InputComponent->BindKey(
			EKeys::RightMouseButton,
			IE_Released,
			this,
			&ThisClass::HandleSpectatorOrbitReleased);
		SpectatorOrbitReleasedBinding.bConsumeInput = false;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 로컬 PlayerState의 생명 상태와 관전 모드를 동기화하고 우클릭 Orbit 입력을 처리한다.
//! \param DeltaTime 프레임 델타 시간
//! \return 없음
void AMyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalPlayerController())
	{
		return;
	}

	SynchronizeDeathSpectatorState();

	if (!bDeathSpectating || !bSpectatorOrbiting || !SpectatorCameraActor)
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	if (FMath::IsNearlyZero(MouseDeltaX) && FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	SpectatorCameraActor->AddOrbitInput(
		MouseDeltaX,
		MouseDeltaY,
		SpectatorOrbitYawSpeed,
		SpectatorOrbitPitchSpeed,
		SpectatorMinPitch,
		SpectatorMaxPitch);
}

////////////////////////////
//! \author HanUl
//! \brief 복제된 로컬 PlayerState 생명 상태에 맞춰 관전 진입 또는 부활 복귀를 실행한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::SynchronizeDeathSpectatorState()
{
	const AMyPlayerState* MyPlayerState = GetPlayerState<AMyPlayerState>();
	if (!MyPlayerState)
	{
		return;
	}

	if (MyPlayerState->IsDead())
	{
		if (!bDeathSpectating)
		{
			EnterDeathSpectating();
		}
		return;
	}

	// 부활 PlayerState보다 새 Pawn 복제가 늦을 수 있으므로 복귀할 Pawn이 준비될 때까지 관전을 유지한다.
	if (bDeathSpectating && IsValid(GetPawn()))
	{
		ExitDeathSpectating();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 현재 플레이 카메라 위치에 Controller 소유 관전 카메라를 배치하고 사망 위치 고정 구간을 시작한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::EnterDeathSpectating()
{
	if (bDeathSpectating || !IsLocalPlayerController())
	{
		return;
	}

	APlayerSpectatorCameraActor* CameraActor = EnsureSpectatorCameraActor();
	if (!CameraActor)
	{
		return;
	}

	FVector CameraLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	FRotator CameraRotation = GetControlRotation();
	if (PlayerCameraManager)
	{
		CameraLocation = PlayerCameraManager->GetCameraLocation();
		CameraRotation = PlayerCameraManager->GetCameraRotation();
	}

	DeathSpectatorOrigin = GetPawn() ? GetPawn()->GetActorLocation() : CameraLocation;
	SpectatorTargets.Reset();
	CurrentSpectatorTarget.Reset();
	CameraActor->ClearFollowTarget();
	CameraActor->ConfigureFollow(
		FVector(0.0f, 0.0f, SpectatorTargetHeightOffset),
		SpectatorFollowInterpSpeed);
	CameraActor->InitializeFromView(
		CameraLocation,
		CameraRotation,
		SpectatorDefaultArmLength,
		SpectatorDefaultFieldOfView);
	CameraActor->SetSpectatorActive(true);

	bDeathSpectating = true;
	bSpectatorTargetSelectionEnabled = false;
	SetViewTarget(CameraActor);

	GetWorldTimerManager().ClearTimer(SpectatorFollowDelayTimerHandle);
	GetWorldTimerManager().ClearTimer(SpectatorTargetRefreshTimerHandle);
	if (DeathSpectatorHoldDuration <= 0.0f)
	{
		StartFollowingSpectatorTarget();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			SpectatorFollowDelayTimerHandle,
			this,
			&ThisClass::StartFollowingSpectatorTarget,
			DeathSpectatorHoldDuration,
			false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 부활한 Pawn으로 카메라를 블렌드 복귀시키고 로컬 관전 상태를 정리한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::ExitDeathSpectating()
{
	if (!bDeathSpectating)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SpectatorFollowDelayTimerHandle);
	GetWorldTimerManager().ClearTimer(SpectatorTargetRefreshTimerHandle);
	RestoreSpectatorOrbitCursor();

	if (SpectatorCameraActor)
	{
		SpectatorCameraActor->ClearFollowTarget();
		SpectatorCameraActor->SetSpectatorActive(false);
	}

	if (APawn* ReturnPawn = GetPawn())
	{
		SetViewTargetWithBlend(
			ReturnPawn,
			FMath::Max(SpectatorReturnBlendTime, 0.0f),
			VTBlend_EaseInOut,
			2.0f,
			true);
	}

	SpectatorTargets.Reset();
	CurrentSpectatorTarget.Reset();
	bSpectatorTargetSelectionEnabled = false;
	bDeathSpectating = false;
}

////////////////////////////
//! \author HanUl
//! \brief 사망 위치 고정 시간이 끝난 뒤 생존 팀원을 선택하고 주기적 유효성 검사를 시작한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::StartFollowingSpectatorTarget()
{
	if (!bDeathSpectating)
	{
		return;
	}

	bSpectatorTargetSelectionEnabled = true;
	RefreshSpectatorTargets();

	const float RefreshInterval = FMath::Max(SpectatorTargetRefreshInterval, 0.05f);
	GetWorldTimerManager().SetTimer(
		SpectatorTargetRefreshTimerHandle,
		this,
		&ThisClass::RefreshSpectatorTargets,
		RefreshInterval,
		true);
}

////////////////////////////
//! \author HanUl
//! \brief GameState의 PlayerArray에서 살아 있는 팀원만 수집하고 현재 대상이 사라졌으면 자동 교체한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::RefreshSpectatorTargets()
{
	if (!bDeathSpectating)
	{
		return;
	}

	SpectatorTargets.Reset();

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GameState)
	{
		for (APlayerState* ListedPlayerState : GameState->PlayerArray)
		{
			AMyPlayerState* CandidatePlayerState = Cast<AMyPlayerState>(ListedPlayerState);
			if (IsValidSpectatorTarget(CandidatePlayerState))
			{
				SpectatorTargets.Add(CandidatePlayerState);
			}
		}
	}

	SpectatorTargets.Sort([](
		const TWeakObjectPtr<AMyPlayerState>& Left,
		const TWeakObjectPtr<AMyPlayerState>& Right)
	{
		const AMyPlayerState* LeftPlayerState = Left.Get();
		const AMyPlayerState* RightPlayerState = Right.Get();
		if (!LeftPlayerState || !RightPlayerState)
		{
			return LeftPlayerState != nullptr;
		}

		if (LeftPlayerState->GetUserIndex() != RightPlayerState->GetUserIndex())
		{
			return LeftPlayerState->GetUserIndex() < RightPlayerState->GetUserIndex();
		}

		return LeftPlayerState->GetPlayerId() < RightPlayerState->GetPlayerId();
	});

	if (IsValidSpectatorTarget(CurrentSpectatorTarget.Get()))
	{
		SetSpectatorTarget(CurrentSpectatorTarget.Get());
		return;
	}

	AMyPlayerState* ClosestPlayerState = nullptr;
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<AMyPlayerState>& Candidate : SpectatorTargets)
	{
		APawn* CandidatePawn = ResolveSpectatorPawn(Candidate.Get());
		if (!CandidatePawn)
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(
			CandidatePawn->GetActorLocation(),
			DeathSpectatorOrigin);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestPlayerState = Candidate.Get();
		}
	}

	SetSpectatorTarget(ClosestPlayerState);
}

////////////////////////////
//! \author HanUl
//! \brief 현재 생존 관전 대상 배열에서 다음 팀원으로 순환한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::CycleSpectatorTarget()
{
	if (!bDeathSpectating || !bSpectatorTargetSelectionEnabled)
	{
		return;
	}

	RefreshSpectatorTargets();
	if (SpectatorTargets.IsEmpty())
	{
		return;
	}

	int32 CurrentIndex = SpectatorTargets.IndexOfByPredicate(
		[this](const TWeakObjectPtr<AMyPlayerState>& Candidate)
		{
			return Candidate == CurrentSpectatorTarget;
		});
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}
	else
	{
		CurrentIndex = (CurrentIndex + 1) % SpectatorTargets.Num();
	}

	SetSpectatorTarget(SpectatorTargets[CurrentIndex].Get());
}

////////////////////////////
//! \author HanUl
//! \brief 현재 관전 PlayerState와 실제 추적 Pawn을 함께 변경하며 nullptr이면 마지막 위치에 고정한다.
//! \param NewTargetPlayerState 새 관전 대상 PlayerState
//! \return 없음
void AMyPlayerController::SetSpectatorTarget(AMyPlayerState* NewTargetPlayerState)
{
	APawn* TargetPawn = ResolveSpectatorPawn(NewTargetPlayerState);
	if (!SpectatorCameraActor || !TargetPawn)
	{
		CurrentSpectatorTarget.Reset();
		if (SpectatorCameraActor)
		{
			SpectatorCameraActor->ClearFollowTarget();
		}
		return;
	}

	CurrentSpectatorTarget = NewTargetPlayerState;
	SpectatorCameraActor->SetFollowTarget(TargetPawn);
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState가 자기 자신이 아닌 살아 있고 접속 중인 관전 가능 팀원인지 검사한다.
//! \param CandidatePlayerState 검사할 PlayerState
//! \return 관전 대상으로 사용할 수 있으면 true
bool AMyPlayerController::IsValidSpectatorTarget(const AMyPlayerState* CandidatePlayerState) const
{
	if (!IsValid(CandidatePlayerState)
		|| CandidatePlayerState == GetPlayerState<AMyPlayerState>()
		|| CandidatePlayerState->IsInactive()
		|| !CandidatePlayerState->IsAuthVerified()
		|| !CandidatePlayerState->IsAlive())
	{
		return false;
	}

	const APlayerCharacterBase* CandidateCharacter = Cast<APlayerCharacterBase>(
		ResolveSpectatorPawn(CandidatePlayerState));
	return CandidateCharacter && !CandidateCharacter->IsReconnectInactive();
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState에 현재 연결된 유효한 Pawn을 반환한다.
//! \param CandidatePlayerState Pawn을 조회할 PlayerState
//! \return 유효한 Pawn, 없으면 nullptr
APawn* AMyPlayerController::ResolveSpectatorPawn(const AMyPlayerState* CandidatePlayerState) const
{
	APawn* CandidatePawn = CandidatePlayerState ? CandidatePlayerState->GetPawn() : nullptr;
	return IsValid(CandidatePawn) ? CandidatePawn : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief PlayerController 수명에 귀속된 로컬 비복제 관전 카메라 Actor를 지연 생성한다.
//! \param 없음
//! \return 생성 또는 재사용한 관전 카메라 Actor
APlayerSpectatorCameraActor* AMyPlayerController::EnsureSpectatorCameraActor()
{
	if (IsValid(SpectatorCameraActor))
	{
		return SpectatorCameraActor;
	}

	UWorld* World = GetWorld();
	if (!World || !IsLocalPlayerController())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpectatorCameraActor = World->SpawnActor<APlayerSpectatorCameraActor>(
		APlayerSpectatorCameraActor::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	return SpectatorCameraActor;
}

////////////////////////////
//! \author HanUl
//! \brief Page Up 입력으로 다음 생존 팀원을 관전한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::HandleSpectatorNextPressed()
{
	CycleSpectatorTarget();
}

////////////////////////////
//! \author HanUl
//! \brief 마우스 휠 위 입력으로 관전 카메라를 확대한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::HandleSpectatorZoomIn()
{
	AdjustSpectatorZoom(1.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 마우스 휠 아래 입력으로 관전 카메라를 축소한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::HandleSpectatorZoomOut()
{
	AdjustSpectatorZoom(-1.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 관전 중인 경우에만 휠 입력을 관전 카메라 Zoom 설정으로 전달한다.
//! \param ZoomInput 휠 위는 양수, 아래는 음수
//! \return 없음
void AMyPlayerController::AdjustSpectatorZoom(float ZoomInput)
{
	if (!bDeathSpectating || !SpectatorCameraActor)
	{
		return;
	}

	SpectatorCameraActor->AdjustZoom(
		ZoomInput,
		SpectatorArmLengthStep,
		SpectatorMinArmLength,
		SpectatorMaxArmLength,
		SpectatorFieldOfViewStep,
		SpectatorMinFieldOfView,
		SpectatorMaxFieldOfView);
}

////////////////////////////
//! \author HanUl
//! \brief 사망 관전 중 우클릭을 누르면 커서를 숨기고 Orbit 입력 캡처를 시작한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::HandleSpectatorOrbitPressed()
{
	if (!bDeathSpectating || bSpectatorOrbiting)
	{
		return;
	}

	bHasSpectatorSavedMousePosition = GetMousePosition(
		SpectatorSavedMouseX,
		SpectatorSavedMouseY);
	bSpectatorOrbiting = true;
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);
}

////////////////////////////
//! \author HanUl
//! \brief 사망 관전 중 우클릭을 놓으면 Orbit을 종료하고 기존 커서 위치를 복원한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::HandleSpectatorOrbitReleased()
{
	if (!bDeathSpectating)
	{
		return;
	}

	RestoreSpectatorOrbitCursor();
}

////////////////////////////
//! \author HanUl
//! \brief 관전 Orbit 종료 시 GameAndUI 입력 모드와 저장된 커서 위치를 복원한다.
//! \param 없음
//! \return 없음
void AMyPlayerController::RestoreSpectatorOrbitCursor()
{
	if (!bSpectatorOrbiting)
	{
		return;
	}

	bSpectatorOrbiting = false;
	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (bHasSpectatorSavedMousePosition)
	{
		SetMouseLocation(
			FMath::RoundToInt(SpectatorSavedMouseX),
			FMath::RoundToInt(SpectatorSavedMouseY));
	}
	bHasSpectatorSavedMousePosition = false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅창 비활성 상태에서 Enter 입력 시 메신저 입력창에 포커스를 준다.
void AMyPlayerController::HandleChatFocusKeyPressed()
{
	if (Messenger)
	{
		Messenger->FocusChatInput();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 창을 토글한다. 열려 있으면 퇴장 슬라이드를 요청하고, 없으면 Menu 레이어에 푸시한다.
void AMyPlayerController::ToggleInventory()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	// 이미 열려 있으면 닫는다.
	// 닫기 버튼으로 닫힌 위젯은 GC 전까지 포인터가 유효하므로 IsActivated까지 확인해야 한다.
	if (ActiveInventoryWidget.IsValid() && ActiveInventoryWidget->IsActivated())
	{
		ActiveInventoryWidget->RequestCloseInventory();
		return;
	}
	ActiveInventoryWidget.Reset();

	if (!InventoryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] ToggleInventory failed - InventoryWidgetClass is not set."));
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		return;
	}

	if (!UIManager->GetPrimaryLayout() && !UIManager->EnsurePrimaryLayout(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] ToggleInventory failed - PrimaryLayout is missing."));
		return;
	}

	ActiveInventoryWidget = Cast<UMyInventoryWidget>(UIManager->PushMenu(InventoryWidgetClass));
	if (!ActiveInventoryWidget.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] ToggleInventory failed - PushMenu returned no Inventory widget."));
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 창을 다시 생성해도 유지할 로컬 아이템 표시 순서를 반환하는 함수
// 반환값 : 현재 저장된 아이템 ID 표시 순서
const TArray<FName>& AMyPlayerController::GetInventoryDisplayOrder() const
{
	return InventoryDisplayOrder;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인벤토리 창을 다시 생성해도 유지할 로컬 아이템 표시 순서를 저장하는 함수
// InDisplayOrder : 새로 저장할 아이템 ID 표시 순서
void AMyPlayerController::SetInventoryDisplayOrder(const TArray<FName>& InDisplayOrder)
{
	InventoryDisplayOrder = InDisplayOrder;
}

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 토글 키(I) 입력 처리 함수.
void AMyPlayerController::HandleToggleInventoryPressed()
{
	ToggleInventory();
}

//! \author 장효제
//! \brief 신 페이지를 토글한다. 열려 있으면 닫고, 없으면 Menu 레이어에 푸시한다.
void AMyPlayerController::ToggleGodPage()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (ActiveGodPageWidget.IsValid() && ActiveGodPageWidget->IsActivated())
	{
		ActiveGodPageWidget->DeactivateWidget();
		ActiveGodPageWidget.Reset();
		return;
	}
	ActiveGodPageWidget.Reset();

	if (!GodPageWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] ToggleGodPage failed - GodPageWidgetClass is not set."));
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] ToggleGodPage failed - UIManager is missing."));
		return;
	}

	if (!UIManager->GetPrimaryLayout() && !UIManager->EnsurePrimaryLayout(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] ToggleGodPage failed - PrimaryLayout is missing."));
		return;
	}

	ActiveGodPageWidget = Cast<UMyGodPageWidget>(UIManager->PushMenu(GodPageWidgetClass));
	if (!ActiveGodPageWidget.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] ToggleGodPage failed - PushMenu returned no God Page widget."));
	}
}

//! \author 장효제
//! \brief 신 페이지를 열고 지정된 신을 선택한다. 이미 열린 페이지는 닫거나 다시 푸시하지 않는다.
void AMyPlayerController::OpenGodPage(FGameplayTag GodTag)
{
	if (!IsLocalPlayerController() || !GodTag.IsValid())
	{
		return;
	}

	if (ActiveGodPageWidget.IsValid() && ActiveGodPageWidget->IsActivated())
	{
		ActiveGodPageWidget->SelectGod(GodTag);
		return;
	}
	ActiveGodPageWidget.Reset();

	if (!GodPageWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] OpenGodPage failed - GodPageWidgetClass is not set."));
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] OpenGodPage failed - UIManager is missing."));
		return;
	}

	if (!UIManager->GetPrimaryLayout() && !UIManager->EnsurePrimaryLayout(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] OpenGodPage failed - PrimaryLayout is missing."));
		return;
	}

	ActiveGodPageWidget = Cast<UMyGodPageWidget>(UIManager->PushMenu(GodPageWidgetClass));
	if (!ActiveGodPageWidget.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GodPage] OpenGodPage failed - PushMenu returned no God Page widget."));
		return;
	}

	ActiveGodPageWidget->SelectGod(GodTag);
}

////////////////////////////
//! \author 장효제
//! \brief 현재 PlayerController 수명 동안 마지막으로 조회한 신을 반환한다.
//! \return 마지막 조회 신 태그
FGameplayTag AMyPlayerController::GetLastViewedGodTag() const
{
	return LastViewedGodTag;
}

////////////////////////////
//! \author 장효제
//! \brief 현재 PlayerController 수명 동안 마지막으로 조회한 신을 저장한다.
//! \param GodTag 저장할 신 태그
void AMyPlayerController::SetLastViewedGodTag(FGameplayTag GodTag)
{
	if (GodTag.IsValid())
	{
		LastViewedGodTag = GodTag;
	}
}

//! \author 장효제
//! \brief G 키 입력을 신 페이지 토글 함수로 전달한다.
void AMyPlayerController::HandleToggleGodPagePressed()
{
	ToggleGodPage();
}

////////////////////////////
//! \author 준혁
//! \brief 퀵슬롯에 등록된 아이템을 사용한다. 사용 검증/차감은 인벤토리 컴포넌트의 Server RPC가 처리한다.
//! \param SlotIndex 사용할 퀵슬롯 인덱스 (0부터)
void AMyPlayerController::UseQuickSlotByIndex(int32 SlotIndex)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const AMyPlayerState* MyPS = GetPlayerState<AMyPlayerState>();
	UMyInventoryComponent* InventoryComponent = MyPS ? MyPS->GetInventoryComponent() : nullptr;
	if (InventoryComponent)
	{
		InventoryComponent->UseQuickSlot(SlotIndex);
	}
}

void AMyPlayerController::HandleUseQuickSlot1Pressed() { UseQuickSlotByIndex(0); }
void AMyPlayerController::HandleUseQuickSlot2Pressed() { UseQuickSlotByIndex(1); }
void AMyPlayerController::HandleUseQuickSlot3Pressed() { UseQuickSlotByIndex(2); }
void AMyPlayerController::HandleUseQuickSlot4Pressed() { UseQuickSlotByIndex(3); }

void AMyPlayerController::InitializeDebugUIVerticalSlice()
{
	if (!bEnableUITestVerticalSlice || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		return;
	}

	if (!UIManager->EnsurePrimaryLayoutUsingClass(this, TestPrimaryLayoutClass))
	{
		return;
	}

	if (TestHUDClass)
	{
		UIManager->PushHUD(TestHUDClass);
	}
}

void AMyPlayerController::HandleDebugUITestMenuPressed()
{
	if (!bEnableUITestVerticalSlice || !IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;

	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("Debug UI vertical slice failed - UIManager is null."));
		return;
	}

	if (!UIManager->GetPrimaryLayout())
	{
		UE_LOG(LogTemp, Warning, TEXT("Debug UI vertical slice - PrimaryLayout is missing. Recreating."));
		if (!UIManager->EnsurePrimaryLayoutUsingClass(this, TestPrimaryLayoutClass))
		{
			UE_LOG(LogTemp, Warning, TEXT("Debug UI vertical slice - PrimaryLayout recreation failed. Aborting menu push."));
			return;
		}
	}

	if (TestMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Debug UI vertical slice - PushMenu."));
		UIManager->PushMenu(TestMenuClass);
	}
}

////////////////////////////
//! \brief 마우스 화면 좌표를 지정된 월드 Z 평면에 투영해 월드 위치를 계산한다.
//! \param PlaneZ 투영할 월드 Z 높이
//! \param OutLocation 계산된 월드 위치
//! \return 계산에 성공하면 true
bool AMyPlayerController::TryGetMouseWorldLocationOnPlane(float PlaneZ, FVector& OutLocation) const
{
	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	const bool bDeprojected = DeprojectMousePositionToWorld(WorldLocation, WorldDirection);
	if (!bDeprojected)
	{
		return false;
	}

	if (FMath::IsNearlyZero(WorldDirection.Z))
	{
		return false;
	}

	const float DistanceAlongRay = (PlaneZ - WorldLocation.Z) / WorldDirection.Z;
	if (DistanceAlongRay < 0.0f)
	{
		return false;
	}

	OutLocation = WorldLocation + (WorldDirection * DistanceAlongRay);
	return true;
}

////////////////////////////
//! \brief 마우스 화면 좌표에서 월드 방향으로 LineTrace를 수행해 지형 또는 Actor HitResult를 계산한다.
//! \param TraceChannel 사용할 CollisionChannel
//! \param OutHitResult Trace 결과
//! \param TraceDistance Trace 거리
//! \param bTraceComplex 복합 Collision 사용 여부
//! \return 유효한 Hit가 있으면 true
bool AMyPlayerController::TryGetMouseWorldHitByChannel(
	TEnumAsByte<ECollisionChannel> TraceChannel,
	FHitResult& OutHitResult,
	float TraceDistance,
	bool bTraceComplex
) const
{
	OutHitResult = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	const bool bDeprojected = DeprojectMousePositionToWorld(WorldLocation, WorldDirection);
	if (!bDeprojected)
	{
		return false;
	}

	const float SafeTraceDistance = FMath::Max(TraceDistance, 1.0f);
	const FVector TraceDirection = WorldDirection.GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector TraceEnd = WorldLocation + (TraceDirection * SafeTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MyPlayerMouseWorldHit), bTraceComplex);
	if (APawn* ControlledPawn = GetPawn())
	{
		QueryParams.AddIgnoredActor(ControlledPawn);
	}

	const bool bHit = World->LineTraceSingleByChannel(
		OutHitResult,
		WorldLocation,
		TraceEnd,
		TraceChannel.GetValue(),
		QueryParams
	);

	const bool bBlockingHit = bHit && OutHitResult.bBlockingHit;


	//UE_LOG(LogTemp, Warning, TEXT("[경고] 디버그 구를 그리다.") );

	//const FColor TraceColor = bBlockingHit ? FColor::Green : FColor::Red;
	//DrawDebugLine(World, WorldLocation, TraceEnd, TraceColor, false, 0.05f, 0, 1.0f);
	//if (bBlockingHit)
	//{
	//	DrawDebugSphere(World, OutHitResult.ImpactPoint, 16.0f, 12, FColor::Green, false, 0.05f);
	//}


	return bBlockingHit;
}

void AMyPlayerController::HandleWhisperTranscriptionCompleted(const FWhisperTranscriptionResult& Result)
{
	if (!IsLocalPlayerController() || !Messenger)
	{
		return;
	}

	const FString TranscribedText = Result.Text.TrimStartAndEnd();
	if (TranscribedText.IsEmpty())
	{
		return;
	}

	const FDateTime LocalNow = FDateTime::Now();
	FMessengerMessage Message;
	Message.Channel = EMessengerChannel::All;
	Message.TimeText = FString::Printf(TEXT("%02d:%02d"), LocalNow.GetHour(), LocalNow.GetMinute());
	Message.Username = TEXT("VOICE");
	Message.Body = TranscribedText;
	Messenger->ReceiveMessengerMessage(Message);
}

void AMyPlayerController::HandleWhisperTranscriptionFailed(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("Whisper transcription failed: %s"), *ErrorMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 클라에서 입력한 메신저 메시지를 서버로 전송 요청한다.
// '/'로 시작하면 채팅 대신 치트 명령으로 처리한다.
// Channel : 보낼 채널 (전체/파티)
// RawText : 입력한 원본 메시지
void AMyPlayerController::SendMessengerMessage(EMessengerChannel Channel, const FString& RawText)
{
	const FString Trimmed = RawText.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	if (Trimmed.StartsWith(TEXT("/")))
	{
		ExecuteChatCheatCommand(Trimmed);
		return;
	}

	ServerSendMessengerMessage(Channel, Trimmed);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// '/'로 시작하는 치트 명령을 처리한다. 로컬 클라에서 호출한다.
// 클라 전용 명령(bRunOnClient)은 서버로 보내지 않고 즉시 실행하고, 그 외에는 서버로 보낸다.
// CommandLine : "/heal 50" 형태의 명령 문자열
void AMyPlayerController::ExecuteChatCheatCommand(const FString& CommandLine)
{
	const FString Trimmed = CommandLine.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	// 첫 토큰(명령 이름)을 뽑아 클라 전용 명령인지 확인한다.
	FString CommandName = Trimmed;
	if (CommandName.StartsWith(TEXT("/")))
	{
		CommandName.RightChopInline(1);
	}
	int32 SpaceIndex = INDEX_NONE;
	if (CommandName.FindChar(TEXT(' '), SpaceIndex))
	{
		CommandName.LeftInline(SpaceIndex);
	}

	UCPP_CheatCommandSubsystem* CheatSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UCPP_CheatCommandSubsystem>() : nullptr;
	FCheatCommandInfo CommandInfo;
	if (CheatSubsystem && CheatSubsystem->FindCommandInfo(CommandName, CommandInfo) && CommandInfo.bRunOnClient)
	{
		const FString Result = CheatSubsystem->AreChatCheatsAllowed()
			? CheatSubsystem->ExecuteCommand(this, Trimmed)
			: TEXT("이 서버에서는 치트가 비활성화되어 있습니다.");
		ClientReceiveCheatResult(Result);
		return;
	}

	ServerExecuteCheatCommand(Trimmed);
}

////////////////////////////
//! \author HanUl
//! \brief 이 로컬 플레이어 화면의 스킬 판정 DebugLine 표시 상태를 설정한다.
//! \param bEnabled true면 이후 생성되는 DebugLine을 표시하고, false면 표시하지 않는다.
//! \return 없음
void AMyPlayerController::SetSkillDebugLineEnabled(bool bEnabled)
{
	bSkillDebugLineEnabled = bEnabled;
}

////////////////////////////
//! \author HanUl
//! \brief 이 로컬 플레이어 화면의 스킬 판정 DebugLine 표시 상태를 반환한다.
//! \param 없음
//! \return DebugLine 표시가 활성화되어 있으면 true
bool AMyPlayerController::IsSkillDebugLineEnabled() const
{
	return bSkillDebugLineEnabled;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 치트 명령을 검증/실행하고 결과를 요청자에게 회신한다. (Server, Reliable)
// CommandLine : 클라가 보낸 명령 문자열
void AMyPlayerController::ServerExecuteCheatCommand_Implementation(const FString& CommandLine)
{
	UWorld* World = GetWorld();
	UCPP_CheatCommandSubsystem* CheatSubsystem = World ? World->GetSubsystem<UCPP_CheatCommandSubsystem>() : nullptr;
	if (!CheatSubsystem)
	{
		ClientReceiveCheatResult(TEXT("치트 시스템을 사용할 수 없습니다."));
		return;
	}

	if (!CheatSubsystem->AreChatCheatsAllowed())
	{
		ClientReceiveCheatResult(TEXT("이 서버에서는 치트가 비활성화되어 있습니다."));
		return;
	}

	const FString Result = CheatSubsystem->ExecuteCommand(this, CommandLine);

	// 누가 어떤 치트를 실행했는지 서버 로그로 남긴다.
	const AMyPlayerState* MyPS = GetPlayerState<AMyPlayerState>();
	UE_LOG(LogTemp, Warning, TEXT("[Cheat] %s : \"%s\" -> %s"),
		MyPS ? *MyPS->GetUsername() : TEXT("Unknown"),
		*CommandLine,
		*Result);

	ClientReceiveCheatResult(Result);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버가 보낸 치트 실행 결과를 채팅창에 [CHEAT] 메시지로 표시한다. (Client, Reliable)
// ResultText : 실행 결과 문자열 (여러 줄이면 줄마다 한 블록씩 표시)
////////////////////////////
//! \author 준혁
//! \brief 서버가 보낸 데미지 확정 알림을 받아, 몬스터와 독립된 로컬 UI 레이어에 데미지 숫자를 띄운다.
//!        릴레번시 밖 등으로 액터를 resolve하지 못했거나 네임플레이트가 없으면 연출을 포기한다(전투 상태에는 영향 없음).
//! \param DamagedActor 데미지를 받은 액터 (네임플레이트를 가진 몬스터)
//! \param DamageAmount 서버가 확정한 최종 데미지
//! \param bCriticalHit 치명타 여부
//! \param bDamageOverTime DOT 피해 여부
//! \param bKilledTarget 이번 데미지로 대상을 처치했는지 여부
//! \param CameraFeedbackTag 공격자 본인 카메라에 재생할 Basic/Skill/Ultimate 적중 피드백 태그
//! \return 없음
void AMyPlayerController::ClientShowDamageNumber_Implementation(
	AActor* DamagedActor,
	float DamageAmount,
	bool bCriticalHit,
	bool bDamageOverTime,
	bool bKilledTarget,
	FGameplayTag CameraFeedbackTag
)
{
	// 대상 액터가 릴레번시 밖이거나 이미 파괴되어도 서버가 확정한 공격자 적중 피드백은 재생한다.
	if (APawn* ControlledPawn = GetPawn())
	{
		if (UPlayerCameraComponent* CameraComponent = ControlledPawn->FindComponentByClass<UPlayerCameraComponent>())
		{
			CameraComponent->PlayAttackerHitCameraFeedback(CameraFeedbackTag);
		}
	}

	if (!IsValid(DamagedActor))
	{
		return;
	}

	TInlineComponentArray<UWidgetComponent*> WidgetComponents(DamagedActor);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

        UUserWidget* HostWidget = WidgetComponent->GetWidget();
        if (!HostWidget)
        {
            continue;
        }

        if (UMyDamageNumberStackWidget* DamageNumberStack = Cast<UMyDamageNumberStackWidget>(
            HostWidget->GetWidgetFromName(TEXT("DamageNumberStack"))))
        {
			const EDamageNumberDisplayType DamageType = bDamageOverTime
				? EDamageNumberDisplayType::Dot
				: (bCriticalHit
					? EDamageNumberDisplayType::Critical
					: EDamageNumberDisplayType::Normal);

			if (!DamageFloatingLayer)
			{
				InitializeDamageFloatingLayer();
			}

			if (DamageFloatingLayer)
			{
				DamageFloatingLayer->PushDamageNumber(
					DamagedActor,
					WidgetComponent,
					DamageAmount,
					DamageType,
					bKilledTarget);
			}
			else
			{
				// 독립 레이어 생성에 실패해도 기존 네임플레이트 표시 기능은 유지한다.
				DamageNumberStack->PushTypedDamage(DamageAmount, DamageType);
			}

            return;
        }
	}
}

void AMyPlayerController::ClientReceiveCheatResult_Implementation(const FString& ResultText)
{
	if (!Messenger)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] %s"), *ResultText);
		return;
	}

	const FDateTime LocalNow = FDateTime::Now();
	const FString TimeText = FString::Printf(TEXT("%02d:%02d"), LocalNow.GetHour(), LocalNow.GetMinute());

	TArray<FString> Lines;
	ResultText.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FMessengerMessage Message;
		Message.Channel = EMessengerChannel::All;
		Message.TimeText = TimeText;
		Message.Username = TEXT("CHEAT");
		Message.Body = Line;
		Messenger->ReceiveMessengerMessage(Message);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 메신저 위젯을 PC에 등록한다. HUD 임베드/단독 생성 위젯 모두 이 경로로 PC와 연결된다.
// InMessenger : 등록할 메신저 위젯
void AMyPlayerController::SetMessengerWidget(UCPP_MessengerWidget* InMessenger)
{
	Messenger = InMessenger;
}

////////////////////////////
//! \author 장효제
//! \brief 로컬 신 채팅과 일반 메신저 UI에 내용 없는 테스트 블록을 추가한다.
//! \param Count 각 채팅 UI에 추가할 블록 수
void AMyPlayerController::MakeTestBubbles(int32 Count)
{
#if !UE_BUILD_SHIPPING
	const int32 SafeCount = FMath::Clamp(Count, 0, 100);
	if (UGameplayMessageSubsystem::HasInstance(this))
	{
		const FMyStreamingChatMessageData EmptyBubbleData;
		for (int32 Index = 0; Index < SafeCount; ++Index)
		{
			UGameplayMessageSubsystem::Get(this).BroadcastMessage(
				MyGameplayTags::Streaming_Channel_UI_Chat,
				EmptyBubbleData);
		}
	}

	if (Messenger)
	{
		Messenger->AddEmptyTestMessages(SafeCount);
	}
#endif
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 클라가 보낸 메시지를 서버에서 검증/포맷 후 대상에게 전달한다. (Server, Reliable)
// Channel : 보낼 채널 (전체/파티)
// RawText : 클라가 보낸 원본 메시지
void AMyPlayerController::ServerSendMessengerMessage_Implementation(EMessengerChannel Channel, const FString& RawText)
{
	AMyPlayerState* MyPS = GetPlayerState<AMyPlayerState>();
	if (!MyPS)
	{
		return;
	}

	// 인증 전이면 username이 비어 있으므로 전송을 막는다.
	if (MyPS->GetUsername().IsEmpty())
	{
		return;
	}

	// 본문 정리 및 길이 제한
	FString Body = RawText.TrimStartAndEnd();
	if (Body.IsEmpty())
	{
		return;
	}
	if (Body.Len() > MaxMessengerMessageLength)
	{
		Body = Body.Left(MaxMessengerMessageLength);
	}

	// 도배 방지 쿨다운(서버 시간 기준)
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	if (MessengerSendCooldownSeconds > 0.0f && (Now - LastMessengerSendServerTime) < MessengerSendCooldownSeconds)
	{
		return;
	}
	LastMessengerSendServerTime = Now;

	// 메시지 생성 및 채널별 대상 선별
	const FMessengerMessage Message = MakeMessengerMessage(Channel, Body);

	TArray<AMyPlayerController*> Recipients;
	if (Channel == EMessengerChannel::Party)
	{
		GetMessengerPartyRecipients(Recipients);
	}
	else
	{
		GetMessengerAllRecipients(Recipients);
	}

	// 대상 클라마다 Client RPC로 전달
	for (AMyPlayerController* Recipient : Recipients)
	{
		if (Recipient)
		{
			Recipient->ClientReceiveMessengerMessage(Message);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버가 보낸 메시지를 수신해 화면 출력 단계로 넘긴다. (Client, Reliable)
// Message : 서버가 완성한 메시지 데이터
void AMyPlayerController::ClientReceiveMessengerMessage_Implementation(const FMessengerMessage& Message)
{
	// 메신저 위젯으로 전달해 화면에 표시한다.
	if (Messenger)
	{
		Messenger->ReceiveMessengerMessage(Message);
	}

	// 수신 로그(필요 시 확인용) 및 BP 확장 지점
	UE_LOG(LogTemp, Verbose, TEXT("[Messenger] %s"), *Message.ToDisplayString());
	OnMessengerMessageReceived(Message);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 전체 채널 수신 대상(현재 서버의 모든 PlayerController)을 채운다.
// OutRecipients : 수신 대상 컨트롤러 목록(출력)
void AMyPlayerController::GetMessengerAllRecipients(TArray<AMyPlayerController*>& OutRecipients) const
{
	OutRecipients.Reset();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMyPlayerController* PC = Cast<AMyPlayerController>(It->Get()))
		{
			OutRecipients.Add(PC);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 보낸 사람 정보를 바탕으로 전달용 메시지 데이터를 생성한다.
// Channel : 채널
// Body : 정리된 본문
// 반환값 : 시각/이름/캐릭터명이 채워진 FMessengerMessage
FMessengerMessage AMyPlayerController::MakeMessengerMessage(EMessengerChannel Channel, const FString& Body) const
{
	FMessengerMessage Message;
	Message.Channel = Channel;
	Message.Body = Body;

	// 시각: 서버 기준 "hh:mm"
	const FDateTime ServerNow = FDateTime::Now();
	Message.TimeText = FString::Printf(TEXT("%02d:%02d"), ServerNow.GetHour(), ServerNow.GetMinute());

	if (const AMyPlayerState* MyPS = GetPlayerState<AMyPlayerState>())
	{
		Message.Username = MyPS->GetUsername();

		// 던전에서만 캐릭터명 포함
		if (ShouldIncludeMessengerCharacterName())
		{
			Message.CharacterName = MessengerUtil::GetCharacterDisplayName(MyPS->GetSelectedCharacterId());
		}
	}

	return Message;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파티 채널 수신 대상을 채운다. 기본 구현은 비어 있으며 로비/던전에서 override 한다.
// OutRecipients : 수신 대상 컨트롤러 목록(출력)
void AMyPlayerController::GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const
{
	OutRecipients.Reset();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 메시지에 캐릭터명을 포함할지 여부. 기본은 false(로비), 던전에서 true로 override 한다.
// 반환값 : 캐릭터명 포함 여부
bool AMyPlayerController::ShouldIncludeMessengerCharacterName() const
{
	return false;
}
