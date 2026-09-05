#include "CPP_AnimNotifyState_BossTelegraphEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "GameFramework/Actor.h"

void UCPP_AnimNotifyState_BossTelegraphEvent::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	SendTelegraphEvent(MeshComp, BossGameplayTags::Event_Boss_Telegraph_Begin, TotalDuration);
}

void UCPP_AnimNotifyState_BossTelegraphEvent::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	SendTelegraphEvent(MeshComp, BossGameplayTags::Event_Boss_Telegraph_End, 0.0f);
}

void UCPP_AnimNotifyState_BossTelegraphEvent::SendTelegraphEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag, float Duration) const
{
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
	PayloadObject->TelegraphDuration = Duration;

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = OwnerActor;
	EventData.Target = OwnerActor;
	EventData.OptionalObject = PayloadObject;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		OwnerActor,
		EventTag,
		EventData
	);
}
