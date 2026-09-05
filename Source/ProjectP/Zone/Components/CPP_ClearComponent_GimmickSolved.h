////////////////////////////
//! \file CPP_ClearComponent_GimmickSolved.h
//! \brief 기믹 클리어(Solved)를 Zone 클리어 조건으로 연결하는 어댑터 컴포넌트 선언 파일이다.
//! \author 준혁
#pragma once

#include "CoreMinimal.h"
#include "ClearComponent.h"
#include "../../Dungeon/Gimmick/CPP_GimmickTypes.h"
#include "../../Dungeon/Interactable/Components/InteractableComponent.h"
#include "CPP_ClearComponent_GimmickSolved.generated.h"

class ACPP_GimmickBase;
class ACPP_ObeliskActor;

////////////////////////////
//! \class UCPP_ClearComponent_GimmickSolved
//! \brief Zone BP에 부착하는 클리어 조건 어댑터. Zone이 Active가 되면 대상 기믹을 Activate하고,
//!        기믹이 Solved가 되면 MarkClearSatisfied로 Zone에 클리어를 통보한다.
//!        TriggerActor(오벨리스크 등 InteractableComponent 보유 액터)를 지정하면 흐름이 바뀐다:
//!        Zone Active 시 기믹을 바로 켜지 않고 TriggerActor의 상호작용만 개방하며,
//!        - 오벨리스크면 OnGimmickTriggered(타이밍: 상호작용 즉시/대화 선택지/수동)를 구독해 발화 시 기믹을 Activate하고,
//!        - 일반 상호작용 액터면 OnInteractionStarted(승인된 상호작용 시작 즉시)를 구독한다.
//!        주의: Zone의 DeactivateClearCondition을 기믹 Deactivate로 매핑하지 않는다.
//!        (기믹 Deactivate는 보상을 Revert하므로 존 클리어 직후 문이 도로 닫힌다)
UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Clear Component (Gimmick Solved)"))
class PROJECTP_API UCPP_ClearComponent_GimmickSolved : public UClearComponent
{
	GENERATED_BODY()

public:
	//! Zone Active 시: TriggerActor가 있으면 상호작용을 개방하고, 없으면 즉시 기믹을 활성화한다.
	virtual void ActivateClearCondition_Implementation() override;

	//! Zone Clear 등 감시 종료 시: TriggerActor의 상호작용을 닫는다. (기믹은 건드리지 않는다)
	virtual void DeactivateClearCondition_Implementation() override;

	//! Zone 재시작 등 리셋 시: 대상 기믹을 리셋하고 TriggerActor의 상호작용을 다시 닫는다.
	virtual void ResetClearCondition_Implementation() override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleGimmickStateChanged(EGimmickState OldState, EGimmickState NewState);

	//! [서버] 오벨리스크의 기믹 트리거(의미 이벤트)가 발화하면 기믹을 활성화한다.
	UFUNCTION()
	void HandleTriggerActivated(AActor* InInstigator);

    //! [서버] 오벨리스크 대화의 초기화 투표 선택을 현재 Zone의 공용 투표 요청으로 연결한다.
    UFUNCTION()
    void HandleGimmickResetVoteRequested(AActor* InInstigator);

	//! [서버] 오벨리스크가 아닌 일반 상호작용 액터의 승인된 상호작용 시작을 트리거로 간주한다.
	UFUNCTION()
	void HandleTriggerInteractionStarted(const FInteractionStartContext& Context);

	//! 이 Zone의 클리어 조건이 되는 기믹. 레벨에서 인스턴스별로 지정한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear")
	TObjectPtr<ACPP_GimmickBase> TargetGimmick;

	//! 기믹 활성화 관문이 되는 상호작용 액터(오벨리스크 등, InteractableComponent 필요).
	//! 미지정 시 기존처럼 Zone Active에 기믹을 즉시 활성화한다.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear")
    TObjectPtr<AActor> TriggerActor;

    //! 실제 기믹 초기화가 성공한 뒤 다음 초기화 투표를 시작할 수 있을 때까지의 시간.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Clear|Gimmick Reset", meta = (ClampMin = "0.0"))
    float GimmickResetCooldownSeconds = 10.0f;

private:
    float GimmickResetCooldownEndServerTime = 0.0f;
	int32 PartyResetCount = 0;

    bool HasZoneAuthority() const;
    UInteractableComponent* GetTriggerInteractable() const;
    void SetTriggerInteractionEnabled(bool bEnabled) const;
    void UnbindTrigger();
    void ActivateGimmick();
    void HandleGimmickResetVotePassed();
};
