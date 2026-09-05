#include "CPP_BossAttributeSet.h"

#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossTargetingComponent.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GameplayEffectExtension.h"
#include "MyGameplayTags.h"
#include "MyPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"

UCPP_BossAttributeSet::UCPP_BossAttributeSet()
{
}

////////////////////////////
//! \author HanSeul
//! \brief Applies boss-specific meta attribute results, including the phase-two HP damage cut.
//! \editor 준혁 (데미지 확정 시 공격자 클라이언트로 데미지 숫자 전송 추가)
//! \param Data GameplayEffect execution result data.
//!
void UCPP_BossAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		ACPP_BossCharacter* BossOwner = nullptr;
		if (Data.Target.AbilityActorInfo.IsValid())
		{
			BossOwner = Cast<ACPP_BossCharacter>(Data.Target.AbilityActorInfo->AvatarActor.Get());
		}

		const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
		AActor* SourceActor = EffectContext.GetOriginalInstigator();
		if (!SourceActor)
		{
			SourceActor = EffectContext.GetEffectCauser();
		}

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
			return;
		}

		// Threat (aggro) is recorded here, at the damage execution point — the Health change delegate
		// cannot see the instigator because SetHealth below is a direct write (no GEModData).
		// Total damage including the shield-absorbed part, so aggro keeps building during the DPS check.
		if (BossOwner)
		{
			if (UCPP_BossTargetingComponent* TargetingComponent = BossOwner->GetBossTargetingComponent())
			{
				TargetingComponent->RecordThreatDamage(Data.EffectSpec.GetEffectContext(), DamageAmount);
			}
		}

		const float AbsorbedByShield = FMath::Min(ShieldBefore, DamageAmount);
		const float RemainingDamage = DamageAmount - AbsorbedByShield;

		if (AbsorbedByShield > 0.0f)
		{
			SetShield(FMath::Max(ShieldBefore - AbsorbedByShield, 0.0f));
		}

		if (RemainingDamage > 0.0f)
		{
			float NewHealth = FMath::Clamp(HealthBefore - RemainingDamage, 0.0f, GetMaxHealth());
			if (BossOwner)
			{
				const float Phase2CutHealth = GetMaxHealth() * BossOwner->GetPhase2HPThreshold();
				const bool bCanStartPhase2Cut = BossOwner->GetCurrentPhase() == EBossPhase::Phase1 && !BossOwner->IsPhaseTransitionPending();
				const bool bShouldMaintainPhase2Cut = BossOwner->IsPhaseTransitionPending() || BossOwner->GetCurrentPhase() == EBossPhase::Transition;

				if (bCanStartPhase2Cut && NewHealth <= Phase2CutHealth)
				{
					NewHealth = Phase2CutHealth;
					BossOwner->RequestPhase2ByHP();
				}
				else if (bShouldMaintainPhase2Cut && NewHealth < Phase2CutHealth)
				{
					NewHealth = Phase2CutHealth;
				}
				else if (BossOwner->GetCurrentPhase() == EBossPhase::Phase2)
				{
					// Reaching zero health in phase two triggers the clear encounter instead of dying.
					// While it runs, health is pinned above zero so only the shield DPS check decides the outcome.
					constexpr float ClearEncounterHealthFloor = 1.0f;
					if (BossOwner->IsClearEncounterActive() || BossOwner->IsClearEncounterPending())
					{
						NewHealth = FMath::Max(NewHealth, ClearEncounterHealthFloor);
					}
					else if (NewHealth <= 0.0f)
					{
						NewHealth = ClearEncounterHealthFloor;
						BossOwner->RequestClearEncounter();
					}
				}
			}

			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}

		//! \author 장효제
		//! \brief 체력 변화를 감지하여 Hit 또는 Kill Payload를 생성하고 이벤트 버스에 띄운다. 매니저는 이를 수신하여 처리한다.
		//! \note 문제는 독뎀 같은 도트 데미지의 경우에 계속해서 이벤트 버스에 띄우는 문제가 있음
		if (BossOwner && BossOwner->HasAuthority())
		{
			const float HealthAfter = GetHealth();
			const float MaxHealthValue = GetMaxHealth();

			FMyStreamingCombatPayload Payload;
			Payload.EventTag = HealthBefore > 0.0f && HealthAfter <= 0.0f
				? MyGameplayTags::Streaming_Event_Combat_Kill
				: MyGameplayTags::Streaming_Event_Combat_Hit;
			Payload.InstigatorTag = UMyStreamingCombatMessageLibrary::ResolveStreamingActorTag(SourceActor);
			Payload.TargetTag = UMyStreamingCombatMessageLibrary::ResolveStreamingActorTag(BossOwner);
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
				UMyStreamingCombatMessageLibrary::BroadcastCombatPayload(BossOwner, Payload);
			}

			//! \author 준혁
			//! \brief 공격자가 플레이어면 본인 클라이언트에만 데미지 숫자를 보낸다. (파티원이 넣은 데미지는 표시하지 않는 정책)
			//!        보스에 네임플레이트 위젯이 없으면 클라이언트 수신부에서 조용히 무시된다.
			if (SourceActor && SourceActor != BossOwner)
			{
				UAbilitySystemComponent* SourceASC = EffectContext.GetInstigatorAbilitySystemComponent();
				AController* SourceController = SourceASC && SourceASC->AbilityActorInfo.IsValid()
					? SourceASC->AbilityActorInfo->PlayerController.Get()
					: nullptr;

				if (AMyPlayerController* SourcePC = Cast<AMyPlayerController>(SourceController))
				{
					FGameplayTag CameraFeedbackTag;
					UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(BossOwner);
					const bool bTargetIsLivingEnemy = HealthBefore > 0.0f
						&& TargetASC
						&& TargetASC->HasMatchingGameplayTag(MyGameplayTags::Faction_Enemy);
					if (!bDamageOverTime && bTargetIsLivingEnemy)
					{
						CameraFeedbackTag = UMyAbilitySystemLibrary::FindAttackerHitCameraFeedbackTag(EffectAssetTags);
					}

					SourcePC->ClientShowDamageNumber(
						BossOwner,
						DamageAmount,
						bWasCriticalHit,
						bDamageOverTime,
						Payload.bIsKill,
						CameraFeedbackTag
					);
				}
			}
		}

		return;
	}

	Super::PostGameplayEffectExecute(Data);
}
