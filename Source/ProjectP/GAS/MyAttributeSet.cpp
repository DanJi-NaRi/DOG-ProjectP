////////////////////////////
//! \page MyAttributeSet.cpp
//! \brief Re Duat 전투 AttributeSet의 기본값, 복제, Attribute 보정, GameplayEffect 적용 결과 처리를 구현한다.
#include "MyAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"
#include "MyPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"

UMyAttributeSet::UMyAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
	InitDamageTakenMultiplier(1.0f);
	InitMoveSpeed(600.0f);
	InitShield(0.0f);
	InitCritChance(0.0f);
	InitCritDamage(1.5f);
	InitAttackSpeed(1.0f);
	InitCooldownReduction(0.0f);
	InitIncomingDamage(0.0f);
	InitIncomingCurseGauge(0.0f);
	InitIncomingCriticalHit(0.0f);
	InitIncomingHeal(0.0f);
	InitIncomingShield(0.0f);

	InitMaxMoveCharge(2.0f);
	InitMoveCharge(2.0f);
	InitCurseGauge(0.0f);
}

////////////////////////////
//! \author 장효제
//! \brief 복제 대상 Attribute를 네트워크 복제 목록에 등록한다.
//! \param OutLifetimeProps 복제 속성 목록
void UMyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, DamageTakenMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, CritDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, CooldownReduction, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, MaxMoveCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, MoveCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMyAttributeSet, CurseGauge, COND_None, REPNOTIFY_Always);
}

////////////////////////////
//! \author 장효제
//! \brief Attribute 값이 변경되기 전에 유효 범위로 보정한다.
//! \editor 준혁 (치명타 확률 상한을 1.0 → CritChanceCap(50%)으로 변경, 기획 스펙)
//! \param Attribute 변경 대상 Attribute
//! \param NewValue 변경될 Attribute 값
void UMyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetDefenseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetDamageTakenMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, CritChanceCap);
	}
	else if (Attribute == GetCritDamageAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetAttackSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.1f);
	}
	else if (Attribute == GetCooldownReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 0.8f);
	}
	else if (Attribute == GetMaxMoveChargeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetMoveChargeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMoveCharge());
	}
	else if (Attribute == GetCurseGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, CurseGaugeMax);
	}
}

////////////////////////////
//! \author 장효제
//! \brief GameplayEffect 적용 이후 Attribute와 메타 Attribute 결과를 처리한다.
//! \editor 준혁 (치명타 확률 상한을 1.0 → CritChanceCap(50%)으로 변경, 기획 스펙)
//! \editor 준혁 (데미지 확정 시 공격자 클라이언트로 데미지 숫자 전송 추가)
//! \param Data GameplayEffect 실행 결과 데이터
void UMyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AActor* TargetActor = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	}

	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
	AActor* SourceActor = EffectContext.GetOriginalInstigator();
	if (!SourceActor)
	{
		SourceActor = EffectContext.GetEffectCauser();
	}

	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 1.0f));
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetAttackPowerAttribute())
	{
		SetAttackPower(FMath::Max(GetAttackPower(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetDefenseAttribute())
	{
		SetDefense(FMath::Max(GetDefense(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetDamageTakenMultiplierAttribute())
	{
		SetDamageTakenMultiplier(FMath::Max(GetDamageTakenMultiplier(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetMoveSpeedAttribute())
	{
		SetMoveSpeed(FMath::Max(GetMoveSpeed(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Max(GetShield(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetCritChanceAttribute())
	{
		SetCritChance(FMath::Clamp(GetCritChance(), 0.0f, CritChanceCap));
	}
	else if (Data.EvaluatedData.Attribute == GetCritDamageAttribute())
	{
		SetCritDamage(FMath::Max(GetCritDamage(), 1.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetAttackSpeedAttribute())
	{
		SetAttackSpeed(FMath::Max(GetAttackSpeed(), 0.1f));
	}
	else if (Data.EvaluatedData.Attribute == GetCooldownReductionAttribute())
	{
		SetCooldownReduction(FMath::Clamp(GetCooldownReduction(), 0.0f, 0.8f));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxMoveChargeAttribute())
	{
		SetMaxMoveCharge(FMath::Max(GetMaxMoveCharge(), 1.0f));
		SetMoveCharge(FMath::Clamp(GetMoveCharge(), 0.0f, GetMaxMoveCharge()));
	}
	else if (Data.EvaluatedData.Attribute == GetMoveChargeAttribute())
	{
		SetMoveCharge(FMath::Clamp(GetMoveCharge(), 0.0f, GetMaxMoveCharge()));
	}
	else if (Data.EvaluatedData.Attribute == GetCurseGaugeAttribute())
	{
		SetCurseGauge(FMath::Clamp(GetCurseGauge(), 0.0f, CurseGaugeMax));
	}
	//! \author 장효제
	//! \brief 치명타 메타 Attribute를 피해 처리 시점까지 보존한다.
	else if (Data.EvaluatedData.Attribute == GetIncomingCriticalHitAttribute())
	{
		SetIncomingCriticalHit(FMath::Max(GetIncomingCriticalHit(), 0.0f));
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingCurseGaugeAttribute())
	{
		const float CurseGaugeAmount = FMath::Max(GetIncomingCurseGauge(), 0.0f);
		SetIncomingCurseGauge(0.0f);

		if (CurseGaugeAmount > 0.0f)
		{
			SetCurseGauge(FMath::Clamp(GetCurseGauge() + CurseGaugeAmount, 0.0f, CurseGaugeMax));
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		FGameplayTagContainer EffectAssetTags;
		Data.EffectSpec.GetAllAssetTags(EffectAssetTags);
		const bool bDamageOverTime = EffectAssetTags.HasTag(MyGameplayTags::Damage_Type_Dot);

		const float DamageAmount = FMath::Max(GetIncomingDamage(), 0.0f);
		const bool bWasCriticalHit = GetIncomingCriticalHit() > 0.0f;
		const float ShieldBefore = GetShield();
		const float HealthBefore = GetHealth();
		SetIncomingDamage(0.0f);
		SetIncomingCriticalHit(0.0f);

		if (DamageAmount <= 0.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("MyGAS Damage Execute skipped - Target: %s, Source: %s, Incoming: %.2f, Shield: %.2f, Health: %.2f"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(SourceActor),
				DamageAmount,
				ShieldBefore,
				HealthBefore);
			return;
		}

		const float AbsorbedByShield = FMath::Min(ShieldBefore, DamageAmount);
		const float RemainingDamage = DamageAmount - AbsorbedByShield;

		if (AbsorbedByShield > 0.0f)
		{
			SetShield(FMath::Max(ShieldBefore - AbsorbedByShield, 0.0f));
		}

		if (RemainingDamage > 0.0f)
		{
			SetHealth(FMath::Clamp(HealthBefore - RemainingDamage, 0.0f, GetMaxHealth()));
		}

		UE_LOG(LogTemp, Log, TEXT("MyGAS Damage Execute - Target: %s, Source: %s, Incoming: %.2f, ShieldBefore: %.2f, HealthBefore: %.2f, ShieldAbsorbed: %.2f, HealthDamage: %.2f, ShieldAfter: %.2f, HealthAfter: %.2f"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(SourceActor),
			DamageAmount,
			ShieldBefore,
			HealthBefore,
			AbsorbedByShield,
			RemainingDamage,
			GetShield(),
			GetHealth());

		//! \author 장효제
		//! \brief 체력 변화를 감지하여 Hit 또는 Kill Payload를 생성하고 이벤트 버스에 발송한다.
		if (TargetActor && TargetActor->HasAuthority())
		{
			const float HealthAfter = GetHealth();
			const float MaxHealthValue = GetMaxHealth();

			FMyStreamingCombatPayload Payload;
			Payload.EventTag = HealthBefore > 0.0f && HealthAfter <= 0.0f
				? MyGameplayTags::Streaming_Event_Combat_Kill
				: MyGameplayTags::Streaming_Event_Combat_Hit;
			Payload.InstigatorTag = UMyStreamingCombatMessageLibrary::ResolveStreamingActorTag(SourceActor);
			Payload.TargetTag = UMyStreamingCombatMessageLibrary::ResolveStreamingActorTag(TargetActor);
			Payload.SkillTag = UMyStreamingCombatMessageLibrary::ResolveStreamingSkillTagFromEffectSpec(Data.EffectSpec);
			Payload.DamageAmount = DamageAmount;
			Payload.TargetCurrentHPRatio = MaxHealthValue > 0.0f
				? FMath::Clamp(HealthAfter / MaxHealthValue, 0.0f, 1.0f)
				: 0.0f;
			Payload.bIsCritical = bWasCriticalHit;
			Payload.bIsKill = Payload.EventTag.MatchesTagExact(MyGameplayTags::Streaming_Event_Combat_Kill);

			const bool bShouldBroadcastStreaming = !bDamageOverTime || Payload.bIsKill;
			if (bShouldBroadcastStreaming)
			{
				UMyStreamingCombatMessageLibrary::BroadcastCombatPayload(TargetActor, Payload);
			}

			//! \author HanUl
			//! \brief 처치 발생 시 킬러 ASC에 처치 이벤트를 보낸다. 처치 스킬의 쿨다운 태그(Spec 꼬리표)를
			//!        payload InstigatorTags로 전달해 "처치 시 쿨 초기화" 같은 기믹이 스킬을 식별할 수 있게 한다.
			if (Payload.bIsKill && SourceActor && SourceActor != TargetActor)
			{
				if (UAbilitySystemComponent* KillerASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(SourceActor))
				{
					FGameplayEventData KillEventData;
					KillEventData.EventTag = MyGameplayTags::Event_Skill_KillConfirmed;
					KillEventData.Instigator = SourceActor;
					KillEventData.Target = TargetActor;

					FGameplayTagContainer SpecAssetTags;
					Data.EffectSpec.GetAllAssetTags(SpecAssetTags);
					for (const FGameplayTag& AssetTag : SpecAssetTags)
					{
						if (AssetTag.ToString().StartsWith(TEXT("Cooldown.")))
						{
							KillEventData.InstigatorTags.AddTag(AssetTag);
						}
					}

					KillerASC->HandleGameplayEvent(KillEventData.EventTag, &KillEventData);
				}
			}

			//! \author 준혁
			//! \brief 공격자가 플레이어면 본인 클라이언트에만 데미지 숫자를 보낸다. (파티원이 넣은 데미지는 표시하지 않는 정책)
			//!        몬스터가 공격자인 경우 컨트롤러가 AIController라 캐스트에 실패해 자연히 전송되지 않는다.
			if (SourceActor && SourceActor != TargetActor && HealthBefore > 0.0f)
			{
				UAbilitySystemComponent* SourceASC = EffectContext.GetInstigatorAbilitySystemComponent();
				AController* SourceController = SourceASC && SourceASC->AbilityActorInfo.IsValid()
					? SourceASC->AbilityActorInfo->PlayerController.Get()
					: nullptr;

				if (AMyPlayerController* SourcePC = Cast<AMyPlayerController>(SourceController))
				{
					FGameplayTag CameraFeedbackTag;
					UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
					const bool bTargetIsLivingEnemy = HealthBefore > 0.0f
						&& TargetASC
						&& TargetASC->HasMatchingGameplayTag(MyGameplayTags::Faction_Enemy);
					if (!bDamageOverTime && bTargetIsLivingEnemy)
					{
						CameraFeedbackTag = UMyAbilitySystemLibrary::FindAttackerHitCameraFeedbackTag(EffectAssetTags);
					}

					SourcePC->ClientShowDamageNumber(
						TargetActor,
						DamageAmount,
						bWasCriticalHit,
						bDamageOverTime,
						Payload.bIsKill,
						CameraFeedbackTag
					);
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float HealAmount = FMath::Max(GetIncomingHeal(), 0.0f);
		const float HealthBefore = GetHealth();
		const float MaxHealthValue = GetMaxHealth();
		SetIncomingHeal(0.0f);

		if (HealAmount > 0.0f)
		{
			SetHealth(FMath::Clamp(HealthBefore + HealAmount, 0.0f, MaxHealthValue));
		}

		UE_LOG(LogTemp, Log, TEXT("MyGAS Heal Execute - Target: %s, Source: %s, Incoming: %.2f, HealthBefore: %.2f, MaxHealth: %.2f, HealthAfter: %.2f"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(SourceActor),
			HealAmount,
			HealthBefore,
			MaxHealthValue,
			GetHealth());
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingShieldAttribute())
	{
		const float ShieldAmount = FMath::Max(GetIncomingShield(), 0.0f);
		const float ShieldBefore = GetShield();
		SetIncomingShield(0.0f);

		if (ShieldAmount > ShieldBefore)
		{
			SetShield(ShieldAmount);
		}

		UE_LOG(LogTemp, Log, TEXT("MyGAS Shield Execute - Target: %s, Source: %s, Incoming: %.2f, ShieldBefore: %.2f, ShieldAfter: %.2f, Policy: ReplaceIfGreater"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(SourceActor),
			ShieldAmount,
			ShieldBefore,
			GetShield());
	}
}

////////////////////////////
//! \brief 서버에서 복제된 Health 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldHealth 복제 이전의 Health 값
void UMyAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, Health, OldHealth);
}

////////////////////////////
//! \brief 서버에서 복제된 최대Health 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param Old최대Health 복제 이전의 최대Health 값
void UMyAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, MaxHealth, OldMaxHealth);
}

////////////////////////////
//! \brief 서버에서 복제된 Health 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldHealth 복제 이전의 Health 값
void UMyAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, AttackPower, OldAttackPower);
}

////////////////////////////
//! \brief 서버에서 복제된 방어력 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param Old방어력 복제 이전의 방어력 값
void UMyAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, Defense, OldDefense);
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 복제된 받는 피해 배율을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldDamageTakenMultiplier 복제 이전의 받는 피해 배율
//! \return 없음
void UMyAttributeSet::OnRep_DamageTakenMultiplier(const FGameplayAttributeData& OldDamageTakenMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, DamageTakenMultiplier, OldDamageTakenMultiplier);
}

////////////////////////////
//! \brief 서버에서 복제된 이동속도 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param Old이속 복제 이전의 이속 값
void UMyAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, MoveSpeed, OldMoveSpeed);
}

////////////////////////////
//! \brief 서버에서 복제된 실드 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param Old실드 복제 이전의 실드 값
//! \note  이건 서버가 알아야 할 듯함; 기획에는 없는데, 서버는 알아야 함.
void UMyAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, Shield, OldShield);
}

////////////////////////////
//! \brief 서버에서 복제된 치명타확률 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param Old치명타확률 복제 이전의 치명타확률 값
void UMyAttributeSet::OnRep_CritChance(const FGameplayAttributeData& OldCritChance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, CritChance, OldCritChance);
}

////////////////////////////
//! \brief 서버에서 복제된 크리티컬 배율 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldCritDamage 복제 이전의 크리티컬 배율 값
void UMyAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, CritDamage, OldCritDamage);
}

////////////////////////////
//! \brief 서버에서 복제된 AttackSpeed 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldAttackSpeed 복제 이전의 AttackSpeed 값
void UMyAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, AttackSpeed, OldAttackSpeed);
}

////////////////////////////
//! \brief 서버에서 복제된 MaxMoveCharge 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldMaxMoveCharge 복제 이전의 MaxMoveCharge 값
void UMyAttributeSet::OnRep_MaxMoveCharge(const FGameplayAttributeData& OldMaxMoveCharge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, MaxMoveCharge, OldMaxMoveCharge);
}

void UMyAttributeSet::OnRep_MoveCharge(const FGameplayAttributeData& OldMoveCharge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, MoveCharge, OldMoveCharge);
}

void UMyAttributeSet::OnRep_CurseGauge(const FGameplayAttributeData& OldCurseGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, CurseGauge, OldCurseGauge);
}

////////////////////////////
//! \brief 서버에서 복제된 CooldownReduction 값을 AbilitySystemComponent의 Attribute 상태에 반영한다.
//! \param OldCooldownReduction 복제 이전의 쿨감(CooldownReduction) 값
void UMyAttributeSet::OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMyAttributeSet, CooldownReduction, OldCooldownReduction);
}
