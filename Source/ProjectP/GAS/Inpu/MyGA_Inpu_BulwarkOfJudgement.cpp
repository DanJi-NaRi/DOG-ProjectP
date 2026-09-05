////////////////////////////
//! \file MyGA_Inpu_BulwarkOfJudgement.cpp
//! \brief Inpu의 심판의 방벽 궁극기 GameplayAbility를 구현한다.

#include "MyGA_Inpu_BulwarkOfJudgement.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MySkillDebugShape.h"
#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "GAS/SkillData/MySkillDefinitionFragment.h"
#include "Player/Area/MyAreaBase.h"
#include "MyGameplayTags.h"
#include "TimerManager.h"

namespace
{
	////////////////////////////
	//! \struct FBulwarkOfJudgementSequenceState
	//! \brief 어빌리티 수명과 분리되어 돔 유지 후 강타를 실행하는 시퀀스 상태.
	struct FBulwarkOfJudgementSequenceState
	{
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TWeakObjectPtr<AActor> AvatarActor;
		TSubclassOf<UGameplayEffect> HitGameplayEffect;
		FGameplayTag CooldownTag;
		FGameplayTag InputTag;
		FGameplayTag ImpactCueTag;

		float ImpactCoefficient = 0.0f;
		float PerShieldedCoefficient = 0.0f;
		float Radius = 0.0f;
		int32 ShieldedCount = 0;

		float DebugLifeTime = 1.0f;
		bool bDrawDebug = false;
	};

	//! \brief 대상 ASC에 Data.Shield SetByCaller 보호막 GameplayEffect를 적용한다.
	bool ApplyShieldToActor(
		UAbilitySystemComponent* SourceASC,
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> ShieldEffectClass,
		float ShieldAmount)
	{
		UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
		if (!SourceASC || !TargetASC || !ShieldEffectClass || ShieldAmount <= 0.0f)
		{
			return false;
		}

		FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
		EffectContext.AddSourceObject(SourceASC->GetAvatarActor());
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ShieldEffectClass, 1.0f, EffectContext);
		if (!SpecHandle.IsValid())
		{
			return false;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Shield, ShieldAmount);
		const FActiveGameplayEffectHandle AppliedHandle =
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		return AppliedHandle.IsValid();
	}

	//! \brief 메이스 강타: 원형 반경 안 적에게 보호막 수 비례 계수 피해를 준다. 능력 전개와 동시에 즉시 실행된다.
	void ExecuteBulwarkImpact(TWeakObjectPtr<UWorld> WeakWorld, TSharedRef<FBulwarkOfJudgementSequenceState> State)
	{
		UWorld* World = WeakWorld.Get();
		if (!World)
		{
			return;
		}

		AActor* AvatarActor = State->AvatarActor.Get();
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		if (!AvatarActor || !SourceASC)
		{
			return;
		}

		const FVector Center = AvatarActor->GetActorLocation();
		const float ImpactCoefficient = State->ImpactCoefficient + State->PerShieldedCoefficient * static_cast<float>(State->ShieldedCount);

		if (State->Radius > 0.0f && ImpactCoefficient > 0.0f && State->HitGameplayEffect)
		{
			TArray<FOverlapResult> OverlapResults;
			const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuBulwarkOfJudgementImpact), false, AvatarActor);
			World->OverlapMultiByObjectType(
				OverlapResults,
				Center,
				FQuat::Identity,
				ObjectQueryParams,
				FCollisionShape::MakeSphere(State->Radius),
				QueryParams
			);

			TSet<AActor*> ProcessedTargets;
			for (const FOverlapResult& OverlapResult : OverlapResults)
			{
				AActor* TargetActor = OverlapResult.GetActor();
				if (!TargetActor || ProcessedTargets.Contains(TargetActor) || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
				{
					continue;
				}
				ProcessedTargets.Add(TargetActor);

				UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
					SourceASC,
					TargetActor,
					State->HitGameplayEffect,
					ImpactCoefficient,
					State->CooldownTag,
					State->InputTag
				);
			}
		}

		FGameplayCueParameters CueParameters;
		CueParameters.Location = Center;
		CueParameters.Instigator = AvatarActor;
		CueParameters.EffectCauser = AvatarActor;
		SourceASC->ExecuteGameplayCue(State->ImpactCueTag, CueParameters);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (State->bDrawDebug)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeCircle(Center, State->Radius, FColor::Orange, State->DebugLifeTime, 2.0f));
		}
#endif

		UE_LOG(LogTemp, Log, TEXT("Inpu Bulwark of Judgement impact - Avatar: %s, Radius: %.1f, ShieldedCount: %d, Coefficient: %.2f"),
			*GetNameSafe(AvatarActor), State->Radius, State->ShieldedCount, ImpactCoefficient);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Bulwark of Judgement 기본값을 초기화한다. 몽타주는 로컬 예측하고 판정과 효과는 서버에서만 수행한다.
//! \param 없음
//! \return 없음
UMyGA_Inpu_BulwarkOfJudgement::UMyGA_Inpu_BulwarkOfJudgement()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 SkillDefinition 스킬 파이프라인으로 Bulwark of Judgement를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 이벤트 데이터
//! \return 없음
void UMyGA_Inpu_BulwarkOfJudgement::ActivateAbility(
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
//! \brief Definition 필수값과 Fragment를 검증한다. 자기중심 스킬이라 조준 방향은 사용하지 않는다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 이벤트 데이터
//! \param SkillData 현재 Bulwark of Judgement SkillDefinition 데이터
//! \return 발동 가능한 데이터면 true
bool UMyGA_Inpu_BulwarkOfJudgement::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;

	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	if (!SkillDefinition)
	{
		return false;
	}

	if (!SkillDefinition->GetAnimation().Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark of Judgement activation failed - LocalPredicted skill requires a montage. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	const UMyBulwarkOfJudgementFragment* Fragment = SkillDefinition->FindFragment<UMyBulwarkOfJudgementFragment>();
	if (!Fragment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark of Judgement activation failed - UMyBulwarkOfJudgementFragment missing. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (SkillData.Targeting.Radius <= 0.0f
		|| SkillData.Timing.ActiveDuration <= 0.0f
		|| !SkillData.Effects.HitGameplayEffect
		|| SkillData.Effects.DamageCoefficient <= 0.0f
		|| !SkillData.Effects.BuffGameplayEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark of Judgement activation failed - Definition tuning invalid. Radius: %.1f, Duration: %.2f, HitGE: %s, Coefficient: %.2f, ShieldGE: %s"),
			SkillData.Targeting.Radius,
			SkillData.Timing.ActiveDuration,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient,
			*GetNameSafe(SkillData.Effects.BuffGameplayEffect));
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 Shoot 시점에 서버 권한에서만 돔 생성·아군 보호막 부여·강타 타이머 예약을 수행한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 이벤트 데이터
//! \param SkillData 현재 Bulwark of Judgement SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_BulwarkOfJudgement::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	// 판정·보호막·피해·스폰은 서버 권한에서만 수행한다. 소유 클라이언트는 몽타주만 로컬 예측한다.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	const UMyBulwarkOfJudgementFragment* Fragment = SkillDefinition ? SkillDefinition->FindFragment<UMyBulwarkOfJudgementFragment>() : nullptr;
	if (!AvatarActor || !World || !SourceASC || !Fragment)
	{
		return;
	}

	const float Radius = SkillData.Targeting.Radius;
	const FVector Center = AvatarActor->GetActorLocation();
	const float DomeDuration = FMath::Max(SkillData.Timing.ActiveDuration, 0.01f);

	// 1) 돔 비주얼 액터를 스폰해 플레이어에 부착한다. 위치만 추적하고 회전은 무시한다.
	//    돔은 SetLifeSpan(DomeDuration)으로 유지되다 자동 소멸한다(강타는 아래에서 즉시 실행).
	if (SkillData.Area.AreaClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = AvatarActor;
		SpawnParams.Instigator = Cast<APawn>(AvatarActor);
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Dome = World->SpawnActor<AActor>(SkillData.Area.AreaClass, Center, FRotator::ZeroRotator, SpawnParams);
		if (Dome)
		{
			Dome->AttachToActor(AvatarActor, FAttachmentTransformRules::KeepWorldTransform);
			if (USceneComponent* DomeRoot = Dome->GetRootComponent())
			{
				// 위치만 추적하고 회전은 무시한다(플레이어가 돌아도 돔은 따라 돌지 않는다).
				DomeRoot->SetUsingAbsoluteRotation(true);
				DomeRoot->SetWorldRotation(FRotator::ZeroRotator);
			}

			const float VisualRadius = SkillData.Area.Radius > 0.0f ? SkillData.Area.Radius : Radius;
			// AreaSkillBase의 ConfigureAreaVisualActor와 동일하게 액터를 스케일해 크기를 반영한다.
			// (돔 Niagara가 User.Radius 파라미터를 크기에 연동하지 않아도 액터 스케일로 커진다.)
			Dome->SetActorScale3D(FVector(VisualRadius / 100.0f));
			if (AMyAreaBase* AreaVisual = Cast<AMyAreaBase>(Dome))
			{
				AreaVisual->ApplyAreaVisualSpec(VisualRadius, DomeDuration);
			}
			Dome->SetLifeSpan(DomeDuration);
		}
	}

	// 2) 반경 안 아군(자신 포함)에게 시전자 MaxHP 비율 보호막을 부여하고 부여 성공 수를 센다.
	int32 ShieldedCount = 0;
	const float MaxHealth = SourceASC->GetNumericAttribute(UMyAttributeSet::GetMaxHealthAttribute());
	const float ShieldAmount = MaxHealth * (Fragment->GetShieldPercentOfMaxHealth() * 0.01f);
	if (SkillData.Effects.BuffGameplayEffect && Radius > 0.0f && ShieldAmount > 0.0f)
	{
		TSet<AActor*> ProcessedTargets;
		ProcessedTargets.Add(AvatarActor);
		if (ApplyShieldToActor(SourceASC, AvatarActor, SkillData.Effects.BuffGameplayEffect, ShieldAmount))
		{
			++ShieldedCount;
		}

		TArray<FOverlapResult> OverlapResults;
		const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuBulwarkOfJudgementShield), false, AvatarActor);
		World->OverlapMultiByObjectType(
			OverlapResults,
			Center,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(Radius),
			QueryParams
		);

		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			AActor* TargetActor = OverlapResult.GetActor();
			if (!TargetActor || ProcessedTargets.Contains(TargetActor) || !UMyAbilitySystemLibrary::IsFriendly(AvatarActor, TargetActor))
			{
				continue;
			}
			ProcessedTargets.Add(TargetActor);
			if (ApplyShieldToActor(SourceASC, TargetActor, SkillData.Effects.BuffGameplayEffect, ShieldAmount))
			{
				++ShieldedCount;
			}
		}

		FGameplayCueParameters ShieldCueParameters;
		ShieldCueParameters.Location = Center;
		ShieldCueParameters.Instigator = AvatarActor;
		ShieldCueParameters.EffectCauser = AvatarActor;
		SourceASC->ExecuteGameplayCue(MyGameplayTags::GameplayCue_Shield_Apply, ShieldCueParameters);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bDrawDebugBulwarkOfJudgement)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeCircle(Center, Radius, FColor::Green, DebugShapeLifeTime, 3.0f));
		}
#endif
	}

	// 3) 러너 상태 구성 후 강타를 능력 전개와 동시에 즉시 실행한다(돔은 SetLifeSpan으로 유지).
	TSharedRef<FBulwarkOfJudgementSequenceState> State = MakeShared<FBulwarkOfJudgementSequenceState>();
	State->SourceASC = SourceASC;
	State->AvatarActor = AvatarActor;
	State->HitGameplayEffect = SkillData.Effects.HitGameplayEffect;
	State->CooldownTag = SkillData.CooldownTag;
	State->InputTag = SkillData.InputTag;
	State->ImpactCueTag = MyGameplayTags::GameplayCue_Ability_Inpu_BulwarkOfJudgement;
	State->ImpactCoefficient = SkillData.Effects.DamageCoefficient;
	State->PerShieldedCoefficient = Fragment->GetPerShieldedCoefficient();
	State->Radius = Radius;
	State->ShieldedCount = ShieldedCount;
	State->DebugLifeTime = DebugShapeLifeTime;
	State->bDrawDebug = bDrawDebugBulwarkOfJudgement;

	TWeakObjectPtr<UWorld> WeakWorld = World;
	ExecuteBulwarkImpact(WeakWorld, State);

	UE_LOG(LogTemp, Log, TEXT("Inpu Bulwark of Judgement started - Avatar: %s, Radius: %.1f, Duration: %.2f, ShieldedCount: %d, ShieldAmount: %.1f"),
		*GetNameSafe(AvatarActor),
		Radius,
		DomeDuration,
		ShieldedCount,
		ShieldAmount);
}
