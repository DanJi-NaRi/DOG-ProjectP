////////////////////////////
//! \file DamageFloatingPresetTests.cpp
//! \brief 기획 툴 JSON과 데미지 플로팅 C++ 프리셋 사이의 변환 및 검증 자동화 테스트.
//! \author 준혁
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Widget/HUD/Nameplate/MyDamageFloatingPreset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDamageFloatingPresetValidJsonTest,
    "ProjectP.UI.DamageFloatingPreset.ValidJson",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기획 툴의 정상 JSON이 C++ 프리셋의 각 필드로 정확히 변환되는지 확인하는 테스트
// Parameters : 자동화 테스트 프레임워크가 전달하는 실행 매개변수
// Return Value : 테스트 실행이 끝나면 true
bool FDamageFloatingPresetValidJsonTest::RunTest(const FString& Parameters)
{
    const FString JsonString = TEXT(R"JSON(
    {
      "schemaVersion": 1,
      "designName": "일반",
      "savedAt": "2026-07-21T05:47:30.861Z",
      "previewDamageStyle": "normal",
      "motionPreset": "custom",
      "motion": {
        "path": "curve",
        "startOffset": { "x": 0, "y": -55 },
        "moveDistance": { "x": 55, "y": -10 },
        "arcHeight": 25
      },
      "scale": { "initial": 0.7, "normal": 0.8, "exit": 0.4 },
      "timingSeconds": { "appear": 0, "hold": 0.55, "exit": 0.35, "total": 0.9 },
      "opacity": { "fadeIn": true, "fadeOut": true }
    }
    )JSON");

    FDamageNumberPreset Preset;
    FString ErrorMessage;
    TestTrue(
        TEXT("정상 JSON 파싱 성공"),
        UMyDamageFloatingPresetJsonLibrary::ParsePresetFromJsonString(JsonString, Preset, ErrorMessage));
    TestEqual(TEXT("스키마 버전"), Preset.SchemaVersion, 1);
    TestEqual(TEXT("디자인 이름"), Preset.DesignName, FString(TEXT("일반")));
    TestEqual(TEXT("데미지 타입"), Preset.DamageType, EDamageNumberDisplayType::Normal);
    TestEqual(TEXT("이동 경로"), Preset.Motion.Path, EDamageNumberMotionPath::Curve);
    TestEqual(TEXT("시작 X"), Preset.Motion.StartOffset.X, 0.0);
    TestEqual(TEXT("시작 Y"), Preset.Motion.StartOffset.Y, -55.0);
    TestEqual(TEXT("이동 X"), Preset.Motion.MoveDistance.X, 55.0);
    TestEqual(TEXT("이동 Y"), Preset.Motion.MoveDistance.Y, -10.0);
    TestEqual(TEXT("곡선 높이"), Preset.Motion.ArcHeight, 25.0f);
    TestEqual(TEXT("등장 크기"), Preset.Scale.Initial, 0.7f);
    TestEqual(TEXT("일반 크기"), Preset.Scale.Normal, 0.8f);
    TestEqual(TEXT("퇴장 크기"), Preset.Scale.Exit, 0.4f);
    TestEqual(TEXT("총 시간"), Preset.Timing.Total, 0.9f);
    TestTrue(TEXT("등장 페이드"), Preset.Opacity.bFadeIn);
    TestTrue(TEXT("퇴장 페이드"), Preset.Opacity.bFadeOut);
    TestTrue(TEXT("성공 시 오류 메시지 비어 있음"), ErrorMessage.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDamageFloatingPresetSetJsonTest,
    "ProjectP.UI.DamageFloatingPreset.PresetSetJson",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// schemaVersion 2 JSON 한 파일에서 Normal/Critical/DOT와 텍스트 스타일을 함께 읽는지 확인하는 테스트
// Parameters : 자동화 테스트 프레임워크가 전달하는 실행 매개변수
// Return Value : 테스트 실행이 끝나면 true
bool FDamageFloatingPresetSetJsonTest::RunTest(const FString& Parameters)
{
    const FString JsonString = TEXT(R"JSON(
    {
      "schemaVersion": 2,
      "presetId": "PresetSetTest",
      "displayName": "프리셋 세트 테스트",
      "savedAt": "2026-07-29T05:13:28.469Z",
      "styles": {
        "normal": {
          "designName": "일반",
          "damageType": "normal",
          "textStyle": {
            "fontAssetPath": "/Engine/EngineFonts/Roboto.Roboto",
            "fontSize": 30,
            "typeface": "Regular",
            "color": "#FFA200",
            "outlineSize": 2,
            "outlineColor": "#000000"
          },
          "motion": {
            "path": "linear",
            "startOffset": { "x": 0, "y": -55 },
            "moveDistance": { "x": 0, "y": -55 },
            "arcHeight": 30
          },
          "scale": { "initial": 1, "normal": 1, "exit": 0.6 },
          "timingSeconds": { "appear": 0.2, "hold": 0.6, "exit": 0.2, "total": 1 },
          "opacity": { "fadeIn": true, "fadeOut": true }
        },
        "critical": {
          "designName": "크리티컬",
          "damageType": "critical",
          "textStyle": {
            "fontAssetPath": "/Engine/EngineFonts/Roboto.Roboto",
            "fontSize": 40,
            "typeface": "Bold",
            "color": "#FF0000",
            "outlineSize": 3,
            "outlineColor": "#000000"
          },
          "motion": {
            "path": "curve",
            "startOffset": { "x": 0, "y": -55 },
            "moveDistance": { "x": -1, "y": -47 },
            "arcHeight": 40
          },
          "scale": { "initial": 1, "normal": 1, "exit": 0.6 },
          "timingSeconds": { "appear": 0.2, "hold": 0.6, "exit": 0.2, "total": 1 },
          "opacity": { "fadeIn": true, "fadeOut": true }
        },
        "dot": {
          "designName": "도트",
          "damageType": "dot",
          "textStyle": {
            "fontAssetPath": "/Engine/EngineFonts/Roboto.Roboto",
            "fontSize": 26,
            "typeface": "Regular",
            "color": "#77D96B",
            "outlineSize": 2,
            "outlineColor": "#000000"
          },
          "motion": {
            "path": "linear",
            "startOffset": { "x": 0, "y": -55 },
            "moveDistance": { "x": 1, "y": -56 },
            "arcHeight": 30
          },
          "scale": { "initial": 1, "normal": 1, "exit": 0.6 },
          "timingSeconds": { "appear": 0.2, "hold": 0.6, "exit": 0.2, "total": 1 },
          "opacity": { "fadeIn": true, "fadeOut": true }
        }
      }
    }
    )JSON");

    FDamageNumberPresetSetData PresetSet;
    FString ErrorMessage;
    TestTrue(
        TEXT("프리셋 세트 JSON 파싱 성공"),
        UMyDamageFloatingPresetJsonLibrary::ParsePresetSetFromJsonString(
            JsonString,
            PresetSet,
            ErrorMessage));
    TestEqual(TEXT("세트 스키마 버전"), PresetSet.SchemaVersion, 2);
    TestEqual(TEXT("프리셋 ID"), PresetSet.PresetId, FString(TEXT("PresetSetTest")));
    TestEqual(TEXT("표시 이름"), PresetSet.DisplayName, FString(TEXT("프리셋 세트 테스트")));
    TestEqual(TEXT("Normal 타입"), PresetSet.Normal.DamageType, EDamageNumberDisplayType::Normal);
    TestEqual(TEXT("Critical 타입"), PresetSet.Critical.DamageType, EDamageNumberDisplayType::Critical);
    TestEqual(TEXT("DOT 타입"), PresetSet.Dot.DamageType, EDamageNumberDisplayType::Dot);
    TestEqual(TEXT("Normal 폰트 크기"), PresetSet.Normal.TextStyle.FontSize, 30);
    TestEqual(TEXT("Critical 타입페이스"), PresetSet.Critical.TextStyle.Typeface, FName(TEXT("Bold")));
    TestEqual(TEXT("DOT 외곽선 크기"), PresetSet.Dot.TextStyle.OutlineSize, 2);
    TestEqual(
        TEXT("폰트 에셋 경로"),
        PresetSet.Normal.TextStyle.FontAssetPath.ToString(),
        FString(TEXT("/Engine/EngineFonts/Roboto.Roboto")));
    TestTrue(TEXT("성공 시 오류 메시지 비어 있음"), ErrorMessage.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDamageFloatingPresetInvalidTotalTest,
    "ProjectP.UI.DamageFloatingPreset.InvalidTotal",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// timingSeconds.total이 세 구간의 합과 다르면 JSON 적용을 거부하는지 확인하는 테스트
// Parameters : 자동화 테스트 프레임워크가 전달하는 실행 매개변수
// Return Value : 테스트 실행이 끝나면 true
bool FDamageFloatingPresetInvalidTotalTest::RunTest(const FString& Parameters)
{
    const FString JsonString = TEXT(R"JSON(
    {
      "schemaVersion": 1,
      "designName": "잘못된 시간",
      "savedAt": "2026-07-21T05:47:30.861Z",
      "previewDamageStyle": "dot",
      "motionPreset": "custom",
      "motion": {
        "path": "linear",
        "startOffset": { "x": 0, "y": -55 },
        "moveDistance": { "x": 0, "y": -35 },
        "arcHeight": 30
      },
      "scale": { "initial": 0.7, "normal": 0.7, "exit": 0.4 },
      "timingSeconds": { "appear": 0, "hold": 0.55, "exit": 0.35, "total": 1.5 },
      "opacity": { "fadeIn": true, "fadeOut": true }
    }
    )JSON");

    FDamageNumberPreset Preset;
    FString ErrorMessage;
    TestFalse(
        TEXT("시간 합계가 다른 JSON 파싱 실패"),
        UMyDamageFloatingPresetJsonLibrary::ParsePresetFromJsonString(JsonString, Preset, ErrorMessage));
    TestTrue(TEXT("실패 원인이 total 필드를 가리킴"), ErrorMessage.Contains(TEXT("total")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDamageFloatingPresetAnimationTest,
    "ProjectP.UI.DamageFloatingPreset.Animation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 수정된 일반 데미지 데이터의 시작 위치, 크기, 페이드와 곡선 이동 종료값을 확인하는 테스트
// Parameters : 자동화 테스트 프레임워크가 전달하는 실행 매개변수
// Return Value : 테스트 실행이 끝나면 true
bool FDamageFloatingPresetAnimationTest::RunTest(const FString& Parameters)
{
    FDamageNumberPreset Preset;
    Preset.SchemaVersion = 1;
    Preset.DesignName = TEXT("일반");
    Preset.DamageType = EDamageNumberDisplayType::Normal;
    Preset.Motion.Path = EDamageNumberMotionPath::Curve;
    Preset.Motion.StartOffset = FVector2D(0.0, -55.0);
    Preset.Motion.MoveDistance = FVector2D(50.0, -5.0);
    Preset.Motion.ArcHeight = 30.0f;
    Preset.Scale.Initial = 0.7f;
    Preset.Scale.Normal = 0.8f;
    Preset.Scale.Exit = 0.4f;
    Preset.Timing.Appear = 0.3f;
    Preset.Timing.Hold = 0.5f;
    Preset.Timing.Exit = 0.3f;
    Preset.Timing.Total = 1.1f;
    Preset.Opacity.bFadeIn = true;
    Preset.Opacity.bFadeOut = true;

    const FDamageNumberAnimationSample StartSample =
        UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(Preset, 0.0f);
    TestEqual(TEXT("시작 X"), StartSample.Translation.X, 0.0);
    TestEqual(TEXT("시작 Y"), StartSample.Translation.Y, -55.0);
    TestEqual(TEXT("시작 크기"), StartSample.Scale, 0.7f);
    TestEqual(TEXT("페이드인 시작 투명도"), StartSample.Opacity, 0.0f);
    TestFalse(TEXT("시작 시 미종료"), StartSample.bFinished);

    const FDamageNumberAnimationSample MiddleSample =
        UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(Preset, 0.55f);
    TestTrue(TEXT("곡선 중간 위치는 직선 보간보다 위쪽"), MiddleSample.Translation.Y < -57.0);
    TestEqual(TEXT("유지 구간 크기"), MiddleSample.Scale, 0.8f);
    TestEqual(TEXT("유지 구간 투명도"), MiddleSample.Opacity, 1.0f);

    const FDamageNumberAnimationSample EndSample =
        UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(Preset, 1.1f);
    TestNearlyEqual(TEXT("종료 X"), EndSample.Translation.X, 50.0, 0.001);
    TestNearlyEqual(TEXT("종료 Y"), EndSample.Translation.Y, -60.0, 0.001);
    TestEqual(TEXT("종료 크기"), EndSample.Scale, 0.4f);
    TestEqual(TEXT("페이드아웃 종료 투명도"), EndSample.Opacity, 0.0f);
    TestTrue(TEXT("총 시간 도달 시 종료"), EndSample.bFinished);
    return true;
}

#endif // WITH_AUTOMATION_TESTS
