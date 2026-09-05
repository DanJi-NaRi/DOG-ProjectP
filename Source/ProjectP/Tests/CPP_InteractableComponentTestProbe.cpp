////////////////////////////
//! \file CPP_InteractableComponentTestProbe.cpp
//! \brief InteractableComponent 상태 자동화 테스트용 프로브 구현 파일이다.
//! \author 준혁
#include "CPP_InteractableComponentTestProbe.h"

////////////////////////////
//! \author 준혁
//! \brief 테스트 정책을 설정하는 함수. RegisterComponent(BeginPlay) 전에 호출해야 초기 Gate에 반영된다.
//! \param InConcurrencyMode 동시 사용 정책
//! \param InReleaseMode 해제 정책
//! \param InUsageMode 사용 제한 정책
//! \param bInStartInteractionEnabled 시작 시 상호작용 가능 여부
void UCPP_InteractableComponentTestProbe::SetPoliciesForTest(EInteractionConcurrencyMode InConcurrencyMode, EInteractionReleaseMode InReleaseMode, EInteractionUsageMode InUsageMode, bool bInStartInteractionEnabled)
{
	ConcurrencyMode = InConcurrencyMode;
	ReleaseMode = InReleaseMode;
	UsageMode = InUsageMode;
	bStartInteractionEnabled = bInStartInteractionEnabled;
}

////////////////////////////
//! \author 준혁
//! \brief 테스트 Interactor 액터에 인증 사용자 ID를 부여하는 함수
//! \param Interactor 대상 액터
//! \param UserId 부여할 인증 사용자 ID
void UCPP_InteractableComponentTestProbe::SetTestUserId(const AActor* Interactor, int32 UserId)
{
	TestUserIds.Add(FObjectKey(Interactor), UserId);
}

////////////////////////////
//! \author 준혁
//! \brief 주입된 테스트 사용자 ID를 반환하는 함수. 부여되지 않은 액터는 ID 없음(INDEX_NONE)으로 판정된다.
//! \param Interactor 상호작용 액터
//! \return 테스트 사용자 ID, 없으면 INDEX_NONE
int32 UCPP_InteractableComponentTestProbe::ResolveInteractorUserId(const AActor* Interactor) const
{
	const int32* FoundId = TestUserIds.Find(FObjectKey(Interactor));
	return FoundId ? *FoundId : INDEX_NONE;
}
