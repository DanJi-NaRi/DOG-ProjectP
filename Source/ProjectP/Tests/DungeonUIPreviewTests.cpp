#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dungeon/MySurrenderVotePanelWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDungeonSurrenderPreviewStateTest,
    "ProjectP.UI.Dungeon.SurrenderPreview.State",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 항복 UI 프리뷰 상태가 찬성 1표와 전체 3명으로 생성되는지 검증하는 테스트
// Parameters : 자동화 테스트 프레임워크가 전달하는 매개 변수
// Return Value : 모든 검증을 통과했으면 true
bool FDungeonSurrenderPreviewStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FDungeonSurrenderVoteState State =
        MySurrenderVotePreview::MakeVoteState(100.0f, 30.0f, 1, 3);

    TestTrue(TEXT("Vote is in progress"), State.bVoteInProgress);
    TestEqual(TEXT("Agree count"), State.AgreeCount, 1);
    TestEqual(TEXT("Disagree count"), State.DisagreeCount, 0);
    TestEqual(TEXT("Required count"), State.RequiredCount, 3);
    TestEqual(TEXT("Start time"), State.VoteStartServerTime, 100.0f);
    TestEqual(TEXT("End time"), State.VoteEndServerTime, 130.0f);
    return true;
}

#endif
