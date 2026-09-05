#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "CPP_AnimNotify_BossAttackWindowEvent.generated.h"

class UAnimInstance;
class UAnimMontage;

UCLASS()
class PROJECTP_API UCPP_AnimNotify_BossAttackWindowEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	//! \brief 재생 중인 몽타주에서 WindowId가 일치하는 다음 윈도우 노티파이까지 남은 실제 시간(초)을 계산한다.
	//!        텔레그래프 채움 시간을 몽타주 노티파이 배치에서 자동으로 얻을 때 사용(수동 튜닝 불필요).
	static float ComputeTimeUntilWindowNotify(const UAnimInstance* AnimInstance, const UAnimMontage* Montage, FName WindowId);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	FName WindowId = NAME_None;

	//! \brief 발사할 GameplayEvent 태그. 비워두면 Event.Boss.AttackWindow로 동작(기존 몽타주 호환). 돌진 등엔 Event.Boss.Dash 지정.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack")
	FGameplayTag EventTag;
};
