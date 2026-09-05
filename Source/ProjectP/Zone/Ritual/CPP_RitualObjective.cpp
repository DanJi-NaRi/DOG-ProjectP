#include "Zone/Ritual/CPP_RitualObjective.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GAS/MyAttributeSet.h"
#include "MyGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Zone/Ritual/CPP_RitualObjectiveAttributeSet.h"

ACPP_RitualObjective::ACPP_RitualObjective()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	AbsorptionRange = CreateDefaultSubobject<USphereComponent>(TEXT("AbsorptionRange"));
	SetRootComponent(AbsorptionRange);
	AbsorptionRange->InitSphereRadius(150.0f);
	AbsorptionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AbsorptionRange->SetCollisionObjectType(ECC_Pawn);
	AbsorptionRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	AbsorptionRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AbsorptionRange->SetGenerateOverlapEvents(true);
	AbsorptionRange->SetCanEverAffectNavigation(false);
	AbsorptionRange->CanCharacterStepUpOn = ECB_No;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UCPP_RitualObjectiveAttributeSet>(TEXT("AttributeSet"));
}

void ACPP_RitualObjective::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
	AbsorptionRange->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ACPP_RitualObjective::HandleAbsorptionRangeBeginOverlap);
}

void ACPP_RitualObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_RitualObjective, CurrentGauge);
	DOREPLIFETIME(ACPP_RitualObjective, bAbsorptionEnabled);
}

UAbilitySystemComponent* ACPP_RitualObjective::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 의식 목표의 흡수 가능 상태를 변경한다.
//! \param bEnabled 범위 진입을 흡수 요청으로 인정할지 여부
void ACPP_RitualObjective::SetAbsorptionEnabled(bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bAbsorptionEnabled = bEnabled && CurrentGauge < GetMaxGauge();
	BP_OnAbsorptionEnabledChanged(bAbsorptionEnabled);
	ForceNetUpdate();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 의식 게이지와 흡수 상태를 초기화한다.
void ACPP_RitualObjective::ResetObjective()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	CurrentGauge = 0.0f;
	bAbsorptionEnabled = false;
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), 1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), 1.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetShieldAttribute(), 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetIncomingDamageAttribute(), 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetIncomingCriticalHitAttribute(), 0.0f);

	OnGaugeChanged.Broadcast(CurrentGauge, GetMaxGauge());
	BP_OnObjectiveReset();
	BP_OnAbsorptionEnabledChanged(false);
	ForceNetUpdate();
}

////////////////////////////
//! \author HanSeul
//! \brief 검증된 적 하나를 흡수해 게이지를 증가시키고 전 클라이언트에 흡수 연출을 전달한다.
//! \param AbsorbedActor 흡수되는 적 Actor
//! \return 흡수가 적용되었으면 true
bool ACPP_RitualObjective::ApplyAbsorption(AActor* AbsorbedActor)
{
	if (!HasAuthority() || !bAbsorptionEnabled || !IsValid(AbsorbedActor))
	{
		return false;
	}

	const FVector AbsorbedLocation = AbsorbedActor->GetActorLocation();
	CurrentGauge = FMath::Clamp(
		CurrentGauge + FMath::Max(GaugeIncreasePerAbsorption, 0.1f),
		0.0f,
		GetMaxGauge());
	OnGaugeChanged.Broadcast(CurrentGauge, GetMaxGauge());
	Multicast_PlayAbsorptionEffects(AbsorbedLocation);
	ForceNetUpdate();

	if (CurrentGauge >= GetMaxGauge())
	{
		bAbsorptionEnabled = false;
		BP_OnAbsorptionEnabledChanged(false);
		Multicast_PlayGaugeFullEffects();
		OnGaugeFull.Broadcast();
	}

	return true;
}

void ACPP_RitualObjective::InitializeAbilitySystem()
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

void ACPP_RitualObjective::HandleAbsorptionRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (HasAuthority() && bAbsorptionEnabled && IsValid(OtherActor))
	{
		OnObjectiveEntered.Broadcast(OtherActor);
	}
}

void ACPP_RitualObjective::Multicast_PlayAbsorptionEffects_Implementation(FVector AbsorbedLocation)
{
	BP_OnAbsorbed(AbsorbedLocation);
}

void ACPP_RitualObjective::Multicast_PlayGaugeFullEffects_Implementation()
{
	BP_OnGaugeFull();
}

void ACPP_RitualObjective::OnRep_CurrentGauge()
{
	OnGaugeChanged.Broadcast(CurrentGauge, GetMaxGauge());
}

void ACPP_RitualObjective::OnRep_AbsorptionEnabled()
{
	BP_OnAbsorptionEnabledChanged(bAbsorptionEnabled);
}
