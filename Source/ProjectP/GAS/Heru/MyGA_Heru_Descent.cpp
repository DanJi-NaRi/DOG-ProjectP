////////////////////////////
//! \file MyGA_Heru_Descent.cpp
//! \brief Heru 자기 강화 + 처치 시 쿨 초기화 스킬 GameplayAbility 구현 파일이다.
#include "MyGA_Heru_Descent.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "../MyAbilitySystemLibrary.h"
#include "../MySkillDebugShape.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../../MyGameplayTags.h"

////////////////////////////
//! \author HanUl
//! \brief Heru Descent 기본값을 초기화한다. 버프/리스너는 서버에서만 적용한다.
//! \param 없음
//! \return 없음
UMyGA_Heru_Descent::UMyGA_Heru_Descent()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인으로 Descent를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_Heru_Descent::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief Ability 종료 시 시전 슈퍼아머 태그 잔여분을 정리한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Heru_Descent::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	RemoveSuperArmorTag(ActorInfo);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

////////////////////////////
//! \author HanUl
//! \brief 버프 GE와 지속시간 데이터가 유효한지 검증한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGA_Heru_Descent::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;

	if (!SkillData.Effects.BuffGameplayEffect || SkillData.Timing.ActiveDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru descent activation failed - buff GE or duration missing. BuffGE: %s, ActiveDuration: %.2f"),
			*GetNameSafe(SkillData.Effects.BuffGameplayEffect),
			SkillData.Timing.ActiveDuration);
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 시전(발동 연출) 동안 슈퍼아머 태그를 부여한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Descent::OnStandardSkillCommitted(
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

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->AddLooseGameplayTag(MyGameplayTags::Status_Buff_SuperArmor, 1, EGameplayTagReplicationState::TagOnly);
		bSuperArmorApplied = true;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 시전이 끝나는 시점(Shoot)에 슈퍼아머를 해제하고 자기 자신에게 버프 GE를 적용한 뒤,
//!        자신(C)을 제외한 Heru 모든 스킬 쿨다운을 1회 초기화한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Descent::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	RemoveSuperArmorTag(ActorInfo);

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// 버프 GE 적용: 지속시간은 Definition의 ActiveDuration을 SetByCaller(Data.Duration)로 전달한다.
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle BuffSpecHandle = ASC->MakeOutgoingSpec(SkillData.Effects.BuffGameplayEffect, 1.0f, EffectContext);
	if (!BuffSpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru descent buff spec creation failed - BuffGE: %s"),
			*GetNameSafe(SkillData.Effects.BuffGameplayEffect));
		return;
	}

	BuffSpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Duration, SkillData.Timing.ActiveDuration);
	ASC->ApplyGameplayEffectSpecToSelf(*BuffSpecHandle.Data.Get());

	// 발동 시 1회, 자신(C = HRU_SK_C)을 제외한 Heru 모든 스킬 쿨다운을 초기화한다(쿨다운 GE 제거 = 즉시 초기화).
	// 자신을 제외해 무한 재사용을 막는다.
	FGameplayTagContainer CooldownTagsToReset;
	CooldownTagsToReset.AddTag(MyGameplayTags::Cooldown_Skill_HRU_BasicAttack);
	CooldownTagsToReset.AddTag(MyGameplayTags::Cooldown_Skill_HRU_SK_Q);
	CooldownTagsToReset.AddTag(MyGameplayTags::Cooldown_Skill_HRU_SK_E);
	CooldownTagsToReset.AddTag(MyGameplayTags::Cooldown_Skill_HRU_SK_R);
	CooldownTagsToReset.AddTag(MyGameplayTags::Cooldown_Skill_HRU_Move);
	ASC->RemoveActiveEffectsWithGrantedTags(CooldownTagsToReset);

	// 레벨2 추가 효과: Definition에 원형 타격 데이터가 채워져 있을 때만 시전 지점 원형 범위 1회 타격.
	ApplyDescentImpact(ActorInfo, SkillData);

	UE_LOG(LogTemp, Log, TEXT("Heru descent buff applied - Avatar: %s, Duration: %.1f, BuffGE: %s"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		SkillData.Timing.ActiveDuration,
		*GetNameSafe(SkillData.Effects.BuffGameplayEffect));
}

////////////////////////////
//! \author HanUl
//! \brief Definition의 피해 계수가 0보다 클 때만(레벨2) 시전 지점 원형 범위 적에게 1회 피해를 적용한다.
//!        레벨1 Definition은 DamageCoefficient를 0으로 두어 타격을 끈다. 서버 권한에서만 호출된다.
//! \param ActorInfo Ability Actor 정보
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Descent::ApplyDescentImpact(const FGameplayAbilityActorInfo* ActorInfo, const FMySkillDataEntry& SkillData) const
{
	// 레벨1: 피해 계수 0이면 원형 타격 없음(기존 동작 유지).
	if (SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		return;
	}

	// 계수는 넣었는데 판정 반경이나 데미지 GE가 비어 있으면 Definition 설정 누락이므로 경고로 알린다.
	if (SkillData.Targeting.Radius <= 0.0f || !SkillData.Effects.HitGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru descent impact skipped - damage coefficient set but radius or hit GE missing. Radius: %.2f, HitGE: %s, Coefficient: %.2f"),
			SkillData.Targeting.Radius,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (!AvatarActor || !World || !SourceASC)
	{
		return;
	}

	const FVector Center = AvatarActor->GetActorLocation();
	const float Radius = SkillData.Targeting.Radius;

	TArray<FOverlapResult> OverlapResults;
	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeruDescentImpact), false, AvatarActor);
	World->OverlapMultiByObjectType(
		OverlapResults,
		Center,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	TSet<AActor*> ProcessedTargets;
	int32 HitCount = 0;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || ProcessedTargets.Contains(TargetActor) || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}
		ProcessedTargets.Add(TargetActor);

		// 피해: 계수만 전달하고 공격력 곱셈/치명타/방어 감쇄는 ExecCalc가 처리한다. 쿨다운 태그는 처치 스킬 식별용 꼬리표.
		UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			SkillData.Effects.HitGameplayEffect,
			SkillData.Effects.DamageCoefficient,
			SkillData.CooldownTag,
			SkillData.InputTag
		);
		++HitCount;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// 실제 판정 그대로 표시한다(구체 = Overlap 판정 볼륨, 원 = 지면 가독용).
	// DrawShapeForOwner 내부에서 /debugline 치트 활성 여부를 확인하고, 원격 소유 클라이언트에는 Client RPC로 전송한다.
	if (bDrawDebugDescentImpact)
	{
		MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
			FMySkillDebugShape::MakeSphere(Center, Radius, FColor::Red, DebugShapeLifeTime, 1.0f));
		MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
			FMySkillDebugShape::MakeCircle(Center, Radius, FColor::Red, DebugShapeLifeTime, 2.0f));
	}
#endif

	UE_LOG(LogTemp, Log, TEXT("Heru descent impact - Avatar: %s, Radius: %.1f, Coefficient: %.2f, Hits: %d"),
		*GetNameSafe(AvatarActor),
		Radius,
		SkillData.Effects.DamageCoefficient,
		HitCount);
}

////////////////////////////
//! \author HanUl
//! \brief 시전 동안 부여한 슈퍼아머 태그를 제거한다.
//! \param ActorInfo Ability Actor 정보
//! \return 없음
void UMyGA_Heru_Descent::RemoveSuperArmorTag(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bSuperArmorApplied)
	{
		return;
	}

	bSuperArmorApplied = false;

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(MyGameplayTags::Status_Buff_SuperArmor, 1, EGameplayTagReplicationState::TagOnly);
	}
}
