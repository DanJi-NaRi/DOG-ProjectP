#include "CPP_DefenseObjective.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "CPP_DefenseObjectiveAttributeSet.h"
#include "GAS/MyAttributeSet.h"
#include "MyGameplayTags.h"

ACPP_DefenseObjective::ACPP_DefenseObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UCPP_DefenseObjectiveAttributeSet>(TEXT("AttributeSet"));
}

void ACPP_DefenseObjective::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
}

void ACPP_DefenseObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_DefenseObjective, bObjectiveDestroyed);
	DOREPLIFETIME(ACPP_DefenseObjective, bDamageEnabled);
}

UAbilitySystemComponent* ACPP_DefenseObjective::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ACPP_DefenseObjective::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float ACPP_DefenseObjective::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

float ACPP_DefenseObjective::GetHealthRatio() const
{
	const float MaxHealth = GetMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 거점의 피해 허용 상태를 변경한다.
//! \param bEnabled 피해를 허용할지 여부
void ACPP_DefenseObjective::SetDamageEnabled(bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bDamageEnabled = bEnabled && !bObjectiveDestroyed;
	BP_OnDamageEnabledChanged(bDamageEnabled);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 거점 체력과 파괴 상태를 초기값으로 복구하고 피해를 비활성화한다.
void ACPP_DefenseObjective::ResetObjective()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	bObjectiveDestroyed = false;
	bDamageEnabled = false;

	const float SafeMaxHealth = FMath::Max(InitialMaxHealth, 1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), SafeMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), SafeMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetShieldAttribute(), 0.0f);

	BP_OnObjectiveReset();
	BP_OnDamageEnabledChanged(false);
}

void ACPP_DefenseObjective::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ACPP_DefenseObjective::HandleHealthChanged);

	if (HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(
			MyGameplayTags::Faction_Objective,
			1,
			EGameplayTagReplicationState::TagOnly);
		ResetObjective();
	}
}

void ACPP_DefenseObjective::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged.Broadcast(ChangeData.NewValue, GetMaxHealth());

	if (HasAuthority() && ChangeData.OldValue > 0.0f && ChangeData.NewValue <= 0.0f)
	{
		HandleObjectiveDestroyed();
	}
}

void ACPP_DefenseObjective::HandleObjectiveDestroyed()
{
	if (bObjectiveDestroyed)
	{
		return;
	}

	bObjectiveDestroyed = true;
	bDamageEnabled = false;
	BP_OnDamageEnabledChanged(false);
	BP_OnObjectiveDestroyed();
	OnObjectiveDestroyed.Broadcast();
}

void ACPP_DefenseObjective::OnRep_ObjectiveDestroyed()
{
	if (bObjectiveDestroyed)
	{
		OnObjectiveDestroyed.Broadcast();
		BP_OnObjectiveDestroyed();
	}
	else
	{
		BP_OnObjectiveReset();
	}
}

void ACPP_DefenseObjective::OnRep_DamageEnabled()
{
	BP_OnDamageEnabledChanged(bDamageEnabled);
}
