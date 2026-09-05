// Fill out your copyright notice in the Description page of Project Settings.

#include "MyGA_Nefer_MoveAbility.h"
#include "../MyAttributeSet.h"
#include "../SkillData/MySkillDefinitionDataAsset.h"
#include "../../Player/Components/PlayerMovementComponent.h"
#include "../../MyGameplayTags.h"

#include "Animation/AnimMontage.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameplayPrediction.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

namespace
{
	constexpr float MoveDashInputMaxAgeSeconds = 0.25f;
}

UMyGA_Nefer_MoveAbility::UMyGA_Nefer_MoveAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

////////////////////////////
//! \author HanUl
//! \brief MoveCharge가 남아 있을 때만 MoveAbility 발동을 허용한다.
//! \param Handle GAS가 전달한 AbilitySpecHandle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param SourceTags Source GameplayTag 목록
//! \param TargetTags Target GameplayTag 목록
//! \param OptionalRelevantTags 실패 사유 태그를 담을 선택적 컨테이너
//! \return 발동 가능하면 true, 충전 수가 없거나 기본 조건이 실패하면 false
bool UMyGA_Nefer_MoveAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags
) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UMySkillDefinitionDataAsset* SkillDefinition = nullptr;
	FMySkillMovementSpec MovementSpec;
	FMySkillTimingSpec TimingSpec;
	if (!TryGetMoveDefinitionData(ActorInfo, SkillDefinition, MovementSpec, TimingSpec))
	{
		return false;
	}

	const bool bHasMoveCharge = HasMoveCharge(ActorInfo);
	if (!bHasMoveCharge)
	{
		UE_LOG(LogTemp, Log, TEXT("사용불가 충전 안됨"));
	}

	return bHasMoveCharge;
}


////////////////////////////
//! \author HanUl
//! \editor 준혁 - 대쉬 방향을 활성화 시점 TriggerEventData로 클라/서버 동기 전달(지연 TargetData 복제 제거, 예측 정합 유지)
//! \brief MoveAbility를 발동하고 권한 인스턴스에서 MoveCharge를 1개 소모한 뒤 재충전 타이머를 시작한다. -> 실제 Ability 실행.
//! \param Handle GAS가 전달한 AbilitySpecHandle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 발동 컨텍스트
//! \param TriggerEventData 대쉬 방향 TargetData가 담긴 GameplayEvent 데이터
//! \return 없음
void UMyGA_Nefer_MoveAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UMySkillDefinitionDataAsset* SkillDefinition = nullptr;
	FMySkillMovementSpec MovementSpec;
	FMySkillTimingSpec TimingSpec;
	if (!TryGetMoveDefinitionData(ActorInfo, SkillDefinition, MovementSpec, TimingSpec))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!HasMoveCharge(ActorInfo))
	{
		UE_LOG(LogTemp, Log, TEXT("사용불가 충전 안됨"));

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 준혁 수정 : 대쉬 방향은 예측 클라이언트가 입력 시점에 확정해 활성화 EventData(TargetData)로 실어 보내고,
	//            서버는 ServerTryActivateAbilityWithEventData로 활성화와 동시에 같은 방향을 받는다.
	//            따라서 로컬/서버 경로를 나누지 않고 양쪽 모두 여기서 동일한 방향을 확정한다.
	//            (정책 RequireMoveInput에서 방향이 없으면 클라·서버 모두 동일하게 발동 실패)
	FVector DashDirection = FVector::ZeroVector;
	if (!ResolveDashDirection(ActorInfo, TriggerEventData, MovementSpec, DashDirection))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplySkillInputBlock(ActorInfo);

	bDashMovementEndHandled = false;

	BeginDash(Handle, ActorInfo, ActivationInfo, SkillDefinition, MovementSpec, TimingSpec, DashDirection);
}


////////////////////////////
//! \author 준혁
//! \brief Ability 종료 시 대쉬 동안 변경한 방향 고정, Pawn 충돌, 몽타주 루트모션 상태를 복원한다.
//! \param Handle Ability Spec Handle
//! \param ActorInfo Ability Actor 정보
//! \param ActivationInfo Ability 활성화 정보
//! \param bReplicateEndAbility 종료 복제 여부
//! \param bWasCancelled 취소 종료 여부
//! \return 없음
void UMyGA_Nefer_MoveAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	StopDashTrailCue(ActorInfo);
	RestoreDashMovementState(ActorInfo);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


////////////////////////////
//! \author 준혁
//! \brief 대쉬 방향이 확정된 뒤 권한 인스턴스에서 MoveCharge를 소모하고 RootMotion 이동과 몽타주를 시작한다.
//! \param Handle GAS가 전달한 AbilitySpecHandle
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param ActivationInfo Ability 발동 컨텍스트
//! \param SkillDefinition 현재 MoveAbility에 연결된 SkillDefinition
//! \param MovementSpec SkillDefinition에 설정된 이동 스킬 데이터
//! \param TimingSpec SkillDefinition에 설정된 대쉬 타이밍 데이터
//! \param DashDirection 확정된 대쉬 방향
//! \return 없음
void UMyGA_Nefer_MoveAbility::BeginDash(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const UMySkillDefinitionDataAsset* SkillDefinition,
	const FMySkillMovementSpec& MovementSpec,
	const FMySkillTimingSpec& TimingSpec,
	const FVector& DashDirection
)
{
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (!ConsumeMoveCharge(ActorInfo))
		{
			UE_LOG(LogTemp, Log, TEXT("사용불가 충전 안됨"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		StartMoveRecharge(ActorInfo, SkillDefinition, MovementSpec);
	}

	ApplyDashMovementState(ActorInfo, DashDirection);

	UAbilityTask_ApplyRootMotionConstantForce* DashMovementTask = CreateDashMovementTask(ActorInfo, MovementSpec, TimingSpec, DashDirection);
	if (!DashMovementTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("발동됨"));

	const FMySkillAnimationSpec* AnimationSpec = SkillDefinition ? &SkillDefinition->GetAnimation() : nullptr;
	UAnimMontage* DashMontageToPlay = AnimationSpec ? AnimationSpec->Montage.Get() : nullptr;

	// Animation Montage
	if(DashMontageToPlay)
	{
		const float PlayRate = AnimationSpec ? AnimationSpec->PlayRate : 1.0f;
		const FName StartSectionName = AnimationSpec ? AnimationSpec->StartSectionName : NAME_None;
		const bool bStopWhenAbilityEnds = AnimationSpec ? AnimationSpec->bStopWhenAbilityEnds : true;

		UAbilityTask_PlayMontageAndWait* Task =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				NAME_None,
				DashMontageToPlay,
				PlayRate,
				StartSectionName,
				bStopWhenAbilityEnds
			);

		if(!Task) // Task 없음
		{
			UE_LOG(LogTemp, Log, TEXT("Task null"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		Task->OnInterrupted.AddDynamic(this, &UMyGA_Nefer_MoveAbility::HandleDashMontageInterrupted);
		Task->OnCancelled.AddDynamic  (this, &UMyGA_Nefer_MoveAbility::HandleDashMontageInterrupted);

		StartDashTrailCue(ActorInfo);
		DashMovementTask->ReadyForActivation();
		Task->ReadyForActivation();

		return;
	}

	StartDashTrailCue(ActorInfo);
	DashMovementTask->ReadyForActivation();
}


////////////////////////////
//! \author HanUl
//! \brief 실제 Dash 이동 시작 직전에 지속형 잔상 GameplayCue를 예측 키와 함께 활성화한다.
//! \param ActorInfo Ability 소유자와 ASC 정보
//! \return 없음
void UMyGA_Nefer_MoveAbility::StartDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (bDashTrailCueActive)
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

	ASC->AddGameplayCue(MyGameplayTags::GameplayCue_Ability_Move_DashTrail, ASC->MakeEffectContext());
	bDashTrailCueActive = true;
}


////////////////////////////
//! \author HanUl
//! \brief 정상 종료·취소·예측 거부를 포함한 모든 Ability 종료 경로에서 잔상 GameplayCue를 제거한다.
//! \param ActorInfo Ability 소유자와 ASC 정보
//! \return 없음
void UMyGA_Nefer_MoveAbility::StopDashTrailCue(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bDashTrailCueActive)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent.Get()
		: GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveGameplayCue(MyGameplayTags::GameplayCue_Ability_Move_DashTrail);
	}

	bDashTrailCueActive = false;
}

////////////////////////////
//! \author HanUl
//! \brief 대쉬 방향으로 바라보기를 고정하고 Pawn 충돌을 무시하며 몽타주 루트모션 이동을 비활성화한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param DashDirection 확정된 대쉬 방향
//! \return 없음
void UMyGA_Nefer_MoveAbility::ApplyDashMovementState(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FVector& DashDirection
)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!AvatarCharacter || DashDirection.IsNearlyZero())
	{
		return;
	}

	if (UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
	{
		CachedPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (UPlayerMovementComponent* PlayerMovementComponent = AvatarCharacter->FindComponentByClass<UPlayerMovementComponent>())
	{
		PlayerMovementComponent->LockFacingYaw(DashDirection.Rotation().Yaw);
	}

	// 대쉬 이동은 예측되는 ConstantForce만 사용한다. 몽타주 루트모션 이동이 합산되면
	// 클라이언트 예측과 서버 결과가 어긋날 수 있으므로 대쉬 동안 이동 스케일을 0으로 유지한다.
	AvatarCharacter->SetAnimRootMotionTranslationScale(0.0f);
	bDashMovementStateModified = true;
}

////////////////////////////
//! \author HanUl
//! \brief 정상 종료·취소·예측 거부 시 대쉬가 변경한 방향 고정, Pawn 충돌, 루트모션 상태를 복원한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 없음
void UMyGA_Nefer_MoveAbility::RestoreDashMovementState(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!bDashMovementStateModified)
	{
		return;
	}

	bDashMovementStateModified = false;

	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!AvatarCharacter)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = AvatarCharacter->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, CachedPawnCollisionResponse);
	}

	if (UPlayerMovementComponent* PlayerMovementComponent = AvatarCharacter->FindComponentByClass<UPlayerMovementComponent>())
	{
		PlayerMovementComponent->UnlockFacingYaw();
	}

	AvatarCharacter->SetAnimRootMotionTranslationScale(1.0f);
}


////////////////////////////
//! \author HanUl
//! \brief SourceObject의 SkillDefinition에서 MoveAbility 실행에 필요한 이동 데이터를 읽고 검증한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param OutSkillDefinition 조회된 SkillDefinition DataAsset
//! \param OutMovementSpec SkillDefinition에 설정된 이동 스킬 데이터
//! \param OutTimingSpec SkillDefinition에 설정된 대쉬 타이밍 데이터
//! \return MoveAbility 실행에 필요한 Definition 데이터가 모두 유효하면 true
bool UMyGA_Nefer_MoveAbility::TryGetMoveDefinitionData(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UMySkillDefinitionDataAsset*& OutSkillDefinition,
	FMySkillMovementSpec& OutMovementSpec,
	FMySkillTimingSpec& OutTimingSpec
) const
{
	OutSkillDefinition = nullptr;
	OutMovementSpec = FMySkillMovementSpec();
	OutTimingSpec = FMySkillTimingSpec();

	const UMySkillDefinitionDataAsset* SkillDefinition = GetSkillDefinitionDataAssetFromActorInfo(ActorInfo);
	if (!SkillDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - SkillDefinition is missing. Ability: %s, SourceObject: %s"),
			*GetNameSafe(this),
			*GetNameSafe(GetCurrentSourceObject()));
		return false;
	}

	if (!SkillDefinition->IsValidDefinition())
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - SkillDefinition is invalid. Ability: %s, SkillDefinition: %s"),
			*GetNameSafe(this),
			*GetNameSafe(SkillDefinition));
		return false;
	}

	OutMovementSpec = SkillDefinition->GetMovement();
	OutTimingSpec = SkillDefinition->GetTiming();
	if (OutMovementSpec.DashStrength <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - Definition Movement.DashStrength must be greater than 0. SkillDefinition: %s, SkillId: %s"),
			*GetNameSafe(SkillDefinition),
			*SkillDefinition->GetSkillId().ToString());
		return false;
	}

	if (OutMovementSpec.RechargeSeconds <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - Definition Movement.RechargeSeconds must be greater than 0. SkillDefinition: %s, SkillId: %s"),
			*GetNameSafe(SkillDefinition),
			*SkillDefinition->GetSkillId().ToString());
		return false;
	}

	if (OutTimingSpec.ActiveDuration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - Definition Timing.ActiveDuration must be greater than 0. SkillDefinition: %s, SkillId: %s"),
			*GetNameSafe(SkillDefinition),
			*SkillDefinition->GetSkillId().ToString());
		return false;
	}

	if (!SkillDefinition->GetCooldownTag().IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility activation failed - Definition CooldownTag is required as recharge state tag. SkillDefinition: %s, SkillId: %s"),
			*GetNameSafe(SkillDefinition),
			*SkillDefinition->GetSkillId().ToString());
		return false;
	}

	OutSkillDefinition = SkillDefinition;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 이벤트와 정책을 기준으로 대쉬에 사용할 최종 방향을 확정한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param MovementSpec SkillDefinition에 설정된 이동 스킬 데이터
//! \param OutDashDirection 확정된 대쉬 방향
//! \return 대쉬 방향을 확정했으면 true
bool UMyGA_Nefer_MoveAbility::ResolveDashDirection(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData,
	const FMySkillMovementSpec& MovementSpec,
	FVector& OutDashDirection
) const
{
	OutDashDirection = FVector::ZeroVector;
	if (TryGetDashDirectionFromEventData(TriggerEventData, OutDashDirection))
	{
		return true;
	}

	AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	const ACharacter* AvatarCharacter = Cast<ACharacter>(AvatarActor);
	const UPlayerMovementComponent* PlayerMovementComponent = AvatarCharacter
		? AvatarCharacter->FindComponentByClass<UPlayerMovementComponent>()
		: nullptr;
	if (PlayerMovementComponent && PlayerMovementComponent->GetLastMoveInputDirection(MoveDashInputMaxAgeSeconds, OutDashDirection))
	{
		OutDashDirection = OutDashDirection.GetSafeNormal2D();
		if (!OutDashDirection.IsNearlyZero())
		{
			return true;
		}
	}

	if (MovementSpec.DashDirectionPolicy == EMyDashDirectionPolicy::UseFacingWhenMoveInputMissing
		&& TryGetFacingDashDirection(AvatarActor, OutDashDirection))
	{
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("MoveAbility dash failed - dash direction is missing. Avatar: %s, Policy: %d, TriggerEventData: %s"),
		*GetNameSafe(AvatarActor),
		static_cast<int32>(MovementSpec.DashDirectionPolicy),
		TriggerEventData ? TEXT("true") : TEXT("false"));
	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 GameplayEvent TargetData에서 클라이언트가 보낸 대쉬 방향을 읽는다.
//! \param TriggerEventData 입력 시점 GameplayEvent 데이터
//! \param OutDashDirection 입력 시점의 대쉬 방향
//! \return 입력 이벤트에서 대쉬 방향을 얻었으면 true
bool UMyGA_Nefer_MoveAbility::TryGetDashDirectionFromEventData(const FGameplayEventData* TriggerEventData, FVector& OutDashDirection) const
{
	OutDashDirection = FVector::ZeroVector;
	if (!TriggerEventData || TriggerEventData->TargetData.Num() <= 0)
	{
		return false;
	}

	for (int32 TargetDataIndex = 0; TargetDataIndex < TriggerEventData->TargetData.Num(); ++TargetDataIndex)
	{
		const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(TargetDataIndex);
		if (!TargetData)
		{
			continue;
		}

		FVector CandidateDirection = FVector::ZeroVector;
		if (TargetData->HasHitResult())
		{
			const FHitResult* HitResult = TargetData->GetHitResult();
			if (HitResult && !HitResult->bBlockingHit)
			{
				CandidateDirection = HitResult->TraceEnd;
				if (CandidateDirection.IsNearlyZero())
				{
					CandidateDirection = HitResult->ImpactPoint.IsNearlyZero() ? HitResult->Location : HitResult->ImpactPoint;
				}
			}
		}
		else if (TargetData->HasEndPoint())
		{
			CandidateDirection = TargetData->GetEndPoint();
		}

		CandidateDirection = CandidateDirection.GetSafeNormal2D();
		if (!CandidateDirection.IsNearlyZero())
		{
			OutDashDirection = CandidateDirection;
			return true;
		}
	}

	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 캐릭터가 현재 바라보는 방향을 대쉬 방향으로 변환한다.
//! \param AvatarActor 대쉬를 수행할 Avatar Actor
//! \param OutDashDirection 캐릭터가 바라보는 방향
//! \return 바라보는 방향을 얻었으면 true
bool UMyGA_Nefer_MoveAbility::TryGetFacingDashDirection(const AActor* AvatarActor, FVector& OutDashDirection) const
{
	OutDashDirection = FVector::ZeroVector;
	if (!AvatarActor)
	{
		return false;
	}

	OutDashDirection = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
	return !OutDashDirection.IsNearlyZero();
}

////////////////////////////
//! \author HanUl
//! \brief RootMotionSource 기반 대쉬 이동 AbilityTask를 생성한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param MovementSpec SkillDefinition에 설정된 이동 스킬 데이터
//! \param TimingSpec SkillDefinition에 설정된 대쉬 타이밍 데이터
//! \param DashDirection 확정된 대쉬 방향
//! \return 생성된 대쉬 이동 AbilityTask, 생성 실패 시 nullptr
UAbilityTask_ApplyRootMotionConstantForce* UMyGA_Nefer_MoveAbility::CreateDashMovementTask(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FMySkillMovementSpec& MovementSpec,
	const FMySkillTimingSpec& TimingSpec,
	const FVector& DashDirection
)
{
	AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!AvatarActor)
	{
		return nullptr;
	}

	if (DashDirection.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveAbility dash failed - resolved dash direction is missing. Avatar: %s"),
			*GetNameSafe(AvatarActor));
		return nullptr;
	}

	UAbilityTask_ApplyRootMotionConstantForce* DashMovementTask =
		UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("MoveDash"),
			DashDirection,
			MovementSpec.DashStrength,
			TimingSpec.ActiveDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.0f,
			true
		);

	if (!DashMovementTask)
	{
		return nullptr;
	}

	DashMovementTask->OnFinish.AddDynamic(this, &UMyGA_Nefer_MoveAbility::HandleDashMovementFinished);

	UE_LOG(LogTemp, Log, TEXT("MoveAbility dash task created - Avatar: %s, Direction: %s, Strength: %.2f, Duration: %.2f, Distance: %.2f"),
		*GetNameSafe(AvatarActor),
		*DashDirection.ToCompactString(),
		MovementSpec.DashStrength,
		TimingSpec.ActiveDuration,
		MovementSpec.DashStrength * TimingSpec.ActiveDuration);

	return DashMovementTask;
}



////////////////////////////
//! \author HanUl
//! \brief MoveCharge가 1개 이상 남아 있는지 확인한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return MoveCharge가 0보다 크면 true
bool UMyGA_Nefer_MoveAbility::HasMoveCharge(const FGameplayAbilityActorInfo* ActorInfo) const
{

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!ASC)
	{
		return false;
	}

	return ASC->GetNumericAttribute(UMyAttributeSet::GetMoveChargeAttribute()) > 0.0f;

}



////////////////////////////
//! \author HanUl
//! \brief 서버 ASC의 MoveCharge를 1개 소모한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \return 소모에 성공하면 true
bool UMyGA_Nefer_MoveAbility::ConsumeMoveCharge(const FGameplayAbilityActorInfo* ActorInfo) const
{

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}

	const float CurrentCharge = ASC->GetNumericAttribute(UMyAttributeSet::GetMoveChargeAttribute());
	const float MaxCharge = ASC->GetNumericAttribute(UMyAttributeSet::GetMaxMoveChargeAttribute());
	if (CurrentCharge <= 0.0f)
	{
		return false;
	}

	ASC->SetNumericAttributeBase(
		UMyAttributeSet::GetMoveChargeAttribute(),
		FMath::Clamp(CurrentCharge - 1.0f, 0.0f, MaxCharge)
	);
	return true;
}



////////////////////////////
//! \author HanUl
//! \brief MoveCharge가 최대치보다 낮으면 다음 충전 타이머를 시작한다.
//! \param ActorInfo Ability 소유자와 Avatar 정보
//! \param SkillDefinition 현재 MoveAbility에 연결된 SkillDefinition
//! \param MovementSpec SkillDefinition에 설정된 이동 스킬 데이터
//! \return 없음
void UMyGA_Nefer_MoveAbility::StartMoveRecharge(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UMySkillDefinitionDataAsset* SkillDefinition,
	const FMySkillMovementSpec& MovementSpec
)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!ASC || !World)
	{
		UE_LOG(LogTemp, Log, TEXT("Move recharge not started - ASC: %s, World: %s, TimerActive: %s"),
			*GetNameSafe(ASC),
			*GetNameSafe(World),
			TEXT("false"));
		return;
	}

	const float CurrentCharge = ASC->GetNumericAttribute(UMyAttributeSet::GetMoveChargeAttribute());
	const float MaxCharge = ASC->GetNumericAttribute(UMyAttributeSet::GetMaxMoveChargeAttribute());
	if (CurrentCharge >= MaxCharge)
	{
		UE_LOG(LogTemp, Log, TEXT("Move recharge not started - already full. Current: %.0f, Max: %.0f"),
			CurrentCharge,
			MaxCharge);
		return;
	}

	const FGameplayTag RechargeStateTag = SkillDefinition ? SkillDefinition->GetCooldownTag() : FGameplayTag();
	if (!RechargeStateTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Move recharge not started - recharge state tag is invalid. SkillDefinition: %s"),
			*GetNameSafe(SkillDefinition));
		return;
	}

	if (ASC->HasMatchingGameplayTag(RechargeStateTag))
	{
		UE_LOG(LogTemp, Log, TEXT("Move recharge not started - already recharging. Current: %.0f, Max: %.0f"),
			CurrentCharge,
			MaxCharge);
		return;
	}

	ASC->AddLooseGameplayTag(RechargeStateTag);
	ScheduleMoveRecharge(ASC, MovementSpec.RechargeSeconds, RechargeStateTag);
}

////////////////////////////
//! \author HanUl
//! \brief Dash Montage가 중단되었을 때 MoveAbility를 취소 종료한다.
//! \param 없음
//! \return 없음
void UMyGA_Nefer_MoveAbility::HandleDashMontageInterrupted()
{
	if (bDashMovementEndHandled)
	{
		return;
	}

	bDashMovementEndHandled = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

////////////////////////////
//! \author HanUl
//! \brief RootMotionSource 대쉬 이동 시간이 끝났을 때 MoveAbility를 정상 종료한다.
//! \param 없음
//! \return 없음
void UMyGA_Nefer_MoveAbility::HandleDashMovementFinished()
{
	if (bDashMovementEndHandled)
	{
		return;
	}

	bDashMovementEndHandled = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}

////////////////////////////
//! \author HanUl
//! \brief ASC 약참조만 캡처해 MoveCharge 재충전 타이머를 예약한다.
//! \param WeakASC 재충전할 AbilitySystemComponent 약참조
//! \param BaseRechargeSeconds 기본 재충전 시간
//! \param RechargeStateTag 재충전 중 상태를 표시할 GameplayTag
//! \return 없음
void UMyGA_Nefer_MoveAbility::ScheduleMoveRecharge(
	TWeakObjectPtr<UAbilitySystemComponent> WeakASC,
	float BaseRechargeSeconds,
	FGameplayTag RechargeStateTag
)
{
	UAbilitySystemComponent* ASC = WeakASC.Get();
	AActor* AvatarActor = ASC ? ASC->GetAvatarActor() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!ASC || !World || !RechargeStateTag.IsValid())
	{
		return;
	}

	const float EffectiveRechargeSeconds = GetEffectiveRechargeSeconds(ASC, BaseRechargeSeconds);
	FTimerDelegate RechargeDelegate = FTimerDelegate::CreateLambda([WeakASC, BaseRechargeSeconds, RechargeStateTag]()
	{
		UAbilitySystemComponent* CapturedASC = WeakASC.Get();
		if (!CapturedASC)
		{
			return;
		}

		const float CurrentCharge = CapturedASC->GetNumericAttribute(UMyAttributeSet::GetMoveChargeAttribute());
		const float MaxCharge = CapturedASC->GetNumericAttribute(UMyAttributeSet::GetMaxMoveChargeAttribute());
		CapturedASC->SetNumericAttributeBase(
			UMyAttributeSet::GetMoveChargeAttribute(),
			FMath::Clamp(CurrentCharge + 1.0f, 0.0f, MaxCharge)
		);

		UE_LOG(LogTemp, Log, TEXT("충전됨1"));

		const float NewCharge = CapturedASC->GetNumericAttribute(UMyAttributeSet::GetMoveChargeAttribute());
		if (NewCharge >= MaxCharge)
		{
			CapturedASC->RemoveLooseGameplayTag(RechargeStateTag);
			return;
		}

		UMyGA_Nefer_MoveAbility::ScheduleMoveRecharge(WeakASC, BaseRechargeSeconds, RechargeStateTag);
	});

	FTimerHandle RechargeTimerHandle;
	World->GetTimerManager().SetTimer(RechargeTimerHandle, RechargeDelegate, EffectiveRechargeSeconds, false);

	UE_LOG(LogTemp, Log, TEXT("Move recharge started - Seconds: %.2f"), EffectiveRechargeSeconds);
}



////////////////////////////
//! \author HanUl
//! \brief ASC의 CooldownReduction을 반영한 MoveCharge 재충전 시간을 계산한다.
//! \param ASC 재충전 시간을 계산할 AbilitySystemComponent
//! \param BaseRechargeSeconds 기본 재충전 시간
//! \return 쿨타임 감소가 적용된 재충전 시간
float UMyGA_Nefer_MoveAbility::GetEffectiveRechargeSeconds(UAbilitySystemComponent* ASC, float BaseRechargeSeconds)
{
	const float CooldownReduction = ASC
		? FMath::Clamp(ASC->GetNumericAttribute(UMyAttributeSet::GetCooldownReductionAttribute()), 0.0f, 0.8f)
		: 0.0f;

	return FMath::Max(0.01f, BaseRechargeSeconds * (1.0f - CooldownReduction));
}
