// Fill out your copyright notice in the Description page of Project Settings.


#include "Zone/ManagingSystems/ZoneManager.h"

#include "GAS/MyPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "MyGameplayTags.h"
#include "Streaming/MyStreamingZoneDonationTypes.h"
#include "Streaming/MyStreamingZoneTypes.h"
#include "Zone/ZoneBase.h"

////////////////////////////
//! \author HanUl
//! \brief ZoneManager 기본값과 기본 윈도우 규칙(0:Clear 유지, +1:Entering, +2:Ready, +3:Preparing)을 설정한다.
//! \param
//! \return
AZoneManager::AZoneManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// Clear된 Zone은 다음 Zone이 Active 될 때 Used로 전환되므로 윈도우에서는 Clear를 유지한다.
	WindowRule.Add(0, EZoneState::Clear);
	WindowRule.Add(1, EZoneState::Entering);
	WindowRule.Add(2, EZoneState::Ready);
	WindowRule.Add(3, EZoneState::Preparing);
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 Zone 델리게이트를 바인딩하고 초기 윈도우 상태를 적용한다.
//! \param
//! \return
void AZoneManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	BindZoneDelegates();
	BindPlayerLifeStateDelegates();
	ApplyZoneWindow();
}

////////////////////////////
//! \author HanUl
//! \brief 등록된 Zone 인덱스로 Zone을 조회한다.
//! \param Index OrderedZones 배열 인덱스
//! \return 해당 인덱스의 Zone, 범위를 벗어나면 nullptr
AZoneBase* AZoneManager::GetZoneAt(int32 Index) const
{
	return OrderedZones.IsValidIndex(Index) ? OrderedZones[Index] : nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 등록된 Zone들의 Clear/입장 보고 델리게이트를 바인딩하고, 잘못 등록된 항목을 검증한다.
//! \param
//! \return
void AZoneManager::BindZoneDelegates()
{
	for (int32 i = 0; i < OrderedZones.Num(); ++i)
	{
		AZoneBase* Zone = OrderedZones[i];
		if (!IsValid(Zone))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: OrderedZones[%d] is invalid."), *GetName(), i);
			continue;
		}

		Zone->OnZoneCleared.AddUObject(this, &AZoneManager::HandleZoneCleared);
		Zone->OnZonePlayerEntered.AddUObject(this, &AZoneManager::HandleZonePlayerEntered);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 서버 GameState에 등록된 플레이어들의 생명 상태 변경을 구독한다. 중복 구독은 무시한다.
//! \param
//! \return
void AZoneManager::BindPlayerLifeStateDelegates()
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (auto It = LifeStateBoundPlayers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (!IsValid(MyPlayerState))
		{
			continue;
		}

		const TWeakObjectPtr<AMyPlayerState> WeakPlayerState(MyPlayerState);
		if (LifeStateBoundPlayers.Contains(WeakPlayerState))
		{
			continue;
		}

		MyPlayerState->OnLifeStateChanged.AddUObject(
			this,
			&AZoneManager::HandlePlayerLifeStateChanged);
		LifeStateBoundPlayers.Add(WeakPlayerState);
	}
}

////////////////////////////
//! \author HanUl
//! \brief CurrentZoneIndex 기준 상대거리에 따라 전체 Zone의 상태를 일괄 적용한다.
//! \param
//! \return
void AZoneManager::ApplyZoneWindow()
{
	for (int32 i = 0; i < OrderedZones.Num(); ++i)
	{
		AZoneBase* Zone = OrderedZones[i];
		if (!IsValid(Zone))
		{
			continue;
		}

		const int32 Delta = i - CurrentZoneIndex;
		Zone->ChangeState(ResolveWindowState(Delta));
	}
}

////////////////////////////
//! \author HanUl
//! \brief 상대거리에 해당하는 윈도우 상태를 결정한다. 지나온 Zone은 Used, 규칙에 없는 먼 Zone은 Locked.
//! \param Delta ZoneIndex - CurrentZoneIndex
//! \return 적용할 Zone 상태
EZoneState AZoneManager::ResolveWindowState(int32 Delta) const
{
	if (Delta < 0)
	{
		return EZoneState::Used;
	}

	if (const EZoneState* Found = WindowRule.Find(Delta))
	{
		return *Found;
	}

	return EZoneState::Locked;
}

////////////////////////////
//! \author HanUl
//! \brief 서버 GameState의 현재 생존 플레이어 전원이 Entering Zone에 입장 완료했는지 판정한다.
//! \param
//! \return 생존 플레이어가 한 명 이상이고 전원이 입장했으면 true
bool AZoneManager::AreAllAlivePlayersEntered() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return false;
	}

	int32 AlivePlayerCount = 0;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
		if (!IsValid(MyPlayerState) || !MyPlayerState->IsAlive())
		{
			continue;
		}

		++AlivePlayerCount;
		if (!EnteredPlayers.Contains(TWeakObjectPtr<AMyPlayerState>(MyPlayerState)))
		{
			return false;
		}
	}

	return AlivePlayerCount > 0;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 Entering Zone을 찾아 모든 생존 플레이어의 입장이 끝났으면 Active로 전환한다.
//! \param
//! \return
void AZoneManager::TryActivateEnteringZone()
{
	const int32 EnteringIndex = CurrentZoneIndex + 1;
	AZoneBase* EnteringZone = GetZoneAt(EnteringIndex);
	if (!IsValid(EnteringZone) || EnteringZone->GetZoneState() != EZoneState::Entering)
	{
		return;
	}

	if (!AreAllAlivePlayersEntered())
	{
		return;
	}

	EnteringZone->ChangeState(EZoneState::Active);
	BroadcastZoneStreamingEvent(
		MyGameplayTags::Streaming_Event_Zone_Activated,
		EnteringIndex);

	// 다음 구역 플레이 시작 → 이전 Clear 구역은 Used로 전환
	if (AZoneBase* PreviousZone = GetZoneAt(CurrentZoneIndex))
	{
		PreviousZone->ChangeState(EZoneState::Used);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Zone Clear 보고를 받아 진행 인덱스를 갱신하고 윈도우를 재적용한다. 마지막 Zone이면 전체 Clear를 통지한다.
//! \param ClearedZone Clear된 Zone
//! \return
void AZoneManager::HandleZoneCleared(AZoneBase* ClearedZone)
{
	const int32 ClearedIndex = OrderedZones.IndexOfByKey(ClearedZone);
	if (ClearedIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: cleared zone %s is not registered."), *GetName(), *GetNameSafe(ClearedZone));
		return;
	}

	if (ClearedIndex != CurrentZoneIndex + 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: zone %s cleared out of order. (expected index %d, got %d)"),
			*GetName(), *GetNameSafe(ClearedZone), CurrentZoneIndex + 1, ClearedIndex);
	}

	const int32 OldIndex = CurrentZoneIndex;
	CurrentZoneIndex = ClearedIndex;
	EnteredPlayers.Reset();

	ApplyZoneWindow();
	OnZoneProgressChanged.Broadcast(OldIndex, CurrentZoneIndex);

	if (CurrentZoneIndex == OrderedZones.Num() - 1)
	{
		OnAllZonesCleared.Broadcast();
	}

	// Zone 진행 상태를 먼저 확정한 뒤 부가 시스템에 Clear 사실만 알린다.
	// Donation 데이터/실행 실패는 동기 Broadcast 이후에도 Zone 진행을 되돌리지 않는다.
	if (HasAuthority() && UGameplayMessageSubsystem::HasInstance(this))
	{
		FMyStreamingZoneClearedPayload Payload;
		Payload.ZoneIndex = ClearedIndex;
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(
			MyGameplayTags::Streaming_Channel_Zone,
			Payload);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Entering Zone의 생존 플레이어 입장 보고를 집계하고, 현재 생존 플레이어 전원이 입장하면 Active로 전환한다.
//! \param Zone 플레이어가 진입한 Zone
//! \param EnteredPlayer 진입한 플레이어의 PlayerState
//! \return
void AZoneManager::HandleZonePlayerEntered(AZoneBase* Zone, APlayerState* EnteredPlayer)
{
	AMyPlayerState* MyEnteredPlayer = Cast<AMyPlayerState>(EnteredPlayer);
	const int32 EnteredZoneIndex = OrderedZones.IndexOfByKey(Zone);
	if (IsValid(Zone)
		&& IsValid(MyEnteredPlayer)
		&& EnteredZoneIndex != INDEX_NONE
		&& (Zone->GetZoneState() == EZoneState::Clear
			|| Zone->GetZoneState() == EZoneState::Used))
	{
		BroadcastZoneStreamingEvent(
			MyGameplayTags::Streaming_Event_Zone_ClearedReentered,
			EnteredZoneIndex,
			MyEnteredPlayer->GetUserIndex());
		return;
	}

	const int32 EnteringIndex = CurrentZoneIndex + 1;
	if (!OrderedZones.IsValidIndex(EnteringIndex) || OrderedZones[EnteringIndex] != Zone)
	{
		return;
	}

	if (Zone->GetZoneState() != EZoneState::Entering)
	{
		return;
	}

	if (!IsValid(MyEnteredPlayer) || !MyEnteredPlayer->IsAlive())
	{
		return;
	}

	BindPlayerLifeStateDelegates();
	EnteredPlayers.Add(MyEnteredPlayer);
	TryActivateEnteringZone();
}

////////////////////////////
//! \author 장효제
//! \brief 서버가 확정한 일반 Zone 사실을 기존 Zone GameplayMessage 채널에 발행한다.
//! \param EventTag Activated 또는 ClearedReentered EventTag다.
//! \param ZoneIndex OrderedZones의 0-based 인덱스다.
//! \param InstigatorUserIndex 재진입 플레이어 UserIndex이며 파티 Fact면 INDEX_NONE이다.
void AZoneManager::BroadcastZoneStreamingEvent(
	const FGameplayTag EventTag,
	const int32 ZoneIndex,
	const int32 InstigatorUserIndex) const
{
	if (!HasAuthority() || !EventTag.IsValid() || ZoneIndex < 0
		|| !UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	FMyStreamingZoneEventPayload Payload;
	Payload.EventTag = EventTag;
	Payload.ZoneIndex = ZoneIndex;
	Payload.InstigatorUserIndex = InstigatorUserIndex;
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_Zone_Event,
		Payload);
}

////////////////////////////
//! \author HanUl
//! \brief 플레이어 생명 상태가 바뀌면 현재 생존자 집합을 기준으로 Entering Zone 활성화 조건을 다시 판정한다.
//! \param OldLifeState 변경 전 생명 상태
//! \param NewLifeState 변경 후 생명 상태
//! \return
void AZoneManager::HandlePlayerLifeStateChanged(
	EPlayerLifeState OldLifeState,
	EPlayerLifeState NewLifeState)
{
	if (OldLifeState == NewLifeState)
	{
		return;
	}

	TryActivateEnteringZone();
}
