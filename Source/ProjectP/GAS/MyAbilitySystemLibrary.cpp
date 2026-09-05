////////////////////////////
//! \page MyAbilitySystemLibrary.cpp
//! \brief MyGAS AbilitySystem 접근과 SetByCaller 값 할당 유틸리티를 구현한다.
#include "MyAbilitySystemLibrary.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "MyAttributeSet.h"
#include "MyCooldownGameplayEffect.h"
#include "MyGameplayAbilityBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "../MyGameplayTags.h"

////////////////////////////
//! \author 장효제
//! \brief AbilitySystemInterface를 구현한 Actor에서 AbilitySystemComponent를 안전하게 가져온다.
//! \param Actor AbilitySystemComponent를 제공할 수 있는 Actor
//! \return 유효한 AbilitySystemComponent 포인터, 없으면 nullptr
UAbilitySystemComponent* UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor);
	if (!AbilitySystemInterface)
	{
		return nullptr;
	}

	return AbilitySystemInterface->GetAbilitySystemComponent();
}

////////////////////////////
//! \author HanSeul
//! \brief 플레이어 공격 대상 검색에 사용할 Pawn과 Destructible Object Type 조건을 생성한다.
//! \return ECC_Pawn과 ECC_Destructible이 등록된 Object Query 조건
FCollisionObjectQueryParams UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery()
{
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Destructible);
	return ObjectQueryParams;
}

////////////////////////////
//! \author 장효제
//! \brief ASC에 부여된 Ability 중 InputTag와 일치하는 Ability를 CooldownTag 상태를 확인한 뒤 발동한다.
//! \param ASC Ability를 보유한 AbilitySystemComponent
//! \param InputTag 발동할 Ability를 찾는 입력 GameplayTag
//! \return Ability 발동 요청에 성공하면 true, 차단되거나 실패하면 false
bool UMyAbilitySystemLibrary::TryActivateAbilityByInputTag(UAbilitySystemComponent* ASC, FGameplayTag InputTag)
{
	if (!IsValid(ASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag failed - ASC is null"));
		return false;
	}

	if (!InputTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag failed - InputTag is invalid. ASC: %s, OwnerActor: %s, AvatarActor: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()));
		return false;
	}

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (!AbilitySpec.Handle.IsValid())
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		UGameplayAbility* Ability = AbilitySpec.Ability.Get();
		if (!Ability)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - Ability is null. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
				*GetNameSafe(ASC),
				*GetNameSafe(ASC->GetOwnerActor()),
				*GetNameSafe(ASC->GetAvatarActor()),
				*InputTag.ToString());
			return false;
		}

		const UMyGameplayAbilityBase* MyAbilityCDO = Cast<UMyGameplayAbilityBase>(AbilitySpec.Ability);
		if (MyAbilityCDO)
		{
			const FGameplayTag CooldownTag = MyAbilityCDO->GetCooldownTag();
			if (CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag))
			{
				UE_LOG(LogTemp, Log, TEXT("MyGAS activate blocked by cooldown - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, CooldownTag: %s, Ability: %s"),
					*GetNameSafe(ASC),
					*GetNameSafe(ASC->GetOwnerActor()),
					*GetNameSafe(ASC->GetAvatarActor()),
					*InputTag.ToString(),
					*CooldownTag.ToString(),
					*GetNameSafe(AbilitySpec.Ability));
				return false;
			}
		}

		const bool bActivated = ASC->TryActivateAbility(AbilitySpec.Handle);
		UE_LOG(LogTemp, Log, TEXT("MyGAS activate by input tag - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, Ability: %s, Result: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()),
			*InputTag.ToString(),
			*GetNameSafe(AbilitySpec.Ability),
			bActivated ? TEXT("true") : TEXT("false"));
		return bActivated;
	}

	UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag failed - no matching ability. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
		*GetNameSafe(ASC),
		*GetNameSafe(ASC->GetOwnerActor()),
		*GetNameSafe(ASC->GetAvatarActor()),
		*InputTag.ToString());
	return false;
}

////////////////////////////
//! \author 장효제
//! \brief ASC에 부여된 Ability 중 InputTag와 일치하는 Ability를 CooldownTag 상태를 확인한 뒤 TriggerEventData를 포함해 발동한다.
//! \param ASC Ability를 보유한 AbilitySystemComponent
//! \param InputTag 발동할 Ability를 찾는 입력 GameplayTag
//! \param EventData Ability 발동 시 ActivateAbility의 TriggerEventData로 전달할 GameplayEventData
//! \return Ability 발동 요청에 성공하면 true, 차단되거나 실패하면 false
bool UMyAbilitySystemLibrary::TryActivateAbilityByInputTagWithEventData(UAbilitySystemComponent* ASC, FGameplayTag InputTag, const FGameplayEventData& EventData)
{
	if (!IsValid(ASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - ASC is null"));
		return false;
	}

	if (!InputTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - InputTag is invalid. ASC: %s, OwnerActor: %s, AvatarActor: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()));
		return false;
	}

	if (!ASC->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - AbilityActorInfo is invalid. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()),
			*InputTag.ToString());
		return false;
	}

	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (!AbilitySpec.Handle.IsValid())
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		UGameplayAbility* Ability = AbilitySpec.Ability.Get();
		if (!Ability)
		{
			UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - Ability is null. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
				*GetNameSafe(ASC),
				*GetNameSafe(ASC->GetOwnerActor()),
				*GetNameSafe(ASC->GetAvatarActor()),
				*InputTag.ToString());
			return false;
		}

		const UMyGameplayAbilityBase* MyAbilityCDO = Cast<UMyGameplayAbilityBase>(AbilitySpec.Ability);
		if (MyAbilityCDO)
		{
			const FGameplayTag CooldownTag = MyAbilityCDO->GetCooldownTag();
			if (CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag))
			{
				UE_LOG(LogTemp, Log, TEXT("MyGAS activate with event data sees cooldown tag; deferring to GAS activation check - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, CooldownTag: %s, CooldownTagCount: %d, Ability: %s"),
					*GetNameSafe(ASC),
					*GetNameSafe(ASC->GetOwnerActor()),
					*GetNameSafe(ASC->GetAvatarActor()),
					*InputTag.ToString(),
					*CooldownTag.ToString(),
					ASC->GetTagCount(CooldownTag),
					*GetNameSafe(AbilitySpec.Ability));
			}
		}

		FGameplayEventData MutableEventData = EventData;
		const FGameplayTag EventTag = MutableEventData.EventTag.IsValid() ? MutableEventData.EventTag : InputTag;
		MutableEventData.EventTag = EventTag;

		const FGameplayAbilityActorInfo* ActorInfo = ASC->AbilityActorInfo.Get();
		const EGameplayAbilityNetExecutionPolicy::Type NetExecutionPolicy = Ability->GetNetExecutionPolicy();
		const bool bShouldRequestServerOnly =
			!ActorInfo->IsNetAuthority()
			&& (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly
				|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

		if (bShouldRequestServerOnly)
		{
			if (!ActorInfo->IsLocallyControlled())
			{
				UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - server initiated ability requested from non-local client. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, Ability: %s"),
					*GetNameSafe(ASC),
					*GetNameSafe(ASC->GetOwnerActor()),
					*GetNameSafe(ASC->GetAvatarActor()),
					*InputTag.ToString(),
					*GetNameSafe(Ability));
				return false;
			}

			FGameplayTagContainer FailureTags;
			const FGameplayTagContainer* SourceTags = &MutableEventData.InstigatorTags;
			const FGameplayTagContainer* TargetTags = &MutableEventData.TargetTags;
			const bool bShouldRespondToEvent = Ability->ShouldAbilityRespondToEvent(ActorInfo, &MutableEventData);
			const bool bCanActivateAbility = bShouldRespondToEvent
				&& Ability->CanActivateAbility(AbilitySpec.Handle, ActorInfo, SourceTags, TargetTags, &FailureTags);
			if (!bShouldRespondToEvent || !bCanActivateAbility)
			{
				ASC->NotifyAbilityFailed(AbilitySpec.Handle, Ability, FailureTags);
				UE_LOG(LogTemp, Log, TEXT("MyGAS activate by input tag with event data rejected before server request - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, EventTag: %s, Ability: %s, FailureTags: %s"),
					*GetNameSafe(ASC),
					*GetNameSafe(ASC->GetOwnerActor()),
					*GetNameSafe(ASC->GetAvatarActor()),
					*InputTag.ToString(),
					*EventTag.ToString(),
					*GetNameSafe(Ability),
					*FailureTags.ToStringSimple());

				if (FailureTags.IsEmpty())
				{
					UE_LOG(LogTemp, Log, TEXT("MyGAS activate by input tag with event data rejected with empty FailureTags before server request - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, EventTag: %s, Ability: %s, SpecActive: %s, ShouldRespondToEvent: %s, CanActivateAbility: %s, NetExecutionPolicy: %d"),
						*GetNameSafe(ASC),
						*GetNameSafe(ASC->GetOwnerActor()),
						*GetNameSafe(ASC->GetAvatarActor()),
						*InputTag.ToString(),
						*EventTag.ToString(),
						*GetNameSafe(Ability),
						AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"),
						bShouldRespondToEvent ? TEXT("true") : TEXT("false"),
						bCanActivateAbility ? TEXT("true") : TEXT("false"),
						static_cast<int32>(NetExecutionPolicy));
				}
				return false;
			}

			UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data requires owner server RPC - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, EventTag: %s, EventMagnitude: %.2f, Ability: %s, NetExecutionPolicy: %d"),
				*GetNameSafe(ASC),
				*GetNameSafe(ASC->GetOwnerActor()),
				*GetNameSafe(ASC->GetAvatarActor()),
				*InputTag.ToString(),
				*EventTag.ToString(),
				MutableEventData.EventMagnitude,
				*GetNameSafe(Ability),
				static_cast<int32>(NetExecutionPolicy));
			return false;
		}

		const bool bActivated = ASC->InternalTryActivateAbility(
			AbilitySpec.Handle,
			FPredictionKey(),
			nullptr,
			nullptr,
			&MutableEventData
		);

		UE_LOG(LogTemp, Log, TEXT("MyGAS activate by input tag with event data - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, EventTag: %s, EventMagnitude: %.2f, Ability: %s, Result: %s, FailureTags: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()),
			*InputTag.ToString(),
			*EventTag.ToString(),
			MutableEventData.EventMagnitude,
			*GetNameSafe(AbilitySpec.Ability),
			bActivated ? TEXT("true") : TEXT("false"),
			*ASC->InternalTryActivateAbilityFailureTags.ToStringSimple());
		if (!bActivated && ASC->InternalTryActivateAbilityFailureTags.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("MyGAS activate by input tag with event data failed with empty FailureTags - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, EventTag: %s, Ability: %s, SpecActive: %s, NetExecutionPolicy: %d"),
				*GetNameSafe(ASC),
				*GetNameSafe(ASC->GetOwnerActor()),
				*GetNameSafe(ASC->GetAvatarActor()),
				*InputTag.ToString(),
				*EventTag.ToString(),
				*GetNameSafe(AbilitySpec.Ability),
				AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"),
				static_cast<int32>(NetExecutionPolicy));
		}
		return bActivated;
	}

	UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag with event data failed - no matching ability. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
		*GetNameSafe(ASC),
		*GetNameSafe(ASC->GetOwnerActor()),
		*GetNameSafe(ASC->GetAvatarActor()),
		*InputTag.ToString());
	return false;
}

////////////////////////////
//! \author HanUl
//! \brief ASC에 부여된 Ability 중 InputTag와 일치하는 충전식 Ability를 쿨다운 선차단 없이 발동한다.
//! \param ASC Ability를 보유한 AbilitySystemComponent
//! \param InputTag 발동할 Ability를 찾는 입력 GameplayTag
//! \return Ability 발동 요청에 성공하면 true, 차단되거나 실패하면 false
bool UMyAbilitySystemLibrary::TryActivateChargeAbilityByInputTag(UAbilitySystemComponent* ASC, FGameplayTag InputTag)
{

	if (!IsValid(ASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag failed - ASC is null"));
		return false;
	}

	if (!InputTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS activate by input tag failed - InputTag is invalid. ASC: %s, OwnerActor: %s, AvatarActor: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()));
		return false;
	}




	const TArray<FGameplayAbilitySpec>& ActivatableAbilities = ASC->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (!AbilitySpec.Handle.IsValid())
		{
			continue;
		}

		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		const bool bActivated = ASC->TryActivateAbility(AbilitySpec.Handle);
		UE_LOG(LogTemp, Log, TEXT("MyGAS charge activate by input tag - ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s, Ability: %s, Result: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC->GetOwnerActor()),
			*GetNameSafe(ASC->GetAvatarActor()),
			*InputTag.ToString(),
			*GetNameSafe(AbilitySpec.Ability),
			bActivated ? TEXT("true") : TEXT("false"));
		return bActivated;
	}

	UE_LOG(LogTemp, Warning, TEXT("MyGAS charge activate by input tag failed - no matching ability. ASC: %s, OwnerActor: %s, AvatarActor: %s, InputTag: %s"),
		*GetNameSafe(ASC),
		*GetNameSafe(ASC->GetOwnerActor()),
		*GetNameSafe(ASC->GetAvatarActor()),
		*InputTag.ToString());
	return false;
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Damage SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Damage 할당할 피해량
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyAbilitySystemLibrary::AssignSetByCallerDamage(FGameplayEffectSpecHandle& SpecHandle, float Damage)
{
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Damage, Damage);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief GameplayEffectSpecHandle에 Data.Coefficient SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Coefficient 할당할 스킬 피해 계수
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyAbilitySystemLibrary::AssignSetByCallerCoefficient(FGameplayEffectSpecHandle& SpecHandle, float Coefficient)
{
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Coefficient, Coefficient);
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Heal SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Heal 할당할 회복량
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyAbilitySystemLibrary::AssignSetByCallerHeal(FGameplayEffectSpecHandle& SpecHandle, float Heal)
{
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Heal, Heal);
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffectSpecHandle에 Data.Shield SetByCaller 크기를 할당한다.
//! \param SpecHandle 값을 적용할 GameplayEffectSpecHandle
//! \param Shield 할당할 보호막 수치
//! \return 값 할당에 성공하면 true, SpecHandle이 유효하지 않으면 false
bool UMyAbilitySystemLibrary::AssignSetByCallerShield(FGameplayEffectSpecHandle& SpecHandle, float Shield)
{
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Shield, Shield);
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief Source ASC가 TargetActor의 ASC에 Data.Damage SetByCaller GameplayEffect를 적용한다.
//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
//! \param TargetActor Damage GameplayEffect를 받을 Actor
//! \param DamageEffectClass Data.Damage SetByCaller를 IncomingDamage에 전달하는 GameplayEffect class
//! \param Damage 적용할 피해량
//! \param Level GameplayEffect Spec level
//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
bool UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Damage,
	float Level,
	float CurseGaugeAmount
)
{
	if (!IsValid(SourceASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - SourceASC is null"));
		return false;
	}

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - TargetActor is null. SourceASC: %s"),
			*GetNameSafe(SourceASC));
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetAbilitySystemComponentFromActor(TargetActor);
	if (!IsValid(TargetASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - Target ASC not found. SourceASC: %s, TargetActor: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor));
		return false;
	}

	if (!DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - DamageEffectClass is null. SourceASC: %s, TargetActor: %s, TargetASC: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(TargetASC));
		return false;
	}

	if (Damage <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - Damage must be positive. SourceASC: %s, TargetActor: %s, Damage: %.2f"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			Damage);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceASC->GetAvatarActor());

	const float SafeLevel = FMath::Max(Level, 1.0f);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, SafeLevel, EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - SpecHandle is invalid. SourceASC: %s, TargetActor: %s, Effect: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(DamageEffectClass));
		return false;
	}

	if (!AssignSetByCallerDamage(SpecHandle, Damage))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply damage effect failed - SetByCaller assignment failed. SourceASC: %s, TargetActor: %s, Effect: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(DamageEffectClass));
		return false;
	}

	if (CurseGaugeAmount > 0.0f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_CurseGauge, CurseGaugeAmount);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogTemp, Log, TEXT("MyGAS apply damage effect to target - SourceASC: %s, SourceAvatar: %s, TargetActor: %s, TargetASC: %s, Effect: %s, Damage: %.2f, Level: %.2f"),
		*GetNameSafe(SourceASC),
		*GetNameSafe(SourceASC->GetAvatarActor()),
		*GetNameSafe(TargetActor),
		*GetNameSafe(TargetASC),
		*GetNameSafe(DamageEffectClass),
		Damage,
		SafeLevel);

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Source ASC가 TargetActor의 ASC에 Data.Coefficient SetByCaller GameplayEffect를 적용한다.
//!        공격력 곱셈은 UMyDamageExecutionCalculation이 캡처한 Source AttackPower로 수행한다.
//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
//! \param TargetActor Damage GameplayEffect를 받을 Actor
//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
//! \param Coefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
//! \param Level GameplayEffect Spec level
//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
bool UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Coefficient,
	float Level,
	float CurseGaugeAmount
)
{
	return ApplySkillCoefficientDamageEffectToTargetActor(
		SourceASC,
		TargetActor,
		DamageEffectClass,
		Coefficient,
		FGameplayTag(),
		Level,
		CurseGaugeAmount
	);
}

////////////////////////////
//! \author HanUl
//! \brief 계수 피해에 스킬 쿨다운 태그를 꼬리표(DynamicAssetTag)로 실어 적용한다.
//!        대상 사망 시 처치 스킬 식별(처치 시 쿨 초기화 등)에 사용된다.
//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
//! \param TargetActor Damage GameplayEffect를 받을 Actor
//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
//! \param Coefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
//! \param SkillCooldownTag Spec에 실을 스킬 쿨다운 태그(무효면 꼬리표 생략)
//! \param Level GameplayEffect Spec level
//! \return 적용 요청을 만들 수 있으면 true, 필수 입력이 없으면 false
bool UMyAbilitySystemLibrary::ApplySkillCoefficientDamageEffectToTargetActor(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Coefficient,
	FGameplayTag SkillCooldownTag,
	float Level,
	float CurseGaugeAmount
)
{
	return ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
		SourceASC,
		TargetActor,
		DamageEffectClass,
		Coefficient,
		SkillCooldownTag,
		FGameplayTag(),
		Level,
		CurseGaugeAmount
	);
}

////////////////////////////
//! \author HanUl
//! \brief 계수 피해에 쿨다운 태그와 스킬 입력 기반 공격자 적중 카메라 피드백 태그를 실어 적용한다.
//! \param SourceASC GameplayEffect Spec을 생성하고 적용을 요청할 Source ASC
//! \param TargetActor Damage GameplayEffect를 받을 Actor
//! \param DamageEffectClass UMyDamageExecutionCalculation을 사용하는 GameplayEffect class
//! \param Coefficient 스킬 피해 계수
//! \param SkillCooldownTag 처치 스킬 식별용 쿨다운 태그
//! \param SkillInputTag Basic/Skill/Ultimate 피드백 분류용 입력 태그
//! \param Level GameplayEffect Spec level
//! \return 적용 요청을 만들 수 있으면 true
bool UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
	UAbilitySystemComponent* SourceASC,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float Coefficient,
	FGameplayTag SkillCooldownTag,
	FGameplayTag SkillInputTag,
	float Level,
	float CurseGaugeAmount
)
{
	if (!IsValid(SourceASC) || !IsValid(TargetActor) || !DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply coefficient damage failed - invalid input. SourceASC: %s, TargetActor: %s, Effect: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(DamageEffectClass));
		return false;
	}

	if (Coefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply coefficient damage failed - Coefficient must be positive. SourceASC: %s, TargetActor: %s, Coefficient: %.3f"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			Coefficient);
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetAbilitySystemComponentFromActor(TargetActor);
	if (!IsValid(TargetASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply coefficient damage failed - Target ASC not found. SourceASC: %s, TargetActor: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor));
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceASC->GetAvatarActor());

	const float SafeLevel = FMath::Max(Level, 1.0f);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, SafeLevel, EffectContext);
	if (!SpecHandle.IsValid() || !AssignSetByCallerCoefficient(SpecHandle, Coefficient))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS apply coefficient damage failed - Spec creation or SetByCaller failed. SourceASC: %s, TargetActor: %s, Effect: %s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(DamageEffectClass));
		return false;
	}

	if (CurseGaugeAmount > 0.0f)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_CurseGauge, CurseGaugeAmount);
	}

	// 처치 스킬 식별용 꼬리표: 대상 사망 시 이 태그가 킬 이벤트 payload로 전달된다.
	if (SkillCooldownTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(SkillCooldownTag);
	}

	AddAttackerHitCameraFeedbackTag(SpecHandle, SkillInputTag);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogTemp, Log, TEXT("MyGAS apply coefficient damage to target - SourceASC: %s, SourceAvatar: %s, TargetActor: %s, Effect: %s, Coefficient: %.3f, Level: %.2f"),
		*GetNameSafe(SourceASC),
		*GetNameSafe(SourceASC->GetAvatarActor()),
		*GetNameSafe(TargetActor),
		*GetNameSafe(DamageEffectClass),
		Coefficient,
		SafeLevel);

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력을 공격자 적중 카메라 피드백의 Basic/Skill/Ultimate 등급으로 변환한다.
//! \param SkillInputTag 분류할 스킬 입력 태그
//! \return 대응하는 카메라 피드백 태그, 지원하지 않으면 무효 태그
FGameplayTag UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(FGameplayTag SkillInputTag)
{
	if (SkillInputTag.MatchesTagExact(MyGameplayTags::Input_Skill_Basic))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Basic;
	}

	if (SkillInputTag.MatchesTagExact(MyGameplayTags::Input_Skill_C))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Ultimate;
	}

	if (SkillInputTag.MatchesTagExact(MyGameplayTags::Input_Skill_Q)
		|| SkillInputTag.MatchesTagExact(MyGameplayTags::Input_Skill_E)
		|| SkillInputTag.MatchesTagExact(MyGameplayTags::Input_Skill_R))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Skill;
	}

	return FGameplayTag();
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력에서 해석한 공격자 적중 카메라 피드백 태그를 EffectSpec에 추가한다.
//! \param SpecHandle 태그를 추가할 GameplayEffectSpecHandle
//! \param SkillInputTag Basic/Q/E/R/C를 구분할 스킬 입력 태그
//! \return 피드백 태그를 추가했으면 true
bool UMyAbilitySystemLibrary::AddAttackerHitCameraFeedbackTag(
	FGameplayEffectSpecHandle& SpecHandle,
	FGameplayTag SkillInputTag
)
{
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	const FGameplayTag CameraFeedbackTag = ResolveAttackerHitCameraFeedbackTag(SkillInputTag);
	if (!CameraFeedbackTag.IsValid())
	{
		return false;
	}

	SpecHandle.Data->AddDynamicAssetTag(CameraFeedbackTag);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Effect AssetTag에서 지원하는 공격자 적중 카메라 피드백 태그를 우선순위 순으로 찾는다.
//! \param EffectAssetTags GameplayEffect의 AssetTag 목록
//! \return 발견한 피드백 태그, 없으면 무효 태그
FGameplayTag UMyAbilitySystemLibrary::FindAttackerHitCameraFeedbackTag(const FGameplayTagContainer& EffectAssetTags)
{
	if (EffectAssetTags.HasTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Ultimate))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Ultimate;
	}

	if (EffectAssetTags.HasTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Skill))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Skill;
	}

	if (EffectAssetTags.HasTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Basic))
	{
		return MyGameplayTags::CameraFeedback_AttackerHit_Basic;
	}

	return FGameplayTag();
}

////////////////////////////
//! \author HanUl
//! \brief 쿨다운 태그를 부여한 활성 GameplayEffect의 남은 시간을 반환한다.
//! \param ASC 조회할 AbilitySystemComponent
//! \param CooldownTag 쿨다운 GameplayTag
//! \return 남은 쿨다운(초). 쿨다운이 없거나 끝났으면 0
float UMyAbilitySystemLibrary::GetCooldownRemainingByTag(const UAbilitySystemComponent* ASC, FGameplayTag CooldownTag)
{
	if (!IsValid(ASC) || !CooldownTag.IsValid())
	{
		return 0.0f;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
	const TArray<float> RemainingTimes = ASC->GetActiveEffectsTimeRemaining(Query);

	float MaxRemaining = 0.0f;
	for (const float RemainingTime : RemainingTimes)
	{
		MaxRemaining = FMath::Max(MaxRemaining, RemainingTime);
	}

	return MaxRemaining;
}

////////////////////////////
//! \author HanUl
//! \brief 쿨다운 태그를 부여한 활성 쿨다운 GameplayEffect의 남은 시간을 지정 초만큼 단축한다.
//! \param ASC 대상 AbilitySystemComponent
//! \param CooldownTag 쿨다운 GameplayTag
//! \param ReduceSeconds 단축할 시간(초, 0 이하 무시)
//! \return 단축 후 남은 쿨다운(초). 쿨다운이 없었거나 전부 소진되면 0
float UMyAbilitySystemLibrary::ReduceCooldownByTag(UAbilitySystemComponent* ASC, FGameplayTag CooldownTag, float ReduceSeconds)
{
	if (!IsValid(ASC) || !CooldownTag.IsValid() || ReduceSeconds <= 0.0f)
	{
		return GetCooldownRemainingByTag(ASC, CooldownTag);
	}

	const float Remaining = GetCooldownRemainingByTag(ASC, CooldownTag);
	if (Remaining <= 0.0f)
	{
		return 0.0f;
	}

	// 기존 쿨다운 GE를 제거하고 남은 시간에서 단축분을 뺀 값으로 재적용한다.
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag));

	const float NewRemaining = Remaining - ReduceSeconds;
	if (NewRemaining <= 0.0f)
	{
		return 0.0f;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UMyCooldownGameplayEffect::StaticClass(), 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, NewRemaining);
		SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return NewRemaining;
}

////////////////////////////
//! \author HanUl
//! \brief 부여 태그로 활성 GameplayEffect를 찾아 스택 수 합을 반환한다.
//! \param ASC 조회할 AbilitySystemComponent
//! \param GrantedTag 스택 GE가 부여하는 GameplayTag
//! \return 활성 스택 수 합, 없으면 0
int32 UMyAbilitySystemLibrary::GetStackCountByGrantedTag(const UAbilitySystemComponent* ASC, FGameplayTag GrantedTag)
{
	if (!IsValid(ASC) || !GrantedTag.IsValid())
	{
		return 0;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(GrantedTag));
	const TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(Query);

	int32 TotalStacks = 0;
	for (const FActiveGameplayEffectHandle& ActiveHandle : ActiveHandles)
	{
		TotalStacks += FMath::Max(ASC->GetCurrentStackCount(ActiveHandle), 0);
	}

	return TotalStacks;
}

////////////////////////////
//! \author HanUl
//! \brief 부여 태그로 활성 스택 GameplayEffect를 전량 소비(제거)하고 소비한 스택 수를 반환한다.
//! \param ASC 대상 AbilitySystemComponent
//! \param GrantedTag 스택 GE가 부여하는 GameplayTag
//! \return 소비한 스택 수, 없었으면 0
int32 UMyAbilitySystemLibrary::ConsumeStacksByGrantedTag(UAbilitySystemComponent* ASC, FGameplayTag GrantedTag)
{
	if (!IsValid(ASC) || !GrantedTag.IsValid())
	{
		return 0;
	}

	const int32 ConsumedStacks = GetStackCountByGrantedTag(ASC, GrantedTag);
	if (ConsumedStacks > 0)
	{
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(GrantedTag));
	}

	return ConsumedStacks;
}

namespace
{
	//! \brief 두 ASC의 Faction 태그를 읽어 진영 플래그를 채운다. Faction.Enemy.Boss는 Faction.Enemy 계층에 포함된다.
	void ReadFactionFlags(const UAbilitySystemComponent* ASC, bool& bOutPlayer, bool& bOutEnemy)
	{
		bOutPlayer = ASC && ASC->HasMatchingGameplayTag(MyGameplayTags::Faction_Player);
		bOutEnemy = ASC && ASC->HasMatchingGameplayTag(MyGameplayTags::Faction_Enemy);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Source 기준으로 Target이 적대 진영(Faction 태그)인지 판정한다.
//! \param SourceActor 판정 기준 Actor
//! \param TargetActor 판정 대상 Actor
//! \param bRequireAlive true면 Target의 Health가 0 이하일 때 false를 반환한다
//! \return 두 Actor의 Faction 태그가 서로 적대 관계이면 true
bool UMyAbilitySystemLibrary::IsHostile(const AActor* SourceActor, const AActor* TargetActor, bool bRequireAlive)
{
	if (!IsValid(SourceActor) || !IsValid(TargetActor) || SourceActor == TargetActor)
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActor(SourceActor);
	const UAbilitySystemComponent* TargetASC = GetAbilitySystemComponentFromActor(TargetActor);
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	bool bSourcePlayer = false;
	bool bSourceEnemy = false;
	bool bTargetPlayer = false;
	bool bTargetEnemy = false;
	ReadFactionFlags(SourceASC, bSourcePlayer, bSourceEnemy);
	ReadFactionFlags(TargetASC, bTargetPlayer, bTargetEnemy);

	const bool bTargetDestructible = TargetASC->HasMatchingGameplayTag(MyGameplayTags::Target_Destructible);
	const bool bTargetObjective = TargetASC->HasMatchingGameplayTag(MyGameplayTags::Faction_Objective);
	const bool bHostile = (bSourcePlayer && bTargetEnemy)
		|| (bSourceEnemy && bTargetPlayer)
		|| (bSourceEnemy && bTargetObjective)
		|| (bSourcePlayer && bTargetDestructible);
	if (!bHostile)
	{
		return false;
	}

	if (bRequireAlive && TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Source 기준으로 Target이 같은 진영(Faction 태그)인지 판정한다. 자기 자신도 아군으로 취급한다.
//! \param SourceActor 판정 기준 Actor
//! \param TargetActor 판정 대상 Actor
//! \param bRequireAlive true면 Target의 Health가 0 이하일 때 false를 반환한다
//! \return 두 Actor의 Faction 태그가 같은 진영이면 true
bool UMyAbilitySystemLibrary::IsFriendly(const AActor* SourceActor, const AActor* TargetActor, bool bRequireAlive)
{
	if (!IsValid(SourceActor) || !IsValid(TargetActor))
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActor(SourceActor);
	const UAbilitySystemComponent* TargetASC = GetAbilitySystemComponentFromActor(TargetActor);
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	bool bSourcePlayer = false;
	bool bSourceEnemy = false;
	bool bTargetPlayer = false;
	bool bTargetEnemy = false;
	ReadFactionFlags(SourceASC, bSourcePlayer, bSourceEnemy);
	ReadFactionFlags(TargetASC, bTargetPlayer, bTargetEnemy);

	const bool bFriendly = (bSourcePlayer && bTargetPlayer) || (bSourceEnemy && bTargetEnemy);
	if (!bFriendly)
	{
		return false;
	}

	if (bRequireAlive && TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		return false;
	}

	return true;
}

////////////////////////////
//! \brief Actor의 ASC Health 속성이 0보다 큰지 확인한다.
bool UMyAbilitySystemLibrary::IsLivingPawn(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		return false;
	}

	return ASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) > 0.0f;
}

////////////////////////////
//! \brief 월드의 모든 PlayerController가 소유한 살아있는 Pawn을 수집한다.
void UMyAbilitySystemLibrary::GetLivingPlayerPawns(const UObject* WorldContextObject, TArray<AActor*>& OutLivingPlayers)
{
	OutLivingPlayers.Reset();

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (PlayerPawn && IsLivingPawn(PlayerPawn))
		{
			OutLivingPlayers.Add(PlayerPawn);
		}
	}
}

////////////////////////////
//! \brief 살아있는 플레이어 Pawn 중 무작위로 하나를 반환한다.
AActor* UMyAbilitySystemLibrary::GetRandomLivingPlayer(const UObject* WorldContextObject)
{
	TArray<AActor*> LivingPlayers;
	GetLivingPlayerPawns(WorldContextObject, LivingPlayers);
	if (LivingPlayers.IsEmpty())
	{
		return nullptr;
	}

	return LivingPlayers[FMath::RandRange(0, LivingPlayers.Num() - 1)];
}

////////////////////////////
//! \brief Origin에서 수평 거리상 가장 가까운 살아있는 플레이어 Pawn을 반환한다.
AActor* UMyAbilitySystemLibrary::GetNearestLivingPlayer(const UObject* WorldContextObject, const FVector& Origin)
{
	TArray<AActor*> LivingPlayers;
	GetLivingPlayerPawns(WorldContextObject, LivingPlayers);

	AActor* NearestPlayer = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (AActor* LivingPlayer : LivingPlayers)
	{
		if (!LivingPlayer)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Origin, LivingPlayer->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestPlayer = LivingPlayer;
		}
	}

	return NearestPlayer;
}
