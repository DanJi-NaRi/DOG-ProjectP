////////////////////////////
//! \file MyGameplayAbility_AreaSkillBase.cpp
//! \brief SkillDefinition 기반 장판형 GameplayAbility 기반 클래스 구현 파일이다.

#include "MyGameplayAbility_AreaSkillBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "MyAbilitySystemLibrary.h"
#include "MyAttributeSet.h"
#include "MySkillDebugShape.h"
#include "NiagaraComponent.h"
#include "SkillData/MySkillDefinitionDataAsset.h"
#include "TimerManager.h"

namespace
{
	constexpr float AreaVisualReferenceRadius = 100.0f;
	constexpr float MinAreaTickInterval = 0.01f;
}

////////////////////////////
//! \author HanUl
//! \brief 장판형 스킬 Ability 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMyGameplayAbility_AreaSkillBase::UMyGameplayAbility_AreaSkillBase()
{
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인으로 장판형 스킬을 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ResetPendingAreaState();
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 대기 중인 장판 입력 상태만 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ResetPendingAreaState();

	if (ShouldDeferEndAbilityForAreaRuntime(bWasCancelled))
	{
		bWaitingForAreaRuntimeCompletion = true;
		return;
	}

	if (bWasCancelled)
	{
		RemoveAllAreaRuntimeInstances(true);
	}

	bWaitingForAreaRuntimeCompletion = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 장판형 스킬 공통 데이터와 목표 위치를 검증하고 파생 클래스 검증을 호출한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGameplayAbility_AreaSkillBase::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!AvatarActor || !SourceASC)
	{
		return false;
	}

	FMyAreaSkillRuntimeSpec AreaSpec;
	if (!BuildAreaRuntimeSpec(ActorInfo, TriggerEventData, SkillData, AreaSpec))
	{
		return false;
	}

	PendingAvatarActor = AvatarActor;
	PendingSourceASC = SourceASC;
	PendingAreaSpec = AreaSpec;
	bHasPendingAreaSpec = true;

	FMyAreaSkillRuntimeContext Context;
	if (!BuildPendingAreaContext(Context))
	{
		return false;
	}

	return CanActivateAreaSkill(ActorInfo, Context);
}

////////////////////////////
//! \author HanUl
//! \brief Commit 직후 파생 장판 스킬의 시전 중 상태 처리를 호출한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::OnStandardSkillCommitted(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;
	(void)SkillData;

	FMyAreaSkillRuntimeContext Context;
	if (BuildPendingAreaContext(Context))
	{
		OnAreaSkillCommitted(ActorInfo, Context);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 표준 Shoot 시점에 서버 권한 장판 런타임을 시작한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;
	(void)SkillData;

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	FMyAreaSkillRuntimeContext Context;
	if (!BuildPendingAreaContext(Context))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (StartAreaRuntime(Context) == INDEX_NONE)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

////////////////////////////
//! \author HanUl
//! \brief EndAttack 이후에도 활성 장판 런타임이 남아 있으면 Ability 종료를 장판 종료까지 지연한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::OnStandardSkillEndAttack(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;
	(void)SkillData;

	if (ActiveAreaRuntimeInstances.Num() > 0)
	{
		bWaitingForAreaRuntimeCompletion = true;
		return;
	}

	Super::OnStandardSkillEndAttack(ActorInfo, TriggerEventData, SkillData);
}

////////////////////////////
//! \author HanUl
//! \brief 파생 장판 스킬의 추가 발동 조건을 확인한다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 발동 가능하면 true
bool UMyGameplayAbility_AreaSkillBase::CanActivateAreaSkill(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	(void)ActorInfo;
	(void)Context;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Commit 직후 파생 장판 스킬이 시전 중 상태를 시작할 수 있도록 호출된다.
//! \param ActorInfo Ability Actor 정보
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::OnAreaSkillCommitted(const FGameplayAbilityActorInfo* ActorInfo, const FMyAreaSkillRuntimeContext& Context)
{
	(void)ActorInfo;
	(void)Context;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 런타임 시작 직후 파생 스킬 또는 BP VFX 연동이 추가 처리를 할 수 있도록 호출된다.
//! \param Context 장판 런타임 컨텍스트
//! \param AreaVisualActor 생성된 VFX Actor
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::OnAreaRuntimeStarted(const FMyAreaSkillRuntimeContext& Context, AActor* AreaVisualActor)
{
	(void)Context;
	(void)AreaVisualActor;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 틱 타이머를 예약할지 반환한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 틱 효과가 필요하면 true
bool UMyGameplayAbility_AreaSkillBase::ShouldScheduleAreaTick(const FMyAreaSkillRuntimeContext& Context) const
{
	(void)Context;
	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 생성 즉시 적용할 효과를 파생 클래스에서 구현한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ApplyAreaInitialEffects(const FMyAreaSkillRuntimeContext& Context)
{
	(void)Context;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 틱마다 적용할 효과를 파생 클래스에서 구현한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ApplyAreaTickEffects(const FMyAreaSkillRuntimeContext& Context)
{
	(void)Context;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 종료 시 적용할 효과를 파생 클래스에서 구현한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ApplyAreaEndEffects(const FMyAreaSkillRuntimeContext& Context)
{
	(void)Context;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 대상 수집에서 시전자 AvatarActor를 제외할지 반환한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 시전자를 제외해야 하면 true
bool UMyGameplayAbility_AreaSkillBase::ShouldIgnoreAreaSourceActor(const FMyAreaSkillRuntimeContext& Context) const
{
	(void)Context;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 후보 Actor가 기본 피해 대상인지 확인한다. 기본 구현은 Faction 태그 기준 적대 대상만 허용한다.
//! \param Context 장판 런타임 컨텍스트
//! \param Candidate 후보 Actor
//! \return 기본 장판 대상이면 true
bool UMyGameplayAbility_AreaSkillBase::ShouldAreaAffectTarget(const FMyAreaSkillRuntimeContext& Context, AActor* Candidate) const
{
	if (!Candidate || Candidate == Context.AvatarActor)
	{
		return false;
	}

	return UMyAbilitySystemLibrary::IsHostile(Context.AvatarActor, Candidate);
}

////////////////////////////
//! \author HanUl
//! \brief 장판 대상 수집에 사용할 CollisionChannel을 반환한다.
//! \param 없음
//! \return Overlap 채널
ECollisionChannel UMyGameplayAbility_AreaSkillBase::GetAreaOverlapChannel() const
{
	return ECC_Pawn;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 반경 안의 유효한 대상을 수집한다.
//! \param Context 장판 런타임 컨텍스트
//! \param OutTargets 수집된 대상 목록
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::CollectValidAreaTargets(const FMyAreaSkillRuntimeContext& Context, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	UWorld* World = Context.AvatarActor ? Context.AvatarActor->GetWorld() : nullptr;
	if (!World || Context.AreaSpec.Radius <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MyAreaSkillOverlap), false);
	if (ShouldIgnoreAreaSourceActor(Context))
	{
		QueryParams.AddIgnoredActor(Context.AvatarActor);
	}

	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		Context.AreaSpec.TargetLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Context.AreaSpec.Radius),
		QueryParams
	);
	if (!bHasOverlap)
	{
		return;
	}

	TSet<AActor*> UniqueTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || UniqueTargets.Contains(TargetActor))
		{
			continue;
		}

		if (!ShouldAreaAffectTarget(Context, TargetActor))
		{
			continue;
		}

		UniqueTargets.Add(TargetActor);
		OutTargets.Add(TargetActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Actor에서 ASC를 조회한다.
//! \param TargetActor ASC를 조회할 Actor
//! \return ASC 포인터, 없으면 nullptr
UAbilitySystemComponent* UMyGameplayAbility_AreaSkillBase::GetTargetAbilitySystemComponent(AActor* TargetActor) const
{
	return UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
}

////////////////////////////
//! \author HanUl
//! \brief Source ASC의 AttackPower Attribute를 반환한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 공격력
float UMyGameplayAbility_AreaSkillBase::GetSourceAttackPower(const FMyAreaSkillRuntimeContext& Context) const
{
	return Context.SourceASC ? Context.SourceASC->GetNumericAttribute(UMyAttributeSet::GetAttackPowerAttribute()) : 0.0f;
}

////////////////////////////
//! \author HanUl
//! \brief 대상의 현재 Health를 반환한다.
//! \param TargetActor 대상 Actor
//! \return 현재 Health
float UMyGameplayAbility_AreaSkillBase::GetTargetHealth(AActor* TargetActor) const
{
	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	return TargetASC ? TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute()) : 0.0f;
}

////////////////////////////
//! \author HanUl
//! \brief 대상의 MaxHealth를 반환한다.
//! \param TargetActor 대상 Actor
//! \return MaxHealth
float UMyGameplayAbility_AreaSkillBase::GetTargetMaxHealth(AActor* TargetActor) const
{
	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	return TargetASC ? TargetASC->GetNumericAttribute(UMyAttributeSet::GetMaxHealthAttribute()) : 0.0f;
}

////////////////////////////
//! \author HanUl
//! \brief 에디터 퍼센트 입력값을 계산용 비율로 변환한다.
//! \param PercentValue 4.0을 4%로 해석하는 퍼센트 입력값
//! \return 0.04처럼 계산에 사용할 비율
float UMyGameplayAbility_AreaSkillBase::PercentValueToRatio(float PercentValue)
{
	return FMath::Max(PercentValue, 0.0f) * 0.01f;
}

////////////////////////////
//! \author HanUl
//! \brief 대상 ASC가 지정 태그 중 하나를 갖고 있는지 확인한다.
//! \param TargetActor 대상 Actor
//! \param Tags 확인할 GameplayTag 목록
//! \return 하나 이상 있으면 true
bool UMyGameplayAbility_AreaSkillBase::TargetHasAnyGameplayTag(AActor* TargetActor, const FGameplayTagContainer& Tags) const
{
	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	return TargetASC && TargetASC->HasAnyMatchingGameplayTags(Tags);
}

////////////////////////////
//! \author HanUl
//! \brief 대상에게 남은 지정 효과 시간을 Tick 개수로 환산한다.
//! \param TargetActor 대상 Actor
//! \param Tags 검색할 Effect 소유 태그
//! \param TickInterval Tick 간격
//! \return 남은 Tick 수
int32 UMyGameplayAbility_AreaSkillBase::GetRemainingEffectTickCountByTags(AActor* TargetActor, const FGameplayTagContainer& Tags, float TickInterval) const
{
	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	if (!TargetASC || Tags.IsEmpty())
	{
		return 0;
	}

	FGameplayEffectQuery EffectQuery;
	EffectQuery.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);

	float MaxRemainingTime = 0.0f;
	const TArray<TPair<float, float>> RemainingTimes = TargetASC->GetActiveEffectsTimeRemainingAndDuration(EffectQuery);
	for (const TPair<float, float>& RemainingTimeAndDuration : RemainingTimes)
	{
		MaxRemainingTime = FMath::Max(MaxRemainingTime, RemainingTimeAndDuration.Key);
	}

	const float SafeTickInterval = FMath::Max(TickInterval, MinAreaTickInterval);
	if (MaxRemainingTime <= 0.0f)
	{
		return TargetHasAnyGameplayTag(TargetActor, Tags) ? 1 : 0;
	}

	return FMath::Max(1, FMath::CeilToInt(MaxRemainingTime / SafeTickInterval));
}

////////////////////////////
//! \author HanUl
//! \brief 대상에게 Data.Coefficient SetByCaller GameplayEffect를 적용한다.
//!        공격력 곱셈은 ExecutionCalculation이 캡처한 Source AttackPower로 수행한다.
//! \param Context 장판 런타임 컨텍스트
//! \param TargetActor 대상 Actor
//! \param DamageGameplayEffectClass 피해 GameplayEffect class
//! \param DamageCoefficient 스킬 피해 계수(Source AttackPower에 곱해짐)
//! \param bOutKilled 적용 후 사망 여부를 받을 포인터
//! \return 적용 요청에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::ApplyDamageGameplayEffectToTarget(
	const FMyAreaSkillRuntimeContext& Context,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
	float DamageCoefficient,
	bool* bOutKilled
) const
{
	if (bOutKilled)
	{
		*bOutKilled = false;
	}

	if (!Context.SourceASC || !TargetActor || !DamageGameplayEffectClass || DamageCoefficient <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = Context.SourceASC->MakeOutgoingSpec(DamageGameplayEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !AssignSetByCallerCoefficient(SpecHandle, DamageCoefficient))
	{
		return false;
	}

	// 처치 스킬 식별용 꼬리표: 대상 사망 시 이 쿨다운 태그가 킬 이벤트 payload로 전달된다.
	if (Context.SkillData.CooldownTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(Context.SkillData.CooldownTag);
	}

	UMyAbilitySystemLibrary::AddAttackerHitCameraFeedbackTag(SpecHandle, Context.SkillData.InputTag);

	const float HealthBefore = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
	Context.SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	const float HealthAfter = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
	if (bOutKilled)
	{
		*bOutKilled = HealthBefore > 0.0f && HealthAfter <= 0.0f;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 대상에게 Data.Heal SetByCaller GameplayEffect를 적용한다.
//! \param Context 장판 런타임 컨텍스트
//! \param TargetActor 대상 Actor
//! \param HealGameplayEffectClass 회복 GameplayEffect class
//! \param Heal 적용할 회복량
//! \return 적용 요청에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::ApplyHealGameplayEffectToTarget(
	const FMyAreaSkillRuntimeContext& Context,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> HealGameplayEffectClass,
	float Heal
) const
{
	if (!Context.SourceASC || !TargetActor || !HealGameplayEffectClass || Heal <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = Context.SourceASC->MakeOutgoingSpec(HealGameplayEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !AssignSetByCallerHeal(SpecHandle, Heal))
	{
		return false;
	}

	Context.SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 대상에게 상태 GameplayEffect를 적용하고 Data.Damage SetByCaller 값을 함께 전달한다.
//! \param Context 장판 런타임 컨텍스트
//! \param TargetActor 대상 Actor
//! \param StatusGameplayEffectClass 상태 GameplayEffect class
//! \param DamageSetByCaller 상태 효과에 전달할 Data.Damage 값
//! \return 적용 요청에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::ApplyStatusGameplayEffectToTarget(
	const FMyAreaSkillRuntimeContext& Context,
	AActor* TargetActor,
	TSubclassOf<UGameplayEffect> StatusGameplayEffectClass,
	float DamageSetByCaller
) const
{
	if (!Context.SourceASC || !TargetActor || !StatusGameplayEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = GetTargetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = Context.SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = Context.SourceASC->MakeOutgoingSpec(StatusGameplayEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !AssignSetByCallerDamage(SpecHandle, FMath::Max(0.0f, DamageSetByCaller)))
	{
		return false;
	}

	Context.SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 아직 Shoot되지 않은 장판 입력 상태를 초기화한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ResetPendingAreaState()
{
	PendingAvatarActor = nullptr;
	PendingSourceASC = nullptr;
	PendingAreaSpec = FMyAreaSkillRuntimeSpec();
	bHasPendingAreaSpec = false;
}

////////////////////////////
//! \author HanUl
//! \brief Definition과 입력 TargetData에서 장판 런타임 Spec을 만든다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \param OutAreaSpec 생성된 장판 런타임 Spec
//! \return 생성에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::BuildAreaRuntimeSpec(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData,
	FMyAreaSkillRuntimeSpec& OutAreaSpec
) const
{
	OutAreaSpec = FMyAreaSkillRuntimeSpec();
	OutAreaSpec.AreaVisualClass = SkillData.Area.AreaClass;
	OutAreaSpec.Radius = FMath::Max(SkillData.Area.Radius, 0.0f);
	OutAreaSpec.ImpactDelay = FMath::Max(SkillData.Timing.ImpactDelay, 0.0f);
	OutAreaSpec.Duration = FMath::Max(SkillData.Timing.ActiveDuration, 0.0f);
	OutAreaSpec.TickInterval = FMath::Max(SkillData.Timing.TickInterval, 0.0f);

	if (!OutAreaSpec.AreaVisualClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS area skill activation failed - Area.AreaClass is invalid. Ability: %s, Definition: %s, SkillId: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*SkillData.SkillId.ToString());
		return false;
	}

	if (OutAreaSpec.Radius <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS area skill activation failed - Area.Radius must be positive. Ability: %s, Definition: %s, SkillId: %s, Radius: %.2f"),
			*GetNameSafe(this),
			*GetNameSafe(GetActiveSkillDefinition()),
			*SkillData.SkillId.ToString(),
			OutAreaSpec.Radius);
		return false;
	}

	if (!ResolveTargetLocation(TriggerEventData, OutAreaSpec.TargetLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS area skill activation failed - TargetData location missing. Ability: %s, Avatar: %s, Definition: %s, SkillId: %s"),
			*GetNameSafe(this),
			ActorInfo && ActorInfo->AvatarActor.IsValid() ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"),
			*GetNameSafe(GetActiveSkillDefinition()),
			*SkillData.SkillId.ToString());
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 GameplayEvent TargetData에서 장판 중심 위치를 얻는다.
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param OutTargetLocation 계산된 장판 중심 위치
//! \return 위치를 얻으면 true
bool UMyGameplayAbility_AreaSkillBase::ResolveTargetLocation(const FGameplayEventData* TriggerEventData, FVector& OutTargetLocation) const
{
	if (!TriggerEventData || TriggerEventData->TargetData.Num() <= 0)
	{
		return false;
	}

	const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(0);
	if (!TargetData)
	{
		return false;
	}

	if (TargetData->HasHitResult())
	{
		const FHitResult* HitResult = TargetData->GetHitResult();
		if (HitResult)
		{
			OutTargetLocation = HitResult->ImpactPoint.IsNearlyZero() ? HitResult->Location : HitResult->ImpactPoint;
			return true;
		}
	}

	if (TargetData->HasEndPoint())
	{
		OutTargetLocation = TargetData->GetEndPoint();
		return true;
	}

	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 대기 중인 장판 상태로 런타임 컨텍스트를 만든다.
//! \param OutContext 생성된 장판 런타임 컨텍스트
//! \return 생성에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::BuildPendingAreaContext(FMyAreaSkillRuntimeContext& OutContext) const
{
	if (!bHasPendingAreaSpec || !PendingAvatarActor || !PendingSourceASC)
	{
		return false;
	}

	OutContext = FMyAreaSkillRuntimeContext();
	OutContext.AreaInstanceId = INDEX_NONE;
	OutContext.AvatarActor = PendingAvatarActor.Get();
	OutContext.SourceASC = PendingSourceASC.Get();
	OutContext.SkillData = CachedSkillDataEntry;
	OutContext.AreaSpec = PendingAreaSpec;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 활성 장판 인스턴스로 런타임 컨텍스트를 만든다.
//! \param AreaInstanceId 조회할 장판 인스턴스 ID
//! \param OutContext 생성된 장판 런타임 컨텍스트
//! \return 생성에 성공하면 true
bool UMyGameplayAbility_AreaSkillBase::BuildActiveAreaContext(int32 AreaInstanceId, FMyAreaSkillRuntimeContext& OutContext) const
{
	const FMyActiveAreaRuntimeInstance* Instance = ActiveAreaRuntimeInstances.Find(AreaInstanceId);
	if (!Instance || !Instance->AvatarActor.IsValid() || !Instance->SourceASC.IsValid())
	{
		return false;
	}

	OutContext = FMyAreaSkillRuntimeContext();
	OutContext.AreaInstanceId = AreaInstanceId;
	OutContext.AvatarActor = Instance->AvatarActor.Get();
	OutContext.SourceASC = Instance->SourceASC.Get();
	OutContext.SkillData = Instance->SkillData;
	OutContext.AreaSpec = Instance->AreaSpec;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 장판 VFX Actor를 생성하고 즉시/틱/종료 타이머를 시작한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 생성된 장판 인스턴스 ID, 실패하면 INDEX_NONE
int32 UMyGameplayAbility_AreaSkillBase::StartAreaRuntime(const FMyAreaSkillRuntimeContext& Context)
{
	if (!Context.AvatarActor || !Context.SourceASC || Context.AreaSpec.Radius <= 0.0f)
	{
		return INDEX_NONE;
	}

	UWorld* World = Context.AvatarActor->GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}

	FMyAreaSkillRuntimeContext RuntimeContext = Context;
	RuntimeContext.AreaInstanceId = NextAreaInstanceId++;

	FMyActiveAreaRuntimeInstance Instance;
	Instance.AreaInstanceId = RuntimeContext.AreaInstanceId;
	Instance.AvatarActor = Context.AvatarActor;
	Instance.SourceASC = Context.SourceASC;
	Instance.SkillData = Context.SkillData;
	Instance.AreaSpec = Context.AreaSpec;
	Instance.AreaVisualActor = SpawnAreaVisualActor(RuntimeContext);

	ActiveAreaRuntimeInstances.Add(RuntimeContext.AreaInstanceId, Instance);

	OnAreaRuntimeStarted(RuntimeContext, Instance.AreaVisualActor.Get());
	DrawDebugAreaEffectRadius(RuntimeContext);

	if (RuntimeContext.AreaSpec.ImpactDelay > 0.0f)
	{
		FTimerDelegate StartDelegate;
		StartDelegate.BindUObject(this, &UMyGameplayAbility_AreaSkillBase::BeginAreaEffects, RuntimeContext.AreaInstanceId);
		World->GetTimerManager().SetTimer(
			ActiveAreaRuntimeInstances[RuntimeContext.AreaInstanceId].EffectStartTimerHandle,
			StartDelegate,
			RuntimeContext.AreaSpec.ImpactDelay,
			false
		);
	}
	else
	{
		BeginAreaEffects(RuntimeContext.AreaInstanceId);
	}

	UE_LOG(LogTemp, Log, TEXT("MyGAS area runtime started - Ability: %s, InstanceId: %d, Visual: %s, Location: %s, Radius: %.2f, ImpactDelay: %.2f, Duration: %.2f, TickInterval: %.2f"),
		*GetNameSafe(this),
		RuntimeContext.AreaInstanceId,
		*GetNameSafe(Instance.AreaVisualActor.Get()),
		*RuntimeContext.AreaSpec.TargetLocation.ToCompactString(),
		RuntimeContext.AreaSpec.Radius,
		RuntimeContext.AreaSpec.ImpactDelay,
		RuntimeContext.AreaSpec.Duration,
		RuntimeContext.AreaSpec.TickInterval);

	return RuntimeContext.AreaInstanceId;
}

////////////////////////////
//! \author HanUl
//! \brief Definition의 AreaClass를 VFX Actor로 생성한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 생성된 VFX Actor, 실패하면 nullptr
AActor* UMyGameplayAbility_AreaSkillBase::SpawnAreaVisualActor(const FMyAreaSkillRuntimeContext& Context) const
{
	if (!Context.AvatarActor || !Context.AreaSpec.AreaVisualClass)
	{
		return nullptr;
	}

	UWorld* World = Context.AvatarActor->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Context.AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(Context.AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* AreaVisualActor = World->SpawnActor<AActor>(
		Context.AreaSpec.AreaVisualClass,
		Context.AreaSpec.TargetLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	ConfigureAreaVisualActor(AreaVisualActor, Context.AreaSpec);
	return AreaVisualActor;
}

////////////////////////////
//! \author HanUl
//! \brief 생성된 VFX Actor에 Definition의 Radius/Duration 값을 반영한다.
//! \param AreaVisualActor 생성된 VFX Actor
//! \param AreaSpec 장판 런타임 Spec
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::ConfigureAreaVisualActor(AActor* AreaVisualActor, const FMyAreaSkillRuntimeSpec& AreaSpec) const
{
	if (!AreaVisualActor)
	{
		return;
	}

	const float Scale = AreaSpec.Radius > 0.0f ? AreaSpec.Radius / AreaVisualReferenceRadius : 1.0f;
	AreaVisualActor->SetActorScale3D(FVector(Scale));

	const float VisualLifeSpan = AreaSpec.Duration > 0.0f ? AreaSpec.ImpactDelay + AreaSpec.Duration : 0.0f;
	if (VisualLifeSpan > 0.0f)
	{
		AreaVisualActor->SetLifeSpan(VisualLifeSpan);
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	AreaVisualActor->GetComponents(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		NiagaraComponent->SetVariableFloat(TEXT("User.Radius"), AreaSpec.Radius);
		NiagaraComponent->SetVariableFloat(TEXT("Radius"), AreaSpec.Radius);
		NiagaraComponent->SetVariableFloat(TEXT("User.Diameter"), AreaSpec.Radius * 2.0f);
		NiagaraComponent->SetVariableFloat(TEXT("Diameter"), AreaSpec.Radius * 2.0f);
		NiagaraComponent->SetVariableFloat(TEXT("User.Duration"), VisualLifeSpan);
		NiagaraComponent->SetVariableFloat(TEXT("Duration"), VisualLifeSpan);
		NiagaraComponent->SetVariableFloat(TEXT("User.ImpactDelay"), AreaSpec.ImpactDelay);
		NiagaraComponent->SetVariableFloat(TEXT("ImpactDelay"), AreaSpec.ImpactDelay);

		if (NiagaraComponent->IsUsingAbsoluteScale())
		{
			NiagaraComponent->SetWorldScale3D(FVector(Scale));
		}

		NiagaraComponent->ReinitializeSystem();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 실제 효과 판정에 쓰는 장판 반경을 월드에 디버그 시각화한다.
//! \param Context 장판 런타임 컨텍스트
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::DrawDebugAreaEffectRadius(const FMyAreaSkillRuntimeContext& Context) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!bDrawDebugEffectRadius || !Context.AvatarActor || Context.AreaSpec.Radius <= 0.0f)
	{
		return;
	}

	const float LifeTime = DebugEffectRadiusLifeTime > 0.0f
		? DebugEffectRadiusLifeTime
		: FMath::Max(Context.AreaSpec.ImpactDelay + Context.AreaSpec.Duration, 2.0f);
	const FColor DebugColor = DebugEffectRadiusColor.ToFColor(true);
	const FVector Center = Context.AreaSpec.TargetLocation + FVector(0.0f, 0.0f, 8.0f);

	// 서버 판정 기준으로 스킬 소유 클라이언트 화면에만 표시한다
	MySkillDebugDraw::DrawShapeForOwner(Context.AvatarActor,
		FMySkillDebugShape::MakeSphere(Center, Context.AreaSpec.Radius, DebugColor, LifeTime, 1.0f));
	MySkillDebugDraw::DrawShapeForOwner(Context.AvatarActor,
		FMySkillDebugShape::MakeCircle(Center, Context.AreaSpec.Radius, DebugColor, LifeTime, 1.0f));
#endif
}

////////////////////////////
//! \author HanUl
//! \brief 연출용 지연이 끝난 뒤 실제 장판 효과와 Tick/End 타이머를 시작한다.
//! \param AreaInstanceId 실행할 장판 인스턴스 ID
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::BeginAreaEffects(int32 AreaInstanceId)
{
	FMyAreaSkillRuntimeContext RuntimeContext;
	if (!BuildActiveAreaContext(AreaInstanceId, RuntimeContext))
	{
		RemoveAreaRuntimeInstance(AreaInstanceId, true);
		return;
	}

	UWorld* World = RuntimeContext.AvatarActor ? RuntimeContext.AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		RemoveAreaRuntimeInstance(AreaInstanceId, true);
		return;
	}

	ApplyAreaInitialEffects(RuntimeContext);

	const bool bShouldTick = ShouldScheduleAreaTick(RuntimeContext)
		&& RuntimeContext.AreaSpec.Duration > 0.0f
		&& RuntimeContext.AreaSpec.TickInterval > 0.0f;
	if (bShouldTick)
	{
		FTimerDelegate TickDelegate;
		TickDelegate.BindUObject(this, &UMyGameplayAbility_AreaSkillBase::HandleAreaTick, AreaInstanceId);
		World->GetTimerManager().SetTimer(
			ActiveAreaRuntimeInstances[AreaInstanceId].TickTimerHandle,
			TickDelegate,
			FMath::Max(RuntimeContext.AreaSpec.TickInterval, MinAreaTickInterval),
			true
		);
	}

	if (RuntimeContext.AreaSpec.Duration > 0.0f)
	{
		FTimerDelegate EndDelegate;
		EndDelegate.BindUObject(this, &UMyGameplayAbility_AreaSkillBase::HandleAreaFinished, AreaInstanceId);
		World->GetTimerManager().SetTimer(
			ActiveAreaRuntimeInstances[AreaInstanceId].EndTimerHandle,
			EndDelegate,
			RuntimeContext.AreaSpec.Duration,
			false
		);
	}
	else if (!bShouldTick)
	{
		RemoveAreaRuntimeInstance(AreaInstanceId, false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 활성 장판 인스턴스의 틱 효과를 실행한다.
//! \param AreaInstanceId 실행할 장판 인스턴스 ID
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::HandleAreaTick(int32 AreaInstanceId)
{
	FMyAreaSkillRuntimeContext Context;
	if (!BuildActiveAreaContext(AreaInstanceId, Context))
	{
		RemoveAreaRuntimeInstance(AreaInstanceId, true);
		return;
	}

	ApplyAreaTickEffects(Context);
}

////////////////////////////
//! \author HanUl
//! \brief 활성 장판 인스턴스를 종료하고 종료 효과를 실행한다.
//! \param AreaInstanceId 종료할 장판 인스턴스 ID
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::HandleAreaFinished(int32 AreaInstanceId)
{
	FMyAreaSkillRuntimeContext Context;
	if (BuildActiveAreaContext(AreaInstanceId, Context))
	{
		ApplyAreaEndEffects(Context);
	}

	RemoveAreaRuntimeInstance(AreaInstanceId, true);
}

////////////////////////////
//! \author HanUl
//! \brief 장판 인스턴스 타이머와 선택적으로 VFX Actor를 정리한다.
//! \param AreaInstanceId 제거할 장판 인스턴스 ID
//! \param bDestroyVisualActor VFX Actor를 즉시 Destroy할지 여부
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::RemoveAreaRuntimeInstance(int32 AreaInstanceId, bool bDestroyVisualActor)
{
	FMyActiveAreaRuntimeInstance Instance;
	if (!ActiveAreaRuntimeInstances.RemoveAndCopyValue(AreaInstanceId, Instance))
	{
		return;
	}

	UWorld* World = Instance.AvatarActor.IsValid() ? Instance.AvatarActor->GetWorld() : nullptr;
	if (!World && Instance.AreaVisualActor.IsValid())
	{
		World = Instance.AreaVisualActor->GetWorld();
	}

	if (World)
	{
		World->GetTimerManager().ClearTimer(Instance.EffectStartTimerHandle);
		World->GetTimerManager().ClearTimer(Instance.TickTimerHandle);
		World->GetTimerManager().ClearTimer(Instance.EndTimerHandle);
	}

	if (bDestroyVisualActor && Instance.AreaVisualActor.IsValid())
	{
		Instance.AreaVisualActor->Destroy();
	}

	FinishAbilityIfAreaRuntimeComplete();
}

////////////////////////////
//! \author HanUl
//! \brief 모든 활성 장판 인스턴스의 타이머와 VFX Actor를 정리한다.
//! \param bDestroyVisualActor VFX Actor를 즉시 Destroy할지 여부
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::RemoveAllAreaRuntimeInstances(bool bDestroyVisualActor)
{
	TArray<int32> AreaInstanceIds;
	ActiveAreaRuntimeInstances.GetKeys(AreaInstanceIds);
	for (const int32 AreaInstanceId : AreaInstanceIds)
	{
		RemoveAreaRuntimeInstance(AreaInstanceId, bDestroyVisualActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 활성 장판 런타임 유지를 위해 Ability 종료를 지연해야 하는지 반환한다.
//! \param bWasCancelled Ability 취소 종료 여부
//! \return 장판 종료까지 Ability 종료를 지연해야 하면 true
bool UMyGameplayAbility_AreaSkillBase::ShouldDeferEndAbilityForAreaRuntime(bool bWasCancelled) const
{
	if (ActiveAreaRuntimeInstances.Num() <= 0)
	{
		return false;
	}

	return bWaitingForAreaRuntimeCompletion || !bWasCancelled;
}

////////////////////////////
//! \author HanUl
//! \brief EndAttack 이후 대기 중인 Area Ability를 모든 장판 런타임 종료 시점에 마무리한다.
//! \param 없음
//! \return 없음
void UMyGameplayAbility_AreaSkillBase::FinishAbilityIfAreaRuntimeComplete()
{
	if (!bWaitingForAreaRuntimeCompletion || ActiveAreaRuntimeInstances.Num() > 0)
	{
		return;
	}

	bWaitingForAreaRuntimeCompletion = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}
