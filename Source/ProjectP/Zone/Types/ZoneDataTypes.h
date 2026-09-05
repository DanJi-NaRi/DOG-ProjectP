// 

#pragma once

#include "CoreMinimal.h"
#include "ZoneDataTypes.generated.h"

UENUM(BlueprintType)
enum class EZoneType : uint8
{
	Starting,
	Battle,
	Puzzle,
	Shop,
	Boss,

};

UENUM(BlueprintType)
enum class EZoneState : uint8 {
	
	Locked,    //잠김
	Preparing, //준비(플레이어 1~2 zone 전 미리 준비)
	Ready,     //직전 Zone이 Active일때 
	Entering,  // Entering
	Active,    // 활성 상태
	Clear, 
	Used,      //사용됨
	ReLocked,  //혹몰
};
