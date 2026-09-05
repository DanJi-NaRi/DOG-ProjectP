#include "Streaming/MyLevelContentOverride.h"

#include "EngineUtils.h"

////////////////////////////
//! \author 장효제
//! \brief Tick과 Replication이 필요 없는 순수 데이터 배치용 Actor로 기본값을 설정한다.
AMyLevelContentTableOverride::AMyLevelContentTableOverride()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = false;
}

////////////////////////////
//! \author 장효제
//! \brief World에 배치된 첫 번째 Override 인스턴스를 찾는다. 레벨마다 최대 하나만 배치하는 것을 전제로 한다.
//! \param World 현재 던전 World다.
//! \return 찾은 Override, 없으면 nullptr이다.
AMyLevelContentTableOverride* AMyLevelContentTableOverride::FindForWorld(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AMyLevelContentTableOverride> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}

	return nullptr;
}
