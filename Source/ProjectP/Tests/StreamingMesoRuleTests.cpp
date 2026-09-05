////////////////////////////
//! \file StreamingMesoRuleTests.cpp
//! \brief 플레이어별 Dungeon Meso 누계 문턱 판정 자동화 테스트다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Streaming/MyStreamingMesoRuleTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStreamingMesoThresholdTest,
	"ProjectP.Streaming.Meso.Threshold",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStreamingMesoThresholdTest::RunTest(const FString& Parameters)
{
	using namespace MyStreamingMesoRulePolicy;

	TestFalse(TEXT("문턱 미도달"), DidCrossThreshold(0, 99, 100));
	TestTrue(TEXT("문턱 정확히 도달"), DidCrossThreshold(99, 100, 100));
	TestTrue(TEXT("한 번에 문턱 통과"), DidCrossThreshold(10, 150, 100));
	TestFalse(TEXT("이미 통과한 문턱 재발행 없음"), DidCrossThreshold(100, 150, 100));
	TestFalse(TEXT("잘못된 문턱 거부"), DidCrossThreshold(0, 100, 0));
	return true;
}

#endif
