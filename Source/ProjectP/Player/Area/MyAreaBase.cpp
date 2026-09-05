////////////////////////////
//! \file MyAreaBase.cpp
//! \brief 기능 없는 장판 VFX 기반 Actor 구현 파일이다.

#include "MyAreaBase.h"

#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"

////////////////////////////
//! \author HanUl
//! \brief 장판 VFX Actor의 기본 컴포넌트를 생성한다.
//! \param 없음
//! \return 없음
AMyAreaBase::AMyAreaBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	DecalComponent->SetupAttachment(SceneRoot);
	DecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
	NiagaraComponent->SetupAttachment(SceneRoot);

	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLight"));
	PointLightComponent->SetupAttachment(SceneRoot);
	PointLightComponent->SetIntensity(0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 필요하면 에디터 미리보기용 반경과 지속시간을 BeginPlay에 적용한다.
//! \param 없음
//! \return 없음
void AMyAreaBase::BeginPlay()
{
	Super::BeginPlay();

	if (bApplyPreviewSpecOnBeginPlay)
	{
		ApplyAreaVisualSpec(PreviewRadius, PreviewDuration);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 장판 반경과 지속시간을 Decal, Niagara, Light, 수명에 반영한다.
//! \param InRadius 장판 반경(cm)
//! \param InDuration 장판 지속시간(초)
//! \return 없음
void AMyAreaBase::ApplyAreaVisualSpec(float InRadius, float InDuration)
{
	CurrentRadius = FMath::Max(InRadius, 0.0f);
	CurrentDuration = FMath::Max(InDuration, 0.0f);

	ApplyDecalVisualSpec(CurrentRadius);
	ApplyNiagaraVisualSpec(CurrentRadius, CurrentDuration);
	ApplyPointLightVisualSpec(CurrentRadius);

	if (bApplyLifeSpanFromDuration && CurrentDuration > 0.0f)
	{
		SetLifeSpan(CurrentDuration);
	}

	OnAreaVisualSpecApplied(CurrentRadius, CurrentDuration);
}

////////////////////////////
//! \author HanUl
//! \brief Decal 컴포넌트의 가로세로 크기를 장판 반경에 맞춘다.
//! \param Radius 장판 반경(cm)
//! \return 없음
void AMyAreaBase::ApplyDecalVisualSpec(float Radius)
{
	if (!DecalComponent || !bApplyDecalSizeFromRadius || Radius <= 0.0f)
	{
		return;
	}

	const float Diameter = Radius * 2.0f;
	DecalComponent->DecalSize = FVector(DecalDepth, Diameter, Diameter);
}

////////////////////////////
//! \author HanUl
//! \brief Niagara User Parameter에 장판 반경, 지름, 지속시간을 전달한다.
//! \param Radius 장판 반경(cm)
//! \param Duration 장판 지속시간(초)
//! \return 없음
void AMyAreaBase::ApplyNiagaraVisualSpec(float Radius, float Duration)
{
	if (!NiagaraComponent)
	{
		return;
	}

	const float Diameter = Radius * 2.0f;
	NiagaraComponent->SetVariableFloat(TEXT("User.Radius"), Radius);
	NiagaraComponent->SetVariableFloat(TEXT("Radius"), Radius);
	NiagaraComponent->SetVariableFloat(TEXT("User.Diameter"), Diameter);
	NiagaraComponent->SetVariableFloat(TEXT("Diameter"), Diameter);
	NiagaraComponent->SetVariableFloat(TEXT("User.Duration"), Duration);
	NiagaraComponent->SetVariableFloat(TEXT("Duration"), Duration);

	if (bReinitializeNiagaraOnApply)
	{
		NiagaraComponent->ReinitializeSystem();
	}
}

////////////////////////////
//! \author HanUl
//! \brief PointLight 영향 반경을 장판 반경에 맞춘다.
//! \param Radius 장판 반경(cm)
//! \return 없음
void AMyAreaBase::ApplyPointLightVisualSpec(float Radius)
{
	if (!PointLightComponent || !bApplyPointLightRadius || Radius <= 0.0f)
	{
		return;
	}

	PointLightComponent->SetAttenuationRadius(Radius);
}
