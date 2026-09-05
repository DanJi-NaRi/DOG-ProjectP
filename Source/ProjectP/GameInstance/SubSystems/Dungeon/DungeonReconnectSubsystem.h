#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../../../Dungeon/DungeonReconnectTypes.h"
#include "DungeonReconnectSubsystem.generated.h"

UCLASS()
class PROJECTP_API UDungeonReconnectSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    bool SaveReconnectSnapshot(int32 UserIndex, const FDungeonReconnectSnapshot& Snapshot);
    bool FindReconnectSnapshot(int32 UserIndex, FDungeonReconnectSnapshot& OutSnapshot) const;
    void RemoveReconnectSnapshot(int32 UserIndex);
    void ClearReconnectSnapshots();

private:
    UPROPERTY(Transient)
    TMap<int32, FDungeonReconnectSnapshot> ReconnectSnapshots;
};
