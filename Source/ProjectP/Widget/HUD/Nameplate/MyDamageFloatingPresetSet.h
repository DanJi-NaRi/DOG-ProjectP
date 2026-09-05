#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MyDamageFloatingPreset.h"
#include "MyDamageFloatingPresetSet.generated.h"

////////////////////////////
//! \author 준혁
//! \brief Normal/Critical/DOT가 포함된 JSON 한 파일을 가져와 패키징 가능한 프리셋으로 보관하는 DataAsset.
UCLASS(BlueprintType)
class PROJECTP_API UMyDamageFloatingPresetSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UMyDamageFloatingPresetSet();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "DamageNumber|Import")
    void ImportJsonPresets();

    const FDamageNumberPreset& GetPreset(EDamageNumberDisplayType DamageType) const;

protected:
    UPROPERTY(EditAnywhere, Category = "DamageNumber|Import", meta = (FilePathFilter = "json"))
    FFilePath JsonFile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString PresetId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString SavedAtUtc;

    // schemaVersion 1의 타입별 JSON 자산을 다시 가져와야 할 때 사용하는 기존 경로.
    UPROPERTY(EditAnywhere, Category = "DamageNumber|Import", meta = (FilePathFilter = "json"))
    FFilePath NormalJsonFile;

    UPROPERTY(EditAnywhere, Category = "DamageNumber|Import", meta = (FilePathFilter = "json"))
    FFilePath CriticalJsonFile;

    UPROPERTY(EditAnywhere, Category = "DamageNumber|Import", meta = (FilePathFilter = "json"))
    FFilePath DotJsonFile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Import")
    FString LastImportMessage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset NormalPreset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset CriticalPreset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset DotPreset;
};
