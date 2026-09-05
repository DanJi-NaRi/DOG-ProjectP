#include "CPP_BossBlackHoleActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Boss/Abilities/CPP_BossAttackData.h"
#include "Boss/Actors/CPP_BossTelegraphActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"
#include "GAS/MyAbilitySystemLibrary.h"

ACPP_BossBlackHoleActor::ACPP_BossBlackHoleActor()
{
	bReplicates = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PullRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PullRangeSphere"));
	PullRangeSphere->SetupAttachment(SceneRoot);
	PullRangeSphere->SetSphereRadius(PullRadius);
	// Purely a debug visualization of the pull range; targets are gathered by querying living players directly.
	// No collision so it never registers as an obstacle in other systems' sweeps (e.g. the boss dash capsule sweep),
	// while the shape still renders its wireframe because rendering does not depend on collision.
	PullRangeSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PullRangeSphere->SetHiddenInGame(false);
}

void ACPP_BossBlackHoleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	RefreshPull();
}

////////////////////////////
//! \brief 서버에서 스폰 직후 호출되어 검은 구를 구동한다.
void ACPP_BossBlackHoleActor::Initialize(
	UAbilitySystemComponent* InSourceASC,
	TSubclassOf<UGameplayEffect> InKillGameplayEffect,
	float InKillDamage
)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceASC = InSourceASC;
	KillGameplayEffect = InKillGameplayEffect;
	KillDamage = InKillDamage;

	SetActorTickEnabled(true);

	// Show the kill-radius telegraph immediately on spawn (replicated telegraph actor, like other boss patterns).
	SpawnKillTelegraph();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LifetimeTimerHandle, this, &ACPP_BossBlackHoleActor::HandleLifetimeExpired, Lifetime, false);
	}
}

////////////////////////////
//! \brief 즉사 반경(KillRadius) 원형 텔레그래프를 스폰한다. 텔레그래프 액터가 스스로 복제되어 전 클라이언트에 표시된다.
void ACPP_BossBlackHoleActor::SpawnKillTelegraph()
{
	if (!HasAuthority() || !KillTelegraphActorClass || KillTelegraph)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FBossHitShapeData KillShape;
	KillShape.Shape = EBossAttackShape::Circle;
	KillShape.InnerRadius = 0.0f;
	KillShape.OuterRadius = KillRadius;
	KillShape.HalfHeight = 200.0f;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	KillTelegraph = World->SpawnActor<ACPP_BossTelegraphActor>(KillTelegraphActorClass, GetActorTransform(), SpawnParameters);
	if (KillTelegraph)
	{
		// Fill duration matches the black hole lifetime so the telegraph completes exactly at the explosion.
		KillTelegraph->Initialize(GetActorTransform(), KillShape, Lifetime);
	}
}

void ACPP_BossBlackHoleActor::DestroyKillTelegraph()
{
	if (KillTelegraph)
	{
		KillTelegraph->Destroy();
		KillTelegraph = nullptr;
	}
}

////////////////////////////
//! \brief PullRadius 내 생존 플레이어에게 당김 소스를 유지하고, 범위를 벗어난 대상의 소스는 제거한다.
void ACPP_BossBlackHoleActor::RefreshPull()
{
	TArray<AActor*> LivingPlayers;
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(this, LivingPlayers);

	const FVector Center = GetActorLocation();

	TSet<ACharacter*> InRangeCharacters;
	for (AActor* LivingPlayer : LivingPlayers)
	{
		ACharacter* Character = Cast<ACharacter>(LivingPlayer);
		if (!Character)
		{
			continue;
		}

		if (FVector::Dist2D(Center, Character->GetActorLocation()) <= PullRadius)
		{
			InRangeCharacters.Add(Character);
			EnsurePullSourceForCharacter(Character);
		}
	}

	// Remove pull sources from characters that left the range or became invalid.
	TArray<TWeakObjectPtr<ACharacter>> TrackedCharacters;
	PullSourceIdsByCharacter.GetKeys(TrackedCharacters);
	for (const TWeakObjectPtr<ACharacter>& TrackedCharacter : TrackedCharacters)
	{
		ACharacter* Character = TrackedCharacter.Get();
		if (!Character || !InRangeCharacters.Contains(Character))
		{
			RemovePullSourceForCharacter(TrackedCharacter);
		}
	}
}

////////////////////////////
//! \brief 한 캐릭터에게 중앙으로 당기는 RadialForce 루트모션 소스를 한 번만 적용하고 ID를 추적한다.
void ACPP_BossBlackHoleActor::EnsurePullSourceForCharacter(ACharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	const TWeakObjectPtr<ACharacter> CharacterPtr(Character);
	if (PullSourceIdsByCharacter.Contains(CharacterPtr))
	{
		return;
	}

	UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
	if (!CharacterMovement)
	{
		return;
	}

	TSharedPtr<FRootMotionSource_RadialForce> PullSource = MakeShared<FRootMotionSource_RadialForce>();
	PullSource->InstanceName = FName(TEXT("BossBlackHolePull"));
	// Additive so the player's own input can resist and walk out of the pull.
	PullSource->AccumulateMode = ERootMotionAccumulateMode::Additive;
	PullSource->Priority = 5;
	PullSource->Location = GetActorLocation();
	PullSource->LocationActor = this;
	PullSource->Radius = PullRadius;
	PullSource->Strength = PullStrength;
	PullSource->bIsPush = false;   // pull toward the center
	PullSource->bNoZForce = true;  // horizontal pull only
	// Outlive the sphere so it never auto-expires mid-encounter; always removed manually on exit/destroy.
	PullSource->Duration = Lifetime + 1.0f;

	const uint16 SourceID = CharacterMovement->ApplyRootMotionSource(PullSource);
	PullSourceIdsByCharacter.Add(CharacterPtr, SourceID);
}

void ACPP_BossBlackHoleActor::RemovePullSourceForCharacter(const TWeakObjectPtr<ACharacter>& CharacterPtr)
{
	const uint16* SourceID = PullSourceIdsByCharacter.Find(CharacterPtr);
	if (!SourceID)
	{
		return;
	}

	if (ACharacter* Character = CharacterPtr.Get())
	{
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->RemoveRootMotionSourceByID(*SourceID);
		}
	}

	PullSourceIdsByCharacter.Remove(CharacterPtr);
}

void ACPP_BossBlackHoleActor::RemoveAllPullSources()
{
	TArray<TWeakObjectPtr<ACharacter>> TrackedCharacters;
	PullSourceIdsByCharacter.GetKeys(TrackedCharacters);
	for (const TWeakObjectPtr<ACharacter>& TrackedCharacter : TrackedCharacters)
	{
		RemovePullSourceForCharacter(TrackedCharacter);
	}
}

////////////////////////////
//! \brief Lifetime 만료(서버): 즉사 판정 → 당김 해제 → 폭발 코스메틱 → 짧은 유지 후 소멸.
void ACPP_BossBlackHoleActor::HandleLifetimeExpired()
{
	ApplyExplosionKill();
	RemoveAllPullSources();
	DestroyKillTelegraph();
	SetActorTickEnabled(false);

	Multicast_PlayExplosionCosmetic();

	UWorld* World = GetWorld();
	if (World && ExplosionLingerTime > 0.0f)
	{
		World->GetTimerManager().SetTimer(ExplosionLingerTimerHandle, this, &ACPP_BossBlackHoleActor::HandleExplosionLingerFinished, ExplosionLingerTime, false);
	}
	else
	{
		Destroy();
	}
}

void ACPP_BossBlackHoleActor::HandleExplosionLingerFinished()
{
	Destroy();
}

////////////////////////////
//! \brief 폭발 순간 KillRadius 내 생존 플레이어를 즉사시킨다(스냅샷).
void ACPP_BossBlackHoleActor::ApplyExplosionKill()
{
	if (!HasAuthority() || !SourceASC || !KillGameplayEffect || KillDamage <= 0.0f)
	{
		return;
	}

	TArray<AActor*> LivingPlayers;
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(this, LivingPlayers);

	const FVector Center = GetActorLocation();
	for (AActor* LivingPlayer : LivingPlayers)
	{
		if (LivingPlayer && FVector::Dist2D(Center, LivingPlayer->GetActorLocation()) <= KillRadius)
		{
			UMyAbilitySystemLibrary::ApplySetByCallerDamageEffectToTargetActor(
				SourceASC,
				LivingPlayer,
				KillGameplayEffect,
				KillDamage,
				1.0f,
				CurseGaugeAmount
			);
		}
	}
}

void ACPP_BossBlackHoleActor::Multicast_PlayExplosionCosmetic_Implementation()
{
	OnBlackHoleExploded();
}

void ACPP_BossBlackHoleActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACPP_BossBlackHoleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (PullRangeSphere)
	{
		PullRangeSphere->SetSphereRadius(PullRadius);
	}
}

void ACPP_BossBlackHoleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllPullSources();
	DestroyKillTelegraph();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifetimeTimerHandle);
		World->GetTimerManager().ClearTimer(ExplosionLingerTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}
