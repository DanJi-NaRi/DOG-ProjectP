#include "CPP_BossRockWarningActor.h"

#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GAS/MyAttributeSet.h"

ACPP_BossRockWarningActor::ACPP_BossRockWarningActor()
{
	bReplicates = true;

	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	WarningArea = CreateDefaultSubobject<USphereComponent>(TEXT("WarningArea"));
	WarningArea->SetupAttachment(SceneRoot);
	WarningArea->SetSphereRadius(WarningRadius);
	WarningArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WarningArea->SetCollisionObjectType(ECC_WorldDynamic);
	WarningArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	WarningArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ACPP_BossRockWarningActor::Initialize(UAbilitySystemComponent* InSourceASC, TSubclassOf<UGameplayEffect> InDamageGameplayEffect)
{
	SourceASC = InSourceASC;
	DamageGameplayEffect = InDamageGameplayEffect;
}

void ACPP_BossRockWarningActor::BeginPlay()
{
	Super::BeginPlay();

	// Authority owns the whole lifecycle: it spawns the (now replicated) telegraph, runs the
	// warning timer, and applies damage. Clients receive the telegraph via replication and must
	// not spawn their own copy or run the damage/destroy timer.
	if (!HasAuthority())
	{
		return;
	}

	SpawnWarningTelegraph();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WarningTimerHandle,
			this,
			&ACPP_BossRockWarningActor::HandleWarningFinished,
			WarningDuration,
			false
		);
	}
}

void ACPP_BossRockWarningActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (WarningArea)
	{
		WarningArea->SetSphereRadius(WarningRadius);
	}
}

void ACPP_BossRockWarningActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningTimerHandle);
	}

	ClearWarningTelegraph();

	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author HanSeul
//! \brief Spawns a circular telegraph matching the rock warning radius.
void ACPP_BossRockWarningActor::SpawnWarningTelegraph()
{
	if (!TelegraphActorClass || ActiveTelegraphActor)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveTelegraphActor = World->SpawnActor<ACPP_BossTelegraphActor>(
		TelegraphActorClass,
		GetActorTransform(),
		SpawnParameters
	);

	if (!ActiveTelegraphActor)
	{
		return;
	}

	FBossHitShapeData ShapeData;
	ShapeData.Shape = EBossAttackShape::Circle;
	ShapeData.OuterRadius = WarningRadius;
	ShapeData.HalfHeight = 100.0f;

	// Fill duration matches the warning timer so the telegraph completes exactly when damage lands.
	ActiveTelegraphActor->Initialize(GetActorTransform(), ShapeData, WarningDuration);
}

void ACPP_BossRockWarningActor::ClearWarningTelegraph()
{
	if (IsValid(ActiveTelegraphActor))
	{
		ActiveTelegraphActor->Destroy();
	}

	ActiveTelegraphActor = nullptr;
}

////////////////////////////
//! \author HanSeul
//! \brief Applies warning damage when the warning timer finishes and then destroys this actor.
void ACPP_BossRockWarningActor::HandleWarningFinished()
{
	ApplyDamageToOverlappingPawns();
	Destroy();
}

////////////////////////////
//! \author HanSeul
//! \brief Applies damage to pawns currently overlapping the rock warning area.
//!        Final damage is the boss attack power multiplied by DamageCoefficient, matching the normal attack pattern.
void ACPP_BossRockWarningActor::ApplyDamageToOverlappingPawns()
{
	if (!WarningArea || !SourceASC.IsValid() || !DamageGameplayEffect || DamageCoefficient <= 0.0f)
	{
		return;
	}

	// Exclude the damage source (the boss) so it cannot be hit by its own rock warning / red lightning,
	// matching the boss attack ability and cross laser which also ignore the avatar actor.
	const AActor* SourceAvatar = SourceASC->GetAvatarActor();

	TArray<AActor*> OverlappingActors;
	WarningArea->GetOverlappingActors(OverlappingActors);

	TSet<AActor*> DamagedActors;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (!OverlappingActor || OverlappingActor == SourceAvatar || DamagedActors.Contains(OverlappingActor))
		{
			continue;
		}

		if (!UMyAbilitySystemLibrary::IsHostile(SourceAvatar, OverlappingActor))
		{
			continue;
		}

		DamagedActors.Add(OverlappingActor);
		UMyAbilitySystemLibrary::ApplyCoefficientDamageEffectToTargetActor(
			SourceASC.Get(),
			OverlappingActor,
			DamageGameplayEffect,
			DamageCoefficient,
			1.0f,
			CurseGaugeAmount
		);
	}
}
