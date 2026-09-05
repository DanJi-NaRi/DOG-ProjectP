#pragma once

#include "CoreMinimal.h"
#include "EPlayerControlMode.generated.h"

UENUM(BlueprintType)

enum class EPlayerControlMode : uint8
{
	MouseAim UMETA(DisplayName = "Mouse Aim"),
	Orbit	 UMETA(DisplayName = "Orbit")
};

