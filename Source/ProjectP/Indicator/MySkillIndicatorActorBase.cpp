////////////////////////////
//! \page MySkillIndicatorActorBase.cpp
//! \brief MyGAS 스킬 인디케이터 Actor 기반 클래스 구현 파일이다.

#include "MySkillIndicatorActorBase.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"

AMySkillIndicatorActorBase::AMySkillIndicatorActorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("DecalComponent"));
	DecalComponent->SetupAttachment(SceneRoot);
	DecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	DecalComponent->DecalSize = FVector(DecalProjectionDepth, MinDecalRadius * 2, MinDecalRadius * 2);
	DecalComponent->SetVisibility(false); 
	
	RangeDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("RangeDecalComponent"));
	RangeDecalComponent->SetupAttachment(SceneRoot);
	RangeDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	RangeDecalComponent->SetVisibility(false);
}

////////////////////////////
//! \brief 인디케이터 표시를 시작하고 소유 Actor와 표시 설정을 저장한다.
//! \param InOwnerActor 인디케이터를 요청한 Actor
//! \param InIndicatorSpec 인디케이터 표시 설정
void AMySkillIndicatorActorBase::BeginIndicator(AActor* InOwnerActor, const FMySkillIndicatorSpec& InIndicatorSpec)
{
	OwnerActor = InOwnerActor;
	CurrentIndicatorSpec = InIndicatorSpec;
	CurrentIndicatorResult = FMySkillIndicatorResult();
	CurrentIndicatorResult.InputTag = InIndicatorSpec.InputTag;
	bIndicatorActive = true;

	UpdateDecalVisual();
}

////////////////////////////
//! \brief 인디케이터의 현재 조준 결과를 갱신하고 Actor transform에 반영한다.
//! \param InIndicatorResult 새 조준 결과
void AMySkillIndicatorActorBase::UpdateIndicator(const FMySkillIndicatorResult& InIndicatorResult)
{
	CurrentIndicatorResult = InIndicatorResult;

	FVector IndicatorLocation = CurrentIndicatorResult.TargetLocation;
	FRotator IndicatorRotation(0.0f, CurrentIndicatorResult.Rotation.Yaw, 0.0f);
	if (IsLineDecalIndicatorType())
	{
		FVector OriginLocation = CurrentIndicatorResult.OriginLocation;
		if (OwnerActor && OriginLocation.IsNearlyZero())
		{
			OriginLocation = OwnerActor->GetActorLocation();
		}

		FVector Direction = CurrentIndicatorResult.Direction.GetSafeNormal2D();
		if (Direction.IsNearlyZero() && OwnerActor)
		{
			Direction = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
		}
		if (Direction.IsNearlyZero())
		{
			Direction = FVector::ForwardVector;
		}

		IndicatorLocation = OriginLocation + Direction * (GetDecalLineLength() * 0.5f);
		if (!CurrentIndicatorResult.TargetLocation.IsNearlyZero())
		{
			IndicatorLocation.Z = CurrentIndicatorResult.TargetLocation.Z;
		}
		IndicatorRotation = FRotator(0.0f, Direction.Rotation().Yaw, 0.0f);
	}
	else if (CurrentIndicatorSpec.bFollowOwner && OwnerActor)
	{
		IndicatorLocation = OwnerActor->GetActorLocation();
	}

	IndicatorLocation.Z += DecalSurfaceOffset;
	SetActorLocation(IndicatorLocation);
	SetActorRotation(IndicatorRotation);

	UpdateDecalVisual();
}

////////////////////////////
//! \brief 인디케이터를 확정 상태로 종료한다.
void AMySkillIndicatorActorBase::FinishIndicator()
{
	bIndicatorActive = false;
	UpdateDecalVisual();
}

////////////////////////////
//! \brief 인디케이터를 취소 상태로 종료한다.
void AMySkillIndicatorActorBase::CancelIndicator()
{
	bIndicatorActive = false;
	UpdateDecalVisual();
}

////////////////////////////
//! \brief 현재 인디케이터가 활성 상태인지 반환한다.
//! \return 활성 상태이면 true
bool AMySkillIndicatorActorBase::IsIndicatorActive() const
{
	return bIndicatorActive;
}

////////////////////////////
//! \brief 현재 인디케이터 표시 설정을 반환한다.
//! \return 현재 표시 설정
FMySkillIndicatorSpec AMySkillIndicatorActorBase::GetCurrentIndicatorSpec() const
{
	return CurrentIndicatorSpec;
}

////////////////////////////
//! \brief 현재 인디케이터 조준 결과를 반환한다.
//! \return 현재 조준 결과
FMySkillIndicatorResult AMySkillIndicatorActorBase::GetCurrentIndicatorResult() const
{
	return CurrentIndicatorResult;
}

////////////////////////////
//! \brief 현재 인디케이터 타입이 DecalComponent로 표현되는 타입인지 확인한다.
//! \return 현재 MVP에서 Decal로 표현할 수 있는 타입이면 true
bool AMySkillIndicatorActorBase::IsDecalIndicatorType() const
{
	return CurrentIndicatorSpec.IndicatorType == EMySkillIndicatorType::Circle
		|| CurrentIndicatorSpec.IndicatorType == EMySkillIndicatorType::GroundTarget
		|| IsLineDecalIndicatorType();
}

////////////////////////////
//! \brief 현재 인디케이터 타입이 선형 Decal로 표현되는 타입인지 확인한다.
//! \return Line 또는 ProjectilePreview이면 true
bool AMySkillIndicatorActorBase::IsLineDecalIndicatorType() const
{
	return CurrentIndicatorSpec.IndicatorType == EMySkillIndicatorType::Line
		|| CurrentIndicatorSpec.IndicatorType == EMySkillIndicatorType::ProjectilePreview;
}

////////////////////////////
//! \brief 현재 표시 설정에서 Decal 반경을 계산한다.
//! \return 최소값이 보정된 Decal 반경
float AMySkillIndicatorActorBase::GetDecalRadius() const
{
	return FMath::Max(CurrentIndicatorSpec.Radius, MinDecalRadius);
}

////////////////////////////
//! \brief 선형 Decal의 길이를 계산한다.
//! \return Range를 우선 사용한 선형 Decal 길이
float AMySkillIndicatorActorBase::GetDecalLineLength() const
{
	return FMath::Max(CurrentIndicatorSpec.Range, MinDecalRadius * 2.0f);
}

////////////////////////////
//! \brief 선형 Decal의 폭을 계산한다.
//! \return Width, Radius 지름, 최소 반경 순서로 보정한 폭
float AMySkillIndicatorActorBase::GetDecalLineWidth() const
{
	if (CurrentIndicatorSpec.Width > 0.0f)
	{
		return CurrentIndicatorSpec.Width;
	}

	if (CurrentIndicatorSpec.Radius > 0.0f)
	{
		return CurrentIndicatorSpec.Radius * 2.0f;
	}

	return MinDecalRadius * 2.0f;
}

////////////////////////////
//! \brief 현재 유효 판정에 맞는 Decal Material을 반환한다.
//! \return 사용할 Material, 설정되지 않았으면 nullptr
UMaterialInterface* AMySkillIndicatorActorBase::GetDecalMaterial() const
{
	if (CurrentIndicatorSpec.bCheckValidTarget && !CurrentIndicatorResult.bIsValidTarget && CurrentIndicatorSpec.InvalidMaterial)
	{
		return CurrentIndicatorSpec.InvalidMaterial;
	}

	return CurrentIndicatorSpec.ValidMaterial;
}

////////////////////////////
//! \brief Decal 표시 상태, 크기, Material을 현재 데이터에 맞게 갱신한다.
void AMySkillIndicatorActorBase::UpdateDecalVisual()
{
	// 1. 기존 목표 위치 (마우스) Target Decal 업데이트
	if (DecalComponent)
	{
		const bool bShouldShowDecal = bIndicatorActive && IsDecalIndicatorType();
		DecalComponent->SetVisibility(bShouldShowDecal);
		DecalComponent->SetHiddenInGame(!bShouldShowDecal);

		if (bShouldShowDecal)
		{
			if (IsLineDecalIndicatorType())
			{
				// Line 타입
				DecalComponent->DecalSize = FVector(DecalProjectionDepth, GetDecalLineWidth() * 0.5f, GetDecalLineLength() * 0.5f);
			}
			else
			{
				// Circle 타입
				const float DecalRadius = GetDecalRadius();
				DecalComponent->DecalSize = FVector(DecalProjectionDepth, DecalRadius, DecalRadius );
			}

			DecalComponent->SetRelativeLocation(FVector::ZeroVector);
			DecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

			if (UMaterialInterface* DecalMaterial = GetDecalMaterial())
			{
				DecalComponent->SetDecalMaterial(DecalMaterial);
			}
		}
	}

	// 2. 시전 가능 사거리 (Range) Visual 데칼 업데이트
	if (RangeDecalComponent)
	{
		const bool bShouldShowRange = bIndicatorActive && CurrentIndicatorSpec.bShowRangeVisual && CurrentIndicatorSpec.Range > 0.0f;
		RangeDecalComponent->SetVisibility(bShouldShowRange);
		RangeDecalComponent->SetHiddenInGame(!bShouldShowRange);

		if (bShouldShowRange)
		{
			RangeDecalComponent->DecalSize = FVector(DecalProjectionDepth, CurrentIndicatorSpec.Range, CurrentIndicatorSpec.Range);

			if (CurrentIndicatorSpec.RangeMaterial)
			{
				RangeDecalComponent->SetDecalMaterial(CurrentIndicatorSpec.RangeMaterial);
			}

			// Range 데칼은 Actor 좌표가 마우스를 쫓아가더라도 절대 좌표계에서 Origin(플레이어 발밑)을 유지해야 함
			FVector OriginLoc = CurrentIndicatorResult.OriginLocation;
			if (OriginLoc.IsNearlyZero() && OwnerActor)
			{
				OriginLoc = OwnerActor->GetActorLocation();
			}

			FVector RangeLocation = OriginLoc;
			RangeLocation.Z += DecalSurfaceOffset;

			RangeDecalComponent->SetWorldLocation(RangeLocation);
			RangeDecalComponent->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
		}
	}

}
