#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZoneSignalReceiver.generated.h"

class AZoneBase;

UINTERFACE(BlueprintType)
class PROJECTP_API UZoneSignalReceiver : public UInterface
{
	GENERATED_BODY()
};

class PROJECTP_API IZoneSignalReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Zone")
	void OnZoneOpen(AZoneBase* SourceZone);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Zone")
	void OnZoneClose(AZoneBase* SourceZone);
};
