#pragma once

#include "CoreMinimal.h"
#include "PlayerLifeTypes.generated.h"

////////////////////////////
//! \enum EPlayerLifeState
//! \brief 플레이어의 생존 여부를 나타내는 상태다.
UENUM(BlueprintType)
enum class EPlayerLifeState : uint8
{
	Alive UMETA(DisplayName = "Alive"),
	Dead UMETA(DisplayName = "Dead")
};
