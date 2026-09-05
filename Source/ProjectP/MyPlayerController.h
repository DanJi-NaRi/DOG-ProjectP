// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Whisper/WhisperTypes.h"
#include "Widget/MyActivatableWidget.h"
#include "Widget/MyPrimaryGameLayout.h"
#include "Messenger/MessengerTypes.h"
#include "MyPlayerController.generated.h"

class UInputMappingContext;
class UCPP_MessengerWidget;
class UMyDamageFloatingLayerWidget;
class UMyGodPageWidget;
class UMyInventoryWidget;
class AMyPlayerState;
class APlayerSpectatorCameraActor;
/**
 * Custom PlayerController
 *
 */
UCLASS()
class PROJECTP_API AMyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> IMC_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Debug")
	bool bEnableUITestVerticalSlice = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Debug")
	TSubclassOf<UMyPrimaryGameLayout> TestPrimaryLayoutClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Debug")
	TSubclassOf<UMyActivatableWidget> TestHUDClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Debug")
	TSubclassOf<UMyActivatableWidget> TestMenuClass;

	// 인벤토리 창 위젯 클래스. (BP_DungeonPC 디폴트에서 WBP_Inventory 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Inventory")
	TSubclassOf<UMyInventoryWidget> InventoryWidgetClass;

	// 인벤토리 창을 열거나 닫는다. (I 키 또는 HUD 버튼에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ToggleInventory();

	//! 인벤토리 창을 다시 생성해도 유지할 로컬 표시 순서를 반환한다.
	const TArray<FName>& GetInventoryDisplayOrder() const;

	//! 인벤토리의 로컬 표시 순서를 저장한다.
	void SetInventoryDisplayOrder(const TArray<FName>& InDisplayOrder);

	//! \author 장효제
	//! \brief 신 페이지를 열거나 닫는다. G 키 또는 다른 UI 이벤트에서 호출한다.
	UFUNCTION(BlueprintCallable, Category = "UI|GodPage")
	void ToggleGodPage();

	//! \author 장효제
	//! \brief 신 페이지를 닫지 않고 열어 둔 채 지정한 신을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "UI|GodPage", meta = (Categories = "God"))
	void OpenGodPage(FGameplayTag GodTag);

	//! 현재 PlayerController 수명 동안 마지막으로 조회한 신을 반환한다.
	FGameplayTag GetLastViewedGodTag() const;

	//! 현재 PlayerController 수명 동안 마지막으로 조회한 신을 저장한다.
	void SetLastViewedGodTag(FGameplayTag GodTag);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GodPage")
	TSubclassOf<UMyGodPageWidget> GodPageWidgetClass;

	// 퀵슬롯에 등록된 아이템을 사용한다. (1~4 키 또는 HUD 슬롯 클릭에서 호출)
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void UseQuickSlotByIndex(int32 SlotIndex);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	//! 사망한 로컬 플레이어가 Controller 소유 관전 카메라를 사용 중인지 반환한다.
	bool IsDeathSpectating() const { return bDeathSpectating; }

	// 서버 이동(ClientTravel) 직전 클라이언트에서 호출된다. 사용 중 캐릭터 ID를 GameInstance에 보관해 이동 후 복원에 사용한다.
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

	UFUNCTION(BlueprintCallable, Category = "Player|Aim")
	bool TryGetMouseWorldLocationOnPlane(float PlaneZ, FVector& OutLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Player|Aim")
	bool TryGetMouseWorldHitByChannel(
		TEnumAsByte<ECollisionChannel> TraceChannel,
		FHitResult& OutHitResult,
		float TraceDistance = 100000.0f,
		bool bTraceComplex = false
	) const;

	// 메신저 위젯. 로컬 클라에서 BeginPlay 시 생성한다.
	UPROPERTY(BlueprintReadOnly, Category = "Messenger")
	TObjectPtr<UCPP_MessengerWidget> Messenger;

	// 생성할 메신저 위젯 클래스. (C2B_LobbyPC / BP_DungeonPC 디폴트에서 WBP_Messenger 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger")
	TSubclassOf<UCPP_MessengerWidget> MessengerWidgetClass;

	// 단독 생성(뷰포트 직접 추가) 시 z-order. HUD에 임베드하는 방식에는 영향 없음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger")
	int32 MessengerViewportZOrder = 100;

	// 메신저(전체/파티 채팅) 메시지를 전송한다. 로컬 클라의 입력 UI가 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Messenger")
	void SendMessengerMessage(EMessengerChannel Channel, const FString& RawText);

	// '/'로 시작하는 치트 명령을 처리한다. 클라이언트 전용 명령은 즉시 실행하고 나머지는 서버로 보낸다.
	UFUNCTION(BlueprintCallable, Category = "Cheat")
	void ExecuteChatCheatCommand(const FString& CommandLine);

	//! \brief 로컬 스킬 판정 DebugLine 표시 여부를 설정한다.
	void SetSkillDebugLineEnabled(bool bEnabled);

	//! \brief 로컬 스킬 판정 DebugLine 표시 여부를 반환한다.
	bool IsSkillDebugLineEnabled() const;

	// 메신저 위젯을 PC에 등록한다. (HUD에 임베드된 위젯이 NativeConstruct에서 자기 자신을 등록)
	UFUNCTION(BlueprintCallable, Category = "Messenger")
	void SetMessengerWidget(UCPP_MessengerWidget* InMessenger);

	//! \author 장효제
	//! \brief 로컬 신 채팅과 일반 메신저 UI에 빈 테스트 블록을 추가한다.
	//! \param Count 각 채팅 UI에 추가할 블록 수
	void MakeTestBubbles(int32 Count);

	// --- 데미지 숫자 표시 ---
	// 서버가 데미지 확정 시 "공격자 본인" 클라이언트에만 보낸다. (파티원이 넣은 데미지는 표시하지 않는 정책)
	// 연출 전용이라 Unreliable — 유실돼도 숫자 하나 안 뜨는 것 이상의 영향이 없다.
	UFUNCTION(Client, Unreliable)
	void ClientShowDamageNumber(
		AActor* DamagedActor,
		float DamageAmount,
		bool bCriticalHit,
		bool bDamageOverTime,
		bool bKilledTarget,
		FGameplayTag CameraFeedbackTag
	);

protected:
	// 파티 채널 수신 대상 PlayerController 목록을 채운다. (기본: 없음, 로비/던전에서 override)
	virtual void GetMessengerPartyRecipients(TArray<AMyPlayerController*>& OutRecipients) const;

	// 메시지에 캐릭터명을 포함할지 여부. (기본: false=로비, 던전에서 true)
	virtual bool ShouldIncludeMessengerCharacterName() const;

	// 전체 채널 수신 대상(현재 서버의 모든 PlayerController)을 채운다. (던전 파티 대상이 곧 전체이므로 재사용)
	void GetMessengerAllRecipients(TArray<AMyPlayerController*>& OutRecipients) const;

	// 1회 전송 최대 글자 수 (서버에서 검증)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger", meta = (ClampMin = "1"))
	int32 MaxMessengerMessageLength = 200;

	// 전송 쿨다운(초). 서버에서 도배 방지용으로 검증한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Messenger", meta = (ClampMin = "0.0"))
	float MessengerSendCooldownSeconds = 0.5f;

	//! 사망 위치를 보여준 뒤 생존 팀원 추적을 시작하기까지의 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator", meta = (ClampMin = "0.0"))
	float DeathSpectatorHoldDuration = 1.0f;

	//! 관전 대상의 생존·연결·Pawn 유효성을 다시 검사하는 주기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator", meta = (ClampMin = "0.05"))
	float SpectatorTargetRefreshInterval = 0.25f;

	//! 관전 카메라 피벗이 팀원에게 이동하는 보간 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator", meta = (ClampMin = "0.0"))
	float SpectatorFollowInterpSpeed = 5.0f;

	//! 팀원 Actor 위치에서 카메라 피벗에 더할 높이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator")
	float SpectatorTargetHeightOffset = 100.0f;

	//! 플레이 중 Zoom 상태와 무관하게 관전을 시작할 기본 SpringArm 길이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "0.0"))
	float SpectatorDefaultArmLength = 2000.0f;

	//! 플레이 중 Zoom 상태와 무관하게 관전을 시작할 기본 FOV
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float SpectatorDefaultFieldOfView = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "0.0"))
	float SpectatorMinArmLength = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "0.0"))
	float SpectatorMaxArmLength = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom")
	float SpectatorArmLengthStep = -100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float SpectatorMinFieldOfView = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float SpectatorMaxFieldOfView = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator|Zoom")
	float SpectatorFieldOfViewStep = -2.0f;

	//! 부활 시 새 Pawn 카메라로 복귀하는 블렌드 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator", meta = (ClampMin = "0.0"))
	float SpectatorReturnBlendTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator")
	float SpectatorOrbitYawSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator")
	float SpectatorOrbitPitchSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator")
	float SpectatorMinPitch = -80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Spectator")
	float SpectatorMaxPitch = -10.0f;

	// 메시지 수신 시 호출되는 BP 확장 이벤트. (Step 4에서 메신저 위젯과 연동)
	UFUNCTION(BlueprintImplementableEvent, Category = "Messenger")
	void OnMessengerMessageReceived(const FMessengerMessage& Message);

private:
	void InitializeDebugUIVerticalSlice();
	void InitializeDamageFloatingLayer();
	void HandleDebugUITestMenuPressed();
	void HandleChatFocusKeyPressed();
	void HandleToggleInventoryPressed();
	void HandleToggleGodPagePressed();

	// 퀵슬롯 사용 키(1~4) 입력 핸들러. BindKey가 인자 없는 함수만 받으므로 키마다 하나씩 둔다.
	void HandleUseQuickSlot1Pressed();
	void HandleUseQuickSlot2Pressed();
	void HandleUseQuickSlot3Pressed();
	void HandleUseQuickSlot4Pressed();
	void HandleSpectatorNextPressed();
	void HandleSpectatorZoomIn();
	void HandleSpectatorZoomOut();
	void HandleSpectatorOrbitPressed();
	void HandleSpectatorOrbitReleased();

	void SynchronizeDeathSpectatorState();
	void EnterDeathSpectating();
	void ExitDeathSpectating();
	void StartFollowingSpectatorTarget();
	void RefreshSpectatorTargets();
	void CycleSpectatorTarget();
	void AdjustSpectatorZoom(float ZoomInput);
	void SetSpectatorTarget(AMyPlayerState* NewTargetPlayerState);
	bool IsValidSpectatorTarget(const AMyPlayerState* CandidatePlayerState) const;
	APawn* ResolveSpectatorPawn(const AMyPlayerState* CandidatePlayerState) const;
	APlayerSpectatorCameraActor* EnsureSpectatorCameraActor();
	void RestoreSpectatorOrbitCursor();

	// 현재 열려 있는 인벤토리 창. 토글 시 닫기 판정에 사용한다.
	TWeakObjectPtr<UMyInventoryWidget> ActiveInventoryWidget;

	//! 인벤토리 위젯을 닫았다 다시 열어도 유지되는 로컬 아이템 표시 순서
	UPROPERTY(Transient)
	TArray<FName> InventoryDisplayOrder;

	//! \author 장효제
	//! \brief 현재 Menu 레이어에 활성화된 신 페이지를 추적한다.
	TWeakObjectPtr<UMyGodPageWidget> ActiveGodPageWidget;

	//! 현재 PlayerController가 유지되는 동안 마지막으로 조회한 신이다. 서버에 복제하지 않는다.
	UPROPERTY(Transient)
	FGameplayTag LastViewedGodTag;

	// 몬스터 생명주기와 독립적으로 로컬 데미지 숫자를 보관하는 화면 레이어
	UPROPERTY(Transient)
	TObjectPtr<UMyDamageFloatingLayerWidget> DamageFloatingLayer;

	UPROPERTY(Transient)
	TObjectPtr<APlayerSpectatorCameraActor> SpectatorCameraActor;

	TArray<TWeakObjectPtr<AMyPlayerState>> SpectatorTargets;
	TWeakObjectPtr<AMyPlayerState> CurrentSpectatorTarget;
	FTimerHandle SpectatorFollowDelayTimerHandle;
	FTimerHandle SpectatorTargetRefreshTimerHandle;
	FVector DeathSpectatorOrigin = FVector::ZeroVector;
	float SpectatorSavedMouseX = 0.0f;
	float SpectatorSavedMouseY = 0.0f;
	bool bDeathSpectating = false;
	bool bSpectatorTargetSelectionEnabled = false;
	bool bSpectatorOrbiting = false;
	bool bHasSpectatorSavedMousePosition = false;

	UFUNCTION()
	void HandleWhisperTranscriptionCompleted(const FWhisperTranscriptionResult& Result);

	UFUNCTION()
	void HandleWhisperTranscriptionFailed(const FString& ErrorMessage);

	// --- 메신저 내부 처리 ---
	UFUNCTION(Server, Reliable)
	void ServerSendMessengerMessage(EMessengerChannel Channel, const FString& RawText);

	UFUNCTION(Client, Reliable)
	void ClientReceiveMessengerMessage(const FMessengerMessage& Message);

	// --- 치트 내부 처리 ---
	UFUNCTION(Server, Reliable)
	void ServerExecuteCheatCommand(const FString& CommandLine);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCheatResult(const FString& ResultText);

	// 보낸 사람 정보를 바탕으로 전달용 메시지 데이터를 생성한다.
	FMessengerMessage MakeMessengerMessage(EMessengerChannel Channel, const FString& Body) const;

	// 마지막 전송 시각(서버 시간). 쿨다운 판정에 사용.
	float LastMessengerSendServerTime = -1000.0f;

	//! \brief 이 로컬 플레이어 화면에 스킬 판정 DebugLine을 표시할지 여부. 게임 내 치트로만 변경한다.
	bool bSkillDebugLineEnabled = false;

};
