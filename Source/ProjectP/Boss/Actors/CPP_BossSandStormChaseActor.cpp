#include "CPP_BossSandStormChaseActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"

ACPP_BossSandStormChaseActor::ACPP_BossSandStormChaseActor()
{
	bReplicates = true;
	SetReplicateMovement(true);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
	DamageSphere->SetupAttachment(SceneRoot);
	DamageSphere->SetSphereRadius(DamageRadius);
	DamageSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageSphere->SetCollisionObjectType(ECC_WorldDynamic);
	DamageSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// Visible in game like the ride sandstorm's sphere; the Blueprint child can add further VFX on top.
	DamageSphere->SetHiddenInGame(false);
}

void ACPP_BossSandStormChaseActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bChasing)
	{
		return;
	}

	RefreshTargetIfNeeded();
	MoveTowardTarget(DeltaSeconds);
}

////////////////////////////
//! \brief 서버에서 스폰 직후 호출되어 추격 폭풍을 구동한다.
void ACPP_BossSandStormChaseActor::Initialize(
	UAbilitySystemComponent* InSourceASC,
	AActor* InInitialTarget,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffect,
	TSubclassOf<UGameplayEffect> InTargetMarkGameplayEffect,
	const FActiveGameplayEffectHandle& InExistingMarkHandle
)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceASC = InSourceASC;
	DamageGameplayEffect = InDamageGameplayEffect;
	TargetMarkGameplayEffect = InTargetMarkGameplayEffect;

	// The ability already applied the mark to the initial target; take ownership of that handle instead of re-applying it.
	CurrentTarget = InInitialTarget;
	TargetMarkHandle = InExistingMarkHandle;
	if (!TargetMarkHandle.IsValid() && InInitialTarget)
	{
		ApplyTargetMark(InInitialTarget);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Contact damage is active immediately on spawn (even while stationary): hit anyone already overlapping.
	World->GetTimerManager().SetTimerForNextTick(this, &ACPP_BossSandStormChaseActor::StartDamageForCurrentOverlaps);

	// Stay stationary for MoveStartDelay, then begin chasing.
	if (MoveStartDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(MoveStartTimerHandle, this, &ACPP_BossSandStormChaseActor::BeginChase, MoveStartDelay, false);
	}
	else
	{
		BeginChase();
	}

	// Self-destruct after the total lifetime measured from spawn.
	if (Lifetime > 0.0f)
	{
		World->GetTimerManager().SetTimer(LifetimeTimerHandle, this, &ACPP_BossSandStormChaseActor::HandleLifetimeExpired, Lifetime, false);
	}
}

void ACPP_BossSandStormChaseActor::BeginChase()
{
	if (!HasAuthority())
	{
		return;
	}

	bChasing = true;
	RefreshTargetIfNeeded();
	SetActorTickEnabled(true);
}

void ACPP_BossSandStormChaseActor::HandleLifetimeExpired()
{
	Destroy();
}

void ACPP_BossSandStormChaseActor::MoveTowardTarget(float DeltaSeconds)
{
	AActor* Target = CurrentTarget.Get();
	if (!Target)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	TargetLocation.Z = CurrentLocation.Z;

	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, TargetLocation, DeltaSeconds, MoveSpeed);
	SetActorLocation(NewLocation);
}

////////////////////////////
//! \brief 표적이 죽었으면 가장 가까운 생존 플레이어로 재지정한다(남은 지속 시간 유지).
void ACPP_BossSandStormChaseActor::RefreshTargetIfNeeded()
{
	if (IsTargetAlive())
	{
		return;
	}

	AActor* NewTarget = UMyAbilitySystemLibrary::GetNearestLivingPlayer(this, GetActorLocation());
	SetCurrentTarget(NewTarget);
}

bool ACPP_BossSandStormChaseActor::IsTargetAlive() const
{
	return CurrentTarget.IsValid() && UMyAbilitySystemLibrary::IsLivingPawn(CurrentTarget.Get());
}

////////////////////////////
//! \brief 표적을 교체하고 발밑 마크(마킹 GE)를 옛 표적에서 새 표적으로 이동시킨다.
void ACPP_BossSandStormChaseActor::SetCurrentTarget(AActor* NewTarget)
{
	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}

	RemoveTargetMark();
	CurrentTarget = NewTarget;

	if (NewTarget)
	{
		ApplyTargetMark(NewTarget);
	}
}

void ACPP_BossSandStormChaseActor::ApplyTargetMark(AActor* MarkTarget)
{
	if (!HasAuthority() || !SourceASC || !TargetMarkGameplayEffect || !MarkTarget)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(MarkTarget);
	if (!TargetASC)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(TargetMarkGameplayEffect, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	TargetMarkHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void ACPP_BossSandStormChaseActor::RemoveTargetMark()
{
	if (!TargetMarkHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* MarkedASC = TargetMarkHandle.GetOwningAbilitySystemComponent())
	{
		MarkedASC->RemoveActiveGameplayEffect(TargetMarkHandle);
	}

	TargetMarkHandle.Invalidate();
}

////////////////////////////
//! \brief 스폰 시점에 이미 폭풍 범위 안에 있던 액터들에게 접촉 피해를 시작한다.
void ACPP_BossSandStormChaseActor::StartDamageForCurrentOverlaps()
{
	if (!HasAuthority() || !DamageSphere)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageSphere->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryStartDamageForActor(OverlappingActor);
	}
}

void ACPP_BossSandStormChaseActor::TryStartDamageForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !ShouldDamageActor(DamageTargetActor))
	{
		return;
	}

	const TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	if (DamageTimerHandlesByActor.Contains(DamageTargetPtr))
	{
		return;
	}

	// Immediate hit on contact, then a repeating per-second tick.
	ApplyDamageToActor(DamageTargetActor);
	StartDamageTimerForActor(DamageTargetActor);
}

void ACPP_BossSandStormChaseActor::StartDamageTimerForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || DamageInterval <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	FTimerHandle& TimerHandle = DamageTimerHandlesByActor.FindOrAdd(DamageTargetPtr);
	World->GetTimerManager().ClearTimer(TimerHandle);

	World->GetTimerManager().SetTimer(
		TimerHandle,
		FTimerDelegate::CreateUObject(this, &ACPP_BossSandStormChaseActor::HandleDamageTimerTick, DamageTargetPtr),
		DamageInterval,
		true
	);
}

void ACPP_BossSandStormChaseActor::StopDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (FTimerHandle* TimerHandle = DamageTimerHandlesByActor.Find(DamageTargetActor))
	{
		World->GetTimerManager().ClearTimer(*TimerHandle);
		DamageTimerHandlesByActor.Remove(DamageTargetActor);
	}
}

void ACPP_BossSandStormChaseActor::StopAllDamageTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& TimerPair : DamageTimerHandlesByActor)
	{
		World->GetTimerManager().ClearTimer(TimerPair.Value);
	}

	DamageTimerHandlesByActor.Reset();
}

void ACPP_BossSandStormChaseActor::HandleDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor)
{
	AActor* DamageTarget = DamageTargetActor.Get();
	if (!DamageTarget || !DamageSphere || !ShouldDamageActor(DamageTarget))
	{
		StopDamageTimerForActor(DamageTargetActor);
		return;
	}

	if (!DamageSphere->IsOverlappingActor(DamageTarget))
	{
		StopDamageTimerForActor(DamageTargetActor);
		return;
	}

	ApplyDamageToActor(DamageTarget);
}

////////////////////////////
//! \brief 표적 현재 체력의 DamageHealthRatio 만큼을 1회 적용한다.
void ACPP_BossSandStormChaseActor::ApplyDamageToActor(AActor* DamageTargetActor)
{
	if (!SourceASC || !DamageGameplayEffect || !ShouldDamageActor(DamageTargetActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UMyAbilitySystemLibrary::GetAbilitySystemComponentFromActor(DamageTargetActor);
	if (!TargetASC)
	{
		return;
	}

	const float CurrentHealth = TargetASC->GetNumericAttribute(UMyAttributeSet::GetHealthAttribute());
	if (CurrentHealth <= 0.0f)
	{
		return;
	}

	const float DamageAmount = CurrentHealth * DamageHealthRatio;
	if (DamageAmount <= 0.0f)
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
		SourceASC,
		DamageTargetActor,
		DamageGameplayEffect,
		DamageAmount,
		1.0f,
		CurseGaugeAmount
	);
}

////////////////////////////
//! \brief 피해 대상이 될 수 있는지: Faction 태그 기준 적대 관계인 살아있는 대상만.
bool ACPP_BossSandStormChaseActor::ShouldDamageActor(const AActor* CandidateActor) const
{
	if (!CandidateActor || CandidateActor == this || CandidateActor == GetOwner())
	{
		return false;
	}

	const AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	return UMyAbilitySystemLibrary::IsHostile(SourceAvatar, CandidateActor);
}

void ACPP_BossSandStormChaseActor::BeginPlay()
{
	Super::BeginPlay();

	if (DamageSphere)
	{
		DamageSphere->OnComponentBeginOverlap.AddDynamic(this, &ACPP_BossSandStormChaseActor::HandleDamageSphereBeginOverlap);
		DamageSphere->OnComponentEndOverlap.AddDynamic(this, &ACPP_BossSandStormChaseActor::HandleDamageSphereEndOverlap);
	}
}

void ACPP_BossSandStormChaseActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (DamageSphere)
	{
		DamageSphere->SetSphereRadius(DamageRadius);
	}
}

void ACPP_BossSandStormChaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllDamageTimers();
	RemoveTargetMark();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveStartTimerHandle);
		World->GetTimerManager().ClearTimer(LifetimeTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ACPP_BossSandStormChaseActor::HandleDamageSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	TryStartDamageForActor(OtherActor);
}

void ACPP_BossSandStormChaseActor::HandleDamageSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex
)
{
	StopDamageTimerForActor(TWeakObjectPtr<AActor>(OtherActor));
}
