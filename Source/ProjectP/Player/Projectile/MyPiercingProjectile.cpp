////////////////////////////
//! \file MyPiercingProjectile.cpp
//! \brief 경로의 모든 적을 한 번씩 관통 타격하는 범용 투사체를 구현한다.

#include "MyPiercingProjectile.h"

#include "../../GAS/MyAbilitySystemLibrary.h"
#include "../../GAS/MySkillDebugShape.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

////////////////////////////
//! \author HanUl
//! \brief 충돌 구체와 투사체 이동 컴포넌트를 구성하고 복제를 설정한다.
//! \param 없음
//! \return 없음
AMyPiercingProjectile::AMyPiercingProjectile()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrimaryActorTick.bCanEverTick = true; // 디버그 판정 표시 전용. Shipping/Test에서는 Tick을 켜지 않는다.
#else
	PrimaryActorTick.bCanEverTick = false;
#endif
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

////////////////////////////
//! \author HanUl
//! \brief 충돌/정지 델리게이트를 바인딩한다.
//! \param 없음
//! \return 없음
void AMyPiercingProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AMyPiercingProjectile::HandleProjectileOverlap);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &AMyPiercingProjectile::HandleProjectileStop);
	}
}

////////////////////////////
//! \author HanUl
//! \brief /debugline 치트가 켜지면 현재 위치의 실제 판정 구체 1개를 매 프레임 표시한다(경로 잔상 없음).
//! \param DeltaSeconds 프레임 델타 시간
//! \return 없음
void AMyPiercingProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebugHitSphere && bProjectileInitialized && !bProjectileFinished && CollisionComponent)
	{
		// DrawShape 내부에서 로컬 PlayerController의 /debugline 활성 여부를 확인한 뒤 그린다.
		// 실제 오버랩 판정과 동일하게, CollisionComponent의 월드 위치와 스케일 반영 반경을 그린다.
		// 표시 시간 0으로 매 프레임 1개만 그려 투사체를 따라 이동한다(경로 잔상 없음).
		MySkillDebugDraw::DrawShape(GetWorld(),
			FMySkillDebugShape::MakeSphere(
				CollisionComponent->GetComponentLocation(),
				CollisionComponent->GetScaledSphereRadius(),
				FColor::Red,
				0.0f,
				1.0f));
	}
#endif
}

////////////////////////////
//! \author HanUl
//! \brief 발사 소스/데미지/사거리/속도/반경/계수/태그를 설정하고 발사 연출을 시작한다.
//! \param InSourceActor 발사 소스 Actor
//! \param InSourceASC 발사 소스 ASC
//! \param InDamageGameplayEffectClass 계수 피해 GameplayEffect(ExecCalc)
//! \param InRange 최대 사거리(cm)
//! \param InProjectileSpeed 투사체 속도(cm/s)
//! \param InProjectileRadius 충돌 구체 반경(cm)
//! \param InDamageCoefficient 스킬 피해 계수
//! \param InSkillCooldownTag 처치 스킬 식별용 쿨다운 태그
//! \param InSkillInputTag 카메라 피드백 등급 해석용 스킬 입력 태그
//! \return 없음
void AMyPiercingProjectile::InitializeProjectile(
	AActor* InSourceActor,
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
	float InRange,
	float InProjectileSpeed,
	float InProjectileRadius,
	float InDamageCoefficient,
	FGameplayTag InSkillCooldownTag,
	FGameplayTag InSkillInputTag
)
{
	SourceActor = InSourceActor;
	SourceASC = InSourceASC;
	DamageGameplayEffectClass = InDamageGameplayEffectClass;
	SkillCooldownTag = InSkillCooldownTag;
	SkillInputTag = InSkillInputTag;

	Range = FMath::Max(InRange, 0.0f);
	InitialSpeed = FMath::Max(InProjectileSpeed, 0.0f);
	ProjectileRadius = FMath::Max(InProjectileRadius, 0.0f);
	DamageCoefficient = FMath::Max(InDamageCoefficient, 0.0f);
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
//! \brief Pawn/Destructible은 Overlap으로 판정하고 지형(WorldStatic)은 무시해 관통시킨다.
//! \param 없음
//! \return 없음
void AMyPiercingProjectile::ConfigureCollisionForProjectile()
{
	if (!CollisionComponent)
	{
		return;
	}

	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Destructible, ECR_Overlap);
	// 지형(WorldStatic)은 무시해 관통한다. 벽에서 멈추지 않고 최대 사거리 수명으로만 종료한다.
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 이동이 예외적으로 정지하면 종료한다(지형은 관통하므로 통상 호출되지 않는다). 서버에서만 처리한다.
//! \param ImpactResult 충돌 결과
//! \return 없음
void AMyPiercingProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	(void)ImpactResult;
	if (!bProjectileInitialized || !HasAuthority())
	{
		return;
	}

	FinishProjectile();
}

////////////////////////////
//! \author HanUl
//! \brief 경로에 닿은 적을 한 번씩 관통 타격한다. 적중해도 소멸하지 않고 계속 진행한다. 서버에서만 판정한다.
//! \param OverlappedComponent 오버랩된 컴포넌트
//! \param OtherActor 오버랩 대상 Actor
//! \param OtherComp 대상 컴포넌트
//! \param OtherBodyIndex 대상 바디 인덱스
//! \param bFromSweep 스윕 여부
//! \param SweepResult 스윕 결과
//! \return 없음
void AMyPiercingProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!bProjectileInitialized || !HasAuthority() || !OtherActor || OtherActor == this || OtherActor == SourceActor)
	{
		return;
	}

	if (!UMyAbilitySystemLibrary::IsHostile(SourceActor, OtherActor))
	{
		return;
	}

	// 이미 관통한 대상은 다시 맞히지 않는다(1회씩). 소멸하지 않고 다음 적으로 계속 진행한다.
	for (const TWeakObjectPtr<AActor>& HitActor : HitActors)
	{
		if (HitActor.Get() == OtherActor)
		{
			return;
		}
	}

	if (!SourceASC || !DamageGameplayEffectClass || DamageCoefficient <= 0.0f)
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplyPlayerSkillCoefficientDamageEffectToTargetActor(
		SourceASC,
		OtherActor,
		DamageGameplayEffectClass,
		DamageCoefficient,
		SkillCooldownTag,
		SkillInputTag
	);
	HitActors.AddUnique(OtherActor);
}

////////////////////////////
//! \author HanUl
//! \brief 초기 발사 시각 상태를 현재 네트워크 인스턴스에 적용한다.
//! \param LaunchLocation 발사 시작 위치
//! \param LaunchDirection 발사 방향
//! \param LaunchSpeed 발사 속도
//! \param LaunchRadius 충돌 반경
//! \param LaunchLifeSeconds 투사체 수명
//! \return 없음
void AMyPiercingProjectile::ApplyProjectileLaunchVisual(const FVector& LaunchLocation, const FVector& LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds)
{
	bProjectileFinished = false;
	bProjectileInitialized = true;

	InitialSpeed = FMath::Max(LaunchSpeed, 0.0f);
	ProjectileRadius = FMath::Max(LaunchRadius, 0.0f);
	LifeSeconds = FMath::Max(LaunchLifeSeconds, 0.1f);

	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(ProjectileRadius);

		// 실제 판정 반경은 컴포넌트 스케일이 곱해진 값이다. BP에서 루트를 스케일업하면 Definition 값보다 커진다.
		UE_LOG(LogTemp, Log, TEXT("PiercingProjectile radius - Definition: %.1f, Scaled(actual hit): %.1f, ActorScale: %s"),
			ProjectileRadius,
			CollisionComponent->GetScaledSphereRadius(),
			*GetActorScale3D().ToCompactString());
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
void AMyPiercingProjectile::ApplyProjectileMovementVisual(const FVector& NewLocation, const FVector& NewDirection, float NewSpeed, float NewLifeSeconds)
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
void AMyPiercingProjectile::ApplyProjectileFinishedVisual(float NewFinishLifeSeconds)
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
void AMyPiercingProjectile::ApplyProjectileCollisionState(bool bEnableAuthorityCollision)
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
//! \brief 투사체 종료를 서버에서 전 인스턴스로 동기화한다(클라는 로컬 적용).
//! \param 없음
//! \return 없음
void AMyPiercingProjectile::FinishProjectile()
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
//! \brief 초기 발사 시각 상태를 모든 네트워크 인스턴스에 동기화한다.
//! \param LaunchLocation 발사 시작 위치
//! \param LaunchDirection 발사 방향
//! \param LaunchSpeed 발사 속도
//! \param LaunchRadius 충돌 반경
//! \param LaunchLifeSeconds 투사체 수명
//! \return 없음
void AMyPiercingProjectile::Multicast_LaunchProjectileVisual_Implementation(FVector_NetQuantize LaunchLocation, FVector_NetQuantizeNormal LaunchDirection, float LaunchSpeed, float LaunchRadius, float LaunchLifeSeconds)
{
	ApplyProjectileLaunchVisual(LaunchLocation, LaunchDirection, LaunchSpeed, LaunchRadius, LaunchLifeSeconds);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 종료 시각 상태를 모든 네트워크 인스턴스에 동기화한다.
//! \param NewFinishLifeSeconds 종료 연출 이후 제거까지 걸리는 시간
//! \return 없음
void AMyPiercingProjectile::Multicast_FinishProjectileVisual_Implementation(float NewFinishLifeSeconds)
{
	ApplyProjectileFinishedVisual(NewFinishLifeSeconds);
}
