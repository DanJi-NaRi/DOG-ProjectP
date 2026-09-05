// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_EnemySpawnPoint.h"

#include "Components/BoxComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

////////////////////////////
//! \author HanUl
//! \brief 스폰 지점의 루트 씬 컴포넌트를 만들고 에디터 시각화용 화살표를 부착한다.
//! \param
//! \return
ACPP_EnemySpawnPoint::ACPP_EnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->SetupAttachment(Root);
		ArrowComponent->ArrowColor = FColor::Red;
		ArrowComponent->bIsEditorOnly = true;
	}

	// SpawnExtent 시각화용. 충돌 없이 와이어프레임만, 게임 중 숨김.
	ExtentVisual = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtentVisual"));
	if (ExtentVisual)
	{
		ExtentVisual->SetupAttachment(Root);
		ExtentVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ExtentVisual->SetCollisionProfileName(TEXT("NoCollision"));
		ExtentVisual->SetGenerateOverlapEvents(false);
		ExtentVisual->ShapeColor = FColor::Green;
		ExtentVisual->bIsEditorOnly = true;
		ExtentVisual->bHiddenInGame = true;
		// 액터 스케일을 무시(위치/회전만 상속)해 실제 스폰 범위(스케일 미반영)와 시각화가 항상 일치하도록 한다.
		ExtentVisual->SetUsingAbsoluteScale(true);
		// Z는 시각화용 얇은 높이. XY는 SpawnExtent.
		ExtentVisual->SetBoxExtent(FVector(SpawnExtent.X, SpawnExtent.Y, 10.f), false);
	}
#endif
}

#if WITH_EDITORONLY_DATA
////////////////////////////
//! \author HanUl
//! \brief SpawnExtent 값을 시각화 박스 크기(XY)에 반영한다.
//! \param
//! \return
void ACPP_EnemySpawnPoint::SyncExtentVisual()
{
	if (ExtentVisual)
	{
		ExtentVisual->SetBoxExtent(FVector(SpawnExtent.X, SpawnExtent.Y, 10.f), false);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 레벨 배치/이동 시 시각화 박스를 최신 SpawnExtent로 맞춘다.
//! \param Transform 배치 트랜스폼
//! \return
void ACPP_EnemySpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncExtentVisual();
}

////////////////////////////
//! \author HanUl
//! \brief 디테일 패널에서 SpawnExtent를 편집하면 시각화 박스를 즉시 갱신한다.
//! \param PropertyChangedEvent 변경된 프로퍼티 정보
//! \return
void ACPP_EnemySpawnPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// FVector2D의 X/Y를 편집하면 GetPropertyName()은 leaf("X"/"Y")를 반환하므로 멤버 프로퍼티로 판정한다.
	const FName MemberName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (MemberName == GET_MEMBER_NAME_CHECKED(ACPP_EnemySpawnPoint, SpawnExtent))
	{
		SyncExtentVisual();
	}
}
#endif
