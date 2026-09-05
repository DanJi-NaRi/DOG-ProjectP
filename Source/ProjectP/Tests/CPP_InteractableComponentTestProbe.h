////////////////////////////
//! \file CPP_InteractableComponentTestProbe.h
//! \brief InteractableComponent 상태 자동화 테스트용 프로브 선언 파일이다.
//! \author 준혁
#pragma once

#include "CoreMinimal.h"
#include "Dungeon/Interactable/Components/InteractableComponent.h"
#include "CPP_InteractableComponentTestProbe.generated.h"

////////////////////////////
//! \class UCPP_InteractableComponentTestProbe
//! \brief 자동화 테스트에서 protected 정책을 설정하고 인증 사용자 ID를 주입하기 위한 테스트 전용 서브클래스다.
//!        production 코드에서는 사용하지 않는다.
UCLASS()
class PROJECTP_API UCPP_InteractableComponentTestProbe : public UInteractableComponent
{
	GENERATED_BODY()

public:
	//! RegisterComponent(BeginPlay) 전에 호출해 테스트 정책을 설정한다.
	void SetPoliciesForTest(EInteractionConcurrencyMode InConcurrencyMode, EInteractionReleaseMode InReleaseMode, EInteractionUsageMode InUsageMode, bool bInStartInteractionEnabled);

	//! 테스트 Interactor 액터에 인증 사용자 ID를 부여한다. 부여하지 않은 액터는 ID 없음으로 판정된다.
	void SetTestUserId(const AActor* Interactor, int32 UserId);

protected:
	virtual int32 ResolveInteractorUserId(const AActor* Interactor) const override;

private:
	TMap<FObjectKey, int32> TestUserIds;
};
