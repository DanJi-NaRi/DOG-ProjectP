////////////////////////////
//! \page MyStreamingCombatMessageLibrary.cpp
//! \brief 전투 Payload를 GameplayMessageSubsystem Combat 채널로 발행하는 헬퍼 구현 파일이다.
#include "MyStreamingCombatMessageLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GAS/MyGameplayAbilityBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectTypes.h"
#include "MyGameplayTags.h"

namespace
{
	//! \author 장효제
	//! \brief 계층형 조건 매칭에 쓸 가장 구체적인 GameplayTag를 찾는다.
	FGameplayTag FindMostSpecificMatchingTag(const FGameplayTagContainer& Tags, FGameplayTag RootTag)
	{
		if (!RootTag.IsValid())
		{
			return FGameplayTag();
		}

		TArray<FGameplayTag> TagArray;
		Tags.GetGameplayTagArray(TagArray);

		FGameplayTag BestTag;
		int32 BestTagLength = INDEX_NONE;
		for (const FGameplayTag& Tag : TagArray)
		{
			if (!Tag.IsValid() || !Tag.MatchesTag(RootTag))
			{
				continue;
			}

			const int32 TagLength = Tag.ToString().Len();
			if (TagLength > BestTagLength)
			{
				BestTag = Tag;
				BestTagLength = TagLength;
			}
		}

		return BestTag;
	}

	FGameplayTag RequestTagNoWarn(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	UAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor)
	{
		const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor);
		return AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
	}
}

//! \author 장효제
//! \brief Combat 채널로 전투 Payload를 발송한다.
void UMyStreamingCombatMessageLibrary::BroadcastCombatPayload(const UObject* WorldContextObject, const FMyStreamingCombatPayload& Payload)
{
	if (!WorldContextObject || !Payload.EventTag.IsValid())
	{
		return;
	}

	if (!UGameplayMessageSubsystem::HasInstance(WorldContextObject))
	{
		return;
	}

	UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_Combat,
		Payload);
}


//! \author 장효제
//! \brief 스킬 사용 이벤트를 Payload로 변환해 Combat 채널에 발송한다.
void UMyStreamingCombatMessageLibrary::BroadcastSkillUsed(const UObject* WorldContextObject, const AActor* InstigatorActor, FGameplayTag SkillTag)
{
	if (!SkillTag.IsValid())
	{
		return;
	}

	FMyStreamingCombatPayload Payload;
	Payload.EventTag = MyGameplayTags::Streaming_Event_Combat_SkillUsed;
	Payload.InstigatorTag = ResolveStreamingActorTag(InstigatorActor);
	Payload.SkillTag = SkillTag;

	BroadcastCombatPayload(WorldContextObject ? WorldContextObject : InstigatorActor, Payload);
}

//! \author 장효제
//! \brief Actor의 ASC에서 Character 태그를 우선 추출하고 없으면 Faction 태그로 대체한다.
FGameplayTag UMyStreamingCombatMessageLibrary::ResolveStreamingActorTag(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return FGameplayTag();
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	if (const FGameplayTag CharacterTag = FindMostSpecificMatchingTag(OwnedTags, RequestTagNoWarn(TEXT("Character"))); CharacterTag.IsValid())
	{
		return CharacterTag;
	}

	// --- 이거 아래에는 fallback용 ---
	if (OwnedTags.HasTag(MyGameplayTags::Faction_Enemy_Boss))
	{
		return MyGameplayTags::Faction_Enemy_Boss;
	}

	if (OwnedTags.HasTag(MyGameplayTags::Faction_Enemy))
	{
		return MyGameplayTags::Faction_Enemy;
	}

	if (OwnedTags.HasTag(MyGameplayTags::Faction_Player))
	{
		return MyGameplayTags::Faction_Player;
	}

	return FindMostSpecificMatchingTag(OwnedTags, RequestTagNoWarn(TEXT("Faction")));
}

//! \author 장효제
//! \brief EffectSpec의 SourceObject와 SourceTags에서 스킬 태그를 추출한다.
FGameplayTag UMyStreamingCombatMessageLibrary::ResolveStreamingSkillTagFromEffectSpec(const FGameplayEffectSpec& EffectSpec)
{
	if (const UObject* SourceObject = EffectSpec.GetContext().GetSourceObject())
	{
		if (const UMyGameplayAbilityBase* Ability = Cast<UMyGameplayAbilityBase>(SourceObject))
		{
			if (const FGameplayTag AbilityTag = Ability->GetAbilityTag(); AbilityTag.IsValid())
			{
				return AbilityTag;
			}
		}
	}

	FGameplayTagContainer CandidateTags;
	EffectSpec.GetAllAssetTags(CandidateTags);

	if (const FGameplayTagContainer* SourceTags = EffectSpec.CapturedSourceTags.GetAggregatedTags())
	{
		CandidateTags.AppendTags(*SourceTags);
	}

	return FindMostSpecificMatchingTag(CandidateTags, RequestTagNoWarn(TEXT("Skill")));
}

////////////////////////////
//! \author 장효제
//! \brief 조작 사실을 스트리밍 채널로 발행한다.
//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
//! \param UserIndex 조작한 플레이어의 UserIndex다.
void MyStreamingPlayerInput::BroadcastPlayerInput(
	const UObject* WorldContextObject,
	const int32 UserIndex)
{
	if (!WorldContextObject || !UGameplayMessageSubsystem::HasInstance(WorldContextObject))
	{
		return;
	}

	FMyStreamingPlayerInputPayload Payload;
	Payload.UserIndex = UserIndex;
	UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_PlayerInput,
		Payload);
}

////////////////////////////
//! \author 장효제
//! \brief 파티 사건 누계 사실을 스트리밍 채널로 발행한다.
//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
//! \param EventTag 무슨 사건인지다.
//! \param SourceTag 사건을 좁히는 태그다.
void MyStreamingCountEvent::BroadcastCountEvent(
	const UObject* WorldContextObject,
	const FGameplayTag EventTag,
	const FGameplayTag SourceTag)
{
	if (!WorldContextObject
		|| !EventTag.IsValid()
		|| !UGameplayMessageSubsystem::HasInstance(WorldContextObject))
	{
		return;
	}

	FMyStreamingCountEventPayload Payload;
	Payload.EventTag = EventTag;
	Payload.SourceTag = SourceTag;
	UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_CountEvent,
		Payload);
}

////////////////////////////
//! \author 장효제
//! \brief 아이템 사건 사실을 스트리밍 채널로 발행한다.
//! \param WorldContextObject GameplayMessageSubsystem World를 찾을 객체다.
//! \param EventTag 사용인지 구매인지다.
//! \param ItemId DT_ItemTable의 RowName이다.
//! \param UserIndex 사건을 일으킨 플레이어다.
//! \param Count 한 번에 쓰거나 산 개수다.
void MyStreamingCountEvent::BroadcastItemEvent(
	const UObject* WorldContextObject,
	const FGameplayTag EventTag,
	const FName ItemId,
	const int32 UserIndex,
	const int32 Count)
{
	if (!WorldContextObject
		|| !EventTag.IsValid()
		|| ItemId.IsNone()
		|| Count <= 0
		|| !UGameplayMessageSubsystem::HasInstance(WorldContextObject))
	{
		return;
	}

	FMyStreamingItemEventPayload Payload;
	Payload.EventTag = EventTag;
	Payload.ItemId = ItemId;
	Payload.UserIndex = UserIndex;
	Payload.Count = Count;
	UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_Item,
		Payload);
}

////////////////////////////
//! \author 장효제
//! \brief 상태 켜짐·꺼짐 사실을 스트리밍 채널로 발행한다.
//! \param SourceActor 상태를 켜거나 끈 객체다. World 문맥이자 출처다.
//! \param StateTag 어떤 상태인지다.
//! \param bEntered 켜졌으면 true, 꺼졌으면 false다.
void MyStreamingState::BroadcastState(
	AActor* const SourceActor,
	const FGameplayTag StateTag,
	const bool bEntered)
{
	// 출처 없이는 어느 객체가 켠 상태인지 알 수 없어 셈이 어긋난다.
	if (!SourceActor
		|| !StateTag.IsValid()
		|| !UGameplayMessageSubsystem::HasInstance(SourceActor))
	{
		return;
	}

	FMyStreamingStatePayload Payload;
	Payload.StateTag = StateTag;
	Payload.SourceActor = SourceActor;
	Payload.bEntered = bEntered;
	UGameplayMessageSubsystem::Get(SourceActor).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_State,
		Payload);
}
