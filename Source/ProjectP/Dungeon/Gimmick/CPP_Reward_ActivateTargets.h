////////////////////////////
//! \file CPP_Reward_ActivateTargets.h
//! \brief 지정한 타겟 액터들을 활성/비활성(문 개방·다리 생성 등)하는 보상 블록 선언 파일이다.
//! \editor 준혁 - ICPP_Activatable 대신 Zone의 IZoneSignalReceiver로 신호 전달(문 인터페이스 통일)
#pragma once

#include "CoreMinimal.h"
#include "CPP_GimmickReward.h"
#include "CPP_Reward_ActivateTargets.generated.h"

////////////////////////////
//! \class UCPP_Reward_ActivateTargets
//! \brief 레벨에 배치된 타겟(IZoneSignalReceiver 구현)들에게 Zone과 동일한 문 신호를 뿌린다.
//!        Execute -> OnZoneClear(문 열림), Revert -> OnZoneActive(문 닫힘).
//!        기믹은 Zone이 아니므로 SourceZone 인자는 nullptr로 전달된다. 타겟 BP는 SourceZone에 의존하면 안 된다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Activate Targets"))
class PROJECTP_API UCPP_Reward_ActivateTargets : public UCPP_GimmickReward
{
	GENERATED_BODY()

public:
	virtual void Execute(ACPP_GimmickBase* Owner) override;
	virtual void Revert(ACPP_GimmickBase* Owner) override;

private:
	void ApplyToTargets(bool bActive) const;

	//! 활성/비활성할 타겟들. 레벨의 문·다리 등을 지정한다(IZoneSignalReceiver 구현 필요).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Reward", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AActor>> Targets;
};
