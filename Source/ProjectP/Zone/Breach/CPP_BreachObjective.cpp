#include "Zone/Breach/CPP_BreachObjective.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "GAS/MyAttributeSet.h"
#include "MyGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Zone/Breach/CPP_BreachObjectiveAttributeSet.h"

ACPP_BreachObjective::ACPP_BreachObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UCPP_BreachObjectiveAttributeSet>(TEXT("AttributeSet"));
}

void ACPP_BreachObjective::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
}

void ACPP_BreachObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BreachObjective, bDamageEnabled);
}

UAbilitySystemComponent* ACPP_BreachObjective::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 돌파 목표의 공격 수신 활성 상태를 변경한다.
//! \param bEnabled 공격을 돌파 적중으로 인정할지 여부
//! \return
void ACPP_BreachObjective::SetDamageEnabled(bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bDamageEnabled = bEnabled;
	BP_OnDamageEnabledChanged(bDamageEnabled);
	ForceNetUpdate();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 목표의 GAS 상태를 복구하고 공격 수신을 비활성화한다.
//! \param
//! \return
void ACPP_BreachObjective::ResetObjective()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	bDamageEnabled = false;
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), 1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), 1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetShieldAttribute(), 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetIncomingDamageAttribute(), 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetIncomingCriticalHitAttribute(), 0.0f);

	BP_OnObjectiveReset();
	BP_OnDamageEnabledChanged(false);
	ForceNetUpdate();
}

void ACPP_BreachObjective::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(
			MyGameplayTags::Faction_Objective,
			1,
			EGameplayTagReplicationState::TagOnly);
		ResetObjective();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 유효 공격자를 돌파 적중 이벤트로 통지하고 Blueprint 피격 연출을 실행한다.
//! \param SourceActor 피해 GameplayEffect의 공격자 Actor
//! \return
void ACPP_BreachObjective::HandleIncomingDamage(AActor* SourceActor)
{
	if (!HasAuthority() || !bDamageEnabled || !IsValid(SourceActor))
	{
		return;
	}

	Multicast_PlayObjectiveHitEffects();
	OnBreachObjectiveHit.Broadcast(SourceActor);
}

void ACPP_BreachObjective::Multicast_PlayObjectiveHitEffects_Implementation()
{
	BP_OnObjectiveHit();
}

void ACPP_BreachObjective::OnRep_DamageEnabled()
{
	BP_OnDamageEnabledChanged(bDamageEnabled);
}
