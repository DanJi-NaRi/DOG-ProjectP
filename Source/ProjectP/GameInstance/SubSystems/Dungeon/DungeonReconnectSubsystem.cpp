#include "DungeonReconnectSubsystem.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 복구용 스냅샷을 UserIndex 기준으로 저장하는 함수
// UserIndex : 스냅샷을 저장할 유저의 DB user_Index
// Snapshot : 저장할 재접속 복구 스냅샷
// Return Value : 저장 성공 여부
bool UDungeonReconnectSubsystem::SaveReconnectSnapshot(int32 UserIndex, const FDungeonReconnectSnapshot& Snapshot)
{
    if (UserIndex <= 0)
    {
        return false;
    }

    FDungeonReconnectSnapshot StoredSnapshot = Snapshot;
    StoredSnapshot.UserIndex = UserIndex;
    StoredSnapshot.bOutGame = true;
    ReconnectSnapshots.Add(UserIndex, StoredSnapshot);

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// UserIndex에 해당하는 재접속 복구 스냅샷을 찾는 함수
// UserIndex : 찾을 유저의 DB user_Index
// OutSnapshot : 찾은 재접속 복구 스냅샷이 복사될 변수
// Return Value : 스냅샷 발견 여부
bool UDungeonReconnectSubsystem::FindReconnectSnapshot(int32 UserIndex, FDungeonReconnectSnapshot& OutSnapshot) const
{
    if (UserIndex <= 0)
    {
        return false;
    }

    const FDungeonReconnectSnapshot* FoundSnapshot = ReconnectSnapshots.Find(UserIndex);
    if (!FoundSnapshot)
    {
        return false;
    }

    OutSnapshot = *FoundSnapshot;
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// UserIndex에 해당하는 재접속 복구 스냅샷을 삭제하는 함수
// UserIndex : 삭제할 유저의 DB user_Index
void UDungeonReconnectSubsystem::RemoveReconnectSnapshot(int32 UserIndex)
{
    if (UserIndex <= 0)
    {
        return;
    }

    ReconnectSnapshots.Remove(UserIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 저장된 모든 재접속 복구 스냅샷을 삭제하는 함수
void UDungeonReconnectSubsystem::ClearReconnectSnapshots()
{
    ReconnectSnapshots.Reset();
}
