#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CPP_BossWindowEventPayload.generated.h"

UCLASS(BlueprintType)
class PROJECTP_API UCPP_BossWindowEventPayload : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Attack")
	FName WindowId = NAME_None;

	//! \brief Telegraph window duration in seconds, used by the telegraph visual to drive its fill progress.
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Attack")
	float TelegraphDuration = 0.0f;
};
