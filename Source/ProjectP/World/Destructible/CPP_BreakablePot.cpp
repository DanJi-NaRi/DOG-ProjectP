#include "World/Destructible/CPP_BreakablePot.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/MyAttributeSet.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "MyGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "World/Destructible/CPP_BreakableAttributeSet.h"
#include "MyGameplayTags.h"
#include "Streaming/MyStreamingPayloads.h"

ACPP_BreakablePot::ACPP_BreakablePot()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	IntactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactMesh"));
	IntactMesh->SetupAttachment(SceneRoot);
	IntactMesh->SetCollisionObjectType(ECC_Destructible);

	GeometryCollection = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	GeometryCollection->SetupAttachment(SceneRoot);
	GeometryCollection->SetCollisionObjectType(ECC_Destructible);
	GeometryCollection->SetVisibility(false, true);
	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeometryCollection->SetSimulatePhysics(false);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UCPP_BreakableAttributeSet>(TEXT("AttributeSet"));
}

void ACPP_BreakablePot::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->AddLooseGameplayTag(MyGameplayTags::Target_Destructible);

	if (HasAuthority())
	{
		AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetMaxHealthAttribute(), InitialHealth);
		AbilitySystemComponent->SetNumericAttributeBase(UMyAttributeSet::GetHealthAttribute(), InitialHealth);
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ACPP_BreakablePot::HandleHealthChanged);
	}
}

void ACPP_BreakablePot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFadingOut)
	{
		UpdateFadeFromServerTime();
	}
}

UAbilitySystemComponent* ACPP_BreakablePot::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ACPP_BreakablePot::OnRep_Broken()
{
	if (bBroken)
	{
		ApplyBrokenVisualState();
	}
}

////////////////////////////
//! \author HanSeul
//! \brief GAS Health가 0이 된 순간 서버에서 항아리 파괴를 시작한다.
//! \param ChangeData Health 변경 전후 값
void ACPP_BreakablePot::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (HasAuthority() && !bBroken && ChangeData.NewValue <= 0.0f)
	{
		BreakPot();
		// 스트리밍 조건이 셀 수 있도록 파괴 사실을 남긴다. bBroken 검사가 중복을 막는다.
		MyStreamingCountEvent::BroadcastCountEvent(
			this,
			MyGameplayTags::Streaming_Event_World_PotBroken,
			FGameplayTag());
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 파괴 상태를 복제하고 서버의 온전한 메시를 Geometry Collection으로 전환한다.
void ACPP_BreakablePot::BreakPot()
{
	if (!HasAuthority() || bBroken)
	{
		return;
	}

	ResolvedBreakImpulseStrength = ResolveBreakImpulseStrength(AttributeSet->GetLastReceivedDamage());
	BreakServerTime = GetSynchronizedServerTime();
	bBroken = true;
	ApplyBrokenVisualState();
	ForceNetUpdate();

	const float TotalLifetime = BrokenPieceLifetime + FadeDuration;
	if (TotalLifetime <= 0.0f)
	{
		DestroyPotOnServer();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			ServerDestroyTimerHandle,
			this,
			&ACPP_BreakablePot::DestroyPotOnServer,
			TotalLifetime,
			false
		);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 온전한 메시를 숨기고 Geometry Collection의 충돌과 Chaos 물리를 활성화한다.
void ACPP_BreakablePot::ApplyBrokenVisualState()
{
	IntactMesh->SetVisibility(false, true);
	IntactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GeometryCollection->SetVisibility(true, true);
	GeometryCollection->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeometryCollection->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GeometryCollection->SetSimulatePhysics(true);
	GeometryCollection->SetScalarParameterValueOnMaterials(TEXT("FadeAmount"), 1.0f);
	ApplyBreakForce();
	RefreshFadeState();
}

////////////////////////////
//! \author HanSeul
//! \brief 서버 파괴 시각을 기준으로 현재 조각 유지 또는 페이드 단계를 복구한다.
void ACPP_BreakablePot::RefreshFadeState()
{
	GetWorldTimerManager().ClearTimer(FadeDelayTimerHandle);
	bFadingOut = false;
	SetActorTickEnabled(false);

	const double ElapsedTime = FMath::Max(0.0, GetSynchronizedServerTime() - BreakServerTime);
	if (ElapsedTime >= BrokenPieceLifetime)
	{
		BeginFadeOut();
		return;
	}

	GeometryCollection->SetScalarParameterValueOnMaterials(TEXT("FadeAmount"), 1.0f);
	const float RemainingDelay = static_cast<float>(BrokenPieceLifetime - ElapsedTime);
	GetWorldTimerManager().SetTimer(
		FadeDelayTimerHandle,
		this,
		&ACPP_BreakablePot::BeginFadeOut,
		RemainingDelay,
		false
	);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버 파괴 시각을 기준으로 파괴 조각의 페이드를 시작한다.
void ACPP_BreakablePot::BeginFadeOut()
{
	bFadingOut = true;
	SetActorTickEnabled(true);
	UpdateFadeFromServerTime();
}

void ACPP_BreakablePot::UpdateFadeFromServerTime()
{
	const double ElapsedTime = GetSynchronizedServerTime() - BreakServerTime;
	const double FadeElapsedTime = ElapsedTime - BrokenPieceLifetime;
	const float FadeProgress = FadeDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(static_cast<float>(FadeElapsedTime / FadeDuration), 0.0f, 1.0f);

	GeometryCollection->SetScalarParameterValueOnMaterials(TEXT("FadeAmount"), 1.0f - FadeProgress);

	if (FadeProgress >= 1.0f)
	{
		bFadingOut = false;
		SetActorTickEnabled(false);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief Geometry Collection의 활성 클러스터를 조각으로 분리하고 방사형 충격을 적용한다.
void ACPP_BreakablePot::ApplyBreakForce()
{
	GeometryCollection->CrumbleActiveClusters();
	GeometryCollection->AddRadialImpulse(
		GeometryCollection->GetComponentLocation(),
		BreakImpulseRadius,
		ResolvedBreakImpulseStrength,
		ERadialImpulseFalloff::RIF_Linear,
		true
	);
}

////////////////////////////
//! \author HanSeul
//! \brief 파괴 조각의 수명이 끝난 항아리 액터를 서버에서 제거한다.
void ACPP_BreakablePot::DestroyPotOnServer()
{
	if (HasAuthority())
	{
		Destroy();
	}
}

double ACPP_BreakablePot::GetSynchronizedServerTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0;
}

////////////////////////////
//! \author HanSeul
//! \brief 실제 피해량이 속한 구간에 맞는 Geometry Collection 충격력을 반환한다.
//! \param DamageAmount 항아리가 받은 실제 피해량
//! \return 피해 구간에 설정된 충격력
float ACPP_BreakablePot::ResolveBreakImpulseStrength(float DamageAmount) const
{
	if (DamageAmount < MediumDamageThreshold)
	{
		return LowDamageImpulseStrength;
	}

	if (DamageAmount < HighDamageThreshold)
	{
		return BreakImpulseStrength;
	}

	return HighDamageImpulseStrength;
}

void ACPP_BreakablePot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACPP_BreakablePot, ResolvedBreakImpulseStrength);
	DOREPLIFETIME(ACPP_BreakablePot, BreakServerTime);
	DOREPLIFETIME(ACPP_BreakablePot, bBroken);
}
