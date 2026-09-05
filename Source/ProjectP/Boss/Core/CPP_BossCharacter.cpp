#include "CPP_BossCharacter.h"

#include "AbilitySystemComponent.h"
#include "Boss/Core/CPP_BossAIController.h"
#include "Boss/Core/CPP_BossAttributeSet.h"
#include "Boss/Core/CPP_BossBrainComponent.h"
#include "Boss/Encounter/CPP_BossEncounterDirectorComponent.h"
#include "Boss/Core/CPP_BossGameplayTags.h"
#include "Boss/Core/CPP_BossTargetingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemy/Core/CPP_EnemyStatTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACPP_BossCharacter::ACPP_BossCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);
	AIControllerClass = ACPP_BossAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	DefaultCharacterTags.AddTag(MyGameplayTags::Character_Enemy.GetTag());
	// 보스를 일반 적과 구분한다. Character.Enemy를 요구하는 기존 Rule은 계층 매칭으로 그대로 걸린다.
	DefaultCharacterTags.AddTag(MyGameplayTags::Character_Enemy_Boss.GetTag());

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	BossAttributeSet = CreateDefaultSubobject<UCPP_BossAttributeSet>(TEXT("BossAttributeSet"));
	BossBrainComponent = CreateDefaultSubobject<UCPP_BossBrainComponent>(TEXT("BossBrainComponent"));
	BossEncounterDirectorComponent = CreateDefaultSubobject<UCPP_BossEncounterDirectorComponent>(TEXT("BossEncounterDirectorComponent"));
	BossTargetingComponent = CreateDefaultSubobject<UCPP_BossTargetingComponent>(TEXT("BossTargetingComponent"));

	// Pattern-gap aiming: the brain sets the AIController focus between patterns, and the CMC turns the boss
	// toward it at RotationRate (slow on purpose — leaves "get behind the boss" as valid melee play).
	// Controller yaw must not drive the pawn directly or the turn would snap instead of interpolate.
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* BossMovement = GetCharacterMovement())
	{
		BossMovement->bUseControllerDesiredRotation = true;
		BossMovement->bOrientRotationToMovement = false;
		BossMovement->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	}
}

void ACPP_BossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BossCharacter, CurrentPhase);
}

void ACPP_BossCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	InitializeBossAbilitySystem();
}

UAbilitySystemComponent* ACPP_BossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UCPP_BossAttributeSet* ACPP_BossCharacter::GetBossAttributeSet() const
{
	return BossAttributeSet;
}

float ACPP_BossCharacter::GetHealth() const
{
	return BossAttributeSet ? BossAttributeSet->GetHealth() : 0.0f;
}

float ACPP_BossCharacter::GetMaxHealth() const
{
	return BossAttributeSet ? BossAttributeSet->GetMaxHealth() : 0.0f;
}

TSubclassOf<UGameplayEffect> ACPP_BossCharacter::GetBossDamageGameplayEffect() const
{
	return BossDamageGameplayEffect;
}

UCPP_BossBrainComponent* ACPP_BossCharacter::GetBossBrainComponent() const
{
	return BossBrainComponent;
}

UCPP_BossTargetingComponent* ACPP_BossCharacter::GetBossTargetingComponent() const
{
	return BossTargetingComponent;
}

UCPP_BossEncounterDirectorComponent* ACPP_BossCharacter::GetBossEncounterDirectorComponent() const
{
	return BossEncounterDirectorComponent;
}

FVector ACPP_BossCharacter::GetArenaCenterLocation() const
{
	return IsValid(ArenaCenterActor) ? ArenaCenterActor->GetActorLocation() : GetActorLocation();
}

EBossPhase ACPP_BossCharacter::GetCurrentPhase() const
{
	return CurrentPhase;
}

bool ACPP_BossCharacter::IsPhaseTransitionPending() const
{
	return bPhaseTransitionPending;
}

float ACPP_BossCharacter::GetPhase2HPThreshold() const
{
	return Phase2HPThreshold;
}

////////////////////////////
//! \author HanSeul
//! \brief Requests phase two from the HP threshold rule and marks the request as consumed.
//! \return true when the HP-based phase-two request is accepted.
bool ACPP_BossCharacter::RequestPhase2ByHP()
{
	if (bPhase2RequestedByHP)
	{
		return false;
	}

	if (!RequestPhaseTwoTransition())
	{
		return false;
	}

	bPhase2RequestedByHP = true;
	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Starts the clear (final judgment) encounter, forwarding to the encounter director. Phase-two only.
//! \return true when the clear encounter starts.
bool ACPP_BossCharacter::RequestClearEncounter()
{
	if (!HasAuthority() || CurrentPhase != EBossPhase::Phase2 || !BossEncounterDirectorComponent || bClearEncounterPending)
	{
		return false;
	}

	// Defer the clear encounter to the next pattern boundary (mirrors the phase-transition pending flow), so the
	// current normal pattern finishes first. Gimmick hazards are cleared immediately at the threshold.
	bClearEncounterPending = true;
	BossEncounterDirectorComponent->ClearGimmickHazards();
	return true;
}

bool ACPP_BossCharacter::IsClearEncounterPending() const
{
	return bClearEncounterPending;
}

////////////////////////////
//! \brief Starts the deferred clear encounter. Called by the boss brain after the current pattern ends.
bool ACPP_BossCharacter::BeginPendingClearEncounter()
{
	if (!HasAuthority() || !bClearEncounterPending || CurrentPhase != EBossPhase::Phase2 || !BossEncounterDirectorComponent)
	{
		return false;
	}

	bClearEncounterPending = false;

	// Same cleanup as the phase transition: the boss enters the clear encounter free of player-applied debuffs.
	RemoveActiveBossDebuffs();

	return BossEncounterDirectorComponent->StartClearEncounter();
}

////////////////////////////
//! \author HanUl
//! \brief 보스에게 걸린 상태이상(Status.Debuff 부여 이펙트)을 전부 제거한다.
//!        페이즈 전환/전멸기 진입 시점에 호출되는 공용 정리 루틴.
void ACPP_BossCharacter::RemoveActiveBossDebuffs()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag DebuffTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff"), false);
	if (!DebuffTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(DebuffTag);
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DebuffTags);
}

bool ACPP_BossCharacter::IsClearEncounterActive() const
{
	return BossEncounterDirectorComponent && BossEncounterDirectorComponent->IsClearEncounterActive();
}

////////////////////////////
//! \author HanUl
//! \brief 전멸기를 무대 세팅과 함께 시작한다: 아레나 중앙으로 연출 텔레포트한 뒤, 등장 시점에
//!        전멸기를 시작하고 브레인을 재시작한다. 텔레포트를 시작할 수 없으면 연출 없이 즉시 시작한다.
//! \return 전멸기 시작(또는 무대 세팅 시작)이 수락되면 true.
bool ACPP_BossCharacter::BeginPendingClearEncounterStaged()
{
	if (!HasAuthority() || !bClearEncounterPending || CurrentPhase != EBossPhase::Phase2 || !BossEncounterDirectorComponent)
	{
		return false;
	}

	if (!BeginStagedTeleport(GetArenaCenterLocation()))
	{
		// Staging unavailable — start the encounter without the theatrics rather than dropping it.
		return BeginPendingClearEncounter();
	}

	bClearEncounterStagingActive = true;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 범용 연출 텔레포트를 시작한다. 숨김+콜리전 해제 후 즉시 목적지로 이동하고(숨김 동안
//!        클라이언트 이동 스무딩이 소화됨), VanishDuration 뒤 밀어내기와 함께 등장한다.
//!        텔레포트 동안 Locked 태그로 브레인 결정과 피격 노출을 막는다.
//! \param Destination 도착 지점(바닥 기준). 캡슐 반높이만큼 올려서 도착한다.
//! \return 텔레포트가 시작되면 true. 이미 진행 중이거나 사망 상태면 false.
bool ACPP_BossCharacter::BeginStagedTeleport(const FVector& Destination)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || bStagedTeleportActive || !World)
	{
		return false;
	}

	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(BossGameplayTags::Boss_State_Dead))
	{
		return false;
	}

	bStagedTeleportActive = true;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(BossGameplayTags::Boss_State_Locked);
	}

	if (UCharacterMovementComponent* BossMovement = GetCharacterMovement())
	{
		BossMovement->StopMovementImmediately();
	}

	Multicast_HandleTeleportVanish(GetActorLocation());

	const UCapsuleComponent* BossCapsule = GetCapsuleComponent();
	const float CapsuleHalfHeight = BossCapsule ? BossCapsule->GetScaledCapsuleHalfHeight() : 0.0f;
	SetActorLocation(Destination + FVector(0.0f, 0.0f, CapsuleHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	ForceNetUpdate();

	World->GetTimerManager().SetTimer(
		StagedTeleportTimerHandle,
		this,
		&ACPP_BossCharacter::HandleStagedTeleportAppear,
		FMath::Max(TeleportVanishDuration, 0.05f),
		false
	);
	return true;
}

bool ACPP_BossCharacter::IsStagedTeleportActive() const
{
	return bStagedTeleportActive;
}

////////////////////////////
//! \author HanUl
//! \brief 연출 텔레포트의 등장 단계: 도착 반경의 플레이어를 밀어내고 보스를 드러낸 뒤 Locked를
//!        해제한다. 전멸기 무대 세팅이었다면 전멸기를 시작하고 브레인을 재시작한다.
void ACPP_BossCharacter::HandleStagedTeleportAppear()
{
	PushPlayersFromTeleportDestination(GetActorLocation());
	Multicast_HandleTeleportAppear(GetActorLocation());

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(BossGameplayTags::Boss_State_Locked);
	}

	bStagedTeleportActive = false;
	OnStagedTeleportFinished.Broadcast();

	if (bClearEncounterStagingActive)
	{
		bClearEncounterStagingActive = false;
		BeginPendingClearEncounter();

		// The boss keeps attacking during the clear encounter (DPS check), so the brain resumes here.
		if (BossBrainComponent)
		{
			BossBrainComponent->StartBrain();
		}
	}

	if (bPhaseTransitionStagingActive)
	{
		bPhaseTransitionStagingActive = false;
		// The brain stays stopped through the transition; the existing completion path restarts it.
		BeginPhaseTwoTransition();
	}
}

////////////////////////////
//! \author HanUl
//! \brief 등장 지점 반경 내 생존 플레이어를 바깥 방향으로 밀어낸다(피해 없음, 서버 판정).
//!        반경/세기/수직 성분은 에디터 프로퍼티로 튜닝한다.
//! \param Center 등장 지점(밀어내기 기준점).
void ACPP_BossCharacter::PushPlayersFromTeleportDestination(const FVector& Center) const
{
	if (TeleportPushRadius <= 0.0f || TeleportPushStrength <= 0.0f)
	{
		return;
	}

	TArray<AActor*> LivingPlayers;
	UMyAbilitySystemLibrary::GetLivingPlayerPawns(this, LivingPlayers);
	for (AActor* PlayerActor : LivingPlayers)
	{
		ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerActor);
		if (!PlayerCharacter || FVector::Dist2D(PlayerCharacter->GetActorLocation(), Center) > TeleportPushRadius)
		{
			continue;
		}

		FVector PushDirection = (PlayerCharacter->GetActorLocation() - Center).GetSafeNormal2D();
		if (PushDirection.IsNearlyZero())
		{
			// Player exactly on the destination: push along the boss facing so the direction stays deterministic.
			PushDirection = GetActorForwardVector().GetSafeNormal2D();
		}

		const FVector LaunchVelocity = PushDirection * TeleportPushStrength + FVector(0.0f, 0.0f, TeleportPushUpwardStrength);
		PlayerCharacter->LaunchCharacter(LaunchVelocity, true, TeleportPushUpwardStrength > 0.0f);
	}
}

////////////////////////////
//! \brief 사라짐 처리(모든 머신 로컬 실행): 숨김 + 콜리전 해제 + 연출 훅. 콜리전을 클라에서도
//!        꺼야 보이지 않는 캡슐에 플레이어 이동 예측이 막히지 않는다.
void ACPP_BossCharacter::Multicast_HandleTeleportVanish_Implementation(FVector VanishLocation)
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	OnTeleportVanishCosmetics(VanishLocation);
}

////////////////////////////
//! \brief 등장 처리(모든 머신 로컬 실행): 숨김 해제 + 콜리전 복구 + 연출 훅.
void ACPP_BossCharacter::Multicast_HandleTeleportAppear_Implementation(FVector AppearLocation)
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	OnTeleportAppearCosmetics(AppearLocation);
}

////////////////////////////
//! \author HanSeul
//! \brief Kills the boss and fires the clear hook once the clear-encounter shield is fully removed.
void ACPP_BossCharacter::HandleBossDefeated()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		// Dead stops the brain from choosing new patterns; CancelAllAbilities stops the attack that is already
		// in flight so the boss does not keep swinging after being defeated.
		AbilitySystemComponent->AddLooseGameplayTag(BossGameplayTags::Boss_State_Dead);
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (BossAttributeSet)
	{
		BossAttributeSet->SetHealth(0.0f);
	}

	OnBossCleared.Broadcast();
}

////////////////////////////
//! \author HanSeul
//! \brief Reserves the phase-two transition without interrupting the current boss action.
//! \return true when the transition request is accepted.
bool ACPP_BossCharacter::RequestPhaseTwoTransition()
{
	if (!HasAuthority() || CurrentPhase != EBossPhase::Phase1 || bPhaseTransitionPending)
	{
		return false;
	}

	bPhaseTransitionPending = true;
	// Clear gimmick hazards immediately at the threshold (the transition itself begins after the current pattern).
	if (BossEncounterDirectorComponent)
	{
		BossEncounterDirectorComponent->ClearGimmickHazards();
	}
	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Enters the phase transition state after a pending transition request.
//! \return true when the transition state is entered.
bool ACPP_BossCharacter::BeginPhaseTwoTransition()
{
	if (!HasAuthority() || CurrentPhase != EBossPhase::Phase1 || !bPhaseTransitionPending)
	{
		return false;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(BossGameplayTags::Boss_State_Locked);
	}
	RemoveActiveBossDebuffs();

	if (!SetPhaseInternal(EBossPhase::Transition))
	{
		return false;
	}

	if (BossEncounterDirectorComponent)
	{
		BossEncounterDirectorComponent->StartPhaseTransitionEncounter();
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 페이즈 전환을 무대 세팅과 함께 시작한다: 아레나 중앙으로 연출 텔레포트한 뒤, 등장 시점에
//!        전환 상태로 들어간다. 텔레포트를 시작할 수 없으면 연출 없이 즉시 전환한다.
//! \return 전환 시작(또는 무대 세팅 시작)이 수락되면 true.
bool ACPP_BossCharacter::BeginPhaseTwoTransitionStaged()
{
	if (!HasAuthority() || CurrentPhase != EBossPhase::Phase1 || !bPhaseTransitionPending)
	{
		return false;
	}

	if (!BeginStagedTeleport(GetArenaCenterLocation()))
	{
		// Staging unavailable — enter the transition without the theatrics rather than dropping it.
		return BeginPhaseTwoTransition();
	}

	bPhaseTransitionStagingActive = true;
	return true;
}

////////////////////////////
//! \author HanSeul
//! \brief Completes the pending transition and activates phase two.
//! \return true when phase two is activated.
bool ACPP_BossCharacter::CompletePhaseTwoTransition()
{
	if (!HasAuthority() || CurrentPhase != EBossPhase::Transition || !bPhaseTransitionPending)
	{
		return false;
	}

	if (!SetPhaseInternal(EBossPhase::Phase2))
	{
		return false;
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(BossGameplayTags::Boss_State_Locked);
	}

	bPhaseTransitionPending = false;
	return true;
}

void ACPP_BossCharacter::InitializeBossAbilitySystem()
{
	if (!AbilitySystemComponent || !BossAttributeSet)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyDefaultCharacterTagsToAbilitySystem(AbilitySystemComponent);

	if (HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(MyGameplayTags::Faction_Enemy_Boss, 1, EGameplayTagReplicationState::TagOnly);
	}

	ApplyDefaultBossAttributes();
	GrantBossAbilities();
	BindHealthChangedDelegate();
	BindMoveSpeedChangedDelegate();

	if (HasAuthority())
	{
		SetPhaseInternal(EBossPhase::Phase1);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 서버 권한 ASC에 DefaultCharacterTags를 Loose Tag로 중복 없이 부여한다.
//! \details 장효제: Faction 태그와 분리된 스트리밍용 Character 정체성 태그를 관리한다.
//! \param ASC 태그를 부여할 AbilitySystemComponent
//! \return 없음
void ACPP_BossCharacter::ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	TArray<FGameplayTag> CharacterTags;
	DefaultCharacterTags.GetGameplayTagArray(CharacterTags);

	for (const FGameplayTag& CharacterTag : CharacterTags)
	{
		if (!CharacterTag.IsValid() || ASC->HasMatchingGameplayTag(CharacterTag))
		{
			continue;
		}

		ASC->AddLooseGameplayTag(CharacterTag, 1, EGameplayTagReplicationState::TagOnly);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief BossBaseStatRow에서 기본 스탯을 읽어 공용 초기화 GameplayEffect의 SetByCaller 값으로 적용한다.
//! \return 없음
void ACPP_BossCharacter::ApplyDefaultBossAttributes()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultBossAttributeEffect)
	{
		return;
	}

	const FString RowContext = FString::Printf(TEXT("%s::ApplyDefaultBossAttributes"), *GetNameSafe(this));
	const FCPP_EnemyBaseStatRow DefaultStatRow;
	const FCPP_EnemyBaseStatRow* StatRow = BossBaseStatRow.DataTable
		? BossBaseStatRow.DataTable->FindRow<FCPP_EnemyBaseStatRow>(
			BossBaseStatRow.RowName,
			RowContext,
			false)
		: nullptr;
	if (!StatRow)
	{
		StatRow = &DefaultStatRow;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		DefaultBossAttributeEffect,
		1.0f,
		EffectContext
	);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_MaxHealth,
		FMath::Max(StatRow->MaxHealth, 1.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_AttackPower,
		FMath::Max(StatRow->AttackPower, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_Defense,
		FMath::Max(StatRow->Defense, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		MyGameplayTags::Data_Stat_MoveSpeed,
		FMath::Max(StatRow->MoveSpeed, 0.0f));

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ACPP_BossCharacter::GrantBossAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultBossAbilitySet)
	{
		return;
	}

	DefaultBossAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles, this);
}

////////////////////////////
//! \author HanSeul
//! \brief Binds boss health changes to the phase-two transition request check.
void ACPP_BossCharacter::BindHealthChangedDelegate()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ACPP_BossCharacter::HandleHealthChanged);
}

////////////////////////////
//! \author HanSeul
//! \brief 보스의 GAS MoveSpeed 변경 델리게이트를 등록하고 현재 값을 실제 이동속도에 반영한다.
//! \return 없음
void ACPP_BossCharacter::BindMoveSpeedChangedDelegate()
{
	if (!AbilitySystemComponent || !BossAttributeSet)
	{
		return;
	}

	AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UCPP_BossAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &ACPP_BossCharacter::HandleMoveSpeedChanged);

	ApplyMoveSpeedToMovement(BossAttributeSet->GetMoveSpeed());
}

////////////////////////////
//! \author HanSeul
//! \brief GAS MoveSpeed 값을 보스 CharacterMovement의 실제 최대 보행 속도에 반영한다.
//! \param NewMoveSpeed 적용할 최대 보행 속도
//! \return 없음
void ACPP_BossCharacter::ApplyMoveSpeedToMovement(float NewMoveSpeed)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(NewMoveSpeed, 0.0f);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 보스의 GAS MoveSpeed Attribute 변경 시 실제 CharacterMovement 속도를 갱신한다.
//! \param ChangeData MoveSpeed의 이전 값과 새 값을 포함하는 변경 데이터
//! \return 없음
void ACPP_BossCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	ApplyMoveSpeedToMovement(ChangeData.NewValue);
}

////////////////////////////
//! \author HanSeul
//! \brief Handles boss health changes and checks whether phase two should be requested.
//! \param ChangeData Attribute change payload from the Ability System Component.
void ACPP_BossCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// NOTE: threat recording does not live here — damage lands via the IncomingDamage meta attribute and
	// SetHealth in the attribute set, so this delegate fires without GEModData (no instigator available).
	// UCPP_BossAttributeSet::PostGameplayEffectExecute feeds the targeting component instead.
	TryRequestPhase2ByHP();
}

////////////////////////////
//! \author HanSeul
//! \brief Requests phase two once when the boss health ratio reaches the configured threshold.
void ACPP_BossCharacter::TryRequestPhase2ByHP()
{
	if (!HasAuthority() || bPhase2RequestedByHP || bPhaseTransitionPending || CurrentPhase != EBossPhase::Phase1)
	{
		return;
	}

	const float MaxHealth = GetMaxHealth();
	if (MaxHealth <= 0.0f)
	{
		return;
	}

	const float HealthRatio = GetHealth() / MaxHealth;
	if (HealthRatio > Phase2HPThreshold)
	{
		return;
	}

	RequestPhase2ByHP();
}

void ACPP_BossCharacter::OnRep_CurrentPhase(EBossPhase PreviousPhase)
{
	if (PreviousPhase != CurrentPhase)
	{
		OnBossPhaseChanged.Broadcast(CurrentPhase);
	}
}

bool ACPP_BossCharacter::SetPhaseInternal(EBossPhase NewPhase)
{
	if (!HasAuthority() || CurrentPhase == NewPhase || !AbilitySystemComponent)
	{
		return false;
	}

	RemoveActivePhaseGameplayEffect();
	CurrentPhase = NewPhase;

	switch (CurrentPhase)
	{
	case EBossPhase::Phase1:
		ActivePhaseEffectHandle = ApplyPhaseGameplayEffect(Phase1GameplayEffect);
		break;
	case EBossPhase::Phase2:
		ActivePhaseEffectHandle = ApplyPhaseGameplayEffect(Phase2GameplayEffect);
		break;
	case EBossPhase::None:
	case EBossPhase::Transition:
	default:
		break;
	}

	OnBossPhaseChanged.Broadcast(CurrentPhase);
	ForceNetUpdate();
	return true;
}

FActiveGameplayEffectHandle ACPP_BossCharacter::ApplyPhaseGameplayEffect(TSubclassOf<UGameplayEffect> PhaseGameplayEffect)
{
	if (!AbilitySystemComponent || !PhaseGameplayEffect)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		PhaseGameplayEffect,
		1.0f,
		EffectContext
	);
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void ACPP_BossCharacter::RemoveActivePhaseGameplayEffect()
{
	if (AbilitySystemComponent && ActivePhaseEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(ActivePhaseEffectHandle);
	}

	ActivePhaseEffectHandle = FActiveGameplayEffectHandle();
}
