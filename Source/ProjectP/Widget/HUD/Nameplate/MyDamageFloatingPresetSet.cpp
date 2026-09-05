#include "MyDamageFloatingPresetSet.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 프로젝트의 기획 툴 JSON 위치를 새 DataAsset의 기본 가져오기 경로로 지정하는 생성자
UMyDamageFloatingPresetSet::UMyDamageFloatingPresetSet()
{
    JsonFile.FilePath = TEXT("Documents/프로그래밍/툴/DamageFloating/Data/LHJ_Preset1.json");
    NormalJsonFile.FilePath = TEXT("Documents/프로그래밍/툴/DamageFloating/Data/일반.json");
    CriticalJsonFile.FilePath = TEXT("Documents/프로그래밍/툴/DamageFloating/Data/크리티컬.json");
    DotJsonFile.FilePath = TEXT("Documents/프로그래밍/툴/DamageFloating/Data/도트.json");
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// JSON 한 파일의 Normal/Critical/DOT 세트를 검증해 갱신하고, 경로가 비어 있으면 기존 세 파일 가져오기를 수행하는 함수
void UMyDamageFloatingPresetSet::ImportJsonPresets()
{
    if (!JsonFile.FilePath.IsEmpty())
    {
        FDamageNumberPresetSetData ImportedPresetSet;
        FString SetErrorMessage;
        if (!UMyDamageFloatingPresetJsonLibrary::LoadPresetSetFromJsonFile(
                JsonFile.FilePath,
                ImportedPresetSet,
                SetErrorMessage))
        {
            LastImportMessage = FString::Printf(TEXT("가져오기 실패: %s"), *SetErrorMessage);
            UE_LOG(LogTemp, Error, TEXT("Damage floating preset set import failed - %s"), *SetErrorMessage);
            return;
        }

        PresetId = ImportedPresetSet.PresetId;
        DisplayName = ImportedPresetSet.DisplayName;
        SavedAtUtc = ImportedPresetSet.SavedAtUtc;
        NormalPreset = MoveTemp(ImportedPresetSet.Normal);
        CriticalPreset = MoveTemp(ImportedPresetSet.Critical);
        DotPreset = MoveTemp(ImportedPresetSet.Dot);
        LastImportMessage = FString::Printf(
            TEXT("프리셋 세트 가져오기 성공: %s (%s)"),
            *DisplayName,
            *PresetId);
        MarkPackageDirty();
        return;
    }

    FDamageNumberPreset ImportedNormalPreset;
    FDamageNumberPreset ImportedCriticalPreset;
    FDamageNumberPreset ImportedDotPreset;
    FString ErrorMessage;

    if (!UMyDamageFloatingPresetJsonLibrary::LoadPresetFromJsonFile(
            NormalJsonFile.FilePath,
            ImportedNormalPreset,
            ErrorMessage)
        || !UMyDamageFloatingPresetJsonLibrary::LoadPresetFromJsonFile(
            CriticalJsonFile.FilePath,
            ImportedCriticalPreset,
            ErrorMessage)
        || !UMyDamageFloatingPresetJsonLibrary::LoadPresetFromJsonFile(
            DotJsonFile.FilePath,
            ImportedDotPreset,
            ErrorMessage))
    {
        LastImportMessage = FString::Printf(TEXT("가져오기 실패: %s"), *ErrorMessage);
        UE_LOG(LogTemp, Error, TEXT("Damage floating preset import failed - %s"), *ErrorMessage);
        return;
    }

    if (ImportedNormalPreset.DamageType != EDamageNumberDisplayType::Normal
        || ImportedCriticalPreset.DamageType != EDamageNumberDisplayType::Critical
        || ImportedDotPreset.DamageType != EDamageNumberDisplayType::Dot)
    {
        LastImportMessage = TEXT("가져오기 실패: JSON 파일과 데미지 타입의 연결이 올바르지 않습니다.");
        UE_LOG(LogTemp, Error, TEXT("Damage floating preset import failed - mismatched damage type."));
        return;
    }

    NormalPreset = MoveTemp(ImportedNormalPreset);
    CriticalPreset = MoveTemp(ImportedCriticalPreset);
    DotPreset = MoveTemp(ImportedDotPreset);
    PresetId = TEXT("LegacyPresetSet");
    DisplayName = TEXT("Legacy Normal/Critical/DOT");
    SavedAtUtc = NormalPreset.SavedAtUtc;
    LastImportMessage = TEXT("Normal, Critical, DOT JSON 가져오기 성공");
    MarkPackageDirty();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 데미지 표시 타입에 대응하는 프리셋을 반환하는 함수
// DamageType : Normal, Critical 또는 DOT 표시 타입
// Return Value : DataAsset에 저장된 해당 타입의 프리셋
const FDamageNumberPreset& UMyDamageFloatingPresetSet::GetPreset(EDamageNumberDisplayType DamageType) const
{
    switch (DamageType)
    {
    case EDamageNumberDisplayType::Critical:
        return CriticalPreset;

    case EDamageNumberDisplayType::Dot:
        return DotPreset;

    case EDamageNumberDisplayType::Normal:
    default:
        return NormalPreset;
    }
}
