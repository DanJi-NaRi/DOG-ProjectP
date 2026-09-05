////////////////////////////
//! \page MyHUDLayout.h
//!
#pragma once

#include "MyActivatableWidget.h"
#include "MyHUDLayout.generated.h"

UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyHUDLayout : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UMyHUDLayout();
};
