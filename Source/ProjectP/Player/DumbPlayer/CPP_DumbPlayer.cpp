#include "CPP_DumbPlayer.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "../../GAS/MyAbilitySystemLibrary.h"
#include "../../GAS/MyAttributeSet.h"
#include "../../MyGameplayTags.h"

ACPP_DumbPlayer::ACPP_DumbPlayer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MyAttributeSet = CreateDefaultSubobject<UMyAttributeSet>(TEXT("MyAttributeSet"));
}

void ACPP_DumbPlayer::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
}

UAbilitySystemComponent* ACPP_DumbPlayer::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

////////////////////////////
//! \author HanSeul
//! \brief 지정한 적 방향으로 기본 공격 Gameplay Ability를 발동한다.
//! \param TargetActor 공격 방향을 결정할 대상 액터
//! \return 기본 공격 발동 요청에 성공하면 true
bool ACPP_DumbPlayer::ActivateBasicAttack(AActor* TargetActor)
{
	if (!HasAuthority() || !AbilitySystemComponent || !IsValid(TargetActor) || TargetActor == this)
	{
		return false;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}

	FGameplayEventData EventData;
	EventData.EventTag = MyGameplayTags::Input_Skill_Basic;
	EventData.Instigator = this;
	EventData.Target = TargetActor;
	EventData.EventMagnitude = ToTarget.Rotation().Yaw;

	return UMyAbilitySystemLibrary::TryActivateAbilityByInputTagWithEventData(
		AbilitySystemComponent,
		MyGameplayTags::Input_Skill_Basic,
		EventData
	);
}

AActor* ACPP_DumbPlayer::GetLeaderActor() const
{
	return LeaderActor;
}

void ACPP_DumbPlayer::SetLeaderActor(AActor* NewLeaderActor)
{
	LeaderActor = NewLeaderActor;
}

////////////////////////////
//! \author HanSeul
//! \brief 이 AI가 소유하는 AbilitySystemComponent를 초기화한다.
void ACPP_DumbPlayer::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent || !MyAttributeSet)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		AbilitySystemComponent->AddLooseGameplayTag(MyGameplayTags::Faction_Player, 1, EGameplayTagReplicationState::TagOnly);
	}

	ApplyDefaultAttributes();
	GrantDefaultAbilities();
}

void ACPP_DumbPlayer::ApplyDefaultAttributes()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultAttributeEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		DefaultAttributeEffect,
		1.0f,
		EffectContext
	);
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void ACPP_DumbPlayer::GrantDefaultAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent || !DefaultAbilitySet)
	{
		return;
	}

	DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilityHandles, this);
}
