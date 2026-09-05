#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "CPP_DungeonCheatManager.generated.h"

class ADungeonPC;

UCLASS()
class PROJECTP_API UCPP_DungeonCheatManager : public UCheatManager
{
    GENERATED_BODY()

public:
    UFUNCTION(Exec)
    void SpawnTestEnemies(int32 Count);



    UFUNCTION(Exec)
    void MakeBubbles();

private:
    ADungeonPC* GetDungeonPlayerController() const;
};
