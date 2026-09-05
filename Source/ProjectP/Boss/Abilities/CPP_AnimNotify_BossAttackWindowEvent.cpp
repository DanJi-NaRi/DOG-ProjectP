#include "CPP_AnimNotify_BossAttackWindowEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "GameFramework/Actor.h"

////////////////////////////
//! \author HanSeul
//! \brief Sends a boss attack window GameplayEvent from an animation notify.
//! \param MeshComp Skeletal mesh component that received this notify.
//! \param Animation Animation asset that owns this notify.
//! \param EventReference Notify event context.
void UCPP_AnimNotify_BossAttackWindowEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || WindowId.IsNone())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UCPP_BossWindowEventPayload* PayloadObject = NewObject<UCPP_BossWindowEventPayload>(GetTransientPackage());
	if (!PayloadObject)
	{
		return;
	}

	PayloadObject->WindowId = WindowId;

	// Default to the attack-window event so existing montages keep working; dash/other notifies override EventTag.
	const FGameplayTag ResolvedEventTag = EventTag.IsValid() ? EventTag : BossGameplayTags::Event_Boss_AttackWindow;

	FGameplayEventData EventData;
	EventData.EventTag = ResolvedEventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;
	EventData.OptionalObject = PayloadObject;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		OwnerActor,
		ResolvedEventTag,
		EventData
	);
}

////////////////////////////
//! \author HanSeul
//! \brief 재생 중인 몽타주의 노티파이 배치에서 WindowId가 일치하는 다음 윈도우 노티파이를 찾아 남은 실제 시간(초)을 계산한다.
//!        실효 재생속도(PlayRate × RateScale)를 반영하므로 배속 몽타주에서도 정확하다.
//! \param AnimInstance 몽타주를 재생 중인 애님 인스턴스.
//! \param Montage 노티파이를 검색할 몽타주.
//! \param WindowId 찾을 윈도우 노티파이의 WindowId(예: "Go").
//! \return 남은 시간(초). 노티파이를 찾지 못하거나 계산 불가하면 0(호출부는 즉시 꽉 찬 텔레그래프로 폴백).
float UCPP_AnimNotify_BossAttackWindowEvent::ComputeTimeUntilWindowNotify(const UAnimInstance* AnimInstance, const UAnimMontage* Montage, FName WindowId)
{
	if (!AnimInstance || !Montage || WindowId.IsNone())
	{
		return 0.0f;
	}

	const float CurrentPosition = AnimInstance->Montage_GetPosition(Montage);
	const float EffectivePlayRate = FMath::Abs(AnimInstance->Montage_GetPlayRate(Montage) * Montage->RateScale);
	if (EffectivePlayRate <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Pick the nearest matching window notify after the current position (montage time is linear between
	// Aim and Go — both live in the same section, so no section-jump handling is needed here).
	float BestTriggerTime = -1.0f;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UCPP_AnimNotify_BossAttackWindowEvent* WindowNotify = Cast<UCPP_AnimNotify_BossAttackWindowEvent>(NotifyEvent.Notify);
		if (!WindowNotify || WindowNotify->WindowId != WindowId)
		{
			continue;
		}

		const float TriggerTime = NotifyEvent.GetTriggerTime();
		if (TriggerTime > CurrentPosition && (BestTriggerTime < 0.0f || TriggerTime < BestTriggerTime))
		{
			BestTriggerTime = TriggerTime;
		}
	}

	return BestTriggerTime >= 0.0f ? (BestTriggerTime - CurrentPosition) / EffectivePlayRate : 0.0f;
}
