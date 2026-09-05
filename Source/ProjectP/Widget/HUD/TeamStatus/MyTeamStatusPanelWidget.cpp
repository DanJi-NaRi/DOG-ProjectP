#include "MyTeamStatusPanelWidget.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "../../../GAS/MyPlayerState.h"
#include "MyTeamMemberStatusWidget.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// 파티원 캐릭터 슬롯을 고정하고 PlayerState 확인 타이머를 시작하는 함수
void UMyTeamStatusPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    RefreshTeamMembers();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            PlayerStateRefreshTimerHandle,
            this,
            &ThisClass::RefreshTeamMembers,
            FMath::Max(PlayerStateRefreshInterval, 0.1f),
            true);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 위젯 제거 시 PlayerState 확인 타이머를 정리하는 함수
void UMyTeamStatusPanelWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PlayerStateRefreshTimerHandle);
    }

    PlayerStateRefreshTimerHandle.Invalidate();
    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 로컬 캐릭터를 제외한 두 캐릭터 ID를 오름차순으로 슬롯에 한 번만 고정하는 함수
// Return Value : 두 슬롯의 캐릭터 ID 고정 성공 여부
bool UMyTeamStatusPanelWidget::TryInitializeCharacterSlots()
{
    if (bCharacterSlotsInitialized)
    {
        return true;
    }

    const APlayerController* OwningPlayer = GetOwningPlayer();
    const AMyPlayerState* OwningPlayerState = OwningPlayer
        ? OwningPlayer->GetPlayerState<AMyPlayerState>()
        : nullptr;
    if (!OwningPlayerState)
    {
        return false;
    }

    const int32 LocalCharacterId = OwningPlayerState->GetSelectedCharacterId();
    if (LocalCharacterId != 100 &&
        LocalCharacterId != 200 &&
        LocalCharacterId != 300)
    {
        return false;
    }

    constexpr int32 CharacterIds[] = {100, 200, 300};
    int32 SlotIndex = 0;
    for (const int32 CharacterId : CharacterIds)
    {
        if (CharacterId == LocalCharacterId)
        {
            continue;
        }

        if (SlotIndex == 0)
        {
            FirstSlotCharacterId = CharacterId;
        }
        else
        {
            SecondSlotCharacterId = CharacterId;
        }

        ++SlotIndex;
    }

    if (FirstSlotCharacterId < 0 || SecondSlotCharacterId < 0)
    {
        return false;
    }

    if (TeamMemberSlot1)
    {
        TeamMemberSlot1->InitializeCharacterSlot(FirstSlotCharacterId);
    }

    if (TeamMemberSlot2)
    {
        TeamMemberSlot2->InitializeCharacterSlot(SecondSlotCharacterId);
    }

    bCharacterSlotsInitialized = true;
    return true;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 두 고정 슬롯에 현재 접속 중인 파티원 PlayerState를 연결하거나 연결 종료 상태를 반영하는 함수
void UMyTeamStatusPanelWidget::RefreshTeamMembers()
{
    if (!TryInitializeCharacterSlots())
    {
        return;
    }

    RefreshCharacterSlot(
        TeamMemberSlot1,
        FirstSlotCharacterId,
        bFirstSlotWasConnected);
    RefreshCharacterSlot(
        TeamMemberSlot2,
        SecondSlotCharacterId,
        bSecondSlotWasConnected);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// GameState에서 지정 캐릭터 ID를 사용 중인 접속 상태의 파티원 PlayerState를 찾는 함수
// CharacterId : 찾을 캐릭터 ID
// Return Value : 조건에 맞는 PlayerState, 없으면 nullptr
AMyPlayerState* UMyTeamStatusPanelWidget::FindActivePlayerStateByCharacterId(
    int32 CharacterId) const
{
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    if (!GameState)
    {
        return nullptr;
    }

    const APlayerController* OwningPlayer = GetOwningPlayer();
    const AMyPlayerState* OwningPlayerState = OwningPlayer
        ? OwningPlayer->GetPlayerState<AMyPlayerState>()
        : nullptr;

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(PlayerState);
        if (!IsValid(MyPlayerState) ||
            MyPlayerState == OwningPlayerState ||
            MyPlayerState->IsInactive() ||
            !MyPlayerState->IsAuthVerified())
        {
            continue;
        }

        if (MyPlayerState->GetSelectedCharacterId() == CharacterId)
        {
            return MyPlayerState;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 한 고정 슬롯을 동일 캐릭터 ID의 PlayerState에 연결하고 이탈 시 Down 상태로 유지하는 함수
// TeamMemberSlot : 갱신할 파티원 상태 위젯
// CharacterId : 슬롯에 고정된 캐릭터 ID
// bWasConnected : 이전 확인 시점의 연결 상태
void UMyTeamStatusPanelWidget::RefreshCharacterSlot(
    UMyTeamMemberStatusWidget* TeamMemberSlot,
    int32 CharacterId,
    bool& bWasConnected)
{
    if (!TeamMemberSlot)
    {
        return;
    }

    AMyPlayerState* PlayerState =
        FindActivePlayerStateByCharacterId(CharacterId);
    if (PlayerState)
    {
        if (!TeamMemberSlot->IsBoundToPlayerState(PlayerState))
        {
            TeamMemberSlot->BindToPlayerState(PlayerState);
        }

        bWasConnected = true;
        return;
    }

    if (bWasConnected)
    {
        TeamMemberSlot->MarkDisconnected();
        bWasConnected = false;
    }
}
