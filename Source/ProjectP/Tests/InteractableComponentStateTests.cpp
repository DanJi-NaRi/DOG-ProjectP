////////////////////////////
//! \file InteractableComponentStateTests.cpp
//! \brief 상호작용 상태 관리 설계(AI_Docs/InteractionStateManagementDesign.md) 15.1의
//!        상태 자동화 테스트 구현 파일이다. "Automation RunTests ProjectP.Interaction"으로 실행한다.
//! \author 준혁
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tests/CPP_InteractableComponentTestProbe.h"

namespace InteractableComponentTestHelpers
{
	////////////////////////////
	//! \author 준혁
	//! \brief 테스트 전용 게임 월드를 만들고 파괴하는 스코프 헬퍼. 액터 스폰과 컴포넌트 BeginPlay에 필요하다.
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		AActor* SpawnActor() const
		{
			return World ? World->SpawnActor<AActor>() : nullptr;
		}
	};

	////////////////////////////
	//! \author 준혁
	//! \brief 정책이 설정된 테스트 프로브 컴포넌트를 소유 액터에 붙여 생성하는 함수.
	//!        소유 액터는 복제 경고를 피하도록 복제 액터로 만든다.
	//! \return 등록(BeginPlay)까지 끝난 프로브
	UCPP_InteractableComponentTestProbe* MakeProbe(
		const FScopedTestWorld& TestWorld,
		EInteractionConcurrencyMode ConcurrencyMode,
		EInteractionReleaseMode ReleaseMode,
		EInteractionUsageMode UsageMode,
		bool bStartEnabled)
	{
		AActor* OwnerActor = TestWorld.SpawnActor();
		if (!OwnerActor)
		{
			return nullptr;
		}

		OwnerActor->SetReplicates(true);

		UCPP_InteractableComponentTestProbe* Probe = NewObject<UCPP_InteractableComponentTestProbe>(OwnerActor);
		Probe->SetPoliciesForTest(ConcurrencyMode, ReleaseMode, UsageMode, bStartEnabled);
		Probe->RegisterComponent();
		return Probe;
	}

	//! 시작 시도 헬퍼. Context/Reason을 채워 반환한다.
	bool TryBegin(UCPP_InteractableComponentTestProbe* Probe, AActor* Interactor, FInteractionStartContext& OutContext, EInteractionRejectReason& OutReason)
	{
		return Probe->TryBeginInteraction(Interactor, OutContext, OutReason);
	}
}

using namespace InteractableComponentTestHelpers;

//! 초기 비활성: bStartInteractionEnabled=false면 Disabled 상태이고 모든 요청이 거절된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableInitialDisabledTest, "ProjectP.Interaction.InitialDisabled",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableInitialDisabledTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, false);
	AActor* PlayerA = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);

	TestEqual(TEXT("초기 상태는 Disabled"), Probe->GetInteractableState(), EInteractableState::Disabled);
	TestFalse(TEXT("Disabled에서 CanInteract는 false"), Probe->CanInteract(PlayerA));

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestFalse(TEXT("Disabled에서 승인 거절"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("거절 사유는 Disabled"), Reason, EInteractionRejectReason::Disabled);
	return true;
}

//! 활성화: SetInteractionEnabled(true)로 Disabled -> Ready가 되고 승인이 가능해진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableEnableTest, "ProjectP.Interaction.Enable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableEnableTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, false);
	AActor* PlayerA = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);

	Probe->SetInteractionEnabled(true);
	TestEqual(TEXT("활성화 후 상태는 Ready"), Probe->GetInteractableState(), EInteractableState::Ready);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestTrue(TEXT("Ready에서 승인 성공"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("실패 사유 없음"), Reason, EInteractionRejectReason::None);
	return true;
}

//! 독점 자동 해제: Exclusive + OnInteractEnd에서 Ready -> Busy -> (End) -> Ready.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableExclusiveAutoReleaseTest, "ProjectP.Interaction.ExclusiveAutoRelease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableExclusiveAutoReleaseTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Exclusive, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestTrue(TEXT("A 승인 성공"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("점유 중 상태는 Busy"), Probe->GetInteractableState(), EInteractableState::Busy);

	TestFalse(TEXT("점유 중 B 거절"), TryBegin(Probe, PlayerB, Context, Reason));
	TestEqual(TEXT("거절 사유는 Busy"), Reason, EInteractionRejectReason::Busy);

	Probe->EndInteraction(PlayerA);
	TestEqual(TEXT("자동 해제 후 Ready"), Probe->GetInteractableState(), EInteractableState::Ready);
	TestTrue(TEXT("해제 후 B 승인 성공"), TryBegin(Probe, PlayerB, Context, Reason));
	return true;
}

//! 독점 수동 해제: Exclusive + Manual에서 End 후에도 Busy가 유지되고 Complete 후 Ready가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableExclusiveManualReleaseTest, "ProjectP.Interaction.ExclusiveManualRelease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableExclusiveManualReleaseTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Exclusive, EInteractionReleaseMode::Manual, EInteractionUsageMode::Unlimited, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestTrue(TEXT("A 승인 성공"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("점유 중 상태는 Busy"), Probe->GetInteractableState(), EInteractableState::Busy);

	Probe->EndInteraction(PlayerA);
	TestEqual(TEXT("End 후에도 Busy 유지"), Probe->GetInteractableState(), EInteractableState::Busy);

	Probe->CompleteInteraction(PlayerA);
	TestEqual(TEXT("Complete 후 Ready"), Probe->GetInteractableState(), EInteractableState::Ready);
	return true;
}

//! 강제 중단: 모든 해제 모드에서 Abort 후 잠금이 풀린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableAbortUnlocksTest, "ProjectP.Interaction.AbortUnlocks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableAbortUnlocksTest::RunTest(const FString& Parameters)
{
	const EInteractionReleaseMode ReleaseModes[] = { EInteractionReleaseMode::OnInteractEnd, EInteractionReleaseMode::Manual };
	for (EInteractionReleaseMode ReleaseMode : ReleaseModes)
	{
		FScopedTestWorld TestWorld;
		UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Exclusive, ReleaseMode, EInteractionUsageMode::Unlimited, true);
		AActor* PlayerA = TestWorld.SpawnActor();
		Probe->SetTestUserId(PlayerA, 101);

		FInteractionStartContext Context;
		EInteractionRejectReason Reason = EInteractionRejectReason::None;
		TestTrue(TEXT("A 승인 성공"), TryBegin(Probe, PlayerA, Context, Reason));
		TestEqual(TEXT("점유 중 상태는 Busy"), Probe->GetInteractableState(), EInteractableState::Busy);

		Probe->AbortInteraction(PlayerA);
		TestEqual(TEXT("Abort 후 Ready"), Probe->GetInteractableState(), EInteractableState::Ready);
		TestFalse(TEXT("Abort 후 활성 상호작용자 아님"), Probe->IsInteractorActive(PlayerA));
	}
	return true;
}

//! 전체 1회: OnceGlobal은 첫 요청만 승인하고 이후 ConsumedGlobal로 거절한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableOnceGlobalTest, "ProjectP.Interaction.OnceGlobal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableOnceGlobalTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::OnceGlobal, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestTrue(TEXT("첫 요청 승인"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("승인 후 상태는 Consumed"), Probe->GetInteractableState(), EInteractableState::Consumed);

	TestFalse(TEXT("두 번째 요청 거절"), TryBegin(Probe, PlayerB, Context, Reason));
	TestEqual(TEXT("거절 사유는 ConsumedGlobal"), Reason, EInteractionRejectReason::ConsumedGlobal);
	return true;
}

//! 사용자별 1회: OncePerPlayer는 각 사용자 최초 요청만 승인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableOncePerPlayerTest, "ProjectP.Interaction.OncePerPlayer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableOncePerPlayerTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::OncePerPlayer, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestTrue(TEXT("A 최초 요청 승인"), TryBegin(Probe, PlayerA, Context, Reason));
	Probe->EndInteraction(PlayerA);

	TestFalse(TEXT("A 재요청 거절"), TryBegin(Probe, PlayerA, Context, Reason));
	TestEqual(TEXT("거절 사유는 ConsumedForPlayer"), Reason, EInteractionRejectReason::ConsumedForPlayer);
	TestFalse(TEXT("A 재요청은 CanInteract도 false"), Probe->CanInteract(PlayerA));

	TestEqual(TEXT("공용 상태는 Ready 유지"), Probe->GetInteractableState(), EInteractableState::Ready);
	TestTrue(TEXT("B 최초 요청 승인"), TryBegin(Probe, PlayerB, Context, Reason));
	return true;
}

//! 최초 전체: 액터 전체에서 한 번만 bFirstGlobalInteraction Context가 나온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableFirstGlobalContextTest, "ProjectP.Interaction.FirstGlobalContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableFirstGlobalContextTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	FInteractionStartContext ContextA;
	FInteractionStartContext ContextB;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;

	TestTrue(TEXT("A 승인"), TryBegin(Probe, PlayerA, ContextA, Reason));
	TestTrue(TEXT("A는 최초 전체"), ContextA.bFirstGlobalInteraction);

	TestTrue(TEXT("B 승인"), TryBegin(Probe, PlayerB, ContextB, Reason));
	TestFalse(TEXT("B는 최초 전체가 아님"), ContextB.bFirstGlobalInteraction);
	return true;
}

//! 최초 사용자: 사용자별로 한 번만 bFirstForInteractor Context가 나온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableFirstForUserContextTest, "ProjectP.Interaction.FirstForUserContext",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableFirstForUserContextTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;

	TestTrue(TEXT("A 최초 승인"), TryBegin(Probe, PlayerA, Context, Reason));
	TestTrue(TEXT("A 최초 사용자 Context"), Context.bFirstForInteractor);
	Probe->EndInteraction(PlayerA);

	TestTrue(TEXT("A 재승인"), TryBegin(Probe, PlayerA, Context, Reason));
	TestFalse(TEXT("A 재사용은 최초가 아님"), Context.bFirstForInteractor);

	TestTrue(TEXT("B 최초 승인"), TryBegin(Probe, PlayerB, Context, Reason));
	TestTrue(TEXT("B 최초 사용자 Context"), Context.bFirstForInteractor);
	return true;
}

//! 사용자 ID 없음: OncePerPlayer에서 인증 사용자 ID가 없으면 MissingUserId로 거절한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableMissingUserIdTest, "ProjectP.Interaction.MissingUserId",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableMissingUserIdTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Shared, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::OncePerPlayer, true);
	AActor* PlayerNoId = TestWorld.SpawnActor();

	FInteractionStartContext Context;
	EInteractionRejectReason Reason = EInteractionRejectReason::None;
	TestFalse(TEXT("ID 없는 요청 거절"), TryBegin(Probe, PlayerNoId, Context, Reason));
	TestEqual(TEXT("거절 사유는 MissingUserId"), Reason, EInteractionRejectReason::MissingUserId);
	return true;
}

//! 동시 독점 요청: 같은 프레임의 두 독점 요청 중 한 요청만 승인되고 나머지는 Busy로 거절된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractableConcurrentExclusiveTest, "ProjectP.Interaction.ConcurrentExclusive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FInteractableConcurrentExclusiveTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UCPP_InteractableComponentTestProbe* Probe = MakeProbe(TestWorld, EInteractionConcurrencyMode::Exclusive, EInteractionReleaseMode::OnInteractEnd, EInteractionUsageMode::Unlimited, true);
	AActor* PlayerA = TestWorld.SpawnActor();
	AActor* PlayerB = TestWorld.SpawnActor();
	Probe->SetTestUserId(PlayerA, 101);
	Probe->SetTestUserId(PlayerB, 102);

	// 서버 게임 스레드에서 검사와 상태 변경이 한 함수 안에서 끝나므로 연속 두 호출이 동시 도착과 등가다.
	FInteractionStartContext ContextA;
	FInteractionStartContext ContextB;
	EInteractionRejectReason ReasonA = EInteractionRejectReason::None;
	EInteractionRejectReason ReasonB = EInteractionRejectReason::None;

	const bool bFirstApproved = TryBegin(Probe, PlayerA, ContextA, ReasonA);
	const bool bSecondApproved = TryBegin(Probe, PlayerB, ContextB, ReasonB);

	TestTrue(TEXT("첫 요청만 승인"), bFirstApproved);
	TestFalse(TEXT("두 번째 요청 거절"), bSecondApproved);
	TestEqual(TEXT("두 번째 거절 사유는 Busy"), ReasonB, EInteractionRejectReason::Busy);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
