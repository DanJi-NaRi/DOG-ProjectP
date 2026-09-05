#include "MyDamageFloatingPreset.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace MyDamageFloatingPresetJson
{
    constexpr int32 LegacySchemaVersion = 1;
    constexpr int32 PresetSetSchemaVersion = 2;

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON 객체에서 필수 문자열 필드를 읽는 함수
    // JsonObject : 필드를 소유한 JSON 객체
    // FieldName : 읽을 필드 이름
    // OutValue : 읽은 문자열을 받을 변수
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 필드 읽기에 성공하면 true, 아니면 false
    bool TryReadString(
        const TSharedPtr<FJsonObject>& JsonObject,
        const TCHAR* FieldName,
        FString& OutValue,
        FString& OutErrorMessage)
    {
        if (!JsonObject.IsValid() || !JsonObject->TryGetStringField(FieldName, OutValue))
        {
            OutErrorMessage = FString::Printf(TEXT("필수 문자열 필드가 없거나 형식이 잘못되었습니다: %s"), FieldName);
            return false;
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON 객체에서 필수 숫자 필드를 읽는 함수
    // JsonObject : 필드를 소유한 JSON 객체
    // FieldName : 읽을 필드 이름
    // OutValue : 읽은 숫자를 받을 변수
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 필드 읽기에 성공하면 true, 아니면 false
    bool TryReadNumber(
        const TSharedPtr<FJsonObject>& JsonObject,
        const TCHAR* FieldName,
        float& OutValue,
        FString& OutErrorMessage)
    {
        double NumberValue = 0.0;
        if (!JsonObject.IsValid() || !JsonObject->TryGetNumberField(FieldName, NumberValue))
        {
            OutErrorMessage = FString::Printf(TEXT("필수 숫자 필드가 없거나 형식이 잘못되었습니다: %s"), FieldName);
            return false;
        }

        OutValue = static_cast<float>(NumberValue);
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON 객체에서 필수 정수 필드를 읽는 함수
    // JsonObject : 필드를 소유한 JSON 객체
    // FieldName : 읽을 필드 이름
    // OutValue : 읽은 정수를 받을 변수
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 정수 필드 읽기에 성공하면 true, 아니면 false
    bool TryReadInteger(
        const TSharedPtr<FJsonObject>& JsonObject,
        const TCHAR* FieldName,
        int32& OutValue,
        FString& OutErrorMessage)
    {
        double NumberValue = 0.0;
        if (!JsonObject.IsValid() || !JsonObject->TryGetNumberField(FieldName, NumberValue))
        {
            OutErrorMessage = FString::Printf(TEXT("필수 정수 필드가 없거나 형식이 잘못되었습니다: %s"), FieldName);
            return false;
        }

        const int64 RoundedValue = FMath::RoundToInt64(NumberValue);
        if (!FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue))
            || RoundedValue < MIN_int32
            || RoundedValue > MAX_int32)
        {
            OutErrorMessage = FString::Printf(TEXT("필드는 int32 범위의 정수여야 합니다: %s"), FieldName);
            return false;
        }

        OutValue = static_cast<int32>(RoundedValue);
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON 객체에서 필수 불리언 필드를 읽는 함수
    // JsonObject : 필드를 소유한 JSON 객체
    // FieldName : 읽을 필드 이름
    // OutValue : 읽은 불리언 값을 받을 변수
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 필드 읽기에 성공하면 true, 아니면 false
    bool TryReadBool(
        const TSharedPtr<FJsonObject>& JsonObject,
        const TCHAR* FieldName,
        bool& OutValue,
        FString& OutErrorMessage)
    {
        if (!JsonObject.IsValid() || !JsonObject->TryGetBoolField(FieldName, OutValue))
        {
            OutErrorMessage = FString::Printf(TEXT("필수 불리언 필드가 없거나 형식이 잘못되었습니다: %s"), FieldName);
            return false;
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON 객체에서 필수 하위 객체를 읽는 함수
    // JsonObject : 필드를 소유한 JSON 객체
    // FieldName : 읽을 하위 객체 필드 이름
    // OutObject : 읽은 하위 객체를 받을 변수
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 하위 객체 읽기에 성공하면 true, 아니면 false
    bool TryReadObject(
        const TSharedPtr<FJsonObject>& JsonObject,
        const TCHAR* FieldName,
        TSharedPtr<FJsonObject>& OutObject,
        FString& OutErrorMessage)
    {
        if (!JsonObject.IsValid() || !JsonObject->HasTypedField<EJson::Object>(FieldName))
        {
            OutErrorMessage = FString::Printf(TEXT("필수 객체 필드가 없거나 형식이 잘못되었습니다: %s"), FieldName);
            return false;
        }

        OutObject = JsonObject->GetObjectField(FieldName);
        return OutObject.IsValid();
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON의 데미지 스타일 문자열을 C++ enum으로 변환하는 함수
    // DamageStyleString : previewDamageStyle 필드 문자열
    // OutDamageType : 변환된 데미지 표시 타입
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 지원하는 스타일이면 true, 아니면 false
    bool TryParseDamageType(
        const FString& DamageStyleString,
        EDamageNumberDisplayType& OutDamageType,
        FString& OutErrorMessage)
    {
        if (DamageStyleString.Equals(TEXT("normal"), ESearchCase::IgnoreCase))
        {
            OutDamageType = EDamageNumberDisplayType::Normal;
            return true;
        }

        if (DamageStyleString.Equals(TEXT("critical"), ESearchCase::IgnoreCase))
        {
            OutDamageType = EDamageNumberDisplayType::Critical;
            return true;
        }

        if (DamageStyleString.Equals(TEXT("dot"), ESearchCase::IgnoreCase))
        {
            OutDamageType = EDamageNumberDisplayType::Dot;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("지원하지 않는 previewDamageStyle입니다: %s"), *DamageStyleString);
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON의 이동 경로 문자열을 C++ enum으로 변환하는 함수
    // PathString : motion.path 필드 문자열
    // OutPath : 변환된 이동 경로 타입
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 지원하는 경로이면 true, 아니면 false
    bool TryParseMotionPath(
        const FString& PathString,
        EDamageNumberMotionPath& OutPath,
        FString& OutErrorMessage)
    {
        if (PathString.Equals(TEXT("linear"), ESearchCase::IgnoreCase))
        {
            OutPath = EDamageNumberMotionPath::Linear;
            return true;
        }

        if (PathString.Equals(TEXT("curve"), ESearchCase::IgnoreCase))
        {
            OutPath = EDamageNumberMotionPath::Curve;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("지원하지 않는 motion.path입니다: %s"), *PathString);
        return false;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // JSON의 x/y 객체를 FVector2D로 변환하는 함수
    // JsonObject : x/y 필드를 가진 JSON 객체
    // OutVector : 변환된 2차원 벡터
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : x/y를 모두 읽었으면 true, 아니면 false
    bool TryReadVector2D(
        const TSharedPtr<FJsonObject>& JsonObject,
        FVector2D& OutVector,
        FString& OutErrorMessage)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (!TryReadNumber(JsonObject, TEXT("x"), X, OutErrorMessage)
            || !TryReadNumber(JsonObject, TEXT("y"), Y, OutErrorMessage))
        {
            return false;
        }

        OutVector = FVector2D(X, Y);
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // #RRGGBB 문자열을 Slate에서 사용할 선형 색상으로 변환하는 함수
    // HexColorString : 변환할 HTML HEX 색상 문자열
    // OutColor : 변환된 선형 색상
    // OutErrorMessage : 실패 원인을 받을 문자열
    // Return Value : 올바른 #RRGGBB 색상이면 true, 아니면 false
    bool TryParseHexColor(
        const FString& HexColorString,
        FLinearColor& OutColor,
        FString& OutErrorMessage)
    {
        FString NormalizedHex = HexColorString.TrimStartAndEnd();
        if (NormalizedHex.StartsWith(TEXT("#")))
        {
            NormalizedHex.RightChopInline(1);
        }

        if (NormalizedHex.Len() != 6)
        {
            OutErrorMessage = FString::Printf(TEXT("색상은 #RRGGBB 형식이어야 합니다: %s"), *HexColorString);
            return false;
        }

        for (const TCHAR Character : NormalizedHex)
        {
            if (!FChar::IsHexDigit(Character))
            {
                OutErrorMessage = FString::Printf(TEXT("색상은 #RRGGBB 형식이어야 합니다: %s"), *HexColorString);
                return false;
            }
        }

        OutColor = FLinearColor::FromSRGBColor(FColor::FromHex(NormalizedHex));
        return true;
    }

    //////////////////////////////////////////////////////////////////////
    // - 준혁 -
    // schemaVersion 2 styles 하위 객체 하나를 런타임 프리셋으로 변환하는 함수
    // StyleObject : normal, critical 또는 dot 스타일 객체
    // ExpectedDamageType : styles 키가 요구하는 데미지 타입
    // SchemaVersion : 루트 JSON 스키마 버전
    // SavedAtUtc : 루트 JSON 저장 시각
    // OutPreset : 변환된 타입별 프리셋
    // OutErrorMessage : 파싱 또는 검증 실패 원인을 받을 문자열
    // Return Value : 스타일 변환과 검증에 성공하면 true, 아니면 false
    bool TryParsePresetStyle(
        const TSharedPtr<FJsonObject>& StyleObject,
        EDamageNumberDisplayType ExpectedDamageType,
        int32 SchemaVersion,
        const FString& SavedAtUtc,
        FDamageNumberPreset& OutPreset,
        FString& OutErrorMessage)
    {
        OutPreset = FDamageNumberPreset();

        FString DamageTypeString;
        FString FontAssetPathString;
        FString TypefaceString;
        FString ColorString;
        FString OutlineColorString;
        FString MotionPathString;
        TSharedPtr<FJsonObject> TextStyleObject;
        TSharedPtr<FJsonObject> MotionObject;
        TSharedPtr<FJsonObject> StartOffsetObject;
        TSharedPtr<FJsonObject> MoveDistanceObject;
        TSharedPtr<FJsonObject> ScaleObject;
        TSharedPtr<FJsonObject> TimingObject;
        TSharedPtr<FJsonObject> OpacityObject;

        if (!TryReadString(StyleObject, TEXT("designName"), OutPreset.DesignName, OutErrorMessage)
            || !TryReadString(StyleObject, TEXT("damageType"), DamageTypeString, OutErrorMessage)
            || !TryParseDamageType(DamageTypeString, OutPreset.DamageType, OutErrorMessage)
            || !TryReadObject(StyleObject, TEXT("textStyle"), TextStyleObject, OutErrorMessage)
            || !TryReadString(TextStyleObject, TEXT("fontAssetPath"), FontAssetPathString, OutErrorMessage)
            || !TryReadInteger(TextStyleObject, TEXT("fontSize"), OutPreset.TextStyle.FontSize, OutErrorMessage)
            || !TryReadString(TextStyleObject, TEXT("typeface"), TypefaceString, OutErrorMessage)
            || !TryReadString(TextStyleObject, TEXT("color"), ColorString, OutErrorMessage)
            || !TryParseHexColor(ColorString, OutPreset.TextStyle.Color, OutErrorMessage)
            || !TryReadInteger(TextStyleObject, TEXT("outlineSize"), OutPreset.TextStyle.OutlineSize, OutErrorMessage)
            || !TryReadString(TextStyleObject, TEXT("outlineColor"), OutlineColorString, OutErrorMessage)
            || !TryParseHexColor(OutlineColorString, OutPreset.TextStyle.OutlineColor, OutErrorMessage)
            || !TryReadObject(StyleObject, TEXT("motion"), MotionObject, OutErrorMessage)
            || !TryReadString(MotionObject, TEXT("path"), MotionPathString, OutErrorMessage)
            || !TryParseMotionPath(MotionPathString, OutPreset.Motion.Path, OutErrorMessage)
            || !TryReadObject(MotionObject, TEXT("startOffset"), StartOffsetObject, OutErrorMessage)
            || !TryReadVector2D(StartOffsetObject, OutPreset.Motion.StartOffset, OutErrorMessage)
            || !TryReadObject(MotionObject, TEXT("moveDistance"), MoveDistanceObject, OutErrorMessage)
            || !TryReadVector2D(MoveDistanceObject, OutPreset.Motion.MoveDistance, OutErrorMessage)
            || !TryReadNumber(MotionObject, TEXT("arcHeight"), OutPreset.Motion.ArcHeight, OutErrorMessage)
            || !TryReadObject(StyleObject, TEXT("scale"), ScaleObject, OutErrorMessage)
            || !TryReadNumber(ScaleObject, TEXT("initial"), OutPreset.Scale.Initial, OutErrorMessage)
            || !TryReadNumber(ScaleObject, TEXT("normal"), OutPreset.Scale.Normal, OutErrorMessage)
            || !TryReadNumber(ScaleObject, TEXT("exit"), OutPreset.Scale.Exit, OutErrorMessage)
            || !TryReadObject(StyleObject, TEXT("timingSeconds"), TimingObject, OutErrorMessage)
            || !TryReadNumber(TimingObject, TEXT("appear"), OutPreset.Timing.Appear, OutErrorMessage)
            || !TryReadNumber(TimingObject, TEXT("hold"), OutPreset.Timing.Hold, OutErrorMessage)
            || !TryReadNumber(TimingObject, TEXT("exit"), OutPreset.Timing.Exit, OutErrorMessage)
            || !TryReadNumber(TimingObject, TEXT("total"), OutPreset.Timing.Total, OutErrorMessage)
            || !TryReadObject(StyleObject, TEXT("opacity"), OpacityObject, OutErrorMessage)
            || !TryReadBool(OpacityObject, TEXT("fadeIn"), OutPreset.Opacity.bFadeIn, OutErrorMessage)
            || !TryReadBool(OpacityObject, TEXT("fadeOut"), OutPreset.Opacity.bFadeOut, OutErrorMessage))
        {
            return false;
        }

        if (OutPreset.DamageType != ExpectedDamageType)
        {
            OutErrorMessage = TEXT("styles 키와 damageType 값이 일치하지 않습니다.");
            return false;
        }

        OutPreset.SchemaVersion = SchemaVersion;
        OutPreset.SavedAtUtc = SavedAtUtc;
        OutPreset.MotionPresetName = MotionPathString;
        OutPreset.TextStyle.FontAssetPath = FSoftObjectPath(FontAssetPathString);
        OutPreset.TextStyle.Typeface = FName(*TypefaceString);
        return UMyDamageFloatingPresetJsonLibrary::ValidatePreset(OutPreset, OutErrorMessage);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기획 툴이 저장한 JSON 파일을 읽어 데미지 플로팅 프리셋으로 변환하는 함수
// JsonFilePath : 읽을 JSON 파일의 절대 또는 프로젝트 기준 경로
// OutPreset : 변환된 프리셋을 받을 변수
// OutErrorMessage : 파일 읽기, 파싱 또는 검증 실패 원인을 받을 문자열
// Return Value : 파일을 읽고 유효한 프리셋으로 변환했으면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::LoadPresetFromJsonFile(
    const FString& JsonFilePath,
    FDamageNumberPreset& OutPreset,
    FString& OutErrorMessage)
{
    const FString ResolvedJsonFilePath = FPaths::IsRelative(JsonFilePath)
        ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), JsonFilePath)
        : JsonFilePath;

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *ResolvedJsonFilePath))
    {
        OutErrorMessage = FString::Printf(TEXT("JSON 파일을 읽지 못했습니다: %s"), *ResolvedJsonFilePath);
        return false;
    }

    return ParsePresetFromJsonString(JsonString, OutPreset, OutErrorMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기획 툴이 저장한 schemaVersion 2 JSON 파일을 Normal/Critical/DOT 프리셋 세트로 변환하는 함수
// JsonFilePath : 읽을 JSON 파일의 절대 또는 프로젝트 기준 경로
// OutPresetSet : 변환된 세 프리셋과 메타데이터를 받을 변수
// OutErrorMessage : 파일 읽기, 파싱 또는 검증 실패 원인을 받을 문자열
// Return Value : 파일을 읽고 유효한 프리셋 세트로 변환했으면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::LoadPresetSetFromJsonFile(
    const FString& JsonFilePath,
    FDamageNumberPresetSetData& OutPresetSet,
    FString& OutErrorMessage)
{
    const FString ResolvedJsonFilePath = FPaths::IsRelative(JsonFilePath)
        ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), JsonFilePath)
        : JsonFilePath;

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *ResolvedJsonFilePath))
    {
        OutErrorMessage = FString::Printf(TEXT("JSON 파일을 읽지 못했습니다: %s"), *ResolvedJsonFilePath);
        return false;
    }

    return ParsePresetSetFromJsonString(JsonString, OutPresetSet, OutErrorMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// JSON 문자열을 데미지 플로팅 프리셋으로 변환하는 함수
// JsonString : 기획 툴에서 저장한 JSON 문자열
// OutPreset : 변환된 프리셋을 받을 변수
// OutErrorMessage : 파싱 또는 검증 실패 원인을 받을 문자열
// Return Value : 유효한 프리셋으로 변환했으면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::ParsePresetFromJsonString(
    const FString& JsonString,
    FDamageNumberPreset& OutPreset,
    FString& OutErrorMessage)
{
    using namespace MyDamageFloatingPresetJson;

    OutPreset = FDamageNumberPreset();
    OutErrorMessage.Reset();

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(JsonReader, RootObject) || !RootObject.IsValid())
    {
        OutErrorMessage = TEXT("JSON 루트 객체를 파싱하지 못했습니다.");
        return false;
    }

    float SchemaVersion = 0.0f;
    FString DamageStyleString;
    FString MotionPathString;
    TSharedPtr<FJsonObject> MotionObject;
    TSharedPtr<FJsonObject> StartOffsetObject;
    TSharedPtr<FJsonObject> MoveDistanceObject;
    TSharedPtr<FJsonObject> ScaleObject;
    TSharedPtr<FJsonObject> TimingObject;
    TSharedPtr<FJsonObject> OpacityObject;

    if (!TryReadNumber(RootObject, TEXT("schemaVersion"), SchemaVersion, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("designName"), OutPreset.DesignName, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("savedAt"), OutPreset.SavedAtUtc, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("previewDamageStyle"), DamageStyleString, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("motionPreset"), OutPreset.MotionPresetName, OutErrorMessage)
        || !TryParseDamageType(DamageStyleString, OutPreset.DamageType, OutErrorMessage)
        || !TryReadObject(RootObject, TEXT("motion"), MotionObject, OutErrorMessage)
        || !TryReadString(MotionObject, TEXT("path"), MotionPathString, OutErrorMessage)
        || !TryParseMotionPath(MotionPathString, OutPreset.Motion.Path, OutErrorMessage)
        || !TryReadObject(MotionObject, TEXT("startOffset"), StartOffsetObject, OutErrorMessage)
        || !TryReadVector2D(StartOffsetObject, OutPreset.Motion.StartOffset, OutErrorMessage)
        || !TryReadObject(MotionObject, TEXT("moveDistance"), MoveDistanceObject, OutErrorMessage)
        || !TryReadVector2D(MoveDistanceObject, OutPreset.Motion.MoveDistance, OutErrorMessage)
        || !TryReadNumber(MotionObject, TEXT("arcHeight"), OutPreset.Motion.ArcHeight, OutErrorMessage)
        || !TryReadObject(RootObject, TEXT("scale"), ScaleObject, OutErrorMessage)
        || !TryReadNumber(ScaleObject, TEXT("initial"), OutPreset.Scale.Initial, OutErrorMessage)
        || !TryReadNumber(ScaleObject, TEXT("normal"), OutPreset.Scale.Normal, OutErrorMessage)
        || !TryReadNumber(ScaleObject, TEXT("exit"), OutPreset.Scale.Exit, OutErrorMessage)
        || !TryReadObject(RootObject, TEXT("timingSeconds"), TimingObject, OutErrorMessage)
        || !TryReadNumber(TimingObject, TEXT("appear"), OutPreset.Timing.Appear, OutErrorMessage)
        || !TryReadNumber(TimingObject, TEXT("hold"), OutPreset.Timing.Hold, OutErrorMessage)
        || !TryReadNumber(TimingObject, TEXT("exit"), OutPreset.Timing.Exit, OutErrorMessage)
        || !TryReadNumber(TimingObject, TEXT("total"), OutPreset.Timing.Total, OutErrorMessage)
        || !TryReadObject(RootObject, TEXT("opacity"), OpacityObject, OutErrorMessage)
        || !TryReadBool(OpacityObject, TEXT("fadeIn"), OutPreset.Opacity.bFadeIn, OutErrorMessage)
        || !TryReadBool(OpacityObject, TEXT("fadeOut"), OutPreset.Opacity.bFadeOut, OutErrorMessage))
    {
        return false;
    }

    OutPreset.SchemaVersion = FMath::RoundToInt(SchemaVersion);
    if (!FMath::IsNearlyEqual(SchemaVersion, static_cast<float>(OutPreset.SchemaVersion)))
    {
        OutErrorMessage = TEXT("schemaVersion은 정수여야 합니다.");
        return false;
    }

    return ValidatePreset(OutPreset, OutErrorMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// schemaVersion 2 JSON 문자열을 Normal/Critical/DOT 프리셋 세트로 변환하는 함수
// JsonString : 기획 툴에서 저장한 JSON 문자열
// OutPresetSet : 변환된 세 프리셋과 메타데이터를 받을 변수
// OutErrorMessage : 파싱 또는 검증 실패 원인을 받을 문자열
// Return Value : 유효한 프리셋 세트로 변환했으면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::ParsePresetSetFromJsonString(
    const FString& JsonString,
    FDamageNumberPresetSetData& OutPresetSet,
    FString& OutErrorMessage)
{
    using namespace MyDamageFloatingPresetJson;

    OutPresetSet = FDamageNumberPresetSetData();
    OutErrorMessage.Reset();

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(JsonReader, RootObject) || !RootObject.IsValid())
    {
        OutErrorMessage = TEXT("JSON 루트 객체를 파싱하지 못했습니다.");
        return false;
    }

    int32 SchemaVersion = 0;
    TSharedPtr<FJsonObject> StylesObject;
    TSharedPtr<FJsonObject> NormalStyleObject;
    TSharedPtr<FJsonObject> CriticalStyleObject;
    TSharedPtr<FJsonObject> DotStyleObject;

    if (!TryReadInteger(RootObject, TEXT("schemaVersion"), SchemaVersion, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("presetId"), OutPresetSet.PresetId, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("displayName"), OutPresetSet.DisplayName, OutErrorMessage)
        || !TryReadString(RootObject, TEXT("savedAt"), OutPresetSet.SavedAtUtc, OutErrorMessage)
        || !TryReadObject(RootObject, TEXT("styles"), StylesObject, OutErrorMessage)
        || !TryReadObject(StylesObject, TEXT("normal"), NormalStyleObject, OutErrorMessage)
        || !TryReadObject(StylesObject, TEXT("critical"), CriticalStyleObject, OutErrorMessage)
        || !TryReadObject(StylesObject, TEXT("dot"), DotStyleObject, OutErrorMessage))
    {
        return false;
    }

    if (SchemaVersion != PresetSetSchemaVersion)
    {
        OutErrorMessage = FString::Printf(
            TEXT("지원하지 않는 프리셋 세트 schemaVersion입니다. Expected: %d, Actual: %d"),
            PresetSetSchemaVersion,
            SchemaVersion);
        return false;
    }

    OutPresetSet.SchemaVersion = SchemaVersion;
    if (!TryParsePresetStyle(
            NormalStyleObject,
            EDamageNumberDisplayType::Normal,
            SchemaVersion,
            OutPresetSet.SavedAtUtc,
            OutPresetSet.Normal,
            OutErrorMessage)
        || !TryParsePresetStyle(
            CriticalStyleObject,
            EDamageNumberDisplayType::Critical,
            SchemaVersion,
            OutPresetSet.SavedAtUtc,
            OutPresetSet.Critical,
            OutErrorMessage)
        || !TryParsePresetStyle(
            DotStyleObject,
            EDamageNumberDisplayType::Dot,
            SchemaVersion,
            OutPresetSet.SavedAtUtc,
            OutPresetSet.Dot,
            OutErrorMessage))
    {
        return false;
    }

    return ValidatePresetSet(OutPresetSet, OutErrorMessage);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 파싱된 데미지 플로팅 프리셋이 런타임에서 안전하게 사용할 수 있는 값인지 검증하는 함수
// Preset : 검증할 데미지 플로팅 프리셋
// OutErrorMessage : 검증 실패 원인을 받을 문자열
// Return Value : 지원 스키마와 값 범위를 만족하면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::ValidatePreset(
    const FDamageNumberPreset& Preset,
    FString& OutErrorMessage)
{
    if (Preset.SchemaVersion != MyDamageFloatingPresetJson::LegacySchemaVersion
        && Preset.SchemaVersion != MyDamageFloatingPresetJson::PresetSetSchemaVersion)
    {
        OutErrorMessage = FString::Printf(
            TEXT("지원하지 않는 schemaVersion입니다. Expected: %d 또는 %d, Actual: %d"),
            MyDamageFloatingPresetJson::LegacySchemaVersion,
            MyDamageFloatingPresetJson::PresetSetSchemaVersion,
            Preset.SchemaVersion);
        return false;
    }

    if (Preset.DesignName.TrimStartAndEnd().IsEmpty())
    {
        OutErrorMessage = TEXT("designName은 비어 있을 수 없습니다.");
        return false;
    }

    if (Preset.SchemaVersion == MyDamageFloatingPresetJson::PresetSetSchemaVersion)
    {
        if (!Preset.TextStyle.FontAssetPath.IsValid())
        {
            OutErrorMessage = TEXT("textStyle.fontAssetPath가 유효한 에셋 경로가 아닙니다.");
            return false;
        }

        if (Preset.TextStyle.FontSize <= 0)
        {
            OutErrorMessage = TEXT("textStyle.fontSize는 0보다 커야 합니다.");
            return false;
        }

        if (Preset.TextStyle.Typeface.IsNone())
        {
            OutErrorMessage = TEXT("textStyle.typeface는 비어 있을 수 없습니다.");
            return false;
        }

        if (Preset.TextStyle.OutlineSize < 0)
        {
            OutErrorMessage = TEXT("textStyle.outlineSize는 음수일 수 없습니다.");
            return false;
        }
    }

    if (Preset.Scale.Initial <= 0.0f || Preset.Scale.Normal <= 0.0f || Preset.Scale.Exit <= 0.0f)
    {
        OutErrorMessage = TEXT("scale의 initial, normal, exit 값은 모두 0보다 커야 합니다.");
        return false;
    }

    if (Preset.Timing.Appear < 0.0f || Preset.Timing.Hold < 0.0f || Preset.Timing.Exit < 0.0f)
    {
        OutErrorMessage = TEXT("timingSeconds의 appear, hold, exit 값은 음수일 수 없습니다.");
        return false;
    }

    const float CalculatedTotal = Preset.Timing.Appear + Preset.Timing.Hold + Preset.Timing.Exit;
    if (!FMath::IsNearlyEqual(Preset.Timing.Total, CalculatedTotal, 0.001f))
    {
        OutErrorMessage = FString::Printf(
            TEXT("timingSeconds.total이 세 구간의 합과 다릅니다. Expected: %.3f, Actual: %.3f"),
            CalculatedTotal,
            Preset.Timing.Total);
        return false;
    }

    if (Preset.Timing.Total <= 0.0f)
    {
        OutErrorMessage = TEXT("timingSeconds.total은 0보다 커야 합니다.");
        return false;
    }

    OutErrorMessage.Reset();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// schemaVersion 2 프리셋 세트의 메타데이터와 Normal/Critical/DOT 구성을 검증하는 함수
// PresetSet : 검증할 프리셋 세트
// OutErrorMessage : 검증 실패 원인을 받을 문자열
// Return Value : 세트 전체가 유효하면 true, 아니면 false
bool UMyDamageFloatingPresetJsonLibrary::ValidatePresetSet(
    const FDamageNumberPresetSetData& PresetSet,
    FString& OutErrorMessage)
{
    if (PresetSet.SchemaVersion != MyDamageFloatingPresetJson::PresetSetSchemaVersion)
    {
        OutErrorMessage = FString::Printf(
            TEXT("지원하지 않는 프리셋 세트 schemaVersion입니다. Expected: %d, Actual: %d"),
            MyDamageFloatingPresetJson::PresetSetSchemaVersion,
            PresetSet.SchemaVersion);
        return false;
    }

    if (PresetSet.PresetId.TrimStartAndEnd().IsEmpty())
    {
        OutErrorMessage = TEXT("presetId는 비어 있을 수 없습니다.");
        return false;
    }

    if (PresetSet.DisplayName.TrimStartAndEnd().IsEmpty())
    {
        OutErrorMessage = TEXT("displayName은 비어 있을 수 없습니다.");
        return false;
    }

    if (PresetSet.Normal.DamageType != EDamageNumberDisplayType::Normal
        || PresetSet.Critical.DamageType != EDamageNumberDisplayType::Critical
        || PresetSet.Dot.DamageType != EDamageNumberDisplayType::Dot)
    {
        OutErrorMessage = TEXT("Normal/Critical/DOT 프리셋의 damageType 구성이 올바르지 않습니다.");
        return false;
    }

    FString PresetErrorMessage;
    if (!ValidatePreset(PresetSet.Normal, PresetErrorMessage))
    {
        OutErrorMessage = FString::Printf(TEXT("Normal 프리셋 검증 실패: %s"), *PresetErrorMessage);
        return false;
    }

    if (!ValidatePreset(PresetSet.Critical, PresetErrorMessage))
    {
        OutErrorMessage = FString::Printf(TEXT("Critical 프리셋 검증 실패: %s"), *PresetErrorMessage);
        return false;
    }

    if (!ValidatePreset(PresetSet.Dot, PresetErrorMessage))
    {
        OutErrorMessage = FString::Printf(TEXT("DOT 프리셋 검증 실패: %s"), *PresetErrorMessage);
        return false;
    }

    OutErrorMessage.Reset();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 프리셋의 이동, 크기, 투명도 설정을 특정 경과 시간의 화면 상태로 계산하는 함수
// Preset : 평가할 데미지 플로팅 프리셋
// ElapsedTime : 데미지 숫자가 생성된 뒤 흐른 시간(초)
// Return Value : 해당 시간의 이동 위치, 크기, 투명도와 종료 여부
FDamageNumberAnimationSample UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(
    const FDamageNumberPreset& Preset,
    float ElapsedTime)
{
    FDamageNumberAnimationSample Sample;
    const float SafeElapsedTime = FMath::Max(ElapsedTime, 0.0f);
    const float SafeTotalTime = FMath::Max(Preset.Timing.Total, 0.0f);
    const float MotionAlpha = SafeTotalTime > 0.0f
        ? FMath::Clamp(SafeElapsedTime / SafeTotalTime, 0.0f, 1.0f)
        : 1.0f;
    const float MotionProgress = FMath::InterpEaseOut(0.0f, 1.0f, MotionAlpha, 3.0f);

    Sample.Translation = Preset.Motion.StartOffset + Preset.Motion.MoveDistance * MotionProgress;
    if (Preset.Motion.Path == EDamageNumberMotionPath::Curve)
    {
        Sample.Translation.Y -= FMath::Sin(PI * MotionProgress) * Preset.Motion.ArcHeight;
    }

    if (Preset.Timing.Appear > 0.0f && SafeElapsedTime < Preset.Timing.Appear)
    {
        const float AppearAlpha = FMath::Clamp(SafeElapsedTime / Preset.Timing.Appear, 0.0f, 1.0f);
        const float AppearProgress = FMath::InterpEaseOut(0.0f, 1.0f, AppearAlpha, 3.0f);
        Sample.Scale = FMath::Lerp(Preset.Scale.Initial, Preset.Scale.Normal, AppearProgress);
        Sample.Opacity = Preset.Opacity.bFadeIn ? AppearProgress : 1.0f;
    }
    else if (SafeElapsedTime < Preset.Timing.Appear + Preset.Timing.Hold)
    {
        Sample.Scale = Preset.Scale.Normal;
        Sample.Opacity = 1.0f;
    }
    else
    {
        const float ExitElapsedTime = SafeElapsedTime - Preset.Timing.Appear - Preset.Timing.Hold;
        const float ExitAlpha = Preset.Timing.Exit > 0.0f
            ? FMath::Clamp(ExitElapsedTime / Preset.Timing.Exit, 0.0f, 1.0f)
            : 1.0f;
        const float ExitProgress = FMath::InterpEaseOut(0.0f, 1.0f, ExitAlpha, 3.0f);
        Sample.Scale = FMath::Lerp(Preset.Scale.Normal, Preset.Scale.Exit, ExitProgress);
        Sample.Opacity = Preset.Opacity.bFadeOut ? 1.0f - ExitAlpha : 1.0f;
    }

    Sample.bFinished = SafeTotalTime <= 0.0f || SafeElapsedTime >= SafeTotalTime;
    return Sample;
}
