////////////////////////////
//! \page MyAbilitySet.cpp
//! \brief MyGAS AbilitySet DataAsset 구현 파일이다.

#include "MyAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "MyGameplayAbilityBase.h"

////////////////////////////
//! \brief 부여된 AbilitySpecHandle을 저장한다.
//! \param Handle 저장할 AbilitySpecHandle
void FMyAbilitySetGrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

////////////////////////////
//! \brief 적용된 ActiveGameplayEffectHandle을 저장한다.
//! \param Handle 저장할 ActiveGameplayEffectHandle
void FMyAbilitySetGrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

////////////////////////////
//! \brief 저장된 AbilitySpecHandle과 ActiveGameplayEffectHandle을 ASC에서 제거하고 목록을 비운다.
//! \param ASC Ability와 GameplayEffect를 제거할 AbilitySystemComponent
void FMyAbilitySetGrantedHandles::TakeFromAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC) || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

////////////////////////////
//! \brief 서버 권한 ASC에 Ability 목록을 부여하고 GameplayEffect 목록을 자신에게 적용한다.
//! \param ASC Ability와 GameplayEffect를 부여할 AbilitySystemComponent
//! \param OutGrantedHandles 부여된 AbilitySpecHandle과 ActiveGameplayEffectHandle을 저장할 선택적 핸들 목록
//! \param SourceObject AbilitySpec에 사용할 SourceObject
void UMyAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, FMyAbilitySetGrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	if (!IsValid(ASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet grant skipped - ASC is null. AbilitySet: %s"), *GetNameSafe(this));
		return;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet grant skipped - ASC owner is not authoritative. ASC: %s, AbilitySet: %s"), *GetNameSafe(ASC), *GetNameSafe(this));
		return;
	}

	UObject* ResolvedSourceObject = SourceObject ? SourceObject : const_cast<UMyAbilitySet*>(this);

	for (const FMyAbilitySetEntry& Entry : GrantedAbilities)
	{
		if (!Entry.AbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet entry skipped - AbilityClass is null. AbilitySet: %s"), *GetNameSafe(this));
			continue;
		}

		const UGameplayAbility* AbilityCDO = Entry.AbilityClass->GetDefaultObject<UGameplayAbility>();
		if (!AbilityCDO)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet entry skipped - Ability CDO is null. AbilityClass: %s"), *GetNameSafe(Entry.AbilityClass));
			continue;
		}

		const int32 AbilityLevel = FMath::Max(Entry.AbilityLevel, 1);
		FGameplayAbilitySpec AbilitySpec(Entry.AbilityClass, AbilityLevel, INDEX_NONE, ResolvedSourceObject);

		const UMyGameplayAbilityBase* MyAbilityCDO = Cast<UMyGameplayAbilityBase>(AbilityCDO);
		if (MyAbilityCDO)
		{
			const FGameplayTag AbilityTag = MyAbilityCDO->GetAbilityTag();
			if (AbilityTag.IsValid())
			{
				AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityTag);
			}

			const FGameplayTag ResolvedInputTag = Entry.InputTag.IsValid() ? Entry.InputTag : MyAbilityCDO->GetInputTag();
			if (ResolvedInputTag.IsValid())
			{
				AbilitySpec.GetDynamicSpecSourceTags().AddTag(ResolvedInputTag);
			}
		}
		else if (Entry.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		const FGameplayAbilitySpecHandle AbilitySpecHandle = ASC->GiveAbility(AbilitySpec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}

		UE_LOG(LogTemp, Log, TEXT("MyAbilitySet granted ability - ASC: %s, Ability: %s, Level: %d"),
			*GetNameSafe(ASC),
			*GetNameSafe(Entry.AbilityClass),
			AbilityLevel);
	}

	for (const FMyAbilitySetGameplayEffect& Entry : GrantedGameplayEffects)
	{
		if (!Entry.GameplayEffectClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet effect entry skipped - GameplayEffectClass is null. AbilitySet: %s"), *GetNameSafe(this));
			continue;
		}

		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(ResolvedSourceObject);

		const float EffectLevel = FMath::Max(Entry.EffectLevel, 0.0f);
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(Entry.GameplayEffectClass, EffectLevel, EffectContext);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("MyAbilitySet effect entry skipped - SpecHandle is invalid. GameplayEffectClass: %s"), *GetNameSafe(Entry.GameplayEffectClass));
			continue;
		}

		const FActiveGameplayEffectHandle GameplayEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GameplayEffectHandle);
		}

		UE_LOG(LogTemp, Log, TEXT("MyAbilitySet granted gameplay effect - ASC: %s, GameplayEffect: %s, Level: %.2f"),
			*GetNameSafe(ASC),
			*GetNameSafe(Entry.GameplayEffectClass),
			EffectLevel);
	}
}
