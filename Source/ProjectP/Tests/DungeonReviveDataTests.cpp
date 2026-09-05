#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Dungeon/Revive/DungeonReviveDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeonReviveDataLookupTest,
	"ProjectP.Dungeon.Revive.DataLookup",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author HanUl
//! \brief 가변 옵션의 활성 필터, 정렬, ID 조회 계약을 검사한다.
//! \param Parameters 자동화 프레임워크 인자
//! \return 모든 데이터 조회 계약이 통과하면 true
bool FDungeonReviveDataLookupTest::RunTest(const FString& Parameters)
{
	UDungeonReviveDataAsset* ReviveData = NewObject<UDungeonReviveDataAsset>();

	FDungeonReviveOption SlowOption;
	SlowOption.OptionId = TEXT("Anubis");
	SlowOption.SortOrder = 20;
	SlowOption.MesoCost = 5000;
	SlowOption.ReviveHealthPercent = 0.9f;
	SlowOption.ReviveDelaySeconds = 13.0f;
	ReviveData->Options.Add(SlowOption);

	FDungeonReviveOption FastOption;
	FastOption.OptionId = TEXT("Horus");
	FastOption.SortOrder = 10;
	FastOption.ReviveHealthPercent = 0.45f;
	FastOption.ReviveDelaySeconds = 8.0f;
	ReviveData->Options.Add(FastOption);

	FDungeonReviveOption DisabledOption;
	DisabledOption.OptionId = TEXT("Disabled");
	DisabledOption.SortOrder = 0;
	DisabledOption.bEnabled = false;
	ReviveData->Options.Add(DisabledOption);

	TArray<FName> EnabledOptionIds;
	ReviveData->GetEnabledOptionIds(EnabledOptionIds);
	TestEqual(TEXT("활성 옵션만 반환한다"), EnabledOptionIds.Num(), 2);
	TestEqual(TEXT("SortOrder가 낮은 옵션이 먼저다"), EnabledOptionIds[0], FName(TEXT("Horus")));
	TestEqual(TEXT("다음 옵션도 정렬된다"), EnabledOptionIds[1], FName(TEXT("Anubis")));
	TestNotNull(TEXT("활성 옵션은 ID로 조회된다"), ReviveData->FindOption(TEXT("Horus"), true));
	TestNull(TEXT("비활성 옵션은 활성 조회에서 제외된다"), ReviveData->FindOption(TEXT("Disabled"), true));
	TestNotNull(TEXT("비활성 요구를 풀면 관리 도구에서 조회할 수 있다"), ReviveData->FindOption(TEXT("Disabled"), false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeonReviveDataValidationTest,
	"ProjectP.Dungeon.Revive.DataValidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author HanUl
//! \brief 잘못된 체력 비율, 중복 ID, 자동 선택 옵션 계약을 데이터 단계에서 거부하는지 검사한다.
//! \param Parameters 자동화 프레임워크 인자
//! \return 모든 데이터 검증 계약이 통과하면 true
bool FDungeonReviveDataValidationTest::RunTest(const FString& Parameters)
{
	UDungeonReviveDataAsset* ReviveData = NewObject<UDungeonReviveDataAsset>();
	FDungeonReviveOption Option;
	Option.OptionId = TEXT("Default");
	Option.ReviveHealthPercent = 0.5f;
	ReviveData->Options.Add(Option);
	ReviveData->DefaultAutoSelectOptionId = Option.OptionId;

	FString Error;
	TestTrue(TEXT("정상 데이터는 검증을 통과한다"), ReviveData->ValidateData(Error));

	ReviveData->Options[0].ReviveHealthPercent = 1.1f;
	TestFalse(TEXT("100% 초과 체력 비율은 거부한다"), ReviveData->ValidateData(Error));

	ReviveData->Options[0].ReviveHealthPercent = 0.5f;
	ReviveData->Options.Add(FDungeonReviveOption(ReviveData->Options[0]));
	TestFalse(TEXT("중복 OptionId는 거부한다"), ReviveData->ValidateData(Error));

	ReviveData->Options.SetNum(1);
	ReviveData->Options[0].bCanAutoSelect = false;
	TestFalse(TEXT("자동 선택 불가 옵션은 기본값으로 사용할 수 없다"), ReviveData->ValidateData(Error));
	return true;
}

#endif
