////////////////////////////
//! \file MyGA_Inpu_BulwarkFissure.cpp
//! \brief Inpu의 방벽 균열 스킬 GameplayAbility를 구현한다.

#include "MyGA_Inpu_BulwarkFissure.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MySkillDebugShape.h"
#include "GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "GAS/SkillData/MySkillDefinitionFragment.h"
#include "MyGameplayTags.h"
#include "TimerManager.h"

namespace
{
	//! \brief 지면 Origin 산출용 하향 트레이스 길이(cm).
	constexpr float BulwarkFissureGroundTraceLength = 500.0f;

	////////////////////////////
	//! \struct FBulwarkFissureSequenceState
	//! \brief 어빌리티 수명과 분리되어 전방으로 전파하는 균열 판정 시퀀스 상태.
	struct FBulwarkFissureSequenceState
	{
		TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
		TWeakObjectPtr<AActor> AvatarActor;
		TSubclassOf<UGameplayEffect> HitGameplayEffect;
		FGameplayTag CooldownTag;
		FGameplayTag InputTag;
		float DamageCoefficient = 0.0f;

		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		float StartOffset = 0.0f;
		float Length = 0.0f;
		float StartHalfWidth = 0.0f;
		float EndHalfWidth = 0.0f;
		float TraceHalfHeight = 0.0f;

		float TotalDuration = 0.0f;
		float SubTickInterval = 0.05f;
		float StartWorldTime = 0.0f;
		float LastElapsed = 0.0f;

		float DebugLifeTime = 0.5f;
		bool bDrawDebug = false;

		//! \brief 이미 적중한 대상(전 전파 구간에 걸쳐 1회만 적중).
		TSet<TWeakObjectPtr<AActor>> HitActors;

		FTimerHandle SubTickTimerHandle;
	};

	//! \brief 경과 시간에 대응하는 균열 프론트의 Origin 기준 축거리(cm)를 반환한다.
	float ComputeFrontDistance(const FBulwarkFissureSequenceState& State, float Elapsed)
	{
		const float Alpha = State.TotalDuration > 0.0f ? FMath::Clamp(Elapsed / State.TotalDuration, 0.0f, 1.0f) : 1.0f;
		return State.StartOffset + State.Length * Alpha;
	}

	//! \brief 축거리 지점에서의 사다리꼴 반폭(cm)을 반환한다(시작 폭 → 끝 폭 선형 보간).
	float ComputeHalfWidthAtDistance(const FBulwarkFissureSequenceState& State, float AxialDistance)
	{
		if (State.Length <= 0.0f)
		{
			return State.StartHalfWidth;
		}
		const float Alpha = FMath::Clamp((AxialDistance - State.StartOffset) / State.Length, 0.0f, 1.0f);
		return FMath::Lerp(State.StartHalfWidth, State.EndHalfWidth, Alpha);
	}

	//! \brief 대상 1명에게 계수 피해를 적용한다(경직은 피해 반응으로 EnemyBase가 자동 처리).
	void ApplyFissureHitToTarget(const TSharedRef<FBulwarkFissureSequenceState>& State, AActor* TargetActor)
	{
		UAbilitySystemComponent* SourceASC = State->SourceASC.Get();
		if (!SourceASC || !TargetActor)
		{
			return;
		}

		UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			State->HitGameplayEffect,
			State->DamageCoefficient,
			State->CooldownTag,
			State->InputTag
		);
	}

	//! \brief 서브틱 1회: 이번 구간에서 새로 덮인 균열 띠 안의 적을 사다리꼴 판정으로 한 번씩 타격한다.
	void ExecuteBulwarkFissureSubTick(TWeakObjectPtr<UWorld> WeakWorld, TSharedRef<FBulwarkFissureSequenceState> State)
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
			World->GetTimerManager().ClearTimer(State->SubTickTimerHandle);
			return;
		}

		const float NowElapsed = FMath::Min(World->GetTimeSeconds() - State->StartWorldTime, State->TotalDuration);
		const float PrevElapsed = State->LastElapsed;
		State->LastElapsed = NowElapsed;

		const float FrontPrev = ComputeFrontDistance(*State, PrevElapsed);
		const float FrontNow = ComputeFrontDistance(*State, NowElapsed);

		if (FrontNow > FrontPrev + KINDA_SMALL_NUMBER)
		{
			const float SegmentHalfLength = (FrontNow - FrontPrev) * 0.5f;
			const float SegmentMaxHalfWidth = ComputeHalfWidthAtDistance(*State, FrontNow);
			const FVector SegmentCenter = State->Origin + State->Direction * ((FrontPrev + FrontNow) * 0.5f);
			const FQuat SegmentRotation = State->Direction.ToOrientationQuat();

			// 구간 최대 폭으로 넉넉히 질의한 뒤, 사다리꼴 taper는 축거리별 반폭 후필터로 정확히 만든다.
			const FVector BoxExtent(SegmentHalfLength + 1.0f, SegmentMaxHalfWidth, State->TraceHalfHeight);

			TArray<FOverlapResult> OverlapResults;
			const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InpuBulwarkFissure), false, AvatarActor);
			World->OverlapMultiByObjectType(
				OverlapResults,
				SegmentCenter,
				SegmentRotation,
				ObjectQueryParams,
				FCollisionShape::MakeBox(BoxExtent),
				QueryParams
			);

			const float MaxAxial = FMath::Min(FrontNow, State->StartOffset + State->Length);
			for (const FOverlapResult& OverlapResult : OverlapResults)
			{
				AActor* TargetActor = OverlapResult.GetActor();
				const TWeakObjectPtr<AActor> WeakTarget(TargetActor);
				if (!TargetActor || State->HitActors.Contains(WeakTarget) || !UMyAbilitySystemLibrary::IsHostile(AvatarActor, TargetActor))
				{
					continue;
				}

				// 사다리꼴 후필터: Origin 기준 축거리(axial)와 측거리(lateral)로 실제 균열 폭 안인지 판정한다.
				const FVector ToTarget = TargetActor->GetActorLocation() - State->Origin;
				const FVector ToTarget2D(ToTarget.X, ToTarget.Y, 0.0f);
				const float Axial = FVector::DotProduct(ToTarget2D, State->Direction);
				const float Lateral = (ToTarget2D - State->Direction * Axial).Size();
				if (Axial < State->StartOffset || Axial > MaxAxial || Lateral > ComputeHalfWidthAtDistance(*State, Axial))
				{
					continue;
				}

				State->HitActors.Add(WeakTarget);
				ApplyFissureHitToTarget(State, TargetActor);
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (State->bDrawDebug)
			{
				MySkillDebugDraw::DrawShapeForOwner(AvatarActor,
					FMySkillDebugShape::MakeBox(SegmentCenter, BoxExtent, State->Direction, FColor::Red, State->DebugLifeTime, 2.0f));
			}
#endif
		}

		if (NowElapsed >= State->TotalDuration)
		{
			World->GetTimerManager().ClearTimer(State->SubTickTimerHandle);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief Bulwark Fissure 기본값을 초기화한다. 몽타주는 로컬 예측하고 판정과 효과는 서버에서만 수행한다.
//! \param 없음
//! \return 없음
UMyGA_Inpu_BulwarkFissure::UMyGA_Inpu_BulwarkFissure()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief 표준 SkillDefinition 스킬 파이프라인으로 Bulwark Fissure를 활성화한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \return 없음
void UMyGA_Inpu_BulwarkFissure::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	CachedFissureDirection = FVector::ZeroVector;
	ActivateStandardSkill(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

////////////////////////////
//! \author HanUl
//! \brief Definition 필수값·Fragment·MouseCursor 조준 정책을 검증하고 마우스 방향을 확정한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param SkillData 현재 Bulwark Fissure SkillDefinition 데이터
//! \return 발동 가능한 데이터와 방향이면 true
bool UMyGA_Inpu_BulwarkFissure::CanActivateStandardSkill(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	const UMySkillDefinitionDataAsset* SkillDefinition = GetActiveSkillDefinition();
	if (!SkillDefinition)
	{
		return false;
	}

	if (!SkillDefinition->GetAnimation().Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark Fissure activation failed - LocalPredicted skill requires a montage. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	const UMyBulwarkFissureFragment* Fragment = SkillDefinition->FindFragment<UMyBulwarkFissureFragment>();
	if (!Fragment)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark Fissure activation failed - UMyBulwarkFissureFragment missing. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (SkillData.Targeting.Range <= 0.0f
		|| SkillData.Targeting.Width <= 0.0f
		|| SkillData.Timing.ActiveDuration <= 0.0f
		|| !SkillData.Effects.HitGameplayEffect
		|| SkillData.Effects.DamageCoefficient <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark Fissure activation failed - Definition tuning invalid. Range: %.1f, Width: %.1f, Duration: %.2f, HitGE: %s, Coefficient: %.2f"),
			SkillData.Targeting.Range,
			SkillData.Targeting.Width,
			SkillData.Timing.ActiveDuration,
			*GetNameSafe(SkillData.Effects.HitGameplayEffect),
			SkillData.Effects.DamageCoefficient);
		return false;
	}

	if (SkillData.Input.AimSource != EMySkillAimSource::MouseCursor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark Fissure activation failed - Input.AimSource must be MouseCursor. Definition: %s"),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	if (!ResolveFissureDirection(ActorInfo, TriggerEventData, CachedFissureDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("Inpu Bulwark Fissure activation failed - aim direction missing. Avatar: %s"),
			ActorInfo && ActorInfo->AvatarActor.IsValid() ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"));
		return false;
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 몽타주 Shoot 시점에 서버 권한에서만 지면 균열 전파 러너와 슬램 GameplayCue를 시작한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param SkillData 현재 Bulwark Fissure SkillDefinition 데이터
//! \return 없음
void UMyGA_Inpu_BulwarkFissure::OnStandardSkillShoot(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillDataEntry& SkillData
)
{
	(void)TriggerEventData;

	// 판정·효과·Cue는 서버 권한에서만 수행한다. 소유 클라이언트는 몽타주만 로컬 예측한다.
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
	const UMyBulwarkFissureFragment* Fragment = SkillDefinition ? SkillDefinition->FindFragment<UMyBulwarkFissureFragment>() : nullptr;
	if (!AvatarActor || !World || !SourceASC || !Fragment || CachedFissureDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Origin = ResolveGroundOrigin(ActorInfo);

	TSharedRef<FBulwarkFissureSequenceState> State = MakeShared<FBulwarkFissureSequenceState>();
	State->SourceASC = SourceASC;
	State->AvatarActor = AvatarActor;
	State->HitGameplayEffect = SkillData.Effects.HitGameplayEffect;
	State->CooldownTag = SkillData.CooldownTag;
	State->InputTag = SkillData.InputTag;
	State->DamageCoefficient = SkillData.Effects.DamageCoefficient;
	State->Origin = Origin;
	State->Direction = CachedFissureDirection;
	State->StartOffset = Fragment->GetStartForwardOffset();
	State->Length = SkillData.Targeting.Range;
	State->StartHalfWidth = SkillData.Targeting.Width * 0.5f;
	State->EndHalfWidth = Fragment->GetEndWidth() * 0.5f;
	State->TraceHalfHeight = Fragment->GetTraceHeight();
	State->TotalDuration = SkillData.Timing.ActiveDuration;
	State->SubTickInterval = Fragment->GetSubTickInterval();
	State->StartWorldTime = World->GetTimeSeconds();
	State->LastElapsed = 0.0f;
	State->DebugLifeTime = DebugShapeLifeTime;
	State->bDrawDebug = bDrawDebugBulwarkFissure;

	// 슬램 GameplayCue(균열 시각·사운드). 서버 실행이 전 클라이언트로 복제된다.
	FGameplayCueParameters CueParameters;
	CueParameters.Location = Origin;
	CueParameters.Normal = CachedFissureDirection;
	CueParameters.Instigator = AvatarActor;
	CueParameters.EffectCauser = AvatarActor;
	SourceASC->ExecuteGameplayCue(MyGameplayTags::GameplayCue_Ability_Inpu_BulwarkFissure, CueParameters);

	TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimer(
		State->SubTickTimerHandle,
		[WeakWorld, State]()
		{
			ExecuteBulwarkFissureSubTick(WeakWorld, State);
		},
		State->SubTickInterval,
		true
	);

	UE_LOG(LogTemp, Log, TEXT("Inpu Bulwark Fissure started - Avatar: %s, Length: %.1f, StartW: %.1f, EndW: %.1f, Duration: %.2f, Origin: %s"),
		*GetNameSafe(AvatarActor),
		State->Length,
		State->StartHalfWidth * 2.0f,
		State->EndHalfWidth * 2.0f,
		State->TotalDuration,
		*Origin.ToCompactString());
}

////////////////////////////
//! \author HanUl
//! \brief TargetData의 마우스 월드 지점을 사용하고, 지점이 없을 때만 현재 전방으로 폴백한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 조준 데이터
//! \param OutDirection 확정된 수평 균열 방향
//! \return 유효한 방향을 얻었으면 true
bool UMyGA_Inpu_BulwarkFissure::ResolveFissureDirection(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	FVector& OutDirection
) const
{
	OutDirection = FVector::ZeroVector;
	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return false;
	}

	if (TriggerEventData)
	{
		for (int32 Index = 0; Index < TriggerEventData->TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(Index);
			const FHitResult* HitResult = TargetData && TargetData->HasHitResult() ? TargetData->GetHitResult() : nullptr;
			if (!HitResult || !HitResult->bBlockingHit)
			{
				continue;
			}

			OutDirection = (HitResult->Location - AvatarActor->GetActorLocation()).GetSafeNormal2D();
			if (!OutDirection.IsNearlyZero())
			{
				return true;
			}
		}

		// CanActivateStandardSkill에서 MouseCursor 정책을 보장하므로 0도 유효한 월드 Yaw다.
		OutDirection = FRotator(0.0f, TriggerEventData->EventMagnitude, 0.0f).Vector().GetSafeNormal2D();
		if (!OutDirection.IsNearlyZero())
		{
			return true;
		}
	}

	OutDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief Avatar 발밑을 기준으로 하향 트레이스하여 균열 시작 지면점을 산출한다(실패 시 캡슐 바닥).
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 균열 전파의 기준 Origin(지면 위 지점)
FVector UMyGA_Inpu_BulwarkFissure::ResolveGroundOrigin(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return FVector::ZeroVector;
	}

	FVector Origin = AvatarActor->GetActorLocation();
	float CapsuleHalfHeight = 0.0f;
	if (const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor))
	{
		if (const UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
		{
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	if (UWorld* World = AvatarActor->GetWorld())
	{
		const FVector TraceStart = Origin;
		const FVector TraceEnd = Origin - FVector(0.0f, 0.0f, CapsuleHalfHeight + BulwarkFissureGroundTraceLength);
		FHitResult GroundHit;
		FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(InpuBulwarkFissureGround), false, AvatarActor);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundParams) && GroundHit.bBlockingHit)
		{
			Origin.Z = GroundHit.Location.Z;
			return Origin;
		}
	}

	Origin.Z -= CapsuleHalfHeight;
	return Origin;
}
