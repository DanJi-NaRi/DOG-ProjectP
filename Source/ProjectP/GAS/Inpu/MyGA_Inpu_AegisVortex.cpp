////////////////////////////
//! \file MyGA_Inpu_AegisVortex.cpp
//! \brief Inpu의 이지스 소용돌이 스킬 GameplayAbility를 구현한다.

#include "MyGA_Inpu_AegisVortex.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MySkillDebugShape.h"
#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "GAS/SkillData/MySkillDefinitionFragment.h"
#include "Enemy/Core/CPP_EnemyBase.h"
#include "MyGameplayTags.h"
#include "TimerManager.h"

namespace
{
	////////////////////////////
	//! \struct FAegisVortexSequenceState
	//! \brief 어빌리티 수명과 분리되어 밀어내기 파동을 반복하고 마지막에 피니셔를 실행하는 시퀀스 상태.
	struct FAegisVortexSequenceState
	{
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TWeakObjectPtr<AActor> AvatarActor;
		TSubclassOf<UGameplayEffect> HitGameplayEffect;
		FGameplayTag CooldownTag;
		FGameplayTag InputTag;
		FGameplayTag FinisherCueTag;

		float PulseDamageCoefficient = 0.0f;
		float FinisherDamageCoefficient = 0.0f;
		float PulseRadius = 0.0f;
		float FinisherRadius = 0.0f;
		float MoveDistance = 0.0f;
		EMyAegisVortexMoveMode MoveMode = EMyAegisVortexMoveMode::Pull;
		float PulseInterval = 0.0f;
		float FinisherDelay = 0.0f;
		int32 PulseCount = 3;
		int32 PulseIndex = 0;

		float DebugLifeTime = 0.5f;
		bool bDrawDebug = false;

		FTimerHandle PulseTimerHandle;
		FTimerHandle FinisherTimerHandle;
	};

	//! \brief 반경 안 적대 대상을 중복 없이 모아 계수 피해를 적용한다(살아있는 적만).
	void ApplyRadialCoefficientDamage(
		const TSharedRef<FAegisVortexSequenceState>& State,
		UWorld* World,
		AActor* AvatarActor,
		UAbilitySystemComponent* SourceASC,
		const FVector& Center,
		float Radius,
		float Coefficient,
		const FCollisionObjectQueryParams& ObjectQueryParams,
		TArray<AActor*>* OutHitEnemies)
	{
		if (Radius <= 0.0f)
		{
			return;
		}

		TArray<FOverlapResult> OverlapResults;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuAegisVortex), false, AvatarActor);
		World->OverlapMultiByObjectType(
			OverlapResults,
			Center,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(Radius),
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
				Coefficient,
				State->CooldownTag,
				State->InputTag
			);

			if (OutHitEnemies)
			{
				OutHitEnemies->Add(TargetActor);
			}
		}
	}

	//! \brief 일반 몬스터를 MoveMode에 따라 중심으로 당기거나 바깥으로 밀어낸다.
	//!        Pull은 중심까지 남은 거리로 클램프해 중심을 넘지 않고, Push는 매 파동 MoveDistance만큼 일정하게 밀어낸다.
	void MoveEnemyByVortex(const TSharedRef<FAegisVortexSequenceState>& State, AActor* TargetActor, const FVector& Center)
	{
		ACPP_EnemyBase* EnemyTarget = Cast<ACPP_EnemyBase>(TargetActor);
		if (!EnemyTarget || State->MoveDistance <= 0.0f)
		{
			return;
		}

		// 파동 피해로 이 프레임에 죽은 대상은 변위에서 제외한다.
		// 사망 처리로 래그돌이 켜진 뒤 위치를 옮기면 위치 델타가 물리 속도로 환산돼 시체가 날아간다.
		if (EnemyTarget->IsDead() || !UMyAbilitySystemLibrary::IsLivingPawn(EnemyTarget))
		{
			return;
		}

		// 대상이 중심과 정확히 겹치면 방향을 정할 수 없어 건너뛴다.
		const FVector TargetLocation = EnemyTarget->GetActorLocation();
		const FVector FromCenter(TargetLocation.X - Center.X, TargetLocation.Y - Center.Y, 0.0f);
		const float Distance = FromCenter.Size();
		const FVector OutwardDirection = FromCenter.GetSafeNormal2D();
		if (OutwardDirection.IsNearlyZero() || Distance <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		FVector MoveDirection = OutwardDirection;
		float MoveAmount = State->MoveDistance;
		if (State->MoveMode == EMyAegisVortexMoveMode::Pull)
		{
			// 당길 때는 중심까지 남은 거리로 클램프한다 → 오버슈트 없음, 중심 근접 적은 이동량이 0에 가깝다.
			MoveDirection = -OutwardDirection;
			MoveAmount = FMath::Min(State->MoveDistance, Distance);
		}

		// AI가 매 프레임 velocity를 제어해도 이동이 상쇄되지 않도록 위치를 직접 옮긴다.
		// bSweep=true라 벽·다른 몬스터 콜라이더와 충돌하면 막혀, 억지 겹침 없이 자연스럽게 이동한다.
		EnemyTarget->AddActorWorldOffset(MoveDirection * MoveAmount, true);
	}

	//! \brief 메이스 지면 강타(피니셔): 피니셔 반경 안 적대 대상에 피해를 주고 강타 GameplayCue를 실행한다.
	void ExecuteAegisVortexFinisher(TWeakObjectPtr<UWorld> WeakWorld, TSharedRef<FAegisVortexSequenceState> State)
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
		const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

		ApplyRadialCoefficientDamage(
			State, World, AvatarActor, SourceASC, Center,
			State->FinisherRadius, State->FinisherDamageCoefficient,
			ObjectQueryParams, nullptr);

		FGameplayCueParameters CueParameters;
		CueParameters.Location = Center;
		CueParameters.Instigator = AvatarActor;
		CueParameters.EffectCauser = AvatarActor;
		SourceASC->ExecuteGameplayCue(State->FinisherCueTag, CueParameters);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (State->bDrawDebug)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeCircle(Center, State->FinisherRadius, FColor::Orange, State->DebugLifeTime, 2.0f));
		}
#endif

		UE_LOG(LogTemp, Log, TEXT("Inpu Aegis Vortex finisher - Avatar: %s, FinisherRadius: %.1f"),
			*GetNameSafe(AvatarActor), State->FinisherRadius);
	}

	//! \brief 파동 1회: 반경 안 적에게 피해를 주고 일반 몬스터를 바깥으로 밀어낸다. 마지막 파동 후 피니셔를 FinisherDelay 뒤에 예약한다.
	void ExecuteAegisVortexPulse(TWeakObjectPtr<UWorld> WeakWorld, TSharedRef<FAegisVortexSequenceState> State)
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
			World->GetTimerManager().ClearTimer(State->PulseTimerHandle);
			return;
		}

		const FVector Center = AvatarActor->GetActorLocation();
		const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

		// 파동: 반경 안 적 전원에게 피해, 일반 몬스터는 MoveMode에 따라 당기거나 밀어냄.
		TArray<AActor*> PulseHitEnemies;
		ApplyRadialCoefficientDamage(
			State, World, AvatarActor, SourceASC, Center,
			State->PulseRadius, State->PulseDamageCoefficient,
			ObjectQueryParams, &PulseHitEnemies);

		for (AActor* TargetActor : PulseHitEnemies)
		{
			MoveEnemyByVortex(State, TargetActor, Center);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (State->bDrawDebug)
		{
			MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
				FMySkillDebugShape::MakeCircle(Center, State->PulseRadius, FColor::Cyan, State->DebugLifeTime, 2.0f));
		}
#endif

		++State->PulseIndex;

		// 마지막 파동 후 파동 타이머를 종료하고, 지면 강타(피니셔)를 FinisherDelay 뒤에 예약한다.
		// (FinisherDelay가 0이면 즉시 실행 → 기존과 동일 동작)
		if (State->PulseIndex >= State->PulseCount)
		{
			World->GetTimerManager().ClearTimer(State->PulseTimerHandle);

			if (State->FinisherDelay > 0.0f)
			{
				World->GetTimerManager().SetTimer(
					State->FinisherTimerHandle,
					[WeakWorld, State]()
					{
						ExecuteAegisVortexFinisher(WeakWorld, State);
					},
					State->FinisherDelay,
					false
				);
			}
			else
			{
				ExecuteAegisVortexFinisher(WeakWorld, State);
			}
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief Aegis Vortex 기본값을 초기화한다. 몽타주는 로컬 예측하고 판정과 효과는 서버에서만 수행한다.
//! \param 없음
//! \return 없음
UMyGA_Inpu_AegisVortex::UMyGA_Inpu_AegisVortex()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 SkillDefinition 스킬 파이프라인으로 Aegis Vortex를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 이벤트 데이터
//! \return 없음
void UMyGA_Inpu_AegisVortex::ActivateAbility(
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
//! \param SkillData 현재 Aegis Vortex SkillDefinition 데이터
//! \return 발동 가능한 데이터면 true
bool UMyGA_Inpu_AegisVortex::CanActivateStandardSkill(
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
		UE_LOG(LogTemp, Warning, TEXT("Inpu Aegis Vortex activation failed - LocalPredicted skill requires a montage. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	const UMyAegisVortexFragment* Fragment = SkillDefinition->FindFragment<UMyAegisVortexFragment>();
	if (!Fragment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Aegis Vortex activation failed - UMyAegisVortexFragment missing. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (SkillData.Targeting.Radius <= 0.0f
		|| SkillData.Timing.ActiveDuration <= 0.0f
		|| !SkillData.Effects.HitGameplayEffect
		|| SkillData.Effects.DamageCoefficient <= 0.0f
		|| SkillData.Effects.SecondaryDamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Aegis Vortex activation failed - Definition tuning invalid. Radius: %.1f, Duration: %.2f, HitGE: %s, PulseCoef: %.2f, FinisherCoef: %.2f"),
			SkillData.Targeting.Radius,
			SkillData.Timing.ActiveDuration,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient,
			SkillData.Effects.SecondaryDamageCoefficient);
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 Shoot 시점에 서버 권한에서만 반복 밀어내기 파동 러너를 시작한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 이벤트 데이터
//! \param SkillData 현재 Aegis Vortex SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_AegisVortex::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	// 판정·피해·밀어내기·Cue는 서버 권한에서만 수행한다. 소유 클라이언트는 몽타주만 로컬 예측한다.
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
	const UMyAegisVortexFragment* Fragment = SkillDefinition ? SkillDefinition->FindFragment<UMyAegisVortexFragment>() : nullptr;
	if (!AvatarActor || !World || !SourceASC || !Fragment)
	{
		return;
	}

	const int32 PulseCount = Fragment->GetPulseCount();
	const float PulseInterval = SkillData.Timing.ActiveDuration / static_cast<float>(FMath::Max(PulseCount, 1));

	TSharedRef<FAegisVortexSequenceState> State = MakeShared<FAegisVortexSequenceState>();
	State->SourceASC = SourceASC;
	State->AvatarActor = AvatarActor;
	State->HitGameplayEffect = SkillData.Effects.HitGameplayEffect;
	State->CooldownTag = SkillData.CooldownTag;
	State->InputTag = SkillData.InputTag;
	State->FinisherCueTag = MyGameplayTags::GameplayCue_Ability_Inpu_AegisVortex;
	State->PulseDamageCoefficient = SkillData.Effects.DamageCoefficient;
	State->FinisherDamageCoefficient = SkillData.Effects.SecondaryDamageCoefficient;
	State->PulseRadius = SkillData.Targeting.Radius;
	State->FinisherRadius = Fragment->GetFinisherRadius();
	State->MoveDistance = Fragment->GetMoveDistance();
	State->MoveMode = Fragment->GetMoveMode();
	State->PulseInterval = PulseInterval;
	State->FinisherDelay = Fragment->GetFinisherDelay();
	State->PulseCount = PulseCount;
	State->PulseIndex = 0;
	State->DebugLifeTime = DebugShapeLifeTime;
	State->bDrawDebug = bDrawDebugAegisVortex;

	TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimer(
		State->PulseTimerHandle,
		[WeakWorld, State]()
		{
			ExecuteAegisVortexPulse(WeakWorld, State);
		},
		FMath::Max(PulseInterval, 0.01f),
		true
	);

	UE_LOG(LogTemp, Log, TEXT("Inpu Aegis Vortex started - Avatar: %s, Radius: %.1f, Duration: %.2f, Pulses: %d, Interval: %.3f, MoveMode: %s, MoveDist: %.1f"),
		*GetNameSafe(AvatarActor),
		State->PulseRadius,
		SkillData.Timing.ActiveDuration,
		PulseCount,
		PulseInterval,
		State->MoveMode == EMyAegisVortexMoveMode::Push ? TEXT("Push") : TEXT("Pull"),
		State->MoveDistance);
}
