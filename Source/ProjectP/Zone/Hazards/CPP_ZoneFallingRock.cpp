#include "Zone/Hazards/CPP_ZoneFallingRock.h"

#include "AbilitySystemComponent.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "GameplayEffect.h"
#include "Player/PlayerCharacterBase.h"
#include "TimerManager.h"

ACPP_ZoneFallingRock::ACPP_ZoneFallingRock()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DamageArea = CreateDefaultSubobject<USphereComponent>(TEXT("DamageArea"));
	DamageArea->SetupAttachment(SceneRoot);
	DamageArea->SetSphereRadius(DamageRadius);
	DamageArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageArea->SetCollisionObjectType(ECC_WorldDynamic);
	DamageArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageArea->SetGenerateOverlapEvents(true);

	SourceAbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("SourceAbilitySystem"));
	SourceAbilitySystem->SetIsReplicated(false);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 환경 피해용 ASC를 초기화하고 텔레그래프와 경고 타이머를 시작한다.
void ACPP_ZoneFallingRock::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (SourceAbilitySystem)
	{
		SourceAbilitySystem->InitAbilityActorInfo(this, this);
	}

	SpawnWarningTelegraph();

	if (WarningDuration <= 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ACPP_ZoneFallingRock::HandleWarningFinished);
		return;
	}

	GetWorldTimerManager().SetTimer(
		WarningTimerHandle,
		this,
		&ACPP_ZoneFallingRock::HandleWarningFinished,
		WarningDuration,
		false);
}

void ACPP_ZoneFallingRock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (DamageArea)
	{
		DamageArea->SetSphereRadius(DamageRadius);
	}
}

void ACPP_ZoneFallingRock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(WarningTimerHandle);
	ClearWarningTelegraph();

	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author HanSeul
//! \brief 피해 반경과 경고 시간을 사용하는 원형 텔레그래프를 서버에서 생성한다.
void ACPP_ZoneFallingRock::SpawnWarningTelegraph()
{
	if (!TelegraphActorClass || IsValid(ActiveTelegraphActor))
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
		SpawnParameters);

	if (!IsValid(ActiveTelegraphActor))
	{
		return;
	}

	FBossHitShapeData ShapeData;
	ShapeData.Shape = EBossAttackShape::Circle;
	ShapeData.OuterRadius = DamageRadius;
	ShapeData.HalfHeight = 100.0f;

	ActiveTelegraphActor->Initialize(GetActorTransform(), ShapeData, WarningDuration);
}

void ACPP_ZoneFallingRock::ClearWarningTelegraph()
{
	if (IsValid(ActiveTelegraphActor))
	{
		ActiveTelegraphActor->Destroy();
	}

	ActiveTelegraphActor = nullptr;
}

////////////////////////////
//! \author HanSeul
//! \brief 경고 종료 시 고정 피해를 한 번 적용하고 낙석 액터를 제거한다.
void ACPP_ZoneFallingRock::HandleWarningFinished()
{
	if (bWarningResolved || !HasAuthority())
	{
		return;
	}

	bWarningResolved = true;
	ApplyFixedDamageToOverlappingPlayers();
	Destroy();
}

////////////////////////////
//! \author HanSeul
//! \brief 충돌 범위 안의 플레이어마다 SetByCaller 고정 피해를 한 번 적용한다.
void ACPP_ZoneFallingRock::ApplyFixedDamageToOverlappingPlayers()
{
	if (!DamageArea || !SourceAbilitySystem || !DamageGameplayEffect || FixedDamage <= 0.0f)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	DamageArea->GetOverlappingActors(OverlappingActors, APlayerCharacterBase::StaticClass());

	TSet<AActor*> DamagedPlayers;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (!IsValid(OverlappingActor) || DamagedPlayers.Contains(OverlappingActor))
		{
			continue;
		}

		DamagedPlayers.Add(OverlappingActor);
		UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
			SourceAbilitySystem,
			OverlappingActor,
			DamageGameplayEffect,
			FixedDamage);
	}
}
