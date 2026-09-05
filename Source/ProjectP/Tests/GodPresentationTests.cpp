////////////////////////////
//! \page GodPresentationTests.cpp
//! \brief 신 대표색의 sRGB 변환과 fallback을 검사한다.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "God/MyGodPresentationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGodPresentationColorTest,
	"ProjectP.God.Presentation.Color",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author 장효제
//! \brief 유효한 sRGB GodColor 변환과 잘못된 값의 흰색 fallback을 검사한다.
//! \param Parameters 자동화 프레임워크 인자다.
//! \return 모든 색상 계약이 통과하면 true다.
bool FGodPresentationColorTest::RunTest(const FString& Parameters)
{
	FMyGodPresentationRow Presentation;
	Presentation.GodColor = TEXT("#40E0D0");
	TestTrue(
		TEXT("터키색은 sRGB로 변환된다"),
		Presentation.GetGodLinearColor().Equals(
			FLinearColor::FromSRGBColor(FColor(0x40, 0xE0, 0xD0)),
			KINDA_SMALL_NUMBER));

	Presentation.GodColor = TEXT("turquoise");
	TestTrue(TEXT("잘못된 색상은 흰색이다"), Presentation.GetGodLinearColor().Equals(FLinearColor::White));
	return true;
}

#endif
