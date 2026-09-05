#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CPP_AnimNotifyState_BossTelegraphEvent.generated.h"

struct FGameplayTag;

UCLASS()
class PROJECTP_API UCPP_AnimNotifyState_BossTelegraphEvent : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph")
	FName WindowId = NAME_None;

private:
	void SendTelegraphEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag, float Duration) const;
};
