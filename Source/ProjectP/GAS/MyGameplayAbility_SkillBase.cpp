////////////////////////////
//! \file MyGameplayAbility_SkillBase.cpp
//! \brief MyGAS 액티브 스킬 GameplayAbility 기반 클래스 구현 파일이다.

#include "MyGameplayAbility_SkillBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "../MyGameplayTags.h"
#include "MyAttributeSet.h"
#include "MyCooldownGameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SkillData/MySkillDefinitionDataAsset.h"
#include "Streaming/MyStreamingCombatMessageLibrary.h"
#include "TimerManager.h"

// 일반적으로 Skill Montage에 들어갈 기본 Notify들. 추후 디테일한 Section과 Notify가 필요하면 추가
const FName UMyGameplayAbility_SkillBase::StandardCastingSectionName = TEXT("Casting");
const FName UMyGameplayAbility_SkillBase::StandardExecuteSectionName = TEXT("Excute");
const FName UMyGameplayAbility_SkillBase::StandardShootNotifyName = TEXT("Shoot");
const FName UMyGameplayAbility_SkillBase::StandardEndAttackNotifyName = TEXT("EndAttack");

namespace
{
	constexpr float MinStandardMontagePlayRate = 0.01f;
}

////////////////////////////
//! \author HanUl
//! \brief 액티브 스킬 Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGameplayAbility_SkillBase::UMyGameplayAbility_SkillBase()
{
	ActivationBlockedTags.AddTag(MyGameplayTags::State_Skill_BlockSkillInput);
	ActivationBlockedTags.AddTag(MyGameplayTags::State_Player_Dead);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 표준 스킬 파이프라인 상태와 이동 입력 차단을 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGameplayAbility_SkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ClearSkillInputBlock(ActorInfo);
	ClearMoveInputBlock(ActorInfo);
	UnbindStandardMontageNotify();
	ResetStandardSkillState();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 그룹 태그를 반환한다.
//! \param 없음
//! \return 스킬 그룹 GameplayTag
FGameplayTag UMyGameplayAbility_SkillBase::GetSkillGroupTag() const
{
	return SkillGroupTag;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 카테고리 태그를 반환한다.
//! \param 없음
//! \return 스킬 카테고리 GameplayTag
FGameplayTag UMyGameplayAbility_SkillBase::GetSkillCategoryTag() const
{
	return SkillCategoryTag;
}

////////////////////////////
//! \author HanUl
//! \brief SourceObject에서 단일 SkillDefinition을 조회한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 조회된 SkillDefinition DataAsset, 없으면 nullptr
////////////////////////////
//! \author 장효제
//! \brief 스트리밍이 이 스킬을 가리킬 태그를 반환한다.
//! \details GA_ 에셋은 AbilityTag를 비워 두고 실제 태그는 SkillDefinition이 들고 있다.
//!          데이터가 있으면 그쪽을, 없으면 어빌리티 자신의 태그를 쓴다.
//! \return 스킬 태그다. 어느 쪽에도 없으면 무효 태그다.
FGameplayTag UMyGameplayAbility_SkillBase::GetStreamingSkillTag() const
{
	if (const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition())
	{
		if (const FGameplayTag DefinitionTag = SkillDefinition->GetAbilityTag();
			DefinitionTag.IsValid())
		{
			return DefinitionTag;
		}
	}

	return Super::GetStreamingSkillTag();
}

const UMySkillDefinitionDataAsset* UMyGameplayAbility_SkillBase::GetSkillDefinitionDataAssetFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo) const
{
	(void)ActorInfo;
	return Cast<UMySkillDefinitionDataAsset>(GetCurrentSourceObject());
}

////////////////////////////
//! \author HanUl
//! \brief 현재 표준 스킬 파이프라인에서 사용 중인 SkillDefinition을 반환한다.
//! \param 없음
//! \return 활성 SkillDefinition, 없으면 nullptr
const UMySkillDefinitionDataAsset* UMyGameplayAbility_SkillBase::GetActiveSkillDefinition() const
{
	return ActiveSkillDefinition ? ActiveSkillDefinition : Cast<UMySkillDefinitionDataAsset>(GetCurrentSourceObject());
}

////////////////////////////
//! \author HanUl
//! \brief 현재 Ability의 SourceObject에 연결된 SkillDefinition 데이터를 조회한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 일치하는 스킬 데이터, 없으면 nullptr
const FMySkillDataEntry* UMyGameplayAbility_SkillBase::FindSkillDataEntryFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (const UMySkillDefinitionDataAsset* SkillDefinition = GetSkillDefinitionDataAssetFromActorInfo(ActorInfo))
	{
		if (SkillDefinition->BuildSkillDataEntry(CachedSkillDataEntry))
		{
			return &CachedSkillDataEntry;
		}

		UE_LOG(LogTemp, Warning, TEXT("MyGAS SkillDefinition data build failed - Ability: %s, SkillDefinition: %s"),
			*GetNameSafe(this),
			*GetNameSafe(SkillDefinition));
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("MyGAS SkillDefinition source object missing - Ability: %s, SourceObject: %s, InputTag: %s, AbilityTag: %s"),
		*GetNameSafe(this),
		*GetNameSafe(GetCurrentSourceObject()),
		*InputTag.ToString(),
		*AbilityTag.ToString());
	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 콤보가 아닌 일반 스킬의 표준 Section/Notify 기반 실행 파이프라인을 시작한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 표준 파이프라인 시작에 성공하면 true
bool UMyGameplayAbility_SkillBase::ActivateStandardSkill(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ResetStandardSkillState();

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	ActiveSkillDefinition = GetSkillDefinitionDataAssetFromActorInfo(ActorInfo);
	if (!ActiveSkillDefinition || !ActiveSkillDefinition->BuildSkillDataEntry(CachedSkillDataEntry))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS standard skill activation failed - SkillDefinition missing or invalid. Ability: %s, Avatar: %s, SourceObject: %s"),
			*GetNameSafe(this),
			*GetNameSafe(ActorInfo->AvatarActor.Get()),
			*GetNameSafe(GetCurrentSourceObject()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	if (TriggerEventData)
	{
		ActiveStandardTriggerEventData = *TriggerEventData;
		bHasActiveStandardTriggerEventData = true;
	}

	OnStandardSkillDataReady(CachedSkillDataEntry);
	ApplyMoveInputBlockFromSkillData(CachedSkillDataEntry, ActorInfo);

	if (!CheckSkillDefinitionCooldown(ActiveSkillDefinition, ActorInfo))
	{
		UE_LOG(LogTemp, Log, TEXT("MyGAS standard skill blocked by cooldown - Ability: %s, Avatar: %s, Definition: %s, CooldownTag: %s"),
			*GetNameSafe(this),
			*GetNameSafe(ActorInfo->AvatarActor.Get()),
			*GetNameSafe(ActiveSkillDefinition),
			*ActiveSkillDefinition->GetCooldownTag().ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	if (ShouldBlockStandardSkillWhileFalling())
	{
		const ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
		const UCharacterMovementComponent* MovementComponent = AvatarCharacter ? AvatarCharacter->GetCharacterMovement() : nullptr;
		if (MovementComponent && MovementComponent->IsFalling())
		{
			UE_LOG(LogTemp, Log, TEXT("MyGAS standard skill blocked while jumping/falling - Ability: %s, Avatar: %s"),
				*GetNameSafe(this),
				*GetNameSafe(ActorInfo->AvatarActor.Get()));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return false;
		}
	}

	if (!CanActivateStandardSkill(ActorInfo, TriggerEventData, CachedSkillDataEntry))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	const FMySkillAnimationSpec& AnimationSpec = ActiveSkillDefinition->GetAnimation();
	if (AnimationSpec.Montage && !ValidateStandardMontageSpec(AnimationSpec))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return false;
	}

	ApplySkillInputBlock(ActorInfo);

	if (ActorInfo->IsNetAuthority())
	{
		//! \author 장효제
		//! \brief 서버 권한에서 스킬 사용 Payload를 Combat 채널에 발송한다.
		FGameplayTag StreamingSkillTag = CachedSkillDataEntry.AbilityTag;
		if (!StreamingSkillTag.IsValid() && ActiveSkillDefinition)
		{
			StreamingSkillTag = ActiveSkillDefinition->GetAbilityTag();
		}
		if (!StreamingSkillTag.IsValid())
		{
			StreamingSkillTag = AbilityTag;
		}

		UMyStreamingCombatMessageLibrary::BroadcastSkillUsed(ActorInfo->AvatarActor.Get(), ActorInfo->AvatarActor.Get(), StreamingSkillTag);
		ApplySkillDefinitionCooldown(ActiveSkillDefinition, ActorInfo);
	}

	OnStandardSkillCommitted(ActorInfo, TriggerEventData, CachedSkillDataEntry);

	if (AnimationSpec.Montage)
	{
		if (!PlayStandardSkillMontage(AnimationSpec))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return false;
		}

		return true;
	}

	if (ActorInfo->IsNetAuthority())
	{
		const float CastTime = ActiveSkillDefinition->GetTiming().CastTime;
		if (CastTime > 0.0f)
		{
			UAbilityTask_WaitDelay* CastDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, CastTime);
			if (CastDelayTask)
			{
				CastDelayTask->OnFinish.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardCastTimeFinished);
				CastDelayTask->ReadyForActivation();
				return true;
			}
		}

		OnStandardCastTimeFinished();
		return true;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 데이터가 준비된 직후 파생 Ability가 데이터를 캐시할 수 있도록 호출된다.
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardSkillDataReady(const FMySkillDataEntry& SkillData)
{
	(void)SkillData;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 공통 검증 후 파생 Ability의 추가 발동 조건을 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 추가 조건을 만족하면 true
bool UMyGameplayAbility_SkillBase::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Commit 성공 이후 표준 Montage 또는 CastTime 대기 전에 파생 Ability가 상태를 시작할 수 있도록 호출된다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardSkillCommitted(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot Notify 또는 fallback 시점에 파생 Ability의 실제 스킬 효과를 실행한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 EndAttack Notify 또는 fallback 시점의 기본 종료 처리를 수행한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardSkillEndAttack(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage가 중단되었을 때 파생 Ability가 추가 정리를 할 수 있도록 호출된다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardSkillMontageInterrupted()
{
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬을 공중 상태에서 차단할지 반환한다.
//! \param 없음
//! \return 공중 시 발동을 차단하면 true
bool UMyGameplayAbility_SkillBase::ShouldBlockStandardSkillWhileFalling() const
{
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition의 쿨다운 태그가 현재 적용되어 있는지 확인한다.
//! \param SkillDefinition 현재 Ability에 연결된 SkillDefinition
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 쿨다운 중이 아니거나 쿨다운 설정이 없으면 true
bool UMyGameplayAbility_SkillBase::CheckSkillDefinitionCooldown(const UMySkillDefinitionDataAsset* SkillDefinition, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!SkillDefinition || !SkillDefinition->GetCooldownTag().IsValid() || SkillDefinition->GetCooldownDuration() <= 0.0f)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	return !ASC || !ASC->HasMatchingGameplayTag(SkillDefinition->GetCooldownTag());
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition의 쿨다운을 공용 쿨다운 GE로 적용한다.
//!        Duration은 SkillDefinition의 CooldownDuration x (1 - CooldownReduction)이며,
//!        쿨다운 태그는 Spec의 DynamicGrantedTags로 주입되어 GE 지속시간 동안 유지된다.
//! \param SkillDefinition 현재 Ability에 연결된 SkillDefinition
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGameplayAbility_SkillBase::ApplySkillDefinitionCooldown(const UMySkillDefinitionDataAsset* SkillDefinition, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!SkillDefinition || !SkillDefinition->GetCooldownTag().IsValid() || SkillDefinition->GetCooldownDuration() <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	const float CooldownReduction = FMath::Clamp(
		ASC->GetNumericAttribute(UMyAttributeSet::GetCooldownReductionAttribute()), 0.0f, 0.8f);
	const float FinalCooldownDuration = FMath::Max(
		SkillDefinition->GetCooldownDuration() * (1.0f - CooldownReduction), 0.01f);

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UMyCooldownGameplayEffect::StaticClass(), 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, FinalCooldownDuration);
	SpecHandle.Data->DynamicGrantedTags.AddTag(SkillDefinition->GetCooldownTag());
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

////////////////////////////
//! \author HanUl
//! \brief SkillDefinition 설정에 따라 스킬 수행 중 이동 입력 차단 상태 태그를 적용한다.
//! \param SkillData 현재 Ability에 대응하는 스킬 데이터
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGameplayAbility_SkillBase::ApplyMoveInputBlockFromSkillData(const FMySkillDataEntry& SkillData, const FGameplayAbilityActorInfo* ActorInfo)
{
	if (bMoveInputBlockApplied || !SkillData.Movement.bBlockMoveInputDuringAbility)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS move input block failed - ASC is null. Ability: %s, SkillId: %s, InputTag: %s"),
			*GetNameSafe(this),
			*SkillData.SkillId.ToString(),
			*SkillData.InputTag.ToString());
		return;
	}

	ASC->AddLooseGameplayTag(MyGameplayTags::State_Skill_BlockMoveInput, 1, EGameplayTagReplicationState::TagOnly);
	bMoveInputBlockApplied = true;

	UE_LOG(LogTemp, Log, TEXT("MyGAS move input block applied - Ability: %s, SkillId: %s, InputTag: %s, ASC: %s"),
		*GetNameSafe(this),
		*SkillData.SkillId.ToString(),
		*SkillData.InputTag.ToString(),
		*GetNameSafe(ASC));
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 이동 입력 차단 상태 태그를 해제한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGameplayAbility_SkillBase::ClearMoveInputBlock(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bMoveInputBlockApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(MyGameplayTags::State_Skill_BlockMoveInput, 1, EGameplayTagReplicationState::TagOnly);
		UE_LOG(LogTemp, Log, TEXT("MyGAS move input block cleared - Ability: %s, ASC: %s"),
			*GetNameSafe(this),
			*GetNameSafe(ASC));
	}

	bMoveInputBlockApplied = false;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 Ability가 이동 입력 차단 태그를 적용했는지 반환한다.
//! \param 없음
//! \return 이동 입력 차단 태그를 적용했으면 true
bool UMyGameplayAbility_SkillBase::IsMoveInputBlockApplied() const
{
	return bMoveInputBlockApplied;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 수행 중 다른 비활성 스킬 입력을 차단하는 상태 태그를 적용한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGameplayAbility_SkillBase::ApplySkillInputBlock(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (bSkillInputBlockApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS skill input block failed - ASC is null. Ability: %s"),
			*GetNameSafe(this));
		return;
	}

	ASC->AddLooseGameplayTag(MyGameplayTags::State_Skill_BlockSkillInput, 1, EGameplayTagReplicationState::TagOnly);
	bSkillInputBlockApplied = true;

	UE_LOG(LogTemp, Log, TEXT("MyGAS skill input block applied - Ability: %s, ASC: %s"),
		*GetNameSafe(this),
		*GetNameSafe(ASC));
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 차단 상태 태그를 해제한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGameplayAbility_SkillBase::ClearSkillInputBlock(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bSkillInputBlockApplied)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(MyGameplayTags::State_Skill_BlockSkillInput, 1, EGameplayTagReplicationState::TagOnly);
		UE_LOG(LogTemp, Log, TEXT("MyGAS skill input block cleared - Ability: %s, ASC: %s"),
			*GetNameSafe(this),
			*GetNameSafe(ASC));
	}

	bSkillInputBlockApplied = false;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 Ability가 스킬 입력 차단 태그를 적용했는지 반환한다.
//! \param 없음
//! \return 스킬 입력 차단 태그를 적용했으면 true
bool UMyGameplayAbility_SkillBase::IsSkillInputBlockApplied() const
{
	return bSkillInputBlockApplied;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인의 런타임 상태를 초기화한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::ResetStandardSkillState()
{
	UnbindStandardMontageNotify();
	ActiveSkillDefinition = nullptr;
	ActiveStandardTriggerEventData = FGameplayEventData();
	bHasActiveStandardTriggerEventData = false;
	CurrentBaseMontagePlayRate = 1.0f;
	CurrentCastingMontagePlayRate = 1.0f;
	bStandardSkillShootHandled = false;
	bStandardSkillUsingMontage = false;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Casting Section에서 Montage를 시작하고 fallback 타이머를 예약한다.
//! \param AnimationSpec SkillDefinition의 애니메이션 설정
//! \return Montage 재생 요청에 성공하면 true
bool UMyGameplayAbility_SkillBase::PlayStandardSkillMontage(const FMySkillAnimationSpec& AnimationSpec)
{
	if (!AnimationSpec.Montage)
	{
		return false;
	}

	BindStandardMontageNotify();

	CurrentBaseMontagePlayRate = FMath::Max(MinStandardMontagePlayRate, AnimationSpec.PlayRate);
	CurrentCastingMontagePlayRate = CurrentBaseMontagePlayRate * GetAttackSpeedMultiplier(GetCurrentActorInfo());
	bStandardSkillUsingMontage = true;

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			AnimationSpec.Montage,
			CurrentCastingMontagePlayRate,
			StandardCastingSectionName,
			AnimationSpec.bStopWhenAbilityEnds
		);
	if (!MontageTask)
	{
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageInterrupted);
	MontageTask->ReadyForActivation();
	StartStandardMontageFallbacks();
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Avatar Mesh의 AnimInstance에 표준 Montage Notify 수신 함수를 바인딩한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::BindStandardMontageNotify()
{
	UnbindStandardMontageNotify();

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const ACharacter* AvatarCharacter = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? Cast<ACharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	USkeletalMeshComponent* MeshComponent = AvatarCharacter ? AvatarCharacter->GetMesh() : nullptr;
	StandardBoundAnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (StandardBoundAnimInstance)
	{
		StandardBoundAnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageNotifyBegin);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage Notify 바인딩을 해제한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::UnbindStandardMontageNotify()
{
	if (StandardBoundAnimInstance)
	{
		StandardBoundAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardMontageNotifyBegin);
		StandardBoundAnimInstance = nullptr;
	}
}

////////////////////////////
//! \author HanUl
//! \brief Montage Notify 누락에 대비해 표준 Shoot/EndAttack fallback 타이머를 예약한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::StartStandardMontageFallbacks()
{
	float MontageStartTime = 0.0f;
	if (!TryGetStandardMontageSectionStartTime(StandardCastingSectionName, MontageStartTime))
	{
		return;
	}

	float ShootNotifyTime = 0.0f;
	const bool bHasShootNotify = TryGetStandardMontageNotifyTime(StandardShootNotifyName, ShootNotifyTime);
	if (bHasShootNotify)
	{
		const float RelativeShootNotifyTime = FMath::Max(0.0f, ShootNotifyTime - MontageStartTime);
		UAbilityTask_WaitDelay* ShootFallbackTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			GetScaledStandardMontageDelay(RelativeShootNotifyTime, RelativeShootNotifyTime)
		);
		if (ShootFallbackTask)
		{
			ShootFallbackTask->OnFinish.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardShootFallback);
			ShootFallbackTask->ReadyForActivation();
		}
	}

	float EndAttackNotifyTime = 0.0f;
	if (TryGetStandardMontageNotifyTime(StandardEndAttackNotifyName, EndAttackNotifyTime))
	{
		const float RelativeShootNotifyTime = bHasShootNotify ? FMath::Max(0.0f, ShootNotifyTime - MontageStartTime) : 0.0f;
		const float RelativeEndAttackNotifyTime = FMath::Max(0.0f, EndAttackNotifyTime - MontageStartTime);
		UAbilityTask_WaitDelay* EndAttackFallbackTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			GetScaledStandardMontageDelay(RelativeEndAttackNotifyTime, RelativeShootNotifyTime)
		);
		if (EndAttackFallbackTask)
		{
			EndAttackFallbackTask->OnFinish.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardEndAttackFallback);
			EndAttackFallbackTask->ReadyForActivation();
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 표준 CastTime이 끝났을 때 Shoot 처리를 실행한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardCastTimeFinished()
{
	HandleStandardSkillShoot();
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot fallback 타이머가 끝났을 때 Shoot 처리를 실행한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardShootFallback()
{
	HandleStandardSkillShoot();
}

////////////////////////////
//! \author HanUl
//! \brief 표준 EndAttack fallback 타이머가 끝났을 때 종료 처리를 실행한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardEndAttackFallback()
{
	HandleStandardSkillEndAttack();
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage가 자연 종료되면 Shoot 처리를 보장한 뒤 Ability를 종료한다.
//! \note 블렌드아웃 시간은 재생 속도와 무관한 고정 실시간이라, 재생 속도가 빨라질수록
//!       Fire 노티파이가 블렌드아웃 시작(OnBlendOut) 뒤로 밀려 유실될 수 있다.
//!       종료 전에 Shoot을 보장해 고속 재생에서도 발사가 누락되지 않게 한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardMontageCompleted()
{
	if (!bStandardSkillShootHandled)
	{
		HandleStandardSkillShoot();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage가 중단되면 파생 정리 후 Ability를 취소 종료한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardMontageInterrupted()
{
	OnStandardSkillMontageInterrupted();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot/EndAttack Montage Notify를 수신해 공통 처리를 실행한다.
//! \param NotifyName 발생한 Notify 이름
//! \param BranchingPointPayload Notify Payload
//! \return 없음
void UMyGameplayAbility_SkillBase::OnStandardMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	(void)BranchingPointPayload;

	if (NotifyName == StandardShootNotifyName)
	{
		HandleStandardSkillShoot();
		return;
	}

	if (NotifyName == StandardEndAttackNotifyName)
	{
		HandleStandardSkillEndAttack();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot 처리를 한 번만 실행하고 Montage 재생 속도를 기본값으로 복구한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::HandleStandardSkillShoot()
{
	if (bStandardSkillShootHandled)
	{
		return;
	}

	bStandardSkillShootHandled = true;
	ResetStandardMontagePlayRateToBase();

	const FGameplayEventData* TriggerEventData = bHasActiveStandardTriggerEventData ? &ActiveStandardTriggerEventData : nullptr;
	OnStandardSkillShoot(CurrentActorInfo, TriggerEventData, CachedSkillDataEntry);

	if (!bStandardSkillUsingMontage)
	{
		FinishStandardSkillAfterPostDelay();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 표준 EndAttack 처리를 실행한다. Shoot이 아직 처리되지 않았다면 먼저 실행한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::HandleStandardSkillEndAttack()
{
	if (!bStandardSkillShootHandled)
	{
		HandleStandardSkillShoot();
	}

	ClearSkillInputBlock(CurrentActorInfo);
	ClearMoveInputBlock(CurrentActorInfo);

	const FGameplayEventData* TriggerEventData = bHasActiveStandardTriggerEventData ? &ActiveStandardTriggerEventData : nullptr;
	OnStandardSkillEndAttack(CurrentActorInfo, TriggerEventData, CachedSkillDataEntry);
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬의 PostDelay가 끝난 뒤 EndAttack 처리를 실행한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::FinishStandardSkillAfterPostDelay()
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const float PostDelay = SkillDefinition ? SkillDefinition->GetTiming().PostDelay : 0.0f;
	if (PostDelay > 0.0f)
	{
		UAbilityTask_WaitDelay* PostDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, PostDelay);
		if (PostDelayTask)
		{
			PostDelayTask->OnFinish.AddDynamic(this, &UMyGameplayAbility_SkillBase::OnStandardEndAttackFallback);
			PostDelayTask->ReadyForActivation();
			return;
		}
	}

	HandleStandardSkillEndAttack();
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage 재생 속도를 Definition의 기본 PlayRate로 되돌린다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_SkillBase::ResetStandardMontagePlayRateToBase() const
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	UAnimMontage* Montage = SkillDefinition ? SkillDefinition->GetAnimation().Montage.Get() : nullptr;
	if (StandardBoundAnimInstance && Montage)
	{
		StandardBoundAnimInstance->Montage_SetPlayRate(Montage, FMath::Max(MinStandardMontagePlayRate, CurrentBaseMontagePlayRate));
	}
}

////////////////////////////
//! \author HanUl
//! \brief 현재 ASC의 AttackSpeed Attribute를 표준 Casting 재생 속도 배율로 반환한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 0보다 큰 공격 속도 배율
float UMyGameplayAbility_SkillBase::GetAttackSpeedMultiplier(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	const float AttackSpeed = ASC ? ASC->GetNumericAttribute(UMyAttributeSet::GetAttackSpeedAttribute()) : 1.0f;
	return FMath::Max(0.1f, AttackSpeed);
}

////////////////////////////
//! \author HanUl
//! \brief Casting 가속과 Shoot 이후 기본 속도를 반영해 표준 Montage Notify 대기 시간을 계산한다.
//! \param RelativeNotifyTime Casting 섹션 시작 기준 Notify 원본 시간
//! \param RelativeShootTime Casting 섹션 시작 기준 Shoot Notify 원본 시간
//! \return 실제 재생 속도 기준 대기 시간
float UMyGameplayAbility_SkillBase::GetScaledStandardMontageDelay(float RelativeNotifyTime, float RelativeShootTime) const
{
	const float SafeNotifyTime = FMath::Max(0.0f, RelativeNotifyTime);
	const float SafeShootTime = FMath::Max(0.0f, RelativeShootTime);
	const float SafeCastingPlayRate = FMath::Max(MinStandardMontagePlayRate, CurrentCastingMontagePlayRate);
	const float SafeBasePlayRate = FMath::Max(MinStandardMontagePlayRate, CurrentBaseMontagePlayRate);

	if (SafeNotifyTime <= SafeShootTime)
	{
		return SafeNotifyTime / SafeCastingPlayRate;
	}

	const float CastingDelay = SafeShootTime / SafeCastingPlayRate;
	const float PostShootDelay = (SafeNotifyTime - SafeShootTime) / SafeBasePlayRate;
	return CastingDelay + PostShootDelay;
}

////////////////////////////
//! \author HanUl
//! \brief 활성 표준 Montage에서 지정 섹션 시작 시간을 찾는다.
//! \param SectionName 찾을 Montage Section 이름
//! \param OutStartTime 찾은 섹션 시작 시간
//! \return 섹션을 찾으면 true
bool UMyGameplayAbility_SkillBase::TryGetStandardMontageSectionStartTime(FName SectionName, float& OutStartTime) const
{
	OutStartTime = 0.0f;

	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const UAnimMontage* Montage = SkillDefinition ? SkillDefinition->GetAnimation().Montage.Get() : nullptr;
	if (!Montage || SectionName.IsNone())
	{
		return false;
	}

	const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return false;
	}

	float IgnoredEndTime = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, OutStartTime, IgnoredEndTime);
	OutStartTime = FMath::Max(0.0f, OutStartTime);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 활성 표준 Montage에서 이름 기반 Notify 시간을 찾는다.
//! \param NotifyName 찾을 Notify 이름
//! \param OutNotifyTime 찾은 Notify 시간
//! \return Notify를 찾으면 true
bool UMyGameplayAbility_SkillBase::TryGetStandardMontageNotifyTime(FName NotifyName, float& OutNotifyTime) const
{
	OutNotifyTime = 0.0f;

	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const UAnimMontage* Montage = SkillDefinition ? SkillDefinition->GetAnimation().Montage.Get() : nullptr;
	if (!Montage || NotifyName.IsNone())
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.NotifyName == NotifyName)
		{
			OutNotifyTime = FMath::Max(0.0f, NotifyEvent.GetTriggerTime());
			return true;
		}

		if (NotifyEvent.Notify && NotifyEvent.Notify->GetNotifyName() == NotifyName.ToString())
		{
			OutNotifyTime = FMath::Max(0.0f, NotifyEvent.GetTriggerTime());
			return true;
		}
	}

	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Montage가 필요한 Section과 Notify를 모두 갖췄는지 검증한다.
//! \param AnimationSpec SkillDefinition의 애니메이션 설정
//! \return 표준 Montage 규격을 만족하면 true
bool UMyGameplayAbility_SkillBase::ValidateStandardMontageSpec(const FMySkillAnimationSpec& AnimationSpec) const
{
	if (!AnimationSpec.Montage)
	{
		return true;
	}

	if (AnimationSpec.Montage->GetSectionIndex(StandardCastingSectionName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS standard skill activation failed - montage missing Casting section. Ability: %s, Definition: %s, Montage: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(AnimationSpec.Montage));
		return false;
	}

	if (AnimationSpec.Montage->GetSectionIndex(StandardExecuteSectionName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS standard skill activation failed - montage missing Excute section. Ability: %s, Definition: %s, Montage: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(AnimationSpec.Montage));
		return false;
	}

	float IgnoredNotifyTime = 0.0f;
	if (!TryGetStandardMontageNotifyTime(StandardShootNotifyName, IgnoredNotifyTime))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS standard skill activation failed - montage missing Shoot notify. Ability: %s, Definition: %s, Montage: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(AnimationSpec.Montage));
		return false;
	}

	if (!TryGetStandardMontageNotifyTime(StandardEndAttackNotifyName, IgnoredNotifyTime))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS standard skill activation failed - montage missing EndAttack notify. Ability: %s, Definition: %s, Montage: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*GetNameSafe(AnimationSpec.Montage));
		return false;
	}

	return true;
}
