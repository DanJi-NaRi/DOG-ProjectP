#include "CPP_BossCrossLaserActor.h"

#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"

ACPP_BossCrossLaserActor::ACPP_BossCrossLaserActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// The two arms are the actual damage overlap volumes (and double as the debug wireframe). QueryOnly, pawns only.
	ArmBoxA = CreateDefaultSubobject<UBoxComponent>(TEXT("ArmBoxA"));
	ArmBoxA->SetupAttachment(SceneRoot);
	ArmBoxA->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArmBoxA->SetCollisionObjectType(ECC_WorldDynamic);
	ArmBoxA->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArmBoxA->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ArmBoxA->SetGenerateOverlapEvents(true);
	ArmBoxA->SetHiddenInGame(false);

	ArmBoxB = CreateDefaultSubobject<UBoxComponent>(TEXT("ArmBoxB"));
	ArmBoxB->SetupAttachment(SceneRoot);
	ArmBoxB->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArmBoxB->SetCollisionObjectType(ECC_WorldDynamic);
	ArmBoxB->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArmBoxB->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ArmBoxB->SetGenerateOverlapEvents(true);
	ArmBoxB->SetHiddenInGame(false);

	UpdateArmBoxExtents();
}

void ACPP_BossCrossLaserActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BossCrossLaserActor, StartServerTime);
}

void ACPP_BossCrossLaserActor::Initialize(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageGameplayEffect)
{
	SourceASC = InSourceASC;
	DamageGameplayEffect = InDamageGameplayEffect;
}

void ACPP_BossCrossLaserActor::BeginPlay()
{
	Super::BeginPlay();

	if (ArmBoxA)
	{
		ArmBoxA->OnComponentBeginOverlap.AddDynamic(this, &ACPP_BossCrossLaserActor::HandleArmBeginOverlap);
		ArmBoxA->OnComponentEndOverlap.AddDynamic(this, &ACPP_BossCrossLaserActor::HandleArmEndOverlap);
	}

	if (ArmBoxB)
	{
		ArmBoxB->OnComponentBeginOverlap.AddDynamic(this, &ACPP_BossCrossLaserActor::HandleArmBeginOverlap);
		ArmBoxB->OnComponentEndOverlap.AddDynamic(this, &ACPP_BossCrossLaserActor::HandleArmEndOverlap);
	}

	if (!HasAuthority())
	{
		return;
	}

	// Stamp the shared rotation reference once; it replicates to clients and never changes afterwards.
	StartServerTime = GetCurrentServerTime();

	// Catch pawns already standing inside the cross at spawn (begin-overlap events won't fire for them).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ACPP_BossCrossLaserActor::StartLaserDamageForCurrentOverlaps);
	}
}

void ACPP_BossCrossLaserActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLaserDamageTimers();

	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author HanSeul
//! \brief Keeps the arm boxes matching the configured arm sizes whenever the actor is placed or edited.
void ACPP_BossCrossLaserActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateArmBoxExtents();
}

void ACPP_BossCrossLaserActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateRotationFromServerTime();
}

////////////////////////////
//! \author HanSeul
//! \brief Recomputes the absolute yaw from synchronized server time. Every machine (including the server's collision)
//!        uses this same formula, so the visual and the hit judgment never drift apart.
void ACPP_BossCrossLaserActor::UpdateRotationFromServerTime()
{
	if (StartServerTime <= 0.0f)
	{
		return;
	}

	const float ElapsedSeconds = GetCurrentServerTime() - StartServerTime;
	const float CurrentYaw = InitialYawDegrees + AngularSpeedDegPerSec * ElapsedSeconds;
	SetActorRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}

float ACPP_BossCrossLaserActor::GetCurrentServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void ACPP_BossCrossLaserActor::UpdateArmBoxExtents()
{
	if (ArmBoxA)
	{
		ArmBoxA->SetBoxExtent(FVector(ArmLength, ArmHalfWidth, ArmHalfHeight), false);
	}

	if (ArmBoxB)
	{
		ArmBoxB->SetBoxExtent(FVector(ArmHalfWidth, ArmLength, ArmHalfHeight), false);
	}
}

void ACPP_BossCrossLaserActor::HandleArmBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryStartLaserDamageForActor(OtherActor);
}

void ACPP_BossCrossLaserActor::HandleArmEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	// The cross has two arms: only stop damage once the target has left BOTH of them.
	if (IsActorInAnyArm(OtherActor))
	{
		return;
	}

	StopLaserDamageTimerForActor(TWeakObjectPtr<AActor>(OtherActor));
}

////////////////////////////
//! \author HanSeul
//! \brief Starts laser damage for actors already overlapping the cross at spawn.
void ACPP_BossCrossLaserActor::StartLaserDamageForCurrentOverlaps()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	if (ArmBoxA)
	{
		ArmBoxA->GetOverlappingActors(OverlappingActors);
	}

	if (ArmBoxB)
	{
		TArray<AActor*> OverlappingActorsB;
		ArmBoxB->GetOverlappingActors(OverlappingActorsB);
		OverlappingActors.Append(OverlappingActorsB);
	}

	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryStartLaserDamageForActor(OverlappingActor);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Applies an immediate contact hit and starts a repeating per-actor damage timer if not already tracked.
//! \param DamageTargetActor Actor that entered an arm of the cross.
void ACPP_BossCrossLaserActor::TryStartLaserDamageForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || DamageTargetActor == this)
	{
		return;
	}

	// Never damage the boss with its own laser (matches the boss attack ability and rock warning).
	if (IsDamageSourceActor(DamageTargetActor))
	{
		return;
	}

	const TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	if (LaserDamageTimerHandlesByActor.Contains(DamageTargetPtr))
	{
		return;
	}

	ApplyLaserDamageToActor(DamageTargetActor);
	StartLaserDamageTimerForActor(DamageTargetActor);
}

void ACPP_BossCrossLaserActor::StartLaserDamageTimerForActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || DamageTickInterval <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<AActor> DamageTargetPtr(DamageTargetActor);
	FTimerHandle& TimerHandle = LaserDamageTimerHandlesByActor.FindOrAdd(DamageTargetPtr);
	World->GetTimerManager().ClearTimer(TimerHandle);

	World->GetTimerManager().SetTimer(
		TimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ACPP_BossCrossLaserActor::HandleLaserDamageTimerTick,
			DamageTargetPtr
		),
		DamageTickInterval,
		true
	);
}

void ACPP_BossCrossLaserActor::StopLaserDamageTimerForActor(TWeakObjectPtr<AActor> DamageTargetActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (FTimerHandle* TimerHandle = LaserDamageTimerHandlesByActor.Find(DamageTargetActor))
	{
		World->GetTimerManager().ClearTimer(*TimerHandle);
		LaserDamageTimerHandlesByActor.Remove(DamageTargetActor);
	}
}

void ACPP_BossCrossLaserActor::StopAllLaserDamageTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& TimerPair : LaserDamageTimerHandlesByActor)
	{
		World->GetTimerManager().ClearTimer(TimerPair.Value);
	}

	LaserDamageTimerHandlesByActor.Reset();
}

////////////////////////////
//! \author HanSeul
//! \brief Repeating damage tick for one target; re-validates it is still inside an arm, otherwise stops the timer.
//! \param DamageTargetActor Weak pointer to the actor that owns this timer.
void ACPP_BossCrossLaserActor::HandleLaserDamageTimerTick(TWeakObjectPtr<AActor> DamageTargetActor)
{
	AActor* DamageTarget = DamageTargetActor.Get();
	if (!DamageTarget || !IsActorInAnyArm(DamageTarget))
	{
		StopLaserDamageTimerForActor(DamageTargetActor);
		return;
	}

	ApplyLaserDamageToActor(DamageTarget);
}

////////////////////////////
//! \author HanSeul
//! \brief Applies one laser damage hit: boss attack power multiplied by the laser coefficient.
//! \param DamageTargetActor Actor that should receive the laser damage.
void ACPP_BossCrossLaserActor::ApplyLaserDamageToActor(AActor* DamageTargetActor)
{
	if (!HasAuthority() || !DamageTargetActor || DamageTargetActor == this || IsDamageSourceActor(DamageTargetActor))
	{
		return;
	}

	if (!SourceASC.IsValid() || !DamageGameplayEffect || DamageCoefficient <= 0.0f)
	{
		return;
	}

	if (!UMyAbilitySystemLibrary::IsHostile(SourceASC->GetAvatarActor(), DamageTargetActor))
	{
		return;
	}

	UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
		SourceASC.Get(),
		DamageTargetActor,
		DamageGameplayEffect,
		DamageCoefficient,
		1.0f,
		CurseGaugeAmount
	);
}

bool ACPP_BossCrossLaserActor::IsActorInAnyArm(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	return (ArmBoxA && ArmBoxA->IsOverlappingActor(OtherActor))
		|| (ArmBoxB && ArmBoxB->IsOverlappingActor(OtherActor));
}

////////////////////////////
//! \author HanSeul
//! \brief True if the actor is the damage source (the boss). Uses the spawn Owner, which is valid from spawn time
//!        onward, so the boss is excluded even before Initialize sets the source ASC.
bool ACPP_BossCrossLaserActor::IsDamageSourceActor(const AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	if (OtherActor == GetOwner())
	{
		return true;
	}

	return SourceASC.IsValid() && OtherActor == SourceASC->GetAvatarActor();
}
