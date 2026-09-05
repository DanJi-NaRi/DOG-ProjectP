////////////////////////////
//! \file MyGA_Heru_Thrust.cpp
//! \brief Heru 전방 찌르기(다단 히트 + 표식 소비) 스킬 GameplayAbility 구현 파일이다.
#include "MyGA_Heru_Thrust.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "../MyAbilitySystemLibrary.h"
#include "../MySkillDebugShape.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../../MyGameplayTags.h"

namespace
{
	//! \brief 히트 간격이 데이터에 없을 때 사용할 기본값(초).
	constexpr float DefaultThrustHitInterval = 0.1f;

	//! \struct FHeruThrustTargetState
	//! \brief 1타 시점에 고정된 대상과 그 대상에게서 소비한 표식 스택 수를 보관한다.
	struct FHeruThrustTargetState
	{
		TWeakObjectPtr<AActor> Target;
		int32 ConsumedStacks = 0;
	};

	//! \struct FHeruThrustSequenceState
	//! \brief 어빌리티 수명과 분리되어 서버에서 진행되는 다단 히트 시퀀스 상태.
	//!        몽타주가 조기 종료되거나 어빌리티가 끝나도 남은 타격이 유실되지 않는다.
	struct FHeruThrustSequenceState
	{
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TArray<FHeruThrustTargetState> Targets;
		TSubclassOf<UGameplayEffect> HitGameplayEffect;
		FGameplayTag SkillCooldownTag;
		FGameplayTag SkillInputTag;
		float BaseCoefficient = 0.0f;
		float PerStackBonusCoefficient = 0.0f;
		float BonusHitCoefficient = 0.0f;
		float HitInterval = DefaultThrustHitInterval;
		int32 RemainingNormalHits = 0;
		int32 BonusHitRequiredStacks = 3;
	};

	//! \brief 고정된 대상들에게 일반 타격 1회를 적용한다. 죽었거나 사라진 대상은 건너뛴다.
	void ApplyThrustHit(const TSharedRef<FHeruThrustSequenceState>& State)
	{
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		if (!SourceASC)
		{
			return;
		}

		for (const FHeruThrustTargetState& TargetState : State->Targets)
		{
			AActor* TargetActor = TargetState.Target.Get();
			if (!TargetActor || !UMyAbilitySystemLibrary::IsLivingPawn(TargetActor))
			{
				continue;
			}

			// 틱 계수 = 기본 계수 x (1 + 스택당 증폭 x 소비 스택 수). 공격력 곱셈/크리/방어는 ExecCalc가 처리한다.
			const float HitCoefficient = State->BaseCoefficient
				* (1.0f + State->PerStackBonusCoefficient * static_cast<float>(TargetState.ConsumedStacks));

			UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
				SourceASC,
				TargetActor,
				State->HitGameplayEffect,
				HitCoefficient,
				State->SkillCooldownTag,
				State->SkillInputTag
			);
		}
	}

	//! \brief 필요 스택 수 이상을 소비했던 대상에게 추가타를 1회 적용한다.
	void ApplyThrustBonusHit(const TSharedRef<FHeruThrustSequenceState>& State)
	{
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		if (!SourceASC || State->BonusHitCoefficient <= 0.0f)
		{
			return;
		}

		for (const FHeruThrustTargetState& TargetState : State->Targets)
		{
			if (TargetState.ConsumedStacks < State->BonusHitRequiredStacks)
			{
				continue;
			}

			AActor* TargetActor = TargetState.Target.Get();
			if (!TargetActor || !UMyAbilitySystemLibrary::IsLivingPawn(TargetActor))
			{
				continue;
			}

			UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
				SourceASC,
				TargetActor,
				State->HitGameplayEffect,
				State->BonusHitCoefficient,
				State->SkillCooldownTag,
				State->SkillInputTag
			);
		}
	}

	//! \brief 다음 타격(일반 타 또는 추가타)을 HitInterval 뒤에 예약한다.
	void ScheduleNextThrustHit(UWorld* World, TSharedRef<FHeruThrustSequenceState> State)
	{
		if (!World)
		{
			return;
		}

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			[WeakWorld = TWeakObjectPtr<UWorld>(World), State]()
			{
				if (State->RemainingNormalHits > 0)
				{
					--State->RemainingNormalHits;
					ApplyThrustHit(State);

					// 남은 일반 타가 있거나, 추가타 대상이 존재하면 다음 타를 예약한다.
					const bool bHasBonusTarget = State->Targets.ContainsByPredicate(
						[&State](const FHeruThrustTargetState& TargetState)
						{
							return TargetState.ConsumedStacks >= State->BonusHitRequiredStacks;
						});

					if (State->RemainingNormalHits > 0 || bHasBonusTarget)
					{
						ScheduleNextThrustHit(WeakWorld.Get(), State);
					}
					return;
				}

				ApplyThrustBonusHit(State);
			},
			FMath::Max(State->HitInterval, 0.01f),
			false
		);
	}
}

////////////////////////////
//! \author HanUl
//! \brief Heru Thrust 기본값을 초기화한다. 몽타주는 예측하되 판정/표식 소비는 서버에서만 실행한다.
//! \param 없음
//! \return 없음
UMyGA_Heru_Thrust::UMyGA_Heru_Thrust()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ConsumeStatusTag = MyGameplayTags::Status_Mark;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인으로 Thrust를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_Heru_Thrust::ActivateAbility(
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
//! \brief 찌르기 방향과 필수 스킬 데이터를 확정한다. 데이터가 없으면 발동을 막는다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGA_Heru_Thrust::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	if (SkillData.Targeting.Range <= 0.0f || SkillData.Targeting.Width <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru thrust activation failed - targeting shape invalid. Range: %.2f, Width: %.2f"),
			SkillData.Targeting.Range,
			SkillData.Targeting.Width);
		return false;
	}

	if (!SkillData.Effects.HitGameplayEffect || SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru thrust activation failed - hit GE or coefficient missing. HitGE: %s, Coefficient: %.2f"),
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient);
		return false;
	}

	if (!ResolveThrustDirection(ActorInfo, TriggerEventData, CachedThrustDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru thrust activation failed - thrust direction is missing. Ability: %s"), *GetNameSafe(this));
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Shoot 시점에 범위 내 적을 고정하고 표식을 소비한 뒤 1타를 적용하고, 남은 타격 시퀀스를 예약한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Thrust::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return;
	}

	// 판정 범위 디버그는 로컬 예측 인스턴스에서 스킬 소유자 화면에만 표시한다.
	if (bDrawDebugBox)
	{
		DrawDebugThrustBox(AvatarActor, CachedThrustDirection, SkillData);
	}

	// 판정/표식 소비/피해는 서버 권위에서만 실행한다(클라는 몽타주 연출만).
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	UWorld* World = AvatarActor->GetWorld();
	if (!SourceASC || !World)
	{
		return;
	}

	// 1타 시점에 대상을 고정한다. 이후 범위 이탈/신규 진입은 시퀀스에 영향을 주지 않는다.
	TArray<AActor*> InitialTargets;
	CollectThrustTargets(AvatarActor, CachedThrustDirection, SkillData, InitialTargets);

	TSharedRef<FHeruThrustSequenceState> State = MakeShared<FHeruThrustSequenceState>();
	State->SourceASC = SourceASC;
	State->HitGameplayEffect = SkillData.Effects.HitGameplayEffect;
	State->SkillCooldownTag = SkillData.CooldownTag;
	State->SkillInputTag = SkillData.InputTag;
	State->BaseCoefficient = SkillData.Effects.DamageCoefficient;
	State->PerStackBonusCoefficient = FMath::Max(SkillData.Effects.StatusDamageCoefficient, 0.0f);
	State->BonusHitCoefficient = FMath::Max(SkillData.Effects.SecondaryDamageCoefficient, 0.0f);
	State->HitInterval = SkillData.Timing.TickInterval > 0.0f ? SkillData.Timing.TickInterval : DefaultThrustHitInterval;
	State->RemainingNormalHits = FMath::Max(ThrustHitCount, 1);
	State->BonusHitRequiredStacks = FMath::Max(BonusHitRequiredStacks, 1);

	// 대상별 표식 스택을 전량 소비하고 증폭 계수 계산용으로 기억한다.
	int32 TotalConsumedStacks = 0;
	for (AActor* TargetActor : InitialTargets)
	{
		UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		FHeruThrustTargetState TargetState;
		TargetState.Target = TargetActor;
		TargetState.ConsumedStacks = ConsumeStatusTag.IsValid()
			? UMyAbilitySystemLibrary::ConsumeStacksByGrantedTag(TargetASC, ConsumeStatusTag)
			: 0;
		TotalConsumedStacks += TargetState.ConsumedStacks;
		State->Targets.Add(TargetState);
	}

	if (State->Targets.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("Heru thrust missed - Avatar: %s"), *GetNameSafe(AvatarActor));
		return;
	}

	// 1타는 즉시 적용하고, 남은 타격과 추가타는 어빌리티 수명과 분리된 월드 타이머로 진행한다.
	--State->RemainingNormalHits;
	ApplyThrustHit(State);
	ScheduleNextThrustHit(World, State);

	UE_LOG(LogTemp, Log, TEXT("Heru thrust started - Avatar: %s, Targets: %d, ConsumedStacks: %d, Hits: %d, Interval: %.2f"),
		*GetNameSafe(AvatarActor),
		State->Targets.Num(),
		TotalConsumedStacks,
		ThrustHitCount,
		State->HitInterval);
}

////////////////////////////
//! \author HanUl
//! \brief 입력 EventData의 조준(마우스) 지점을 향하는 방향을 확정한다. 없으면 바라보는 방향으로 폴백한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param OutDirection 확정된 찌르기 방향(수평 정규화)
//! \return 방향을 확정했으면 true
bool UMyGA_Heru_Thrust::ResolveThrustDirection(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;

	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return false;
	}

	// 조준(마우스) 지점 = TargetData 중 bBlockingHit==true 항목(이동 방향 항목은 false라 제외된다).
	if (TriggerEventData)
	{
		for (int32 Index = 0; Index < TriggerEventData->TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(Index);
			if (!TargetData || !TargetData->HasHitResult())
			{
				continue;
			}

			const FHitResult* HitResult = TargetData->GetHitResult();
			if (!HitResult || !HitResult->bBlockingHit)
			{
				continue;
			}

			FVector ToAim = HitResult->Location - AvatarActor->GetActorLocation();
			ToAim.Z = 0.0f;
			const FVector Candidate = ToAim.GetSafeNormal2D();
			if (!Candidate.IsNearlyZero())
			{
				OutDirection = Candidate;
				return true;
			}
		}
	}

	OutDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief 시전자 전방 직사각형(깊이 Range x 폭 Width) 범위의 적대 대상을 수집한다.
//! \param AvatarActor 판정 기준 Avatar
//! \param Direction 찌르기 방향(수평 정규화)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \param OutTargets 수집된 대상 목록(출력)
//! \return 없음
void UMyGA_Heru_Thrust::CollectThrustTargets(const AActor* AvatarActor, const FVector& Direction, const FMySkillDataEntry& SkillData, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	const UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World || SkillData.Targeting.Range <= 0.0f || SkillData.Targeting.Width <= 0.0f)
	{
		return;
	}

	FVector Forward = Direction;
	if (Forward.IsNearlyZero())
	{
		Forward = AvatarActor->GetActorForwardVector();
	}
	Forward = Forward.GetSafeNormal2D();

	const FVector Origin = AvatarActor->GetActorLocation();
	const FVector BoxCenter = Origin + Forward * (SkillData.Targeting.Range * 0.5f);
	const FVector BoxExtent(SkillData.Targeting.Range * 0.5f, SkillData.Targeting.Width * 0.5f, FMath::Max(HitBoxHalfHeight, 1.0f));
	const FQuat BoxRotation = Forward.ToOrientationQuat();

	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeruThrustBox), false, AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(
			OverlapResults,
			BoxCenter,
			BoxRotation,
			ObjectQueryParams,
			FCollisionShape::MakeBox(BoxExtent),
			QueryParams))
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

		if (!UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
		{
			continue;
		}

		UniqueTargets.Add(TargetActor);
		OutTargets.Add(TargetActor);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 찌르기 판정 박스를 디버그로 시각화한다(임시).
//! \param AvatarActor 판정 기준 Avatar
//! \param Direction 찌르기 방향(수평 정규화)
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Thrust::DrawDebugThrustBox(AActor* AvatarActor, const FVector& Direction, const FMySkillDataEntry& SkillData) const
{
	if (!AvatarActor || SkillData.Targeting.Range <= 0.0f || SkillData.Targeting.Width <= 0.0f)
	{
		return;
	}

	FVector Forward = Direction.IsNearlyZero() ? AvatarActor->GetActorForwardVector() : Direction;
	Forward = Forward.GetSafeNormal2D();

	const FVector Origin = AvatarActor->GetActorLocation();
	const FVector BoxCenter = Origin + Forward * (SkillData.Targeting.Range * 0.5f);
	const FVector BoxExtent(SkillData.Targeting.Range * 0.5f, SkillData.Targeting.Width * 0.5f, FMath::Max(HitBoxHalfHeight, 1.0f));
	constexpr float DebugDuration = 2.0f;

	MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
		FMySkillDebugShape::MakeBox(BoxCenter, BoxExtent, Forward, FColor::Red, DebugDuration, 2.0f));
	MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
		FMySkillDebugShape::MakeLine(Origin, Origin + Forward * SkillData.Targeting.Range, FColor::Yellow, DebugDuration, 3.0f));
}
