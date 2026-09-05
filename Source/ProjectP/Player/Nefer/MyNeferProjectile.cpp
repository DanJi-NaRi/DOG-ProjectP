////////////////////////////
//! \page MyNeferProjectile.cpp

#include "MyNeferProjectile.h"

#include "../../GAS/MyAbilitySystemLibrary.h"
#include "../../GAS/MyAttributeSet.h"
#include "../../GAS/MySkillDebugShape.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"

AMyNeferProjectile::AMyNeferProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(ProjectileRadius);
	ConfigureCollisionForProjectile();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->UpdatedComponent = CollisionComponent;
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->ProjectileGravityScale = 0.0f;
}

void AMyNeferProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AMyNeferProjectile::HandleProjectileOverlap);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &AMyNeferProjectile::HandleProjectileStop);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 한슬이의 CPP_EnemyProjectileBase에서 모방했음. 단, 투사체가 타겟이 설정되지는 않아서 변수에는 없는 모습
void AMyNeferProjectile::InitializeProjectile(
	AActor* InSourceActor,
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
	float InRange,
	float InProjectileSpeed,
	float InProjectileRadius,
	float InDamageCoefficient,
	TSubclassOf<UGameplayEffect> InDecayGameplayEffectClass,
	float InDecayTickDamageCoefficient,
	float InKnockbackDistance
)
{
	SourceActor = InSourceActor;
	SourceASC = InSourceASC;
	DamageGameplayEffectClass = InDamageGameplayEffectClass;
	DecayGameplayEffectClass = InDecayGameplayEffectClass;

	Range = FMath::Max(InRange, 0.0f);
	InitialSpeed = FMath::Max(InProjectileSpeed, 0.0f);
	ProjectileRadius = FMath::Max(InProjectileRadius, 0.0f);
	DamageCoefficient = FMath::Max(InDamageCoefficient, 0.0f);
	DecayTickDamageCoefficient = FMath::Max(InDecayTickDamageCoefficient, 0.0f);
	KnockbackDistance = FMath::Max(InKnockbackDistance, 0.0f);
	LifeSeconds = InitialSpeed > 0.0f && Range > 0.0f ? Range / InitialSpeed : LifeSeconds;

	const FVector LaunchDirection = GetActorForwardVector().GetSafeNormal();
	if (HasAuthority())
	{
		Multicast_LaunchProjectileVisual(GetActorLocation(), LaunchDirection, InitialSpeed, ProjectileRadius, LifeSeconds);
		ForceNetUpdate();
		return;
	}

	ApplyProjectileLaunchVisual(GetActorLocation(), LaunchDirection, InitialSpeed, ProjectileRadius, LifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 적중 시 적중 지점에서 원형 범위로 폭발하도록 설정한다. 호출하지 않으면 폭발하지 않는다.
//! \param InExplosionRadius 폭발 판정 반경(cm). 0 이하이면 폭발하지 않는다
//! \param InExplosionDamageCoefficient 폭발 피해 계수(공격력에 곱해짐)
//! \return 없음
void AMyNeferProjectile::ConfigureExplosion(float InExplosionRadius, float InExplosionDamageCoefficient)
{
	ExplosionRadius = FMath::Max(InExplosionRadius, 0.0f);
	ExplosionDamageCoefficient = FMath::Max(InExplosionDamageCoefficient, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체가 첫 적중 이후 주변 대상에게 연쇄될 수 있도록 설정한다.
//! \param InMaxAdditionalTargets 첫 적중 이후 추가로 연쇄할 수 있는 대상 수
//! \param InSearchRadius 다음 대상 탐색 반경
//! \param bInHitEachTargetOnce 같은 대상을 한 번만 적중시킬지 여부
//! \return 없음
void AMyNeferProjectile::ConfigureChain(int32 InMaxAdditionalTargets, float InSearchRadius, bool bInHitEachTargetOnce)
{
	RemainingChainCount = FMath::Max(InMaxAdditionalTargets, 0);
	ChainSearchRadius = FMath::Max(InSearchRadius, 0.0f);
	bHitEachTargetOnce = bInHitEachTargetOnce;
	HitActors.Reset();
}

////////////////////////////
//! \author HanUl
//! \brief 대상이 특정 상태를 이미 갖고 있을 때 즉시 추가 피해를 주도록 설정한다.
//! \param InStatusTags 기존 상태로 인정할 GameplayTag 목록
//! \param InBonusDamageCoefficient 공격력에 곱할 추가 피해 계수
//! \return 없음
void AMyNeferProjectile::ConfigureExistingStatusBonus(const FGameplayTagContainer& InStatusTags, float InBonusDamageCoefficient)
{
	ExistingStatusTags = InStatusTags;
	ExistingStatusBonusDamageCoefficient = FMath::Max(InBonusDamageCoefficient, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 직접 적중 피해에 사용할 공격자 본인 카메라 피드백 등급을 스킬 입력에서 해석해 저장한다.
//! \param InSkillInputTag Basic/Q/E/R/C를 구분할 스킬 입력 태그
//! \return 없음
void AMyNeferProjectile::ConfigureAttackerHitCameraFeedback(FGameplayTag InSkillInputTag)
{
	AttackerHitCameraFeedbackTag = UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(InSkillInputTag);
}

void AMyNeferProjectile::ConfigureCollisionForProjectile()
{
	if (!CollisionComponent)
	{
		return;
	}

	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void AMyNeferProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (!bProjectileInitialized || !HasAuthority())
	{
		return;
	}

	FinishProjectile();
}

void AMyNeferProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,  
	const FHitResult& SweepResult
)
{
	if (!bProjectileInitialized || !HasAuthority() || !OtherActor || OtherActor == this || OtherActor == SourceActor)
	{
		return;
	}

	if (!UMyAbilitySystemLibrary::IsHostile(SourceActor, OtherActor))
	{
		return;
	}

	if (bHitEachTargetOnce && HasActorAlreadyBeenHit(OtherActor))
	{
		return;
	}

	if (ApplyDamageToActor(OtherActor))
	{
		HitActors.AddUnique(OtherActor);
		ApplyKnockbackToActor(OtherActor);

		// 폭발이 설정된 스킬만 적중 지점에서 원형 범위 추가 타격을 수행한다(미설정 스킬은 내부에서 즉시 반환).
		ApplyExplosionDamage(GetActorLocation());

		if (RemainingChainCount > 0 && ChainSearchRadius > 0.0f)
		{
			if (AActor* NextTarget = FindNextChainTarget())
			{
				ContinueChainToTarget(NextTarget);
				return;
			}
		}

		FinishProjectile();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 적중한 대상을 투사체 진행 방향으로 밀어낸다. 서버에서만 호출된다.
//! \param HitActor 밀어낼 대상 Actor
//! \return 없음
void AMyNeferProjectile::ApplyKnockbackToActor(AActor* HitActor) const
{
	if (KnockbackDistance <= 0.0f || !HitActor)
	{
		return;
	}

	// 이 적중의 피해로 죽은 대상은 넉백에서 제외한다.
	// 사망 시 래그돌이 켜지면 Launch 속도가 물리 초기 속도로 승계돼 시체가 날아간다.
	if (!UMyAbilitySystemLibrary::IsLivingPawn(HitActor))
	{
		return;
	}

	FVector KnockbackDirection = ProjectileMovementComponent
		? ProjectileMovementComponent->Velocity.GetSafeNormal2D()
		: FVector::ZeroVector;
	if (KnockbackDirection.IsNearlyZero())
	{
		KnockbackDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	if (KnockbackDirection.IsNearlyZero())
	{
		return;
	}

	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		HitCharacter->LaunchCharacter(KnockbackDirection * KnockbackDistance * KnockbackVelocityPerDistance, true, false);
		return;
	}

	HitActor->AddActorWorldOffset(KnockbackDirection * KnockbackDistance, true);
}

bool AMyNeferProjectile::ApplyDamageToActor(AActor* HitActor)
{
	if (!SourceASC || !DamageGameplayEffectClass || !HitActor || !UMyAbilitySystemLibrary::IsHostile(SourceActor, HitActor))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(HitActor);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const bool bHadExistingStatus = HasExistingStatus(TargetASC);
	if (!ApplyDamageSpecToTarget(TargetASC, DamageCoefficient, EffectContext))
	{
		return false;
	}

	if (bHadExistingStatus && ExistingStatusBonusDamageCoefficient > 0.0f)
	{
		ApplyDamageSpecToTarget(TargetASC, ExistingStatusBonusDamageCoefficient, EffectContext);
	}

	if (DecayGameplayEffectClass)
	{
		FGameplayEffectSpecHandle DecaySpecHandle = SourceASC->MakeOutgoingSpec(DecayGameplayEffectClass, 1.0f, EffectContext);
		if (DecaySpecHandle.IsValid())
		{
			// Decay 틱은 상태 GE라 완성값(Data.Damage)을 유지한다.
			const float SourceAttackPower = SourceASC->GetNumericAttribute(UMyAttributeSet::GetAttackPowerAttribute());
			const float DecayTickDamage = SourceAttackPower * DecayTickDamageCoefficient;
			UMyAbilitySystemLibrary::AssignSetByCallerDamage(DecaySpecHandle, DecayTickDamage);
			SourceASC->ApplyGameplayEffectSpecToTarget(*DecaySpecHandle.Data.Get(), TargetASC);
		}
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 적중 지점 기준 원형 범위의 적대 대상에게 폭발 피해를 적용한다. 직격 대상도 폭발 피해를 함께 받는다.
//!        ConfigureExplosion으로 반경이 설정된 스킬만 동작하며, 미설정 스킬은 즉시 반환해 기존 동작을 유지한다.
//! \param ExplosionLocation 폭발 중심 위치(적중 지점)
//! \return 없음
void AMyNeferProjectile::ApplyExplosionDamage(const FVector& ExplosionLocation)
{
	if (ExplosionRadius <= 0.0f || ExplosionDamageCoefficient <= 0.0f || !SourceASC || !DamageGameplayEffectClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MyNeferProjectileExplosion), false, this);
	QueryParams.AddIgnoredActor(this);
	if (SourceActor)
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	World->OverlapMultiByObjectType(
		OverlapResults,
		ExplosionLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams
	);

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	TSet<AActor*> ProcessedTargets;
	int32 ExplosionHitCount = 0;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = OverlapResult.GetActor();
		if (!TargetActor || TargetActor == this || TargetActor == SourceActor || ProcessedTargets.Contains(TargetActor))
		{
			continue;
		}

		if (!UMyAbilitySystemLibrary::IsHostile(SourceActor, TargetActor))
		{
			continue;
		}
		ProcessedTargets.Add(TargetActor);

		UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		if (ApplyDamageSpecToTarget(TargetASC, ExplosionDamageCoefficient, EffectContext))
		{
			++ExplosionHitCount;
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// 실제 폭발 판정 반경을 /debugline 치트가 켜졌을 때만 표시한다.
	// 폭발은 서버에서만 실행되므로, 원격 소유 클라이언트에는 DrawShapeForOwner가 Client RPC로 전송한다.
	if (bDrawDebugExplosion)
	{
		MySkillDebugDraw::DrawShapeForOwner(SourceActor,
			FMySkillDebugShape::MakeSphere(ExplosionLocation, ExplosionRadius, FColor::Orange, DebugExplosionLifeTime, 1.0f));
	}
#endif

	UE_LOG(LogTemp, Log, TEXT("Nefer projectile explosion - Source: %s, Radius: %.1f, Coefficient: %.2f, Hits: %d"),
		*GetNameSafe(SourceActor),
		ExplosionRadius,
		ExplosionDamageCoefficient,
		ExplosionHitCount);
}

////////////////////////////
//! \author HanUl
//! \brief DamageGameplayEffectClass를 사용해 대상 ASC에 Data.Coefficient SetByCaller 피해를 적용한다.
//!        공격력 곱셈은 ExecutionCalculation이 캡처한 Source AttackPower로 수행한다.
//! \param TargetASC 피해를 받을 대상 ASC
//! \param InDamageCoefficient 적용할 스킬 피해 계수
//! \param EffectContext GameplayEffect Context
//! \return 피해 GameplayEffect 적용 요청을 만들었으면 true
bool AMyNeferProjectile::ApplyDamageSpecToTarget(UAbilitySystemComponent* TargetASC, float InDamageCoefficient, const FGameplayEffectContextHandle& EffectContext) const
{
	if (!SourceASC || !TargetASC || !DamageGameplayEffectClass || InDamageCoefficient <= 0.0f)
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageGameplayEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	UMyAbilitySystemLibrary::AssignSetByCallerCoefficient(SpecHandle, InDamageCoefficient);
	if (AttackerHitCameraFeedbackTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(AttackerHitCameraFeedbackTag);
	}
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 대상이 이미 이 투사체에 적중된 적 있는지 확인한다.
//! \param TargetActor 확인할 대상 Actor
//! \return 이미 적중된 대상이면 true
bool AMyNeferProjectile::HasActorAlreadyBeenHit(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& HitActor : HitActors)
	{
		if (HitActor.Get() == TargetActor)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////
//! \author HanUl
//! \brief 대상 ASC가 기존 상태로 인정할 태그를 갖고 있는지 확인한다.
//! \param TargetASC 확인할 대상 ASC
//! \return 기존 상태 태그가 있으면 true
bool AMyNeferProjectile::HasExistingStatus(UAbilitySystemComponent* TargetASC) const
{
	if (!TargetASC || ExistingStatusTags.IsEmpty())
	{
		return false;
	}

	return TargetASC->HasAnyMatchingGameplayTags(ExistingStatusTags);
}

////////////////////////////
//! \author HanUl
//! \brief 현재 투사체 위치 주변에서 다음 연쇄 대상을 찾는다.
//! \param 없음
//! \return 가장 가까운 유효 대상, 없으면 nullptr
AActor* AMyNeferProjectile::FindNextChainTarget() const
{
	UWorld* World = GetWorld();
	if (!World || ChainSearchRadius <= 0.0f)
	{
		return nullptr;
	}

	const FCollisionObjectQueryParams ObjectQueryParams = UMyAbilitySystemLibrary::MakePlayerAttackObjectQuery();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MyNeferProjectileChain), false, this);
	QueryParams.AddIgnoredActor(this);
	if (SourceActor)
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	TArray<FOverlapResult> OverlapResults;
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ChainSearchRadius),
		QueryParams
	);

	if (!bHasOverlap)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* CandidateActor = OverlapResult.GetActor();
		if (!CandidateActor || CandidateActor == this || CandidateActor == SourceActor)
		{
			continue;
		}

		if (!UMyAbilitySystemLibrary::IsHostile(SourceActor, CandidateActor))
		{
			continue;
		}

		if (bHitEachTargetOnce && HasActorAlreadyBeenHit(CandidateActor))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(GetActorLocation(), CandidateActor->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestTarget = CandidateActor;
		}
	}

	return BestTarget;
}

////////////////////////////
//! \author HanUl
//! \brief 투사체를 다음 연쇄 대상 방향으로 재지향한다.
//! \param TargetActor 다음 연쇄 대상
//! \return 없음
void AMyNeferProjectile::ContinueChainToTarget(AActor* TargetActor)
{
	if (!TargetActor || !ProjectileMovementComponent || InitialSpeed <= 0.0f)
	{
		FinishProjectile();
		return;
	}

	const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		FinishProjectile();
		return;
	}

	--RemainingChainCount;

	const float ChainLifeSeconds = ChainSearchRadius > 0.0f
		? (ChainSearchRadius / InitialSpeed) + 0.1f
		: LifeSeconds;
	const float SafeChainLifeSeconds = FMath::Max(ChainLifeSeconds, 0.1f);
	if (HasAuthority())
	{
		Multicast_ContinueChainVisual(GetActorLocation(), Direction, InitialSpeed, SafeChainLifeSeconds);
		ForceNetUpdate();
		return;
	}

	ApplyProjectileMovementVisual(GetActorLocation(), Direction, InitialSpeed, SafeChainLifeSeconds);
}

void AMyNeferProjectile::FinishProjectile()
{
	if (bProjectileFinished)
	{
		return;
	}

	if (HasAuthority())
	{
		Multicast_FinishProjectileVisual(FinishLifeSeconds);
		ForceNetUpdate();
		return;
	}

	ApplyProjectileFinishedVisual(FinishLifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 초기 발사 시각 상태를 현재 네트워크 인스턴스에 적용한다.
//! \param LaunchLocation 발사 시작 위치
//! \param LaunchDirection 발사 방향
//! \param LaunchSpeed 발사 속도
//! \param LaunchRadius 투사체 충돌 반경
//! \param LaunchLifeSeconds 투사체 수명
//! \return 없음
void AMyNeferProjectile::ApplyProjectileLaunchVisual(const FVector& LaunchLocation, const FVector& LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds)
{
	bProjectileFinished = false;
	bProjectileInitialized = true;

	InitialSpeed = FMath::Max(LaunchSpeed, 0.0f);
	ProjectileRadius = FMath::Max(LaunchRadius, 0.0f);
	LifeSeconds = FMath::Max(LaunchLifeSeconds, 0.1f);

	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(ProjectileRadius);
	}

	ApplyProjectileCollisionState(HasAuthority());
	ApplyProjectileMovementVisual(LaunchLocation, LaunchDirection, InitialSpeed, LifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 위치, 방향, 속도, 수명을 현재 네트워크 인스턴스에 적용한다.
//! \param NewLocation 적용할 위치
//! \param NewDirection 적용할 이동 방향
//! \param NewSpeed 적용할 이동 속도
//! \param NewLifeSeconds 적용할 남은 수명
//! \return 없음
void AMyNeferProjectile::ApplyProjectileMovementVisual(const FVector& NewLocation, const FVector& NewDirection, float NewSpeed, float NewLifeSeconds)
{
	const FVector SafeDirection = NewDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(SafeDirection.Rotation());

	const float SafeSpeed = FMath::Max(NewSpeed, 0.0f);
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = SafeSpeed;
		ProjectileMovementComponent->MaxSpeed = SafeSpeed;
		ProjectileMovementComponent->Velocity = SafeDirection * SafeSpeed;
		ProjectileMovementComponent->Activate(true);
	}

	SetLifeSpan(FMath::Max(NewLifeSeconds, 0.1f));
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 종료 시각 상태를 현재 네트워크 인스턴스에 적용한다.
//! \param NewFinishLifeSeconds 종료 연출 이후 제거까지 걸리는 시간
//! \return 없음
void AMyNeferProjectile::ApplyProjectileFinishedVisual(float NewFinishLifeSeconds)
{
	if (bProjectileFinished)
	{
		return;
	}

	bProjectileFinished = true;
	bProjectileInitialized = false;
	ApplyProjectileCollisionState(false);

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}

	OnProjectileFinished();
	SetLifeSpan(FMath::Max(NewFinishLifeSeconds, 0.01f));
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 collision을 서버 판정용 또는 클라이언트 시각 전용 상태로 전환한다.
//! \param bEnableAuthorityCollision true이면 서버 판정용 collision을 켠다.
//! \return 없음
void AMyNeferProjectile::ApplyProjectileCollisionState(bool bEnableAuthorityCollision)
{
	if (!CollisionComponent)
	{
		return;
	}

	if (bEnableAuthorityCollision)
	{
		ConfigureCollisionForProjectile();
		CollisionComponent->SetGenerateOverlapEvents(true);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}

	CollisionComponent->SetGenerateOverlapEvents(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

////////////////////////////
//! \author HanUl
//! \brief 초기 발사 시각 상태를 모든 네트워크 인스턴스에 동기화한다.
//! \param LaunchLocation 발사 시작 위치
//! \param LaunchDirection 발사 방향
//! \param LaunchSpeed 발사 속도
//! \param LaunchRadius 투사체 충돌 반경
//! \param LaunchLifeSeconds 투사체 수명
//! \return 없음
void AMyNeferProjectile::Multicast_LaunchProjectileVisual_Implementation(FVector_NetQuantize LaunchLocation, FVector_NetQuantizeNormal LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds)
{
	ApplyProjectileLaunchVisual(LaunchLocation, LaunchDirection, LaunchSpeed, LaunchRadius, LaunchLifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 연쇄 튕김 시각 상태를 모든 네트워크 인스턴스에 동기화한다.
//! \param ChainLocation 연쇄 시작 위치
//! \param ChainDirection 다음 대상 방향
//! \param ChainSpeed 연쇄 이동 속도
//! \param ChainLifeSeconds 연쇄 구간 수명
//! \return 없음
void AMyNeferProjectile::Multicast_ContinueChainVisual_Implementation(FVector_NetQuantize ChainLocation, FVector_NetQuantizeNormal ChainDirection, float ChainSpeed, float ChainLifeSeconds)
{
	ApplyProjectileMovementVisual(ChainLocation, ChainDirection, ChainSpeed, ChainLifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 종료 시각 상태를 모든 네트워크 인스턴스에 동기화한다.
//! \param NewFinishLifeSeconds 종료 연출 이후 제거까지 걸리는 시간
//! \return 없음
void AMyNeferProjectile::Multicast_FinishProjectileVisual_Implementation(float NewFinishLifeSeconds)
{
	ApplyProjectileFinishedVisual(NewFinishLifeSeconds);
}
