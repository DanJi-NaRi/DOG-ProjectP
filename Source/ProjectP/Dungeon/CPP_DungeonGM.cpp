#include "CPP_DungeonGM.h"

#include "DungeonGS.h"
#include "DungeonPC.h"
#include "Dungeon/Revive/DungeonReviveDataAsset.h"
#include "AbilitySystemComponent.h"
#include "../GameInstance/SubSystems/Dungeon/DungeonReconnectSubsystem.h"
#include "../GameInstance/SubSystems/NetSub/ServerConfigSubsystem.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "../GAS/MyAttributeSet.h"
#include "../GAS/MyCooldownGameplayEffect.h"
#include "../GAS/MyPlayerState.h"
#include "../Item/MyInventoryComponent.h"
#include "../MyGameplayTags.h"
#include "Streaming/MyStreamingManagerComponent.h"
#include "../Player/PlayerCharacterBase.h"
#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

ACPP_DungeonGM::ACPP_DungeonGM()
{
    RequiredPlayerCount = 3;
    PlayerStateClass = AMyPlayerState::StaticClass();
}

void ACPP_DungeonGM::BeginPlay()
{
    Super::BeginPlay();

    CurrentPlayerCount = 0;
    bRequiredPlayerCountReached = false;
    bDungeonSessionActive = true;
    InGameUserIndexes.Reset();
    OutGameUserIndexes.Reset();
    LastSurrenderVoteFailedTime = -SurrenderVoteCooldownSeconds;
    ReturnToLobbyServerTime = 0.0f;
    bReturnToLobbyCountdownInProgress = false;
    PendingDungeonRevives.Reset();

    NotifyPlayerCountChanged();
    UpdateSurrenderVoteState();
}

void ACPP_DungeonGM::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    ++CurrentPlayerCount;
    CancelEmptyDungeonShutdown();

    UE_LOG(LogTemp, Log, TEXT("Dungeon player joined. Current: %d / Required: %d"), CurrentPlayerCount, RequiredPlayerCount);

    NotifyPlayerCountChanged();
    CheckRequiredPlayersReady();

    if (bSurrenderVoteInProgress)
    {
        UpdateSurrenderVoteState();
    }
    else if (bReturnToLobbyCountdownInProgress)
    {
        UpdateSurrenderVoteState(TEXT("항복 투표가 가결되었습니다."));
    }
}

void ACPP_DungeonGM::Logout(AController* Exiting)
{
    AssignedDungeonEntryPlayerStarts.Remove(TWeakObjectPtr<AController>(Exiting));

    APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
    const bool bStoredDisconnectedPlayer = StoreDisconnectedPlayer(Exiting);
    bool bMovedAuthenticatedPlayerToOutGame = false;
    if (ExitingPlayerState)
    {
        if (AMyPlayerState* ExitingMyPlayerState = Cast<AMyPlayerState>(ExitingPlayerState))
        {
            CancelPendingRevive(ExitingMyPlayerState);
            ExitingMyPlayerState->OnLifeStateChanged.RemoveAll(this);
        }

        SurrenderAgreePlayerStates.Remove(ExitingPlayerState);
        DemoInitializedPlayerStates.Remove(ExitingPlayerState);
        bMovedAuthenticatedPlayerToOutGame = MoveAuthenticatedPlayerToOutGame(ExitingPlayerState);
    }

    Super::Logout(Exiting);

    CurrentPlayerCount = FMath::Max(0, CurrentPlayerCount - 1);
    if (InGameUserIndexes.Num() < RequiredPlayerCount)
    {
        bRequiredPlayerCountReached = false;
    }

    UE_LOG(LogTemp, Log, TEXT("Dungeon player left. Current: %d / Required: %d"), CurrentPlayerCount, RequiredPlayerCount);

    NotifyPlayerCountChanged();
    if (bMovedAuthenticatedPlayerToOutGame)
    {
        if (!bStoredDisconnectedPlayer)
        {
            UE_LOG(LogTemp, Warning, TEXT("Dungeon user moved OutGame but reconnect snapshot was not saved. PlayerState: %s"), *GetNameSafe(ExitingPlayerState));
        }

        EndDungeonSessionIfNoInGameUsers();
    }

    if (CurrentPlayerCount <= 0)
    {
        ScheduleEmptyDungeonShutdown();
    }

    if (bSurrenderVoteInProgress)
    {
        if (GetSurrenderRequiredPlayerCount() <= 0)
        {
            const FString CancelMessage = ActivePartyVoteType == EDungeonPartyVoteType::GimmickReset
                ? TEXT("투표 대상자가 없어 기믹 초기화 투표가 취소되었습니다.")
                : TEXT("투표 대상자가 없어 항복 투표가 취소되었습니다.");
            FailSurrenderVote(CancelMessage);
            return;
        }

        if (GetSurrenderAgreeCount() >= GetSurrenderRequiredPlayerCount())
        {
            CompleteActivePartyVote();
            return;
        }

        UpdateSurrenderVoteState();
    }
    else if (bReturnToLobbyCountdownInProgress)
    {
        UpdateSurrenderVoteState(TEXT("항복 투표가 가결되었습니다."));
    }
}

////////////////////////////
//! \author HanUl
//! \brief DungeonEntry 태그의 PlayerStart 중 다른 플레이어에게 할당되지 않고 Pawn에 점유되지 않은 위치를 서버에서 무작위 선택한다.
//! \param Player 스폰 위치를 선택할 플레이어 Controller
//! \return 선택된 PlayerStart. 태그가 없으면 기본 선택 결과, 모든 태그 위치가 점유되었으면 nullptr
AActor* ACPP_DungeonGM::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!Player)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }

    for (auto It = AssignedDungeonEntryPlayerStarts.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !It.Value().IsValid())
        {
            It.RemoveCurrent();
        }
    }

    const TWeakObjectPtr<AController> PlayerKey(Player);
    if (const TWeakObjectPtr<APlayerStart>* ExistingStart = AssignedDungeonEntryPlayerStarts.Find(PlayerKey))
    {
        if (ExistingStart->IsValid())
        {
            return ExistingStart->Get();
        }
    }

    TArray<APlayerStart*> TaggedPlayerStarts;
    TSet<const APlayerStart*> AssignedPlayerStarts;

    for (const TPair<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>>& Assignment : AssignedDungeonEntryPlayerStarts)
    {
        if (Assignment.Value.IsValid())
        {
            AssignedPlayerStarts.Add(Assignment.Value.Get());
        }
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        APlayerStart* PlayerStart = *It;
        if (IsValid(PlayerStart) && PlayerStart->PlayerStartTag == DungeonEntryPlayerStartTag)
        {
            TaggedPlayerStarts.Add(PlayerStart);
        }
    }

    if (TaggedPlayerStarts.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("No PlayerStart with PlayerStartTag '%s' was found. Falling back to the default PlayerStart selection."),
            *DungeonEntryPlayerStartTag.ToString());
        return Super::ChoosePlayerStart_Implementation(Player);
    }

    if (TaggedPlayerStarts.Num() < RequiredPlayerCount)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("DungeonEntry PlayerStart count is smaller than RequiredPlayerCount. Starts: %d, Required: %d"),
            TaggedPlayerStarts.Num(),
            RequiredPlayerCount);
    }

    TArray<APlayerStart*> AvailablePlayerStarts;
    for (APlayerStart* PlayerStart : TaggedPlayerStarts)
    {
        if (!AssignedPlayerStarts.Contains(PlayerStart) &&
            IsDungeonEntryPlayerStartAvailable(PlayerStart, Player))
        {
            AvailablePlayerStarts.Add(PlayerStart);
        }
    }

    if (AvailablePlayerStarts.IsEmpty())
    {
        UE_LOG(LogTemp, Error,
            TEXT("All PlayerStarts tagged '%s' are assigned or occupied. Player spawn was rejected to prevent overlap."),
            *DungeonEntryPlayerStartTag.ToString());
        return nullptr;
    }

    const int32 SelectedIndex = FMath::RandRange(0, AvailablePlayerStarts.Num() - 1);
    APlayerStart* SelectedPlayerStart = AvailablePlayerStarts[SelectedIndex];
    AssignedDungeonEntryPlayerStarts.Add(PlayerKey, SelectedPlayerStart);

    UE_LOG(LogTemp, Log,
        TEXT("Dungeon entry PlayerStart assigned. Controller: %s, PlayerStart: %s, AvailableStarts: %d"),
        *GetNameSafe(Player),
        *GetNameSafe(SelectedPlayerStart),
        AvailablePlayerStarts.Num());

    return SelectedPlayerStart;
}

////////////////////////////
//! \author HanUl
//! \brief PlayerStart 주변의 다른 Pawn을 검사해 신규 플레이어가 겹치지 않고 스폰 가능한지 확인한다.
//! \param PlayerStart 검사할 던전 입장 PlayerStart
//! \param Player 스폰을 요청한 Controller
//! \return 최소 거리 안에 다른 Pawn이 없으면 true
bool ACPP_DungeonGM::IsDungeonEntryPlayerStartAvailable(const APlayerStart* PlayerStart, const AController* Player) const
{
    UWorld* World = GetWorld();
    if (!World || !PlayerStart)
    {
        return false;
    }

    if (DungeonEntryMinimumPawnDistance <= 0.0f)
    {
        return true;
    }

    const float MinimumDistanceSquared = FMath::Square(DungeonEntryMinimumPawnDistance);
    const FVector PlayerStartLocation = PlayerStart->GetActorLocation();

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        const APawn* ExistingPawn = *It;
        if (!IsValid(ExistingPawn) ||
            ExistingPawn->GetController() == Player ||
            !ExistingPawn->GetActorEnableCollision())
        {
            continue;
        }

        if (FVector::DistSquared(PlayerStartLocation, ExistingPawn->GetActorLocation()) < MinimumDistanceSquared)
        {
            return false;
        }
    }

    return true;
}

int32 ACPP_DungeonGM::GetCurrentPlayerCount() const
{
    return CurrentPlayerCount;
}

int32 ACPP_DungeonGM::GetRequiredPlayerCount() const
{
    return RequiredPlayerCount;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 세션이 활성 상태인지 확인하는 함수
// Return Value : 던전 세션 활성 여부
bool ACPP_DungeonGM::IsDungeonSessionActive() const
{
    return bDungeonSessionActive;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 안에 접속 중인 인증 유저 수를 반환하는 함수
// Return Value : InGame 상태 유저 수
int32 ACPP_DungeonGM::GetInGameUserCount() const
{
    return InGameUserIndexes.Num();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전에서 나갔지만 세션 복구 대상인 인증 유저 수를 반환하는 함수
// Return Value : OutGame 상태 유저 수
int32 ACPP_DungeonGM::GetOutGameUserCount() const
{
    return OutGameUserIndexes.Num();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 진행 중인 항복 투표의 시작 서버 시간을 반환하는 함수
// Return Value : 항복 투표 시작 서버 시간
float ACPP_DungeonGM::GetSurrenderVoteStartServerTime() const
{
    return SurrenderVoteStartServerTime;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 던전 플레이어를 InGame 유저 목록에 등록하는 함수
// PlayerController : 인증이 완료된 플레이어 컨트롤러
// Return Value : InGame 등록 성공 여부
bool ACPP_DungeonGM::RegisterAuthenticatedDungeonPlayer(APlayerController* PlayerController, const FString& LoginToken)
{
    if (!bDungeonSessionActive || LoginToken.IsEmpty())
    {
        return false;
    }

    AMyPlayerState* MyPlayerState = PlayerController ? PlayerController->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!MyPlayerState || !MyPlayerState->IsAuthVerified() || MyPlayerState->GetUserIndex() <= 0)
    {
        return false;
    }

    const int32 UserIndex = MyPlayerState->GetUserIndex();
    bool bRestoredFromReconnect = false;
    if (!RestoreDisconnectedPlayer(PlayerController, UserIndex, bRestoredFromReconnect))
    {
        return false;
    }

    EnsureSelectedCharacterPawn(PlayerController);
    if (!bRestoredFromReconnect && !InitializeNewPlayerData(PlayerController))
    {
        return false;
    }

    OutGameUserIndexes.Remove(UserIndex);
    InGameUserIndexes.Add(UserIndex);
    ReportDungeonMemberState(UserIndex, TEXT("InGame"));
    BindPlayerLifeState(MyPlayerState);

    UE_LOG(LogTemp, Log, TEXT("Dungeon user registered InGame. UserIndex: %d, InGame: %d, OutGame: %d"), UserIndex, InGameUserIndexes.Num(), OutGameUserIndexes.Num());

    CheckRequiredPlayersReady();
    UpdateSurrenderVoteState();

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// PIE 또는 시연 허용 설정에서 인증 토큰이 없는 플레이어에게 초기 데이터를 한 번만 적용하는 함수
// PlayerController : 초기 데이터를 적용할 인증 없는 플레이어 컨트롤러
// Return Value : 이미 적용되었거나 이번 요청에서 적용에 성공했으면 true
bool ACPP_DungeonGM::TryInitializeDemoPlayer(APlayerController* PlayerController)
{
    if (!HasAuthority() || !PlayerController)
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const bool bIsPlayInEditor = World && World->WorldType == EWorldType::PIE;
    if (!bIsPlayInEditor && !bAllowUnauthenticatedDemoInitialization)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unauthenticated demo initialization rejected outside PIE. Enable bAllowUnauthenticatedDemoInitialization in BP_DungeonGM for a packaged demo."));
        return false;
    }

    AMyPlayerState* MyPlayerState = PlayerController->GetPlayerState<AMyPlayerState>();
    if (!MyPlayerState || MyPlayerState->IsAuthVerified())
    {
        return false;
    }

    if (!DemoInitializedPlayerStates.Contains(MyPlayerState))
    {
        if (!InitializeNewPlayerData(PlayerController))
        {
            return false;
        }

        DemoInitializedPlayerStates.Add(MyPlayerState);
        BindPlayerLifeState(MyPlayerState);
        UE_LOG(LogTemp, Log, TEXT("Unauthenticated demo player initialized. PIE: %s, PlayerState: %s"),
            bIsPlayInEditor ? TEXT("true") : TEXT("false"),
            *GetNameSafe(MyPlayerState));
    }

#if !UE_BUILD_SHIPPING
	if (World && World->GetNetMode() == NM_Standalone)
	{
		if (ADungeonGS* DungeonGS = World->GetGameState<ADungeonGS>())
		{
			if (UMyStreamingManagerComponent* StreamingManager = DungeonGS->GetStreamingManager())
			{
				StreamingManager->StartStandaloneMissionLoop(MyPlayerState);
				StreamingManager->StartSmallTalkScheduler();
				StreamingManager->StartAntiAFK();
			}
		}
	}
#endif

    return true;
}

////////////////////////////
//! \author 준혁
//! \brief 인증 완료된 플레이어의 폰을 선택 캐릭터 ID에 대응하는 폰 클래스로 교체하는 함수.
//!        이미 올바른 클래스면(재접속 복구 폰 포함) 교체하지 않는다. 서버에서만 호출된다.
//! \param PlayerController 인증이 완료된 플레이어 컨트롤러
void ACPP_DungeonGM::EnsureSelectedCharacterPawn(APlayerController* PlayerController)
{
    AMyPlayerState* MyPlayerState = PlayerController ? PlayerController->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!MyPlayerState)
    {
        return;
    }

    const int32 SelectedCharacterId = MyPlayerState->GetSelectedCharacterId();
    const TSoftClassPtr<APawn>* PawnClassPtr = CharacterPawnClasses.Find(SelectedCharacterId);
    if (!PawnClassPtr)
    {
        UE_LOG(LogTemp, Warning, TEXT("No pawn class mapped for CharacterId %d. Keeping current pawn. (Set CharacterPawnClasses in BP_DungeonGM)"), SelectedCharacterId);
        return;
    }

    UClass* PawnClass = PawnClassPtr->LoadSynchronous();
    if (!PawnClass || !PawnClass->IsChildOf(APawn::StaticClass()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Pawn class for CharacterId %d failed to load."), SelectedCharacterId);
        return;
    }

    APawn* CurrentPawn = PlayerController->GetPawn();
    if (CurrentPawn && CurrentPawn->IsA(PawnClass))
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTransform SpawnTransform = FTransform::Identity;
    if (CurrentPawn)
    {
        SpawnTransform = CurrentPawn->GetActorTransform();
    }
    else if (const AActor* PlayerStart = FindPlayerStart(PlayerController))
    {
        SpawnTransform = PlayerStart->GetActorTransform();
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = PlayerController;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APawn* NewPawn = World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParameters);
    if (!NewPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to spawn selected character pawn. CharacterId: %d"), SelectedCharacterId);
        return;
    }

    if (CurrentPawn)
    {
        PlayerController->UnPossess();
        CurrentPawn->Destroy();
    }

    PlayerController->Possess(NewPawn);

    UE_LOG(LogTemp, Log, TEXT("Selected character pawn spawned. CharacterId: %d, Pawn: %s"), SelectedCharacterId, *GetNameSafe(NewPawn));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 신규 던전 입장 플레이어에게 BP_DungeonGM의 초기 데이터를 서버 권위로 적용하는 함수
// PlayerController : 초기 데이터를 적용할 플레이어 컨트롤러
// Return Value : 초기 데이터 적용 성공 여부
bool ACPP_DungeonGM::InitializeNewPlayerData(APlayerController* PlayerController) const
{
    if (!HasAuthority() || !PlayerController)
    {
        return false;
    }

    AMyPlayerState* MyPlayerState = PlayerController->GetPlayerState<AMyPlayerState>();
    APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(PlayerController->GetPawn());
    UMyInventoryComponent* InventoryComponent = MyPlayerState ? MyPlayerState->GetInventoryComponent() : nullptr;
    UAbilitySystemComponent* AbilitySystemComponent = MyPlayerState ? MyPlayerState->GetAbilitySystemComponent() : nullptr;
    UMyAttributeSet* AttributeSet = MyPlayerState ? MyPlayerState->GetMyAttributeSet() : nullptr;
    if (!MyPlayerState || !PlayerCharacter || !InventoryComponent || !AbilitySystemComponent || !AttributeSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("Initial player data could not be applied because a required object is missing. Controller: %s, Pawn: %s"),
            *GetNameSafe(PlayerController),
            *GetNameSafe(PlayerController->GetPawn()));
        return false;
    }

    PlayerCharacter->SetCharacterLevel(InitialPlayerData.CharacterLevel);

    const int32 RequiredExp = PlayerCharacter->GetExpRequiredForNextLevel();
    const int32 ClampedExp = RequiredExp > 0
        ? FMath::Clamp(InitialPlayerData.CharacterExp, 0, RequiredExp - 1)
        : 0;
    MyPlayerState->SetCharacterExp(ClampedExp);
    InventoryComponent->SetMeso(InitialPlayerData.Meso);

    const float MaxHealth = FMath::Max(AttributeSet->GetMaxHealth(), 1.0f);
    const float InitialHealth = InitialPlayerData.Health <= 0.0f
        ? MaxHealth
        : FMath::Clamp(InitialPlayerData.Health, 0.0f, MaxHealth);
    const float MaxMoveCharge = FMath::Max(AttributeSet->GetMaxMoveCharge(), 1.0f);
    const float InitialMoveCharge = FMath::Clamp(InitialPlayerData.MoveCharge, 0.0f, MaxMoveCharge);
    const float InitialCurseGauge = FMath::Clamp(InitialPlayerData.CurseGauge, 0.0f, 100.0f);

    AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), InitialHealth);
    AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetShieldAttribute(), FMath::Max(InitialPlayerData.Shield, 0.0f));
    AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMoveChargeAttribute(), InitialMoveCharge);
    AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetCurseGaugeAttribute(), InitialCurseGauge);

    UE_LOG(LogTemp, Log, TEXT("Initial player data applied. UserIndex: %d, Meso: %d, Level: %d, Exp: %d, Health: %.1f/%.1f, Shield: %.1f, MoveCharge: %.1f/%.1f, CurseGauge: %.1f"),
        MyPlayerState->GetUserIndex(),
        InventoryComponent->GetMeso(),
        MyPlayerState->GetCharacterLevel(),
        MyPlayerState->GetCharacterExp(),
        AttributeSet->GetHealth(),
        AttributeSet->GetMaxHealth(),
        AttributeSet->GetShield(),
        AttributeSet->GetMoveCharge(),
        AttributeSet->GetMaxMoveCharge(),
        AttributeSet->GetCurseGauge());

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표를 시작하는 함수
// RequestingController : 항복 투표를 시작한 플레이어 컨트롤러
// Return Value : 항복 투표 시작 성공 여부
bool ACPP_DungeonGM::StartSurrenderVote(APlayerController* RequestingController)
{
    if (!RequestingController)
    {
        return false;
    }

    APlayerState* RequestingPlayerState = GetSurrenderVoterState(RequestingController);
    if (!RequestingPlayerState)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start surrender vote because requesting PlayerState is null. Controller: %s"), *GetNameSafe(RequestingController));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    if (bSurrenderVoteInProgress)
    {
        SendVoteNotice(
            RequestingController,
            NSLOCTEXT("DungeonPartyVote", "AnotherVoteInProgress", "이미 다른 투표가 진행 중입니다."));
        return false;
    }

    if (bReturnToLobbyCountdownInProgress)
    {
        SendVoteNotice(
            RequestingController,
            NSLOCTEXT("DungeonPartyVote", "ReturnToLobbyInProgress", "방송 종료가 진행 중이라 새 투표를 시작할 수 없습니다."));
        return false;
    }

    const float CurrentServerTime = World->GetTimeSeconds();
    const float CooldownEndServerTime = LastSurrenderVoteFailedTime + SurrenderVoteCooldownSeconds;
    if (CurrentServerTime < CooldownEndServerTime)
    {
        const int32 RemainingCooldownSeconds = FMath::Max(0, FMath::CeilToInt(CooldownEndServerTime - CurrentServerTime));
        SendVoteNotice(
            RequestingController,
            FText::FromString(FString::Printf(TEXT("항복 투표 쿨타임이 %d초 남아 있습니다."), RemainingCooldownSeconds)));
        return false;
    }

    bSurrenderVoteInProgress = true;
    ActivePartyVoteType = EDungeonPartyVoteType::Surrender;
    ActiveGimmickResetVoteContext.Reset();
    GimmickResetVotePassedDelegate.Unbind();
    SurrenderAgreePlayerStates.Reset();
    SurrenderDisagreeCount = 0;
    SurrenderVoteStartServerTime = CurrentServerTime;
    SurrenderAgreePlayerStates.Add(RequestingPlayerState);

    World->GetTimerManager().SetTimer(SurrenderVoteTimerHandle, this, &ACPP_DungeonGM::HandleSurrenderVoteTimeout, SurrenderVoteDurationSeconds, false);

    UE_LOG(LogTemp, Log, TEXT("Surrender vote started. VoterState: %s, Agree: %d / Required: %d"), *GetNameSafe(RequestingPlayerState), GetSurrenderAgreeCount(), GetSurrenderRequiredPlayerCount());

    UpdateSurrenderVoteState(TEXT("항복 투표가 시작되었습니다."));

    if (GetSurrenderAgreeCount() >= GetSurrenderRequiredPlayerCount())
    {
        CompleteActivePartyVote();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 Zone 기믹 초기화에 사용할 공용 만장일치 투표를 시작하는 함수
// RequestingController : 기믹 초기화 투표를 시작한 플레이어 컨트롤러
// VoteContext : 투표 대상 Zone을 식별하고 Zone Clear 시 취소하기 위한 객체
// OnVotePassed : 만장일치 가결 시 실행할 기믹 초기화 콜백
// Return Value : 기믹 초기화 투표 시작 성공 여부
bool ACPP_DungeonGM::StartGimmickResetVote(
    APlayerController* RequestingController,
    UObject* VoteContext,
    const FSimpleDelegate& OnVotePassed)
{
    if (!RequestingController || !IsValid(VoteContext) || !OnVotePassed.IsBound())
    {
        return false;
    }

    APlayerState* RequestingPlayerState = GetSurrenderVoterState(RequestingController);
    if (!RequestingPlayerState)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot start gimmick reset vote because requesting PlayerState is null. Controller: %s"), *GetNameSafe(RequestingController));
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    if (bSurrenderVoteInProgress)
    {
        SendVoteNotice(
            RequestingController,
            NSLOCTEXT("DungeonPartyVote", "AnotherVoteInProgress", "이미 다른 투표가 진행 중입니다."));
        return false;
    }

    if (bReturnToLobbyCountdownInProgress)
    {
        SendVoteNotice(
            RequestingController,
            NSLOCTEXT("DungeonPartyVote", "ReturnToLobbyInProgress", "방송 종료가 진행 중이라 새 투표를 시작할 수 없습니다."));
        return false;
    }

    bSurrenderVoteInProgress = true;
    ActivePartyVoteType = EDungeonPartyVoteType::GimmickReset;
    ActiveGimmickResetVoteContext = VoteContext;
    GimmickResetVotePassedDelegate = OnVotePassed;
    SurrenderAgreePlayerStates.Reset();
    SurrenderDisagreeCount = 0;
    SurrenderVoteStartServerTime = World->GetTimeSeconds();
    SurrenderAgreePlayerStates.Add(RequestingPlayerState);

    World->GetTimerManager().SetTimer(
        SurrenderVoteTimerHandle,
        this,
        &ACPP_DungeonGM::HandleSurrenderVoteTimeout,
        SurrenderVoteDurationSeconds,
        false);

    UE_LOG(LogTemp, Log, TEXT("Gimmick reset vote started. VoterState: %s, Agree: %d / Required: %d"),
        *GetNameSafe(RequestingPlayerState), GetSurrenderAgreeCount(), GetSurrenderRequiredPlayerCount());

    UpdateSurrenderVoteState(TEXT("기믹 초기화 투표가 시작되었습니다."));

    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(RequestingController))
    {
        DungeonPC->NotifyPartyVoteSubmitted(SurrenderVoteStartServerTime);
    }

    if (GetSurrenderAgreeCount() >= GetSurrenderRequiredPlayerCount())
    {
        CompleteActivePartyVote();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Zone Clear 등으로 대상이 무효화되었을 때 해당 기믹 초기화 투표만 취소하는 함수
// VoteContext : 취소할 투표를 시작할 때 등록한 Zone 식별 객체
void ACPP_DungeonGM::CancelGimmickResetVote(UObject* VoteContext)
{
    if (!bSurrenderVoteInProgress || ActivePartyVoteType != EDungeonPartyVoteType::GimmickReset ||
        !IsValid(VoteContext) || ActiveGimmickResetVoteContext.Get() != VoteContext)
    {
        return;
    }

    FailSurrenderVote(TEXT("Zone이 클리어되어 기믹 초기화 투표가 취소되었습니다."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 찬반을 제출하는 함수
// VotingController : 투표를 제출한 플레이어 컨트롤러
// bAgree : 항복 투표 찬성 여부
// Return Value : 항복 투표가 신규 표로 처리되었는지 여부
bool ACPP_DungeonGM::SubmitSurrenderVote(APlayerController* VotingController, bool bAgree)
{
    if (!VotingController)
    {
        return false;
    }

    APlayerState* VotingPlayerState = GetSurrenderVoterState(VotingController);
    if (!VotingPlayerState)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot submit surrender vote because voting PlayerState is null. Controller: %s"), *GetNameSafe(VotingController));
        return false;
    }

    if (!bSurrenderVoteInProgress)
    {
        SendVoteNotice(
            VotingController,
            NSLOCTEXT("DungeonPartyVote", "NoVoteInProgress", "진행 중인 투표가 없습니다."));
        return false;
    }

    if (!bAgree)
    {
        SurrenderDisagreeCount = 1;
        UE_LOG(LogTemp, Log, TEXT("Surrender vote rejected. VoterState: %s, Agree: %d / Required: %d"), *GetNameSafe(VotingPlayerState), GetSurrenderAgreeCount(), GetSurrenderRequiredPlayerCount());
        const FString RejectedMessage = ActivePartyVoteType == EDungeonPartyVoteType::GimmickReset
            ? TEXT("반대 투표로 기믹 초기화 투표가 부결되었습니다.")
            : TEXT("반대 투표로 항복 투표가 부결되었습니다.");
        FailSurrenderVote(RejectedMessage);
        return true;
    }

    const TWeakObjectPtr<APlayerState> VotingPlayerStatePtr(VotingPlayerState);
    const bool bAlreadyVoted = SurrenderAgreePlayerStates.Contains(VotingPlayerStatePtr);
    if (!bAlreadyVoted)
    {
        SurrenderAgreePlayerStates.Add(VotingPlayerStatePtr);
    }

    UE_LOG(LogTemp, Log, TEXT("Surrender vote submitted. VoterState: %s, Accepted: %s, Agree: %d / Required: %d"), *GetNameSafe(VotingPlayerState), bAlreadyVoted ? TEXT("false") : TEXT("true"), GetSurrenderAgreeCount(), GetSurrenderRequiredPlayerCount());

    if (bAlreadyVoted)
    {
        UpdateSurrenderVoteState();
        return false;
    }

    if (GetSurrenderAgreeCount() >= GetSurrenderRequiredPlayerCount())
    {
        CompleteActivePartyVote();
        return true;
    }

    UpdateSurrenderVoteState();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 스트레스 테스트용 적을 지정된 수만큼 스폰하는 함수
// Count : 스폰할 테스트 적 수
void ACPP_DungeonGM::SpawnTestEnemiesForStressTest(int32 Count)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!StressTestEnemyClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnTestEnemiesForStressTest failed. StressTestEnemyClass is not set."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const int32 ClampedCount = FMath::Clamp(Count, 0, MaxStressTestEnemySpawnCount);
    if (ClampedCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnTestEnemiesForStressTest skipped. Count: %d"), Count);
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    int32 SpawnedCount = 0;
    for (int32 SpawnIndex = 0; SpawnIndex < ClampedCount; ++SpawnIndex)
    {
        ACPP_EnemyBase* SpawnedEnemy = World->SpawnActor<ACPP_EnemyBase>(
            StressTestEnemyClass,
            MakeStressTestEnemySpawnTransform(SpawnIndex),
            SpawnParameters
        );

        if (SpawnedEnemy)
        {
            ++SpawnedCount;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("SpawnTestEnemiesForStressTest finished. Requested: %d, Spawned: %d"), Count, SpawnedCount);
}

void ACPP_DungeonGM::NotifyPlayerCountChanged()
{
    OnDungeonPlayerCountChanged(CurrentPlayerCount, RequiredPlayerCount);
}

void ACPP_DungeonGM::CheckRequiredPlayersReady()
{
    if (bRequiredPlayerCountReached || InGameUserIndexes.Num() < RequiredPlayerCount)
    {
        return;
    }

    bRequiredPlayerCountReached = true;

    UE_LOG(LogTemp, Log, TEXT("Required dungeon players are ready."));

	if (ADungeonGS* DungeonGS = GetGameState<ADungeonGS>())
	{
		if (UMyStreamingManagerComponent* StreamingManager = DungeonGS->GetStreamingManager())
		{
			StreamingManager->StartMissionLoop(InGameUserIndexes.Array());
			StreamingManager->StartSmallTalkScheduler();
			StreamingManager->StartAntiAFK();
		}
	}

    OnRequiredDungeonPlayersReady();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 스트레스 테스트용 적의 격자형 스폰 위치를 계산하는 함수
// SpawnIndex : 생성 순서 인덱스
// Return Value : 계산된 스폰 Transform
FTransform ACPP_DungeonGM::MakeStressTestEnemySpawnTransform(int32 SpawnIndex) const
{
    const int32 ColumnCount = 10;
    const int32 Row = SpawnIndex / ColumnCount;
    const int32 Column = SpawnIndex % ColumnCount;

    FVector BaseLocation = FVector::ZeroVector;
    FRotator BaseRotation = FRotator::ZeroRotator;
    if (const UWorld* World = GetWorld())
    {
        const APlayerController* FirstPlayerController = World->GetFirstPlayerController();
        const APawn* FirstPlayerPawn = FirstPlayerController ? FirstPlayerController->GetPawn() : nullptr;
        if (FirstPlayerPawn)
        {
            BaseLocation = FirstPlayerPawn->GetActorLocation() + FirstPlayerPawn->GetActorForwardVector() * 600.0f;
            BaseRotation = FirstPlayerPawn->GetActorRotation();
        }
    }

    const FVector Offset(Row * StressTestEnemySpawnSpacing, Column * StressTestEnemySpawnSpacing, 50.0f);
    return FTransform(BaseRotation, BaseLocation + Offset);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 연결이 끊긴 인증 플레이어의 재접속 복구 스냅샷을 저장하는 함수
// Exiting : 연결 종료 중인 플레이어 컨트롤러
// Return Value : 스냅샷 저장 성공 여부
bool ACPP_DungeonGM::StoreDisconnectedPlayer(AController* Exiting)
{
    if (!bDungeonSessionActive || !Exiting)
    {
        return false;
    }

    const AMyPlayerState* MyPlayerState = Exiting->GetPlayerState<AMyPlayerState>();
    if (!MyPlayerState || !MyPlayerState->IsAuthVerified() || MyPlayerState->GetUserIndex() <= 0)
    {
        return false;
    }

    const int32 UserIndex = MyPlayerState->GetUserIndex();
    if (!InGameUserIndexes.Contains(UserIndex))
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UDungeonReconnectSubsystem* ReconnectSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UDungeonReconnectSubsystem>()
        : nullptr;
    if (!ReconnectSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot save reconnect snapshot because DungeonReconnectSubsystem is missing. UserIndex: %d"), UserIndex);
        return false;
    }

    APawn* Pawn = Exiting->GetPawn();
    if (!Pawn)
    {
        const ADungeonPC* DungeonPC = Cast<ADungeonPC>(Exiting);
        Pawn = DungeonPC ? DungeonPC->GetReconnectPreservedPawn() : nullptr;
    }

    FDungeonReconnectSnapshot Snapshot;
    Snapshot.UserIndex = UserIndex;
    Snapshot.Username = MyPlayerState->GetUsername();
    Snapshot.SelectedCharacterId = MyPlayerState->GetSelectedCharacterId();
    Snapshot.SavedServerTimeSeconds = FPlatformTime::Seconds();
    if (const UMyInventoryComponent* InventoryComponent = MyPlayerState->GetInventoryComponent())
    {
        Snapshot.Meso = InventoryComponent->GetMeso();
        Snapshot.InventoryEntries = InventoryComponent->GetEntries();
        Snapshot.QuickSlotItemIds = InventoryComponent->GetQuickSlotItemIds();
        Snapshot.ItemStatEffects = InventoryComponent->MakeReconnectItemStatEffectSnapshots(
            MyPlayerState->GetAbilitySystemComponent());
    }
    Snapshot.SkillCooldowns = MakeDungeonSkillCooldownSnapshot(MyPlayerState->GetAbilitySystemComponent());
    Snapshot.AttributeSnapshot = MakeDungeonAttributeSnapshot(MyPlayerState->GetMyAttributeSet());
    Snapshot.AttributeSnapshot.CharacterLevel = MyPlayerState->GetCharacterLevel();
    Snapshot.AttributeSnapshot.CharacterExp = MyPlayerState->GetCharacterExp();
    Snapshot.bOutGame = true;

    if (Pawn)
    {
        Snapshot.SavedTransform = Pawn->GetActorTransform();
        Snapshot.CharacterClass = TSoftClassPtr<APawn>(Pawn->GetClass());
    }

    if (!ResolveCurrentDungeonStep(Snapshot.SavedDungeonStep))
    {
        UE_LOG(LogTemp, Warning, TEXT("Current dungeon map is not mapped to EDungeonReconnectStep. Snapshot uses Stage1 fallback. UserIndex: %d"), UserIndex);
    }

    const bool bSaved = ReconnectSubsystem->SaveReconnectSnapshot(UserIndex, Snapshot);
    if (bSaved && Pawn)
    {
        ScheduleDisconnectedPawnDisable(UserIndex, Pawn);
    }

    UE_LOG(LogTemp, Log, TEXT("Dungeon reconnect snapshot save result. UserIndex: %d, Saved: %s, CharacterId: %d"),
        UserIndex,
        bSaved ? TEXT("true") : TEXT("false"),
        Snapshot.SelectedCharacterId);

    return bSaved;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// AttributeSet의 현재 자원 값과 GameplayEffect가 제외된 기본 스탯을 재접속 스냅샷으로 복사하는 함수
// AttributeSet : 값을 읽어올 MyGAS AttributeSet
// Return Value : 재접속 복구용 Attribute 스냅샷
FDungeonAttributeSnapshot ACPP_DungeonGM::MakeDungeonAttributeSnapshot(const UMyAttributeSet* AttributeSet) const
{
    FDungeonAttributeSnapshot Snapshot;
    if (!AttributeSet)
    {
        return Snapshot;
    }

    const UAbilitySystemComponent* AbilitySystemComponent =
        AttributeSet->GetOwningAbilitySystemComponent();
    const auto GetBaseValue =
        [AbilitySystemComponent](const FGameplayAttribute& Attribute, float FallbackValue)
        {
            return AbilitySystemComponent
                ? AbilitySystemComponent->GetNumericAttributeBase(Attribute)
                : FallbackValue;
        };

    Snapshot.Health = AttributeSet->GetHealth();
    Snapshot.MaxHealth = GetBaseValue(
        UMyAttributeSet::GetMaxHealthAttribute(),
        AttributeSet->GetMaxHealth());
    Snapshot.AttackPower = GetBaseValue(
        UMyAttributeSet::GetAttackPowerAttribute(),
        AttributeSet->GetAttackPower());
    Snapshot.Defense = GetBaseValue(
        UMyAttributeSet::GetDefenseAttribute(),
        AttributeSet->GetDefense());
    Snapshot.MoveSpeed = GetBaseValue(
        UMyAttributeSet::GetMoveSpeedAttribute(),
        AttributeSet->GetMoveSpeed());
    Snapshot.Shield = AttributeSet->GetShield();
    Snapshot.CritChance = GetBaseValue(
        UMyAttributeSet::GetCritChanceAttribute(),
        AttributeSet->GetCritChance());
    Snapshot.CritDamage = GetBaseValue(
        UMyAttributeSet::GetCritDamageAttribute(),
        AttributeSet->GetCritDamage());
    Snapshot.AttackSpeed = GetBaseValue(
        UMyAttributeSet::GetAttackSpeedAttribute(),
        AttributeSet->GetAttackSpeed());
    Snapshot.CooldownReduction = GetBaseValue(
        UMyAttributeSet::GetCooldownReductionAttribute(),
        AttributeSet->GetCooldownReduction());
    Snapshot.MaxMoveCharge = GetBaseValue(
        UMyAttributeSet::GetMaxMoveChargeAttribute(),
        AttributeSet->GetMaxMoveCharge());
    Snapshot.MoveCharge = AttributeSet->GetMoveCharge();
    Snapshot.CurseGauge = AttributeSet->GetCurseGauge();

    return Snapshot;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// ASC에 적용된 스킬 쿨타임 GameplayEffect의 태그와 남은 시간을 저장하는 함수
// AbilitySystemComponent : 스킬 쿨타임을 조회할 AbilitySystemComponent
// 반환값 : 재접속 복구용 스킬 쿨타임 스냅샷 배열
TArray<FDungeonSkillCooldownSnapshot> ACPP_DungeonGM::MakeDungeonSkillCooldownSnapshot(
    const UAbilitySystemComponent* AbilitySystemComponent) const
{
    TArray<FDungeonSkillCooldownSnapshot> CooldownSnapshots;
    if (!AbilitySystemComponent)
    {
        return CooldownSnapshots;
    }

    const FGameplayTag SkillCooldownRootTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Cooldown.Skill")),
        false);
    const UWorld* World = AbilitySystemComponent->GetWorld();
    if (!SkillCooldownRootTag.IsValid() || !World)
    {
        return CooldownSnapshots;
    }

    TMap<FGameplayTag, float> RemainingSecondsByTag;
    const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles =
        AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery());
    for (const FActiveGameplayEffectHandle& ActiveEffectHandle : ActiveEffectHandles)
    {
        const FActiveGameplayEffect* ActiveEffect =
            AbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);
        if (!ActiveEffect)
        {
            continue;
        }

        const float RemainingSeconds = ActiveEffect->GetTimeRemaining(World->GetTimeSeconds());
        if (RemainingSeconds <= 0.0f)
        {
            continue;
        }

        FGameplayTagContainer GrantedTags;
        ActiveEffect->Spec.GetAllGrantedTags(GrantedTags);
        for (const FGameplayTag& GrantedTag : GrantedTags)
        {
            if (!GrantedTag.IsValid()
                || GrantedTag == SkillCooldownRootTag
                || !GrantedTag.MatchesTag(SkillCooldownRootTag))
            {
                continue;
            }

            float& StoredRemainingSeconds = RemainingSecondsByTag.FindOrAdd(GrantedTag);
            StoredRemainingSeconds = FMath::Max(StoredRemainingSeconds, RemainingSeconds);
        }
    }

    CooldownSnapshots.Reserve(RemainingSecondsByTag.Num());
    for (const TPair<FGameplayTag, float>& CooldownPair : RemainingSecondsByTag)
    {
        FDungeonSkillCooldownSnapshot CooldownSnapshot;
        CooldownSnapshot.CooldownTag = CooldownPair.Key;
        CooldownSnapshot.RemainingSeconds = CooldownPair.Value;
        CooldownSnapshots.Add(CooldownSnapshot);
    }

    return CooldownSnapshots;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 월드의 맵 이름을 던전 재접속 단계 Enum으로 변환하는 함수
// OutDungeonStep : 현재 맵에 해당하는 던전 재접속 단계
// Return Value : 현재 맵 이름을 던전 단계로 변환했으면 true, 아니면 false
bool ACPP_DungeonGM::ResolveCurrentDungeonStep(EDungeonReconnectStep& OutDungeonStep) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FString MapName = World->GetMapName();
    if (!World->StreamingLevelsPrefix.IsEmpty())
    {
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
    }

    if (MapName.Equals(TEXT("Map_Stage1"), ESearchCase::IgnoreCase))
    {
        OutDungeonStep = EDungeonReconnectStep::Stage1;
        return true;
    }

    if (MapName.Equals(TEXT("Map_Stage2"), ESearchCase::IgnoreCase))
    {
        OutDungeonStep = EDungeonReconnectStep::Stage2;
        return true;
    }

    if (MapName.Equals(TEXT("Map_Boss"), ESearchCase::IgnoreCase))
    {
        OutDungeonStep = EDungeonReconnectStep::Boss;
        return true;
    }

    OutDungeonStep = EDungeonReconnectStep::Stage1;
    return false;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 스냅샷이 있으면 새 PlayerController에 복구된 Pawn과 전투 데이터를 연결하는 함수
// NewPlayer : 재접속 인증이 완료된 새 PlayerController
// UserIndex : 재접속 복구를 시도할 유저의 DB user_Index
// bOutRestored : 재접속 스냅샷을 실제로 복구했으면 true
// Return Value : 스냅샷이 없거나 복구에 성공하면 true, 복구 대상이 있는데 실패하면 false
bool ACPP_DungeonGM::RestoreDisconnectedPlayer(APlayerController* NewPlayer, int32 UserIndex, bool& bOutRestored)
{
    bOutRestored = false;

    if (!NewPlayer || UserIndex <= 0)
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UDungeonReconnectSubsystem* ReconnectSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UDungeonReconnectSubsystem>()
        : nullptr;
    if (!ReconnectSubsystem)
    {
        return true;
    }

    FDungeonReconnectSnapshot Snapshot;
    if (!ReconnectSubsystem->FindReconnectSnapshot(UserIndex, Snapshot))
    {
        return true;
    }

    FTransform SpawnTransform = FTransform::Identity;
    if (!ResolveReconnectSpawnTransform(NewPlayer, Snapshot, SpawnTransform))
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because spawn transform could not be resolved. UserIndex: %d"), UserIndex);
        return false;
    }

    APawn* RestoredPawn = ResolveReconnectPawn(NewPlayer, Snapshot, SpawnTransform);
    if (!RestoredPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because pawn could not be resolved. UserIndex: %d"), UserIndex);
        return false;
    }

    APawn* CurrentPawn = NewPlayer->GetPawn();
    if (CurrentPawn && CurrentPawn != RestoredPawn)
    {
        NewPlayer->UnPossess();
        CurrentPawn->Destroy();
    }

    if (AController* ExistingController = RestoredPawn->GetController())
    {
        if (ExistingController != NewPlayer)
        {
            ExistingController->UnPossess();
        }
    }

    RestoredPawn->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    SetPawnReconnectInactive(RestoredPawn, false);
    NewPlayer->Possess(RestoredPawn);

    AMyPlayerState* MyPlayerState = NewPlayer->GetPlayerState<AMyPlayerState>();
    UMyInventoryComponent* InventoryComponent = MyPlayerState ? MyPlayerState->GetInventoryComponent() : nullptr;
    if (!InventoryComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because InventoryComponent is missing. UserIndex: %d"), UserIndex);
        return false;
    }
    if (!InventoryComponent->RestoreReconnectInventory(
        Snapshot.InventoryEntries,
        Snapshot.QuickSlotItemIds,
        Snapshot.Meso))
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because inventory data could not be applied. UserIndex: %d"), UserIndex);
        return false;
    }

    UAbilitySystemComponent* AbilitySystemComponent =
        MyPlayerState ? MyPlayerState->GetAbilitySystemComponent() : nullptr;
    const double ElapsedSeconds = Snapshot.SavedServerTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - Snapshot.SavedServerTimeSeconds)
        : 0.0;
    if (!InventoryComponent->RestoreReconnectItemStatEffects(
        AbilitySystemComponent,
        Snapshot.ItemStatEffects,
        static_cast<float>(ElapsedSeconds)))
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because item stat effects could not be applied. UserIndex: %d"), UserIndex);
        return false;
    }

    // 최대 체력 버프가 남아 있다면 먼저 GE를 복원해야 현재 체력이 버프 포함 최대 체력으로 보정된다.
    if (!ApplyDungeonAttributeSnapshot(MyPlayerState, Snapshot.AttributeSnapshot))
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because AttributeSnapshot could not be applied. UserIndex: %d"), UserIndex);
        return false;
    }

    if (!ApplyDungeonSkillCooldownSnapshot(
        AbilitySystemComponent,
        Snapshot))
    {
        UE_LOG(LogTemp, Warning, TEXT("Reconnect restore failed because skill cooldowns could not be applied. UserIndex: %d"), UserIndex);
        return false;
    }

    if (MyPlayerState)
    {
        if (!Snapshot.Username.IsEmpty())
        {
            MyPlayerState->SetAuthenticatedUser(Snapshot.UserIndex, Snapshot.Username);
        }
        MyPlayerState->SetSelectedCharacterId(Snapshot.SelectedCharacterId);
    }

    ClearDisconnectedPawnTracking(UserIndex);
    ReconnectSubsystem->RemoveReconnectSnapshot(UserIndex);
    bOutRestored = true;

    UE_LOG(LogTemp, Log, TEXT("Reconnect restore succeeded. UserIndex: %d, CharacterId: %d, Pawn: %s"),
        UserIndex,
        Snapshot.SelectedCharacterId,
        *GetNameSafe(RestoredPawn));

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 복구에 사용할 기존 Pawn을 찾거나 스냅샷 클래스 기준으로 새 Pawn을 스폰하는 함수
// NewPlayer : 재접속 인증이 완료된 새 PlayerController
// Snapshot : 재접속 복구 스냅샷
// SpawnTransform : 복구할 위치와 회전
// Return Value : 복구에 사용할 Pawn
APawn* ACPP_DungeonGM::ResolveReconnectPawn(APlayerController* NewPlayer, const FDungeonReconnectSnapshot& Snapshot, const FTransform& SpawnTransform)
{
    if (!NewPlayer)
    {
        return nullptr;
    }

    EDungeonReconnectStep CurrentDungeonStep = EDungeonReconnectStep::Stage1;
    const bool bSameDungeonStep = ResolveCurrentDungeonStep(CurrentDungeonStep) && CurrentDungeonStep == Snapshot.SavedDungeonStep;
    if (bSameDungeonStep)
    {
        const TWeakObjectPtr<APawn>* ExistingPawnPtr = DisconnectedPawns.Find(Snapshot.UserIndex);
        APawn* ExistingPawn = ExistingPawnPtr ? ExistingPawnPtr->Get() : nullptr;
        if (IsValid(ExistingPawn))
        {
            return ExistingPawn;
        }
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    UClass* PawnClass = Snapshot.CharacterClass.LoadSynchronous();
    if (!PawnClass)
    {
        PawnClass = DefaultPawnClass;
    }

    if (!PawnClass || !PawnClass->IsChildOf(APawn::StaticClass()))
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = NewPlayer;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    return World->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParameters);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 복귀 위치를 현재 던전 단계와 스냅샷 단계 기준으로 결정하는 함수
// NewPlayer : 재접속 인증이 완료된 새 PlayerController
// Snapshot : 재접속 복구 스냅샷
// OutTransform : 결정된 복귀 위치와 회전
// Return Value : 복귀 위치 결정 성공 여부
bool ACPP_DungeonGM::ResolveReconnectSpawnTransform(APlayerController* NewPlayer, const FDungeonReconnectSnapshot& Snapshot, FTransform& OutTransform)
{
    EDungeonReconnectStep CurrentDungeonStep = EDungeonReconnectStep::Stage1;
    if (ResolveCurrentDungeonStep(CurrentDungeonStep) && CurrentDungeonStep == Snapshot.SavedDungeonStep)
    {
        OutTransform = Snapshot.SavedTransform;
        return true;
    }

    AActor* StartSpot = FindPlayerStart(NewPlayer);
    if (StartSpot)
    {
        OutTransform = StartSpot->GetActorTransform();
        return true;
    }

    OutTransform = Snapshot.SavedTransform;
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 저장된 Attribute 스냅샷 값을 새 PlayerState의 AttributeSet에 적용하는 함수
// MyPlayerState : AttributeSet을 소유한 PlayerState
// Snapshot : 적용할 Attribute 스냅샷
// Return Value : Attribute 적용 성공 여부
bool ACPP_DungeonGM::ApplyDungeonAttributeSnapshot(AMyPlayerState* MyPlayerState, const FDungeonAttributeSnapshot& Snapshot) const
{
    UMyAttributeSet* AttributeSet = MyPlayerState ? MyPlayerState->GetMyAttributeSet() : nullptr;
    if (!AttributeSet)
    {
        return false;
    }

    // 레벨을 먼저 복원해 이후 폰 Possess 시 레벨 스탯 재적용이 스냅샷과 같은 레벨 기준으로 동작하게 한다.
    MyPlayerState->SetCharacterLevel(Snapshot.CharacterLevel);
    MyPlayerState->SetCharacterExp(Snapshot.CharacterExp);

    AttributeSet->SetMaxHealth(Snapshot.MaxHealth);
    AttributeSet->SetHealth(Snapshot.Health);
    AttributeSet->SetAttackPower(Snapshot.AttackPower);
    AttributeSet->SetDefense(Snapshot.Defense);
    AttributeSet->SetMoveSpeed(Snapshot.MoveSpeed);
    AttributeSet->SetShield(Snapshot.Shield);
    AttributeSet->SetCritChance(Snapshot.CritChance);
    AttributeSet->SetCritDamage(Snapshot.CritDamage);
    AttributeSet->SetAttackSpeed(Snapshot.AttackSpeed);
    AttributeSet->SetCooldownReduction(Snapshot.CooldownReduction);
    AttributeSet->SetMaxMoveCharge(Snapshot.MaxMoveCharge);
    AttributeSet->SetMoveCharge(Snapshot.MoveCharge);
    AttributeSet->SetCurseGauge(Snapshot.CurseGauge);

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 저장된 스킬 쿨타임에서 접속 종료 후 경과 시간을 차감해 새 ASC에 적용하는 함수
// AbilitySystemComponent : 스킬 쿨타임을 적용할 새 PlayerState의 AbilitySystemComponent
// Snapshot : 저장 시각과 스킬 쿨타임 목록을 가진 재접속 스냅샷
// 반환값 : 남아 있는 모든 스킬 쿨타임을 적용했거나 적용할 쿨타임이 없으면 true
bool ACPP_DungeonGM::ApplyDungeonSkillCooldownSnapshot(
    UAbilitySystemComponent* AbilitySystemComponent,
    const FDungeonReconnectSnapshot& Snapshot) const
{
    if (Snapshot.SkillCooldowns.IsEmpty())
    {
        return true;
    }

    if (!AbilitySystemComponent)
    {
        return false;
    }

    const double ElapsedSeconds = Snapshot.SavedServerTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - Snapshot.SavedServerTimeSeconds)
        : 0.0;

    bool bAppliedAllCooldowns = true;
    for (const FDungeonSkillCooldownSnapshot& CooldownSnapshot : Snapshot.SkillCooldowns)
    {
        const float RemainingSeconds =
            CooldownSnapshot.RemainingSeconds - static_cast<float>(ElapsedSeconds);
        if (!CooldownSnapshot.CooldownTag.IsValid() || RemainingSeconds <= 0.0f)
        {
            continue;
        }

        AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(
            FGameplayTagContainer(CooldownSnapshot.CooldownTag));

        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            UMyCooldownGameplayEffect::StaticClass(),
            1.0f,
            EffectContext);
        if (!SpecHandle.IsValid())
        {
            bAppliedAllCooldowns = false;
            continue;
        }

        SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, RemainingSeconds);
        SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownSnapshot.CooldownTag);
        if (!AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).IsValid())
        {
            bAppliedAllCooldowns = false;
        }
    }

    return bAppliedAllCooldowns;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 복구가 끝난 유저의 Pawn 추적 정보와 비활성화 타이머를 정리하는 함수
// UserIndex : 정리할 유저의 DB user_Index
void ACPP_DungeonGM::ClearDisconnectedPawnTracking(int32 UserIndex)
{
    UWorld* World = GetWorld();
    if (World)
    {
        if (FTimerHandle* TimerHandle = DisconnectedPawnDisableTimerHandles.Find(UserIndex))
        {
            World->GetTimerManager().ClearTimer(*TimerHandle);
        }
    }

    DisconnectedPawnDisableTimerHandles.Remove(UserIndex);
    DisconnectedPawns.Remove(UserIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 연결이 끊긴 플레이어 Pawn을 일정 시간 뒤 재접속 대기 비활성 상태로 바꾸는 타이머를 예약하는 함수
// UserIndex : 연결이 끊긴 유저의 DB user_Index
// Pawn : 비활성화할 플레이어 Pawn
void ACPP_DungeonGM::ScheduleDisconnectedPawnDisable(int32 UserIndex, APawn* Pawn)
{
    if (UserIndex <= 0 || !Pawn)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (FTimerHandle* ExistingTimerHandle = DisconnectedPawnDisableTimerHandles.Find(UserIndex))
    {
        World->GetTimerManager().ClearTimer(*ExistingTimerHandle);
    }

    DisconnectedPawns.Add(UserIndex, Pawn);

    if (DisconnectedPawnDisableDelaySeconds <= 0.0f)
    {
        DisableDisconnectedPawn(UserIndex);
        return;
    }

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ACPP_DungeonGM::DisableDisconnectedPawn, UserIndex);
    World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, DisconnectedPawnDisableDelaySeconds, false);
    DisconnectedPawnDisableTimerHandles.Add(UserIndex, TimerHandle);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 대기 시간이 지난 플레이어 Pawn을 인게임 영향에서 제외하는 함수
// UserIndex : 비활성화할 유저의 DB user_Index
void ACPP_DungeonGM::DisableDisconnectedPawn(int32 UserIndex)
{
    DisconnectedPawnDisableTimerHandles.Remove(UserIndex);

    const TWeakObjectPtr<APawn>* PawnPtr = DisconnectedPawns.Find(UserIndex);
    APawn* Pawn = PawnPtr ? PawnPtr->Get() : nullptr;
    if (!IsValid(Pawn))
    {
        DisconnectedPawns.Remove(UserIndex);
        return;
    }

    SetPawnReconnectInactive(Pawn, true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 대기 Pawn의 표시, 충돌, 이동, Tick, 데미지 가능 상태를 변경하는 함수
// Pawn : 상태를 변경할 플레이어 Pawn
// bInactive : true이면 인게임 영향에서 제외, false이면 일반 상태로 복구
void ACPP_DungeonGM::SetPawnReconnectInactive(APawn* Pawn, bool bInactive)
{
    if (!Pawn)
    {
        return;
    }

    Pawn->SetActorHiddenInGame(bInactive);
    Pawn->SetActorEnableCollision(!bInactive);
    Pawn->SetCanBeDamaged(!bInactive);
    Pawn->SetActorTickEnabled(!bInactive);

    TArray<UActorComponent*> Components;
    Pawn->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (Component)
        {
            Component->SetComponentTickEnabled(!bInactive);
        }
    }

    if (ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
        {
            if (bInactive)
            {
                MovementComponent->DisableMovement();
            }
            else
            {
                MovementComponent->SetMovementMode(MOVE_Walking);
            }
        }
    }

    if (APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(Pawn))
    {
        PlayerCharacter->SetReconnectInactive(bInactive);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 플레이어를 InGame 목록에서 OutGame 목록으로 이동시키는 함수
// ExitingPlayerState : 연결이 끊긴 플레이어의 PlayerState
// Return Value : OutGame 이동 성공 여부
bool ACPP_DungeonGM::MoveAuthenticatedPlayerToOutGame(APlayerState* ExitingPlayerState)
{
    if (!bDungeonSessionActive)
    {
        return false;
    }

    const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(ExitingPlayerState);
    if (!MyPlayerState || !MyPlayerState->IsAuthVerified() || MyPlayerState->GetUserIndex() <= 0)
    {
        return false;
    }

    const int32 UserIndex = MyPlayerState->GetUserIndex();
    const int32 RemovedCount = InGameUserIndexes.Remove(UserIndex);
    if (RemovedCount <= 0)
    {
        return false;
    }

    OutGameUserIndexes.Add(UserIndex);
    ReportDungeonMemberState(UserIndex, TEXT("OutGame"));

    UE_LOG(LogTemp, Log, TEXT("Dungeon user moved OutGame. UserIndex: %d, InGame: %d, OutGame: %d"), UserIndex, InGameUserIndexes.Num(), OutGameUserIndexes.Num());
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 안에 남은 InGame 유저가 없으면 세션을 종료하고 OutGame 기록을 정리하는 함수
void ACPP_DungeonGM::EndDungeonSessionIfNoInGameUsers()
{
    if (!bDungeonSessionActive || InGameUserIndexes.Num() > 0)
    {
        return;
    }

    ScheduleEmptyDungeonShutdown();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버에 활성 클라이언트가 없을 때 종료 타이머를 예약하는 함수
void ACPP_DungeonGM::ScheduleEmptyDungeonShutdown()
{
    if (CurrentPlayerCount > 0)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (EmptyDungeonShutdownDelaySeconds <= 0.0f)
    {
        HandleEmptyDungeonShutdownTimer();
        return;
    }

    World->GetTimerManager().SetTimer(
        EmptyDungeonShutdownTimerHandle,
        this,
        &ACPP_DungeonGM::HandleEmptyDungeonShutdownTimer,
        EmptyDungeonShutdownDelaySeconds,
        false);

    UE_LOG(LogTemp, Log, TEXT("Empty dungeon shutdown scheduled. Delay: %.1f"), EmptyDungeonShutdownDelaySeconds);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 종료 타이머를 취소하는 함수
void ACPP_DungeonGM::CancelEmptyDungeonShutdown()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(EmptyDungeonShutdownTimerHandle);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 종료 타이머가 끝났을 때 클라이언트 수를 재확인하고 종료를 요청하는 함수
void ACPP_DungeonGM::HandleEmptyDungeonShutdownTimer()
{
    if (CurrentPlayerCount > 0)
    {
        return;
    }

    CleanupDungeonSessionForShutdown();
    RequestDungeonServerShutdown();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 종료 전에 남아 있는 던전 유저 상태를 Offline으로 보고하고 내부 상태를 정리하는 함수
void ACPP_DungeonGM::CleanupDungeonSessionForShutdown()
{
    if (CurrentPlayerCount > 0)
    {
        return;
    }

    TSet<int32> UserIndexesToCleanup;
    for (int32 UserIndex : InGameUserIndexes)
    {
        UserIndexesToCleanup.Add(UserIndex);
    }

    for (int32 UserIndex : OutGameUserIndexes)
    {
        UserIndexesToCleanup.Add(UserIndex);
    }

    for (int32 UserIndex : UserIndexesToCleanup)
    {
        ReportDungeonMemberState(UserIndex, TEXT("Offline"));
    }

    bDungeonSessionActive = false;
    InGameUserIndexes.Reset();
    OutGameUserIndexes.Reset();

    UE_LOG(LogTemp, Log, TEXT("Dungeon session cleaned up before server shutdown."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 실행 중인 던전 서버 세션 ID를 가져오는 함수
// Return Value : 커맨드라인 또는 설정에서 확인한 던전 서버 세션 ID
FString ACPP_DungeonGM::GetRuntimeDungeonSessionId() const
{
    FString DungeonSessionId;
    if (FParse::Value(FCommandLine::Get(), TEXT("DungeonSessionId="), DungeonSessionId))
    {
        DungeonSessionId.TrimStartAndEndInline();
        if (!DungeonSessionId.IsEmpty())
        {
            return DungeonSessionId;
        }
    }

    if (FParse::Value(FCommandLine::Get(), TEXT("DungeonServerAddress="), DungeonSessionId))
    {
        DungeonSessionId.TrimStartAndEndInline();
        if (!DungeonSessionId.IsEmpty())
        {
            return DungeonSessionId;
        }
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    return ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonServerAddress() : TEXT("");
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// GameBackend에 현재 던전 서버 종료를 요청하는 함수
void ACPP_DungeonGM::RequestDungeonServerShutdown()
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString ShutdownUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonShutdownUrl() : TEXT("");
    const FString ServerAuthKey = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonStateServerAuthKey() : TEXT("");
    const FString DungeonSessionId = GetRuntimeDungeonSessionId();
    if (ShutdownUrl.IsEmpty() || ServerAuthKey.IsEmpty() || DungeonSessionId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot request dungeon server shutdown because shutdown URL, auth key, or session ID is empty."));
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("dungeonSessionId"), DungeonSessionId);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(ShutdownUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &ACPP_DungeonGM::HandleDungeonServerShutdownResponse);

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to start dungeon server shutdown request. SessionId: %s"), *DungeonSessionId);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 종료 요청 응답을 처리하는 함수
// Request : 완료된 HTTP 요청
// Response : HTTP 응답
// bWasSuccessful : HTTP 요청 성공 여부
void ACPP_DungeonGM::HandleDungeonServerShutdownResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Dungeon server shutdown request failed."));
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        UE_LOG(LogTemp, Warning, TEXT("Dungeon server shutdown request failed. HTTP %d"), ResponseCode);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Dungeon server shutdown requested."));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 접속 상태를 로그인 서버에 보고하는 함수
// UserIndex : 상태를 보고할 유저의 DB user_Index
// ServerAuthKey : 설정 파일에서 읽은 던전 상태 보고 서버 전용키를 HTTP 헤더로 사용함
// ConnectionState : 보고할 접속 상태 문자열
void ACPP_DungeonGM::ReportDungeonMemberState(int32 UserIndex, const FString& ConnectionState)
{
    if (UserIndex <= 0 || ConnectionState.IsEmpty())
    {
        return;
    }

    const UGameInstance* GameInstance = GetGameInstance();
    const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UServerConfigSubsystem>() : nullptr;
    const FString ReportUrl = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonMemberStateUrl() : TEXT("");
    const FString ServerAuthKey = ServerConfigSubsystem ? ServerConfigSubsystem->GetDungeonStateServerAuthKey() : TEXT("");
    if (ReportUrl.IsEmpty() || ServerAuthKey.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot report dungeon member state because report URL or server auth key is empty. UserIndex: %d, State: %s"), UserIndex, *ConnectionState);
        return;
    }

    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetNumberField(TEXT("userIndex"), UserIndex);
    JsonObject->SetStringField(TEXT("connectionState"), ConnectionState);

    FString DungeonSessionId = GetRuntimeDungeonSessionId();
    if (DungeonSessionId.IsEmpty())
    {
        const UWorld* World = GetWorld();
        const FString MapName = World ? World->GetMapName() : TEXT("");
        DungeonSessionId = FString::Printf(TEXT("%s:%s"), *GetNameSafe(this), *MapName);
    }
    JsonObject->SetStringField(TEXT("dungeonSessionId"), DungeonSessionId);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject, Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(ReportUrl);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Server-Auth"), ServerAuthKey);
    HttpRequest->SetContentAsString(RequestBody);

    TWeakObjectPtr<ACPP_DungeonGM> WeakThis(this);
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis, UserIndex, ConnectionState](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (ACPP_DungeonGM* DungeonGM = WeakThis.Get())
            {
                DungeonGM->HandleReportDungeonMemberStateResponse(Request, Response, bWasSuccessful, UserIndex, ConnectionState);
            }
        });

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to start dungeon member state report. UserIndex: %d, State: %s"), UserIndex, *ConnectionState);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 멤버 접속 상태 보고 응답을 처리하는 함수
// Request : HTTP 요청 객체
// Response : HTTP 응답 객체
// bWasSuccessful : HTTP 요청이 성공적으로 완료되었는지 여부
// UserIndex : 보고 대상 유저의 DB user_Index
// ConnectionState : 보고한 접속 상태 문자열
void ACPP_DungeonGM::HandleReportDungeonMemberStateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 UserIndex, FString ConnectionState)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Dungeon member state report failed. UserIndex: %d, State: %s"), UserIndex, *ConnectionState);
        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        UE_LOG(LogTemp, Warning, TEXT("Dungeon member state report failed. HTTP %d, UserIndex: %d, State: %s"), ResponseCode, UserIndex, *ConnectionState);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Dungeon member state reported. UserIndex: %d, State: %s"), UserIndex, *ConnectionState);
}

////////////////////////////
//! \author HanUl
//! \brief 부활 데이터 옵션을 서버에서 검증하고 비용을 즉시 차감한 뒤 옵션의 지연 부활을 시작한다.
//! \param RequestingController 부활을 요청한 플레이어 컨트롤러
//! \param OptionId 사용할 부활 옵션 ID
//! \param OutResultMessage 치트와 향후 UI 요청 결과에 전달할 설명
//! \return 비용 차감과 부활 예약까지 성공하면 true
bool ACPP_DungeonGM::StartPlayerRevive(
    APlayerController* RequestingController,
    FName OptionId,
    FString& OutResultMessage)
{
    OutResultMessage.Reset();

    if (!HasAuthority() || !bDungeonSessionActive || bReturnToLobbyCountdownInProgress)
    {
        OutResultMessage = TEXT("던전 종료 중에는 부활을 시작할 수 없습니다.");
        return false;
    }

    AMyPlayerState* MyPlayerState = RequestingController
        ? RequestingController->GetPlayerState<AMyPlayerState>()
        : nullptr;
    APlayerCharacterBase* PlayerCharacter = RequestingController
        ? Cast<APlayerCharacterBase>(RequestingController->GetPawn())
        : nullptr;
    if (!MyPlayerState || !PlayerCharacter || !MyPlayerState->IsDead())
    {
        OutResultMessage = TEXT("부활할 사망 플레이어를 찾을 수 없습니다.");
        return false;
    }

    const bool bAuthenticatedInGamePlayer =
        MyPlayerState->IsAuthVerified()
        && InGameUserIndexes.Contains(MyPlayerState->GetUserIndex());
    const bool bDemoPlayer = DemoInitializedPlayerStates.Contains(MyPlayerState);
    if (!bAuthenticatedInGamePlayer && !bDemoPlayer)
    {
        OutResultMessage = TEXT("현재 던전 참가자로 등록된 플레이어가 아닙니다.");
        return false;
    }

    if (AreAllRegisteredPlayersDead())
    {
        OutResultMessage = TEXT("파티가 전멸하여 개인 부활을 시작할 수 없습니다.");
        return false;
    }

    if (PendingDungeonRevives.Contains(MyPlayerState))
    {
        OutResultMessage = TEXT("이미 부활 대기 중입니다.");
        return false;
    }

    const ADungeonGS* DungeonGS = GetGameState<ADungeonGS>();
    const UDungeonReviveDataAsset* ReviveData = DungeonGS ? DungeonGS->GetReviveData() : nullptr;
    if (!ReviveData)
    {
        OutResultMessage = TEXT("DungeonGS에 ReviveDataAsset이 지정되지 않았습니다.");
        return false;
    }

    FString DataError;
    if (!ReviveData->ValidateData(DataError))
    {
        OutResultMessage = FString::Printf(TEXT("부활 데이터가 잘못되었습니다: %s"), *DataError);
        return false;
    }

    const FDungeonReviveOption* ReviveOption = ReviveData->FindOption(OptionId, true);
    if (!ReviveOption)
    {
        OutResultMessage = FString::Printf(TEXT("활성 부활 옵션을 찾을 수 없습니다: '%s'"), *OptionId.ToString());
        return false;
    }

    TSubclassOf<UGameplayEffect> LoadedPostReviveEffectClass;
    if (!ReviveOption->PostReviveEffectClass.IsNull())
    {
        UClass* LoadedEffectClass = ReviveOption->PostReviveEffectClass.LoadSynchronous();
        if (!LoadedEffectClass || !LoadedEffectClass->IsChildOf(UGameplayEffect::StaticClass()))
        {
            OutResultMessage = FString::Printf(
                TEXT("'%s'의 PostReviveEffectClass를 불러올 수 없습니다."),
                *OptionId.ToString());
            return false;
        }
        LoadedPostReviveEffectClass = LoadedEffectClass;
    }

    UMyInventoryComponent* InventoryComponent = MyPlayerState->GetInventoryComponent();
    if (!InventoryComponent)
    {
        OutResultMessage = TEXT("부활 비용을 처리할 인벤토리 컴포넌트가 없습니다.");
        return false;
    }

	if (ReviveOption->MesoCost > 0
		&& !InventoryComponent->TryConsumeMeso(
			ReviveOption->MesoCost,
			MyGameplayTags::Meso_Source_Revive))
    {
        OutResultMessage = FString::Printf(
            TEXT("메소가 부족합니다. 필요: %d, 보유: %d"),
            ReviveOption->MesoCost,
            InventoryComponent->GetMeso());
        return false;
    }

    FPendingDungeonRevive& PendingRevive = PendingDungeonRevives.Add(MyPlayerState);
    PendingRevive.Character = PlayerCharacter;
    PendingRevive.OptionId = ReviveOption->OptionId;
    PendingRevive.ConsumedMeso = ReviveOption->MesoCost;
    PendingRevive.HealthPercent = ReviveOption->ReviveHealthPercent;
    PendingRevive.PostReviveEffectClass = LoadedPostReviveEffectClass;

    const float ReviveDelaySeconds = ReviveOption->ReviveDelaySeconds;
    if (ReviveDelaySeconds <= 0.0f)
    {
        CompletePendingRevive(MyPlayerState);
        OutResultMessage = FString::Printf(
            TEXT("부활 옵션 '%s'을 즉시 실행했습니다. 비용: %d, 체력: %.0f%%"),
            *ReviveOption->OptionId.ToString(),
            ReviveOption->MesoCost,
            ReviveOption->ReviveHealthPercent * 100.0f);
        return MyPlayerState->IsAlive();
    }

    FTimerDelegate ReviveTimerDelegate = FTimerDelegate::CreateUObject(
        this,
        &ThisClass::CompletePendingRevive,
        TWeakObjectPtr<AMyPlayerState>(MyPlayerState));
    GetWorldTimerManager().SetTimer(
        PendingRevive.TimerHandle,
        ReviveTimerDelegate,
        ReviveDelaySeconds,
        false);

    OutResultMessage = FString::Printf(
        TEXT("부활 옵션 '%s' 확정. 비용 %d 차감, %.1f초 후 체력 %.0f%%로 부활합니다."),
        *ReviveOption->OptionId.ToString(),
        ReviveOption->MesoCost,
        ReviveDelaySeconds,
        ReviveOption->ReviveHealthPercent * 100.0f);
    return true;
}

////////////////////////////
//! \author HanUl
//! \brief 부활 대기 종료 시 전멸 여부를 재검증하고 동일 Pawn을 사망 위치에서 부활시킨다.
//! \param PlayerState 대기 세션의 플레이어 상태
//! \return 없음
void ACPP_DungeonGM::CompletePendingRevive(TWeakObjectPtr<AMyPlayerState> PlayerState)
{
    FPendingDungeonRevive* FoundPendingRevive = PendingDungeonRevives.Find(PlayerState);
    if (!FoundPendingRevive)
    {
        return;
    }

    const FPendingDungeonRevive PendingRevive = *FoundPendingRevive;
    PendingDungeonRevives.Remove(PlayerState);

    AMyPlayerState* MyPlayerState = PlayerState.Get();
    APlayerCharacterBase* PlayerCharacter = PendingRevive.Character.Get();
    if (!bDungeonSessionActive
        || bReturnToLobbyCountdownInProgress
        || AreAllRegisteredPlayersDead()
        || !MyPlayerState
        || !MyPlayerState->IsDead()
        || !PlayerCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pending revive cancelled without refund - Option: %s, PlayerState: %s, Cost: %d"),
            *PendingRevive.OptionId.ToString(),
            *GetNameSafe(MyPlayerState),
            PendingRevive.ConsumedMeso);
        return;
    }

    if (!PlayerCharacter->ReviveAtLastDeathLocation(PendingRevive.HealthPercent))
    {
        UE_LOG(LogTemp, Error, TEXT("Pending revive execution failed without refund - Option: %s, Player: %s, Cost: %d"),
            *PendingRevive.OptionId.ToString(),
            *GetNameSafe(PlayerCharacter),
            PendingRevive.ConsumedMeso);
        return;
    }

    if (PendingRevive.PostReviveEffectClass)
    {
        UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetAbilitySystemComponent();
        if (AbilitySystemComponent)
        {
            FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
            EffectContext.AddSourceObject(this);
            const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
                PendingRevive.PostReviveEffectClass,
                1.0f,
                EffectContext);
            if (SpecHandle.IsValid())
            {
                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Player revive completed - Option: %s, Player: %s, HealthPercent: %.2f, Cost: %d"),
        *PendingRevive.OptionId.ToString(),
        *GetNameSafe(PlayerCharacter),
        PendingRevive.HealthPercent,
        PendingRevive.ConsumedMeso);
}

////////////////////////////
//! \author HanUl
//! \brief 한 플레이어의 부활 대기 타이머와 세션을 환불 없이 취소한다.
//! \param PlayerState 취소할 플레이어 상태
//! \return 없음
void ACPP_DungeonGM::CancelPendingRevive(AMyPlayerState* PlayerState)
{
    FPendingDungeonRevive* PendingRevive = PlayerState
        ? PendingDungeonRevives.Find(PlayerState)
        : nullptr;
    if (!PendingRevive)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(PendingRevive->TimerHandle);
    UE_LOG(LogTemp, Warning, TEXT("Pending revive cancelled without refund - Option: %s, PlayerState: %s, Cost: %d"),
        *PendingRevive->OptionId.ToString(),
        *GetNameSafe(PlayerState),
        PendingRevive->ConsumedMeso);
    PendingDungeonRevives.Remove(PlayerState);
}

////////////////////////////
//! \author HanUl
//! \brief 전멸 또는 던전 종료 시 모든 부활 대기 세션을 환불 없이 취소한다.
//! \param 없음
//! \return 없음
void ACPP_DungeonGM::CancelAllPendingRevives()
{
    for (TPair<TWeakObjectPtr<AMyPlayerState>, FPendingDungeonRevive>& RevivePair : PendingDungeonRevives)
    {
        GetWorldTimerManager().ClearTimer(RevivePair.Value.TimerHandle);
        UE_LOG(LogTemp, Warning, TEXT("Pending revive cancelled without refund - Option: %s, PlayerState: %s, Cost: %d"),
            *RevivePair.Value.OptionId.ToString(),
            *GetNameSafe(RevivePair.Key.Get()),
            RevivePair.Value.ConsumedMeso);
    }
    PendingDungeonRevives.Reset();
}

////////////////////////////
//! \author HanUl
//! \brief 던전 참가 PlayerState의 서버 생명 상태 변경을 중복 없이 구독하고 이미 전멸 상태인지 확인한다.
//! \param MyPlayerState 구독할 던전 참가 PlayerState
//! \return 없음
void ACPP_DungeonGM::BindPlayerLifeState(AMyPlayerState* MyPlayerState)
{
    if (!HasAuthority() || !MyPlayerState)
    {
        return;
    }

    MyPlayerState->OnLifeStateChanged.RemoveAll(this);
    MyPlayerState->OnLifeStateChanged.AddUObject(
        this,
        &ThisClass::HandlePlayerLifeStateChanged);

    if (bDungeonSessionActive
        && !bReturnToLobbyCountdownInProgress
        && MyPlayerState->IsDead()
        && AreAllRegisteredPlayersDead())
    {
        CompleteSurrenderVote();
    }
}

////////////////////////////
//! \author HanUl
//! \brief 등록된 플레이어가 Dead로 변경될 때 현재 참가자 전멸 여부를 검사해 기존 항복 완료 경로를 실행한다.
//! \param OldLifeState 변경 전 생명 상태
//! \param NewLifeState 변경 후 생명 상태
//! \return 없음
void ACPP_DungeonGM::HandlePlayerLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState)
{
    (void)OldLifeState;

    if (NewLifeState != EPlayerLifeState::Dead
        || !bDungeonSessionActive
        || bReturnToLobbyCountdownInProgress
        || !AreAllRegisteredPlayersDead())
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("All registered dungeon players are dead. Reusing surrender completion flow."));
    CompleteSurrenderVote();
}

////////////////////////////
//! \author HanUl
//! \brief 현재 DungeonGM에 등록된 인증 InGame 또는 데모 플레이어가 전원 Dead인지 확인하되 설정된 1인 부활 테스트는 제외한다.
//! \param 없음
//! \return 전원이 Dead이고 실제 전멸 처리 대상인 파티면 true
bool ACPP_DungeonGM::AreAllRegisteredPlayersDead() const
{
    const ADungeonGS* DungeonGS = GetGameState<ADungeonGS>();
    if (!DungeonGS)
    {
        return false;
    }

    int32 RegisteredPlayerCount = 0;
    for (APlayerState* ListedPlayerState : DungeonGS->PlayerArray)
    {
        const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(ListedPlayerState);
        if (!MyPlayerState)
        {
            continue;
        }

        const bool bAuthenticatedInGamePlayer =
            MyPlayerState->IsAuthVerified()
            && InGameUserIndexes.Contains(MyPlayerState->GetUserIndex());
        const bool bDemoPlayer = DemoInitializedPlayerStates.Contains(ListedPlayerState);
        if (!bAuthenticatedInGamePlayer && !bDemoPlayer)
        {
            continue;
        }

        ++RegisteredPlayerCount;
        if (MyPlayerState->IsAlive())
        {
            return false;
        }
    }

    const int32 MinimumPartyWipePlayerCount = bAllowSoloReviveTesting ? 2 : 1;
    return RegisteredPlayerCount >= MinimumPartyWipePlayerCount;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 투표 시작이 거절된 사유를 요청자 한 명에게만 Notice로 전달하는 함수
// TargetController : Notice를 받을 플레이어 컨트롤러
// Message : 표시할 안내 문구
void ACPP_DungeonGM::SendVoteNotice(APlayerController* TargetController, const FText& Message) const
{
    if (ADungeonPC* DungeonPC = Cast<ADungeonPC>(TargetController))
    {
        DungeonPC->SendNoticeToClient(Message, 0.0f);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표를 부결 처리하는 함수
// ReasonMessage : UI에 표시할 부결 사유 메시지
void ACPP_DungeonGM::FailSurrenderVote(const FString& ReasonMessage)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(SurrenderVoteTimerHandle);
        World->GetTimerManager().ClearTimer(ReturnToLobbyTimerHandle);
        if (ActivePartyVoteType == EDungeonPartyVoteType::Surrender)
        {
            LastSurrenderVoteFailedTime = World->GetTimeSeconds();
        }
    }

    bSurrenderVoteInProgress = false;
    bReturnToLobbyCountdownInProgress = false;
    ReturnToLobbyServerTime = 0.0f;

    UpdateSurrenderVoteState(ReasonMessage);

    SurrenderAgreePlayerStates.Reset();
    SurrenderDisagreeCount = 0;
    SurrenderVoteStartServerTime = 0.0f;
    ActiveGimmickResetVoteContext.Reset();
    GimmickResetVotePassedDelegate.Unbind();
    ActivePartyVoteType = EDungeonPartyVoteType::None;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 현재 진행 중인 공용 투표 종류에 맞는 가결 처리를 실행하는 함수
void ACPP_DungeonGM::CompleteActivePartyVote()
{
    if (ActivePartyVoteType == EDungeonPartyVoteType::Surrender)
    {
        CompleteSurrenderVote();
        return;
    }

    if (ActivePartyVoteType != EDungeonPartyVoteType::GimmickReset)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SurrenderVoteTimerHandle);

    FSimpleDelegate VotePassedDelegate = GimmickResetVotePassedDelegate;
    bSurrenderVoteInProgress = false;
    UpdateSurrenderVoteState(TEXT("기믹 초기화 투표가 가결되었습니다."));

    SurrenderAgreePlayerStates.Reset();
    SurrenderDisagreeCount = 0;
    SurrenderVoteStartServerTime = 0.0f;
    ActiveGimmickResetVoteContext.Reset();
    GimmickResetVotePassedDelegate.Unbind();
    ActivePartyVoteType = EDungeonPartyVoteType::None;

    VotePassedDelegate.ExecuteIfBound();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표를 가결 처리하는 함수
void ACPP_DungeonGM::CompleteSurrenderVote()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

	// 항복 또는 전멸로 던전 종료가 시작되면 확정 비용은 환불하지 않고 대기 부활을 모두 폐기한다.
	CancelAllPendingRevives();
	if (ADungeonGS* DungeonGS = GetGameState<ADungeonGS>())
	{
		if (UMyStreamingManagerComponent* StreamingManager = DungeonGS->GetStreamingManager())
		{
			StreamingManager->StopAntiAFK();
		}
	}

    World->GetTimerManager().ClearTimer(SurrenderVoteTimerHandle);
    World->GetTimerManager().ClearTimer(ReturnToLobbyTimerHandle);

    ActivePartyVoteType = EDungeonPartyVoteType::Surrender;
    ActiveGimmickResetVoteContext.Reset();
    GimmickResetVotePassedDelegate.Unbind();
    bSurrenderVoteInProgress = false;
    bReturnToLobbyCountdownInProgress = true;
    ReturnToLobbyServerTime = World->GetTimeSeconds() + ReturnToLobbyDelaySeconds;

    UE_LOG(LogTemp, Log, TEXT("Surrender vote passed. Agree: %d / Required: %d, TravelDelay: %.1f"), GetSurrenderAgreeCount(), GetSurrenderRequiredPlayerCount(), ReturnToLobbyDelaySeconds);
    UpdateSurrenderVoteState(TEXT("항복 투표가 가결되었습니다."));

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ADungeonPC* DungeonPC = Cast<ADungeonPC>(It->Get());
        if (!DungeonPC)
        {
            continue;
        }

        if (ReturnToLobbyDelaySeconds > 0.0f)
        {
            DungeonPC->SendCountdownNoticeToClient(
                NSLOCTEXT(
                    "DungeonSurrender",
                    "VotePassedCountdown",
                    "항복 투표가 가결되었습니다. {0}초 후 로비로 이동합니다."),
                ReturnToLobbyServerTime);
        }
        else
        {
            DungeonPC->SendNoticeToClient(
                NSLOCTEXT(
                    "DungeonSurrender",
                    "VotePassed",
                    "항복 투표가 가결되었습니다."),
                0.0f);
        }
    }

    if (ReturnToLobbyDelaySeconds <= 0.0f)
    {
        HandleReturnToLobbyCountdownFinished();
        return;
    }

    World->GetTimerManager().SetTimer(ReturnToLobbyTimerHandle, this, &ACPP_DungeonGM::HandleReturnToLobbyCountdownFinished, ReturnToLobbyDelaySeconds, false);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 제한 시간이 끝났을 때 부결 처리하는 함수
void ACPP_DungeonGM::HandleSurrenderVoteTimeout()
{
    const FString TimeoutMessage = ActivePartyVoteType == EDungeonPartyVoteType::GimmickReset
        ? TEXT("제한 시간 안에 전원이 찬성하지 않아 기믹 초기화 투표가 부결되었습니다.")
        : TEXT("제한 시간 안에 전원이 찬성하지 않아 항복 투표가 부결되었습니다.");
    FailSurrenderVote(TimeoutMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 가결 후 로비 이동 카운트다운이 끝났을 때 이동을 실행하는 함수
void ACPP_DungeonGM::HandleReturnToLobbyCountdownFinished()
{
    bReturnToLobbyCountdownInProgress = false;
    ReturnToLobbyServerTime = 0.0f;
    SurrenderAgreePlayerStates.Reset();

    TravelAllPlayersToLobby();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 내부 상태를 초기화하는 함수
void ACPP_DungeonGM::ResetSurrenderVote()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(SurrenderVoteTimerHandle);
        World->GetTimerManager().ClearTimer(ReturnToLobbyTimerHandle);
    }

    bSurrenderVoteInProgress = false;
    bReturnToLobbyCountdownInProgress = false;
    SurrenderAgreePlayerStates.Reset();
    SurrenderDisagreeCount = 0;
    SurrenderVoteStartServerTime = 0.0f;
    ReturnToLobbyServerTime = 0.0f;
    ActiveGimmickResetVoteContext.Reset();
    GimmickResetVotePassedDelegate.Unbind();
    ActivePartyVoteType = EDungeonPartyVoteType::None;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 던전 서버에 접속한 모든 플레이어를 로비 서버로 이동시키는 함수
void ACPP_DungeonGM::TravelAllPlayersToLobby()
{
    FString LobbyServerAddress;
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        const UServerConfigSubsystem* ServerConfigSubsystem = GameInstance->GetSubsystem<UServerConfigSubsystem>();
        const FString ConfigLobbyServerAddress = ServerConfigSubsystem ? ServerConfigSubsystem->GetLobbyServerAddress() : TEXT("");
        if (!ConfigLobbyServerAddress.IsEmpty())
        {
            LobbyServerAddress = ConfigLobbyServerAddress;
        }
    }

    if (LobbyServerAddress.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot return dungeon players to lobby because LobbyServerAddress is empty."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (int32 UserIndex : InGameUserIndexes)
    {
        ReportDungeonMemberState(UserIndex, TEXT("Online"));
    }

    bDungeonSessionActive = false;
    InGameUserIndexes.Reset();
    OutGameUserIndexes.Reset();

    UE_LOG(LogTemp, Warning, TEXT("Returning dungeon players to lobby server. Address: %s"), *LobbyServerAddress);
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PlayerController = It->Get();
        if (PlayerController)
        {
            PlayerController->ClientTravel(LobbyServerAddress, TRAVEL_Absolute);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// - Codex : PIE 또는 명시적으로 허용된 데모 플레이어의 서버 투표 참여 조건 추가
// 항복 투표를 제출한 PlayerController에서 인증된 InGame 또는 서버 초기화된 데모 PlayerState를 가져오는 함수
// VotingController : 투표를 제출한 플레이어 컨트롤러
// Return Value : 서버가 인정한 InGame 또는 데모 투표자 PlayerState
APlayerState* ACPP_DungeonGM::GetSurrenderVoterState(APlayerController* VotingController) const
{
    const AMyPlayerState* MyPlayerState = VotingController ? VotingController->GetPlayerState<AMyPlayerState>() : nullptr;
    if (!MyPlayerState)
    {
        return nullptr;
    }

    const bool bAuthenticatedInGamePlayer =
        MyPlayerState->IsAuthVerified() &&
        InGameUserIndexes.Contains(MyPlayerState->GetUserIndex());
    const bool bServerInitializedDemoPlayer = DemoInitializedPlayerStates.Contains(VotingController->PlayerState);
    if (!bAuthenticatedInGamePlayer && !bServerInitializedDemoPlayer)
    {
        return nullptr;
    }

    return VotingController->PlayerState;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 서버가 인정한 항복 찬성 표 수를 반환하는 함수
// Return Value : 유효한 PlayerState 기준 찬성 표 수
int32 ACPP_DungeonGM::GetSurrenderAgreeCount() const
{
    int32 AgreeCount = 0;
    for (const TWeakObjectPtr<APlayerState>& AgreePlayerState : SurrenderAgreePlayerStates)
    {
        if (AgreePlayerState.IsValid())
        {
            ++AgreeCount;
        }
    }

    return AgreeCount;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// - Codex : PIE 또는 명시적으로 허용된 데모 플레이어를 만장일치 필요 인원에 포함
// 항복 투표에 필요한 현재 인증된 InGame 및 서버 초기화된 데모 유저 수를 반환하는 함수
// Return Value : 현재 던전 서버가 투표자로 인정한 전체 유저 수
int32 ACPP_DungeonGM::GetSurrenderRequiredPlayerCount() const
{
    int32 DemoPlayerCount = 0;
    for (const TWeakObjectPtr<APlayerState>& DemoPlayerState : DemoInitializedPlayerStates)
    {
        if (DemoPlayerState.IsValid())
        {
            ++DemoPlayerCount;
        }
    }

    return InGameUserIndexes.Num() + DemoPlayerCount;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 투표 상태를 GameState에 반영하는 함수
// ResultMessage : UI에 표시할 결과 또는 안내 메시지
void ACPP_DungeonGM::UpdateSurrenderVoteState(const FString& ResultMessage)
{
    ADungeonGS* DungeonGS = GetGameState<ADungeonGS>();
    if (!DungeonGS)
    {
        return;
    }

    UWorld* World = GetWorld();
    const float CurrentServerTime = World ? World->GetTimeSeconds() : 0.0f;

    FDungeonSurrenderVoteState NewState;
    NewState.VoteType = ActivePartyVoteType;
    NewState.bVoteInProgress = bSurrenderVoteInProgress;
    NewState.AgreeCount = GetSurrenderAgreeCount();
    NewState.DisagreeCount = SurrenderDisagreeCount;
    NewState.RequiredCount = GetSurrenderRequiredPlayerCount();

    NewState.ResultMessage = ResultMessage;

    if (bSurrenderVoteInProgress && World)
    {
        NewState.VoteStartServerTime = SurrenderVoteStartServerTime;
        NewState.VoteEndServerTime = CurrentServerTime + World->GetTimerManager().GetTimerRemaining(SurrenderVoteTimerHandle);
    }

    const float CooldownEndServerTime = LastSurrenderVoteFailedTime + SurrenderVoteCooldownSeconds;
    if (CurrentServerTime < CooldownEndServerTime)
    {
        NewState.CooldownEndServerTime = CooldownEndServerTime;
    }

    if (bReturnToLobbyCountdownInProgress)
    {
        NewState.LobbyTravelServerTime = ReturnToLobbyServerTime;
    }

    DungeonGS->SetSurrenderVoteState(NewState);
}
