#pragma once

#include "CoreMinimal.h"
#include "CPP_BossTypes.generated.h"

UENUM(BlueprintType)
enum class EBossPhase : uint8
{
	None UMETA(DisplayName = "None"),
	Phase1 UMETA(DisplayName = "Phase 1"),
	Transition UMETA(DisplayName = "Transition"),
	Phase2 UMETA(DisplayName = "Phase 2")
};

UENUM(BlueprintType)
enum class EBossAttackShape : uint8
{
	Circle UMETA(DisplayName = "Circle"),
	Sector UMETA(DisplayName = "Sector"),
	Rectangle UMETA(DisplayName = "Rectangle")
};
