////////////////////////////
//! \file MyGA_Heru_Whirl.cpp
//! \brief Heru 회전 베기(칼날 스윕 다단 히트 + 표식 부여) 스킬 GameplayAbility 구현 파일이다.
#include "MyGA_Heru_Whirl.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "../MyAbilitySystemLibrary.h"
#include "../MyAttributeSet.h"
#include "../MySkillDebugShape.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"

namespace
{
	//! \brief 바퀴당 시간이 데이터에 없을 때 사용할 기본값(초).
	constexpr float DefaultWhirlRevolutionPeriod = 0.3f;

	//! \brief 각도를 [0, 360) 범위로 정규화한다.
	float NormalizeDegrees(float Degrees)
	{
		Degrees = FMath::Fmod(Degrees, 360.0f);
		return Degrees < 0.0f ? Degrees + 360.0f : Degrees;
	}

	//! \struct FHeruWhirlSequenceState
	//! \brief 어빌리티 수명과 분리되어 진행되는 칼날 스윕 판정 또는 로컬 디버그 시퀀스 상태.
	struct FHeruWhirlSequenceState
	{
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TWeakObjectPtr<AActor> AvatarActor;
		TSubclassOf<UGameplayEffect> HitGameplayEffect;
		TSubclassOf<UGameplayEffect> MarkGameplayEffect;
		FGameplayTag SkillCooldownTag;
		FGameplayTag SkillInputTag;
		float DamageCoefficient = 0.0f;
		float Radius = 0.0f;
		float RevolutionPeriod = DefaultWhirlRevolutionPeriod;
		float TotalDuration = 0.0f;
		float StartWorldTime = 0.0f;
		float StartAngleDegrees = 0.0f;
		float LastElapsed = 0.0f;
		float DirectionSign = 1.0f;
		bool bApplyGameplayEffects = false;
		bool bDrawDebug = false;

		//! \brief 대상별 마지막 타격 월드 시각(ICD 판정용).
		TMap<TWeakObjectPtr<AActor>, float> LastHitWorldTimeByTarget;

		FTimerHandle SubTickTimerHandle;
	};

	//! \brief 대상 1명에게 타격(피해 + 표식)을 적용한다.
	void ApplyWhirlHitToTarget(const TSharedRef<FHeruWhirlSequenceState>& State, AActor* TargetActor)
	{
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		AActor* AvatarActor = State->AvatarActor.Get();
		if (!SourceASC || !AvatarActor || !TargetActor)
		{
			return;
		}

		UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			return;
		}

		const float HealthBefore = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());

		UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			State->HitGameplayEffect,
			State->DamageCoefficient,
			State->SkillCooldownTag,
			State->SkillInputTag
		);

		// 표식 1스택 부여(살아있는 대상만). 스택 상한/지속/갱신은 표식 GE의 스태킹 설정이 관리한다.
		const float HealthAfter = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
		if (HealthAfter > 0.0f && State->MarkGameplayEffect)
		{
			FGameplayEffectContextHandle MarkContext = SourceASC->MakeEffectContext();
			MarkContext.AddSourceObject(AvatarActor);
			FGameplayEffectSpecHandle MarkSpec = SourceASC->MakeOutgoingSpec(State->MarkGameplayEffect, 1.0f, MarkContext);
			if (MarkSpec.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*MarkSpec.Data.Get(), TargetASC);
			}
		}

		(void)HealthBefore;
	}

	//! \brief 서브틱 1회: 이번 구간 동안 칼날이 쓸고 간 각도 범위 안의 적을 타격한다.
	void ExecuteWhirlSubTick(TWeakObjectPtr<UWorld> WeakWorld, TSharedRef<FHeruWhirlSequenceState> State)
	{
		UWorld* World = WeakWorld.Get();
		AActor* AvatarActor = State->AvatarActor.Get();
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		if (!World)
		{
			return;
		}

		if (!AvatarActor || (State->bApplyGameplayEffects && !SourceASC))
		{
			World->GetTimerManager().ClearTimer(State->SubTickTimerHandle);
			return;
		}

		const float NowElapsed = FMath::Min(World->GetTimeSeconds() - State->StartWorldTime, State->TotalDuration);
		const float PrevElapsed = State->LastElapsed;
		State->LastElapsed = NowElapsed;

		// 이번 서브틱 동안 칼날이 쓸고 간 각도 구간(회전 진행 각 기준, 양수).
		const float SweepStartProgress = 360.0f * PrevElapsed / State->RevolutionPeriod;
		const float SweepDeltaDegrees = 360.0f * (NowElapsed - PrevElapsed) / State->RevolutionPeriod;
		const float BladePrevAngle = State->StartAngleDegrees + State->DirectionSign * SweepStartProgress;

		const FVector Origin = AvatarActor->GetActorLocation();

		if (State->bDrawDebug)
		{
			const float BladeNowAngle = BladePrevAngle + State->DirectionSign * SweepDeltaDegrees;
			const FVector BladeDirection = FRotator(0.0f, BladeNowAngle, 0.0f).Vector();
			MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
				FMySkillDebugShape::MakeCircle(Origin, State->Radius, FColor::Red, 0.06f, 1.5f));
			MySkillDebugDraw::DrawShapeForLocalOwner(AvatarActor,
				FMySkillDebugShape::MakeLine(Origin, Origin + BladeDirection * State->Radius, FColor::Yellow, 0.06f, 3.0f));
		}

		if (State->bApplyGameplayEffects && SweepDeltaDegrees > 0.0f)
		{
			const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HeruWhirlSweep), false, AvatarActor);

			TArray<FOverlapResult> OverlapResults;
			World->OverlapMultiByObjectType(
				OverlapResults,
				Origin,
				FQuat::Identity,
				ObjectQueryParams,
				FCollisionShape::MakeSphere(State->Radius),
				QueryParams
			);

			TSet<AActor*> ProcessedTargets;
			for (const FOverlapResult& OverlapResult : OverlapResults)
			{
				AActor* TargetActor = OverlapResult.GetActor();
				if (!TargetActor || ProcessedTargets.Contains(TargetActor))
				{
					continue;
				}
				ProcessedTargets.Add(TargetActor);

				if (!UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
				{
					continue;
				}

				// 칼날 스윕 판정: 대상 방위각이 이번 구간 [BladePrev → BladePrev + Delta] (회전 방향 기준) 안에 있어야 맞는다.
				const FVector ToTarget = TargetActor->GetActorLocation() - Origin;
				const float TargetBearing = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
				const float AngleFromBlade = NormalizeDegrees(State->DirectionSign * (TargetBearing - BladePrevAngle));
				if (AngleFromBlade > SweepDeltaDegrees + KINDA_SMALL_NUMBER)
				{
					continue;
				}

				// 동일 대상 ICD: 한 바퀴 시간 안에는 다시 맞지 않는다.
				const float NowWorldTime = World->GetTimeSeconds();
				if (const float* LastHitTime = State->LastHitWorldTimeByTarget.Find(TargetActor))
				{
					if (NowWorldTime - *LastHitTime < State->RevolutionPeriod - 0.01f)
					{
						continue;
					}
				}

				ApplyWhirlHitToTarget(State, TargetActor);
				State->LastHitWorldTimeByTarget.Add(TargetActor, NowWorldTime);
			}
		}

		if (NowElapsed >= State->TotalDuration)
		{
			World->GetTimerManager().ClearTimer(State->SubTickTimerHandle);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief Heru Whirl 기본값을 초기화한다. 몽타주는 예측하되 판정/표식은 서버에서만 실행한다.
//! \param 없음
//! \return 없음
UMyGA_Heru_Whirl::UMyGA_Heru_Whirl()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 스킬 파이프라인으로 Whirl을 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \return 없음
void UMyGA_Heru_Whirl::ActivateAbility(
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
//! \brief 판정 반경과 필수 스킬 데이터를 검증한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 발동 가능하면 true
bool UMyGA_Heru_Whirl::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)ActorInfo;
	(void)TriggerEventData;

	if (SkillData.Targeting.Radius <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru whirl activation failed - targeting radius invalid. Radius: %.2f"),
			SkillData.Targeting.Radius);
		return false;
	}

	if (!SkillData.Effects.HitGameplayEffect || SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Heru whirl activation failed - hit GE or coefficient missing. HitGE: %s, Coefficient: %.2f"),
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient);
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief Shoot 시점에 서버 판정 러너 또는 소유 클라이언트 로컬 디버그 러너를 시작한다.
//!        두 러너는 어빌리티 수명과 분리되어 전체 회전을 완주한다.
//! \param ActorInfo Ability Actor 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param SkillData 현재 Ability에 대응하는 SkillDefinition 데이터
//! \return 없음
void UMyGA_Heru_Whirl::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	if (!ActorInfo)
	{
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const bool bApplyGameplayEffects = ActorInfo->IsNetAuthority();
	const bool bDrawDebugLocally = bDrawDebugWhirl && AvatarPawn && AvatarPawn->IsLocallyControlled();
	if (!bApplyGameplayEffects && !bDrawDebugLocally)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = bApplyGameplayEffects
		? (ActorInfo->AbilitySystemComponent.IsValid()
			? ActorInfo->AbilitySystemComponent.Get()
			: GetAbilitySystemComponentFromActorInfo())
		: nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || (bApplyGameplayEffects && !SourceASC) || !World)
	{
		return;
	}

	TSharedRef<FHeruWhirlSequenceState> State = MakeShared<FHeruWhirlSequenceState>();
	State->SourceASC = SourceASC;
	State->AvatarActor = AvatarActor;
	State->HitGameplayEffect = SkillData.Effects.HitGameplayEffect;
	State->MarkGameplayEffect = SkillData.Effects.StatusGameplayEffect;
	State->SkillCooldownTag = SkillData.CooldownTag;
	State->SkillInputTag = SkillData.InputTag;
	State->DamageCoefficient = SkillData.Effects.DamageCoefficient;
	State->Radius = SkillData.Targeting.Radius;
	State->RevolutionPeriod = SkillData.Timing.TickInterval > 0.0f ? SkillData.Timing.TickInterval : DefaultWhirlRevolutionPeriod;
	State->TotalDuration = State->RevolutionPeriod * FMath::Max(WhirlSpinCount, 1);
	State->StartWorldTime = World->GetTimeSeconds();
	State->StartAngleDegrees = NormalizeDegrees(AvatarActor->GetActorRotation().Yaw);
	State->DirectionSign = bReverseSpinDirection ? -1.0f : 1.0f;
	State->bApplyGameplayEffects = bApplyGameplayEffects;
	State->bDrawDebug = bDrawDebugLocally;

	TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimer(
		State->SubTickTimerHandle,
		[WeakWorld, State]()
		{
			ExecuteWhirlSubTick(WeakWorld, State);
		},
		FMath::Max(SweepSubTickInterval, 0.01f),
		true
	);

	UE_LOG(LogTemp, Log, TEXT("Heru whirl started - Avatar: %s, Radius: %.1f, Spins: %d, Period: %.2f, Duration: %.2f"),
		*GetNameSafe(AvatarActor),
		State->Radius,
		WhirlSpinCount,
		State->RevolutionPeriod,
		State->TotalDuration);
}
