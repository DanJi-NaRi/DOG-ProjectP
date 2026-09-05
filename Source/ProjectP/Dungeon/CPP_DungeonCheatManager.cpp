#include "CPP_DungeonCheatManager.h"

#include "DungeonPC.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 스트레스 테스트용 적 스폰 콘솔 명령을 던전 PlayerController 요청으로 전달하는 함수
// Count : 스폰할 테스트 적 수
void UCPP_DungeonCheatManager::SpawnTestEnemies(int32 Count)
{
    ADungeonPC* DungeonPC = GetDungeonPlayerController();
    if (!DungeonPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnTestEnemies failed. Dungeon PlayerController is not available."));
        return;
    }

    DungeonPC->RequestSpawnTestEnemies(Count);
}



////////////////////////////
//! \author 장효제
//! \brief 비Shipping 로컬 신 채팅과 일반 메신저 UI에 내용 없는 테스트 블록 20개씩을 채운다.
//! \note CSV, Streaming Rule, 메신저 히스토리, 서버 상태를 변경하지 않는다.
void UCPP_DungeonCheatManager::MakeBubbles()
{
#if !UE_BUILD_SHIPPING
    if (ADungeonPC* DungeonPC = GetDungeonPlayerController())
    {
        DungeonPC->MakeTestBubbles(20);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("MakeBubbles failed. Dungeon PlayerController is not available."));
#else
    UE_LOG(LogTemp, Warning, TEXT("MakeBubbles is not available in Shipping."));
#endif
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 CheatManager를 소유한 던전 PlayerController를 반환하는 함수
// Return Value : ADungeonPC 포인터, 던전 PlayerController가 아니면 nullptr
ADungeonPC* UCPP_DungeonCheatManager::GetDungeonPlayerController() const
{
    return Cast<ADungeonPC>(GetPlayerController());
}
