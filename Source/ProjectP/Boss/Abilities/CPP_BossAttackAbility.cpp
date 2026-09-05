#include "CPP_BossAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Core/CPP_BossAttributeSet.h"
#include "Boss/Core/CPP_BossCharacter.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "Boss/Abilities/CPP_BossWindowEventPayload.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"

UCPP_BossAttackAbility::UCPP_BossAttackAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

////////////////////////////
//! \author HanSeul
//! \brief Validates boss attack data and starts the authored attack montage.
//! \param Handle Active ability spec handle.
//! \param ActorInfo Ability owner and avatar information.
//! \param ActivationInfo Ability activation replication information.
//! \param TriggerEventData Optional gameplay event payload used to activate the ability.
void UCPP_BossAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData
)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!HasAuthority(&ActivationInfo) || !AttackData || !AttackData->GetAttackMontage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveAttackWindowEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_AttackWindow,
		nullptr,
		false,
		true
	);

	if (!ActiveAttackWindowEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveAttackWindowEventTask->EventReceived.AddDynamic(this, &UCPP_BossAttackAbility::HandleAttackWindowEvent);
	ActiveAttackWindowEventTask->ReadyForActivation();

	ActiveTelegraphBeginEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_Telegraph_Begin,
		nullptr,
		false,
		true
	);

	if (!ActiveTelegraphBeginEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTelegraphBeginEventTask->EventReceived.AddDynamic(this, &UCPP_BossAttackAbility::HandleTelegraphBeginEvent);
	ActiveTelegraphBeginEventTask->ReadyForActivation();

	ActiveTelegraphEndEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		BossGameplayTags::Event_Boss_Telegraph_End,
		nullptr,
		false,
		true
	);

	if (!ActiveTelegraphEndEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveTelegraphEndEventTask->EventReceived.AddDynamic(this, &UCPP_BossAttackAbility::HandleTelegraphEndEvent);
	ActiveTelegraphEndEventTask->ReadyForActivation();

	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData->GetAttackMontage()
	);

	if (!ActiveMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMontageTask->OnCompleted.AddDynamic(this, &UCPP_BossAttackAbility::HandleMontageCompleted);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &UCPP_BossAttackAbility::HandleMontageInterrupted);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &UCPP_BossAttackAbility::HandleMontageCancelled);
	ActiveMontageTask->OnBlendOut.AddDynamic(this, &UCPP_BossAttackAbility::HandleMontageBlendOut);
	ActiveMontageTask->ReadyForActivation();
}

const FGameplayTagContainer* UCPP_BossAttackAbility::GetCooldownTags() const
{
	BossCooldownTags.Reset();

	if (CooldownTag.IsValid())
	{
		BossCooldownTags.AddTag(CooldownTag);
	}

	return &BossCooldownTags;
}

////////////////////////////
//! \author HanSeul
//! \brief Applies this boss ability's GAS cooldown effect with its dynamic cooldown tag and duration.
//! \param Handle Ability spec handle supplied by GAS.
//! \param ActorInfo Owner/avatar information supplied by GAS.
//! \param ActivationInfo Activation context supplied by GAS.
void UCPP_BossAttackAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo
) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !BossCooldownGameplayEffectClass || !CooldownTag.IsValid() || CooldownSeconds <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		BossCooldownGameplayEffectClass,
		GetAbilityLevel(Handle, ActorInfo)
	);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, CooldownSeconds);
	SpecHandle.Data->SetDuration(CooldownSeconds, true);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UCPP_BossAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled
)
{
	ClearActiveTelegraphs();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UCPP_BossAttackAbility::GetCooldownSeconds() const
{
	return CooldownSeconds;
}

////////////////////////////
//! \author HanSeul
//! \brief Finds the attack window requested by montage notify events.
//! \param WindowId Attack window identifier authored in the boss attack data asset.
//! \return true when the window exists and can be executed.
bool UCPP_BossAttackAbility::ExecuteAttackWindow(FName WindowId)
{
	if (!HasAuthority(&CurrentActivationInfo) || !AttackData || WindowId.IsNone())
	{
		return false;
	}

	const FBossAttackWindowData* AttackWindow = AttackData->FindAttackWindow(WindowId);
	if (!AttackWindow)
	{
		return false;
	}

	TSet<AActor*> HitTargets;
	CollectTargetsFromAttackWindow(*AttackWindow, HitTargets);
	return ApplyDamageToTargets(*AttackWindow, HitTargets);
}

void UCPP_BossAttackAbility::CollectTargetsFromAttackWindow(const FBossAttackWindowData& AttackWindow, TSet<AActor*>& OutTargets) const
{
	for (const FBossHitShapeData& HitShape : AttackWindow.HitShapes)
	{
		CollectTargetsFromHitShape(HitShape, OutTargets);
	}
}

void UCPP_BossAttackAbility::CollectTargetsFromHitShape(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	switch (HitShape.Shape)
	{
	case EBossAttackShape::Circle:
		CollectTargetsFromCircle(HitShape, OutTargets);
		break;
	case EBossAttackShape::Sector:
		CollectTargetsFromSector(HitShape, OutTargets);
		break;
	case EBossAttackShape::Rectangle:
		CollectTargetsFromRectangle(HitShape, OutTargets);
		break;
	default:
		break;
	}
}

void UCPP_BossAttackAbility::CollectTargetsFromCircle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.OuterRadius <= 0.0f)
	{
		return;
	}

	// Location + rotation only (no scale): boss actor scale must not affect attack offset or range.
	const FVector ShapeOrigin = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossAttackCircle), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const float HorizontalDistanceSquared = FVector(ToTarget.X, ToTarget.Y, 0.0f).SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;

		if (bInRadius && bInHeight)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

void UCPP_BossAttackAbility::CollectTargetsFromSector(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.OuterRadius <= 0.0f || HitShape.SectorAngleDegrees <= 0.0f)
	{
		return;
	}

	// Location + rotation only (no scale): boss actor scale must not affect attack offset or range.
	const FVector ShapeOrigin = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);
	const FRotator ShapeRotation(0.0f, AvatarActor->GetActorRotation().Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossAttackSector), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(HitShape.OuterRadius),
		QueryParams
	);

	const float InnerRadiusSquared = FMath::Square(HitShape.InnerRadius);
	const float OuterRadiusSquared = FMath::Square(HitShape.OuterRadius);
	const float HalfAngleDegrees = HitShape.SectorAngleDegrees * 0.5f;
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	const bool bFullCircle = HitShape.SectorAngleDegrees >= 360.0f;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - ShapeOrigin;
		const FVector HorizontalToTarget(ToTarget.X, ToTarget.Y, 0.0f);
		const float HorizontalDistanceSquared = HorizontalToTarget.SizeSquared();
		const bool bInRadius = HorizontalDistanceSquared >= InnerRadiusSquared && HorizontalDistanceSquared <= OuterRadiusSquared;
		const bool bInHeight = FMath::Abs(ToTarget.Z) <= HitShape.HalfHeight;
		const bool bInAngle = bFullCircle || (HorizontalDistanceSquared > KINDA_SMALL_NUMBER && FVector::DotProduct(ShapeForward, HorizontalToTarget.GetSafeNormal()) >= MinDot);

		if (bInRadius && bInHeight && bInAngle)
		{
			OutTargets.Add(TargetActor);
		}
	}
}

void UCPP_BossAttackAbility::CollectTargetsFromRectangle(const FBossHitShapeData& HitShape, TSet<AActor*>& OutTargets) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || HitShape.ForwardLength <= 0.0f || HitShape.HalfWidth <= 0.0f || HitShape.HalfHeight <= 0.0f)
	{
		return;
	}

	const FRotator ShapeRotation(0.0f, AvatarActor->GetActorRotation().Yaw + HitShape.LocalYawDegrees, 0.0f);
	const FVector ShapeForward = ShapeRotation.Vector();
	// Location + rotation only (no scale): boss actor scale must not affect attack offset or range.
	const FVector ShapeStart = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(HitShape.LocalOffset);
	const FVector ShapeCenter = ShapeStart + ShapeForward * (HitShape.ForwardLength * 0.5f);
	const FVector BoxExtent(HitShape.ForwardLength * 0.5f, HitShape.HalfWidth, HitShape.HalfHeight);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BossAttackRectangle), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ShapeCenter,
		ShapeRotation.Quaternion(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(BoxExtent),
		QueryParams
	);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (AActor* TargetActor = OverlapResult.GetActor())
		{
			OutTargets.Add(TargetActor);
		}
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Applies calculated boss attack damage to every target collected by one attack window.
//! \param AttackWindow Attack window data containing the damage coefficient.
//! \param Targets Unique target actors collected from the attack window shapes.
//! \return true when at least one target receives the damage GameplayEffect.
bool UCPP_BossAttackAbility::ApplyDamageToTargets(const FBossAttackWindowData& AttackWindow, const TSet<AActor*>& Targets) const
{
	if (Targets.IsEmpty())
	{
		return true;
	}

	const ACPP_BossCharacter* BossAvatar = Cast<ACPP_BossCharacter>(GetAvatarActorFromActorInfo());
	if (!BossAvatar || !BossAvatar->GetBossAttributeSet() || !BossAvatar->GetBossDamageGameplayEffect())
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC)
	{
		return false;
	}

	if (AttackWindow.DamageCoefficient <= 0.0f)
	{
		return false;
	}

	bool bAppliedAnyDamage = false;
	for (AActor* TargetActor : Targets)
	{
		if (!IsValid(TargetActor) || !UMyAbilitySystemLibrary::IsHostile(BossAvatar, TargetActor))
		{
			continue;
		}

		const bool bAppliedDamage = UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
			SourceASC,
			TargetActor,
			BossAvatar->GetBossDamageGameplayEffect(),
			AttackWindow.DamageCoefficient,
			1.0f,
			AttackWindow.CurseGaugeAmount
		);

		bAppliedAnyDamage |= bAppliedDamage;
	}

	return bAppliedAnyDamage;
}

void UCPP_BossAttackAbility::SpawnTelegraphsForWindow(FName WindowId, float TelegraphDuration)
{
	if (!AttackData || WindowId.IsNone())
	{
		return;
	}

	const FBossAttackWindowData* AttackWindow = AttackData->FindAttackWindow(WindowId);
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AttackWindow || !AvatarActor || !World)
	{
		return;
	}

	RemoveTelegraphsForWindow(WindowId);

	TSubclassOf<ACPP_BossTelegraphActor> TelegraphActorClass = AttackData->GetTelegraphActorClass();
	if (!TelegraphActorClass)
	{
		TelegraphActorClass = ACPP_BossTelegraphActor::StaticClass();
	}

	TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>& TelegraphActors = ActiveTelegraphActorsByWindow.FindOrAdd(WindowId);
	for (const FBossHitShapeData& HitShape : AttackWindow->HitShapes)
	{
		ACPP_BossTelegraphActor* TelegraphActor = World->SpawnActor<ACPP_BossTelegraphActor>(TelegraphActorClass);
		if (!TelegraphActor)
		{
			continue;
		}

		TelegraphActor->Initialize(AvatarActor->GetActorTransform(), HitShape, TelegraphDuration);
		TelegraphActors.Add(TelegraphActor);
	}
}

void UCPP_BossAttackAbility::RemoveTelegraphsForWindow(FName WindowId)
{
	TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>> TelegraphActors;
	if (!ActiveTelegraphActorsByWindow.RemoveAndCopyValue(WindowId, TelegraphActors))
	{
		return;
	}

	for (const TWeakObjectPtr<ACPP_BossTelegraphActor>& TelegraphActor : TelegraphActors)
	{
		if (TelegraphActor.IsValid())
		{
			TelegraphActor->Destroy();
		}
	}
}

void UCPP_BossAttackAbility::ClearActiveTelegraphs()
{
	for (const TPair<FName, TArray<TWeakObjectPtr<ACPP_BossTelegraphActor>>>& TelegraphPair : ActiveTelegraphActorsByWindow)
	{
		for (const TWeakObjectPtr<ACPP_BossTelegraphActor>& TelegraphActor : TelegraphPair.Value)
		{
			if (TelegraphActor.IsValid())
			{
				TelegraphActor->Destroy();
			}
		}
	}

	ActiveTelegraphActorsByWindow.Empty();
}

void UCPP_BossAttackAbility::HandleAttackWindowEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	ExecuteAttackWindow(WindowPayload->WindowId);
}

void UCPP_BossAttackAbility::HandleTelegraphBeginEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	SpawnTelegraphsForWindow(WindowPayload->WindowId, WindowPayload->TelegraphDuration);
}

void UCPP_BossAttackAbility::HandleTelegraphEndEvent(FGameplayEventData Payload)
{
	const UCPP_BossWindowEventPayload* WindowPayload = Cast<UCPP_BossWindowEventPayload>(Payload.OptionalObject);
	if (!WindowPayload)
	{
		return;
	}

	RemoveTelegraphsForWindow(WindowPayload->WindowId);
}

void UCPP_BossAttackAbility::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCPP_BossAttackAbility::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCPP_BossAttackAbility::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCPP_BossAttackAbility::HandleMontageBlendOut()
{
}
