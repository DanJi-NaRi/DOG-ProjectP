#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyDamageFloatingPreset.generated.h"

////////////////////////////
//! \brief 화면에 표시할 데미지 숫자의 종류.
UENUM(BlueprintType)
enum class EDamageNumberDisplayType : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    Critical UMETA(DisplayName = "Critical"),
    Dot UMETA(DisplayName = "DOT")
};

////////////////////////////
//! \brief 데미지 숫자가 시작 위치에서 종료 위치로 이동하는 경로의 종류.
UENUM(BlueprintType)
enum class EDamageNumberMotionPath : uint8
{
    Linear UMETA(DisplayName = "Linear"),
    Curve UMETA(DisplayName = "Curve")
};

////////////////////////////
//! \brief JSON motion 객체와 대응하는 이동 설정.
USTRUCT(BlueprintType)
struct FDamageNumberMotionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Motion")
    EDamageNumberMotionPath Path = EDamageNumberMotionPath::Linear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Motion")
    FVector2D StartOffset = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Motion")
    FVector2D MoveDistance = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Motion")
    float ArcHeight = 0.0f;
};

////////////////////////////
//! \brief JSON scale 객체와 대응하는 등장/유지/퇴장 크기 설정.
USTRUCT(BlueprintType)
struct FDamageNumberScaleSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Scale", meta = (ClampMin = "0.01"))
    float Initial = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Scale", meta = (ClampMin = "0.01"))
    float Normal = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Scale", meta = (ClampMin = "0.01"))
    float Exit = 1.0f;
};

////////////////////////////
//! \brief JSON timingSeconds 객체와 대응하는 노출 시간 설정.
USTRUCT(BlueprintType)
struct FDamageNumberTimingSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
    float Appear = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
    float Hold = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
    float Exit = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Timing")
    float Total = 0.0f;
};

////////////////////////////
//! \brief JSON opacity 객체와 대응하는 등장/퇴장 투명도 설정.
USTRUCT(BlueprintType)
struct FDamageNumberOpacitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Opacity")
    bool bFadeIn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Opacity")
    bool bFadeOut = true;
};

////////////////////////////
//! \brief JSON textStyle 객체와 대응하는 폰트, 색상, 외곽선 설정.
USTRUCT(BlueprintType)
struct FDamageNumberTextPresetSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text")
    FSoftObjectPath FontAssetPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text", meta = (ClampMin = "1"))
    int32 FontSize = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text")
    FName Typeface = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text", meta = (ClampMin = "0"))
    int32 OutlineSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Text")
    FLinearColor OutlineColor = FLinearColor::Black;
};

////////////////////////////
//! \brief 기획 툴이 저장한 데미지 플로팅 JSON 한 파일의 런타임 데이터.
USTRUCT(BlueprintType)
struct FDamageNumberPreset
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    int32 SchemaVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Metadata")
    FString DesignName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString SavedAtUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber|Metadata")
    EDamageNumberDisplayType DamageType = EDamageNumberDisplayType::Normal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString MotionPresetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
    FDamageNumberTextPresetSettings TextStyle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
    FDamageNumberMotionSettings Motion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
    FDamageNumberScaleSettings Scale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
    FDamageNumberTimingSettings Timing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
    FDamageNumberOpacitySettings Opacity;
};

////////////////////////////
//! \brief schemaVersion 2 JSON 한 파일에 포함된 Normal/Critical/DOT 프리셋 세트.
USTRUCT(BlueprintType)
struct FDamageNumberPresetSetData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    int32 SchemaVersion = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString PresetId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString DisplayName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Metadata")
    FString SavedAtUtc;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset Normal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset Critical;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
    FDamageNumberPreset Dot;
};

////////////////////////////
//! \brief 특정 경과 시간에서 계산한 데미지 플로팅의 화면 상태.
USTRUCT(BlueprintType)
struct FDamageNumberAnimationSample
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Animation")
    FVector2D Translation = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Animation")
    float Scale = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Animation")
    float Opacity = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DamageNumber|Animation")
    bool bFinished = false;
};

////////////////////////////
//! \author 준혁
//! \brief 기획 툴이 생성한 데미지 플로팅 JSON을 C++ 프리셋으로 변환하고 검증하는 함수 모음.
UCLASS()
class PROJECTP_API UMyDamageFloatingPresetJsonLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|DamageNumber")
    static bool LoadPresetFromJsonFile(
        const FString& JsonFilePath,
        FDamageNumberPreset& OutPreset,
        FString& OutErrorMessage);

    static bool ParsePresetFromJsonString(
        const FString& JsonString,
        FDamageNumberPreset& OutPreset,
        FString& OutErrorMessage);

    UFUNCTION(BlueprintCallable, Category = "UI|DamageNumber")
    static bool LoadPresetSetFromJsonFile(
        const FString& JsonFilePath,
        FDamageNumberPresetSetData& OutPresetSet,
        FString& OutErrorMessage);

    static bool ParsePresetSetFromJsonString(
        const FString& JsonString,
        FDamageNumberPresetSetData& OutPresetSet,
        FString& OutErrorMessage);

    static bool ValidatePreset(
        const FDamageNumberPreset& Preset,
        FString& OutErrorMessage);

    static bool ValidatePresetSet(
        const FDamageNumberPresetSetData& PresetSet,
        FString& OutErrorMessage);

    UFUNCTION(BlueprintPure, Category = "UI|DamageNumber")
    static FDamageNumberAnimationSample EvaluatePresetAtTime(
        const FDamageNumberPreset& Preset,
        float ElapsedTime);
};
