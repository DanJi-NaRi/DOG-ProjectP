////////////////////////////
//! \page MySkillIndicatorComponent.cpp
//! \brief MyGAS 스킬 인디케이터 관리 Component 구현 파일이다.

#include "MySkillIndicatorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "../MyPlayerController.h"
#include "MySkillIndicatorActorBase.h"
#include "MySkillIndicatorDataAsset.h"

UMySkillIndicatorComponent::UMySkillIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
}

void UMySkillIndicatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ActiveIndicatorActor)
	{
		SetComponentTickEnabled(false);
		return;
	}

	FMySkillIndicatorResult IndicatorResult;
	if (BuildIndicatorResult(IndicatorResult))
	{
		UpdateIndicator(IndicatorResult);
	}
}

void UMySkillIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearIndicator();

	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \brief DataAsset 설정을 사용해 인디케이터 표시를 시작한다.
//! \param IndicatorDataAsset 인디케이터 표시 설정 DataAsset
//! \return 인디케이터 표시 시작에 성공하면 true
bool UMySkillIndicatorComponent::BeginIndicator(UMySkillIndicatorDataAsset* IndicatorDataAsset)
{
	if (!IndicatorDataAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS indicator begin failed - DataAsset is null. Component: %s"), *GetNameSafe(this));
		return false;
	}

	TSubclassOf<AMySkillIndicatorActorBase> IndicatorActorClass = IndicatorDataAsset->GetIndicatorActorClass();
	if (!IndicatorActorClass)
	{
		IndicatorActorClass = AMySkillIndicatorActorBase::StaticClass();
	}

	return BeginIndicatorWithSpec(IndicatorDataAsset->GetIndicatorSpec(), IndicatorActorClass);
}

////////////////////////////
//! \brief 명시적 표시 설정과 Actor 클래스를 사용해 인디케이터 표시를 시작한다.
//! \param IndicatorSpec 인디케이터 표시 설정
//! \param IndicatorActorClass 스폰할 인디케이터 Actor 클래스
//! \return 인디케이터 표시 시작에 성공하면 true
bool UMySkillIndicatorComponent::BeginIndicatorWithSpec(const FMySkillIndicatorSpec& IndicatorSpec, TSubclassOf<AMySkillIndicatorActorBase> IndicatorActorClass)
{
	if (!IndicatorActorClass)
	{
		IndicatorActorClass = AMySkillIndicatorActorBase::StaticClass();
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS indicator begin failed - World or Owner is null. Component: %s, Owner: %s"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor));
		return false;
	}

	if (!IsLocalIndicatorOwner())
	{
		UE_LOG(LogTemp, Log, TEXT("MyGAS indicator begin skipped - owner is not local. Component: %s, Owner: %s"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor));
		return false;
	}

	ClearIndicator();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveIndicatorActor = World->SpawnActor<AMySkillIndicatorActorBase>(
		IndicatorActorClass,
		OwnerActor->GetActorLocation(),
		OwnerActor->GetActorRotation(),
		SpawnParams
	);

	if (!ActiveIndicatorActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGAS indicator begin failed - SpawnActor returned null. Component: %s, ActorClass: %s"),
			*GetNameSafe(this),
			*GetNameSafe(IndicatorActorClass));
		return false;
	}

	ActiveIndicatorSpec = IndicatorSpec;
	ActiveIndicatorResult = FMySkillIndicatorResult();
	ActiveIndicatorResult.InputTag = IndicatorSpec.InputTag;
	ActiveIndicatorResult.OriginLocation = OwnerActor->GetActorLocation();
	ActiveIndicatorResult.TargetLocation = OwnerActor->GetActorLocation();
	ActiveIndicatorResult.Direction = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	ActiveIndicatorResult.Rotation = OwnerActor->GetActorRotation();

	ActiveIndicatorActor->BeginIndicator(OwnerActor, ActiveIndicatorSpec);
	ActiveIndicatorActor->UpdateIndicator(ActiveIndicatorResult);
	SetComponentTickEnabled(true);

	return true;
}

////////////////////////////
//! \brief 활성 인디케이터의 조준 결과를 갱신한다.
//! \param IndicatorResult 새 조준 결과
void UMySkillIndicatorComponent::UpdateIndicator(const FMySkillIndicatorResult& IndicatorResult)
{
	if (!ActiveIndicatorActor)
	{
		return;
	}

	ActiveIndicatorResult = IndicatorResult;
	ActiveIndicatorActor->UpdateIndicator(ActiveIndicatorResult);
}

////////////////////////////
//! \brief 활성 인디케이터를 확정하고 마지막 조준 결과를 반환한다.
//! \param OutIndicatorResult 확정된 조준 결과
//! \return 확정할 활성 인디케이터가 있으면 true
bool UMySkillIndicatorComponent::ConfirmIndicator(FMySkillIndicatorResult& OutIndicatorResult)
{
	if (!ActiveIndicatorActor)
	{
		return false;
	}

	OutIndicatorResult = ActiveIndicatorResult;
	ActiveIndicatorActor->FinishIndicator();
	ClearIndicator();
	return true;
}

////////////////////////////
//! \brief 활성 인디케이터를 취소하고 정리한다.
void UMySkillIndicatorComponent::CancelIndicator()
{
	if (ActiveIndicatorActor)
	{
		ActiveIndicatorActor->CancelIndicator();
	}

	ClearIndicator();
}

////////////////////////////
//! \brief 활성 인디케이터 Actor와 캐시된 표시 상태를 정리한다.
void UMySkillIndicatorComponent::ClearIndicator()
{
	if (ActiveIndicatorActor)
	{
		ActiveIndicatorActor->Destroy();
		ActiveIndicatorActor = nullptr;
	}

	ActiveIndicatorSpec = FMySkillIndicatorSpec();
	ActiveIndicatorResult = FMySkillIndicatorResult();
	SetComponentTickEnabled(false);
}

////////////////////////////
//! \brief 활성 인디케이터가 있는지 반환한다.
//! \return 활성 인디케이터가 있으면 true
bool UMySkillIndicatorComponent::IsIndicatorActive() const
{
	return ActiveIndicatorActor != nullptr && ActiveIndicatorActor->IsIndicatorActive();
}

////////////////////////////
//! \brief 활성 인디케이터 표시 설정을 반환한다.
//! \return 활성 표시 설정
FMySkillIndicatorSpec UMySkillIndicatorComponent::GetActiveIndicatorSpec() const
{
	return ActiveIndicatorSpec;
}

////////////////////////////
//! \brief 활성 인디케이터 조준 결과를 반환한다.
//! \return 활성 조준 결과
FMySkillIndicatorResult UMySkillIndicatorComponent::GetActiveIndicatorResult() const
{
	return ActiveIndicatorResult;
}

////////////////////////////
//! \brief 현재 Component Owner가 로컬 인디케이터 표시를 수행할 수 있는 Actor인지 확인한다.
//! \return 로컬 조종 Pawn/Controller이거나 Standalone 비네트워크 Actor이면 true
bool UMySkillIndicatorComponent::IsLocalIndicatorOwner() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		return OwnerPawn->IsLocallyControlled();
	}

	if (const AController* OwnerController = Cast<AController>(OwnerActor))
	{
		return OwnerController->IsLocalController();
	}

	return OwnerActor->GetNetMode() != NM_DedicatedServer;
}

////////////////////////////
//! \brief Component Owner와 연결된 PlayerController를 찾는다.
//! \return PlayerController 포인터, 찾지 못하면 nullptr
const APlayerController* UMySkillIndicatorComponent::GetOwningPlayerController() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (const APlayerController* OwnerPlayerController = Cast<APlayerController>(OwnerActor))
	{
		return OwnerPlayerController;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		return Cast<APlayerController>(OwnerPawn->GetController());
	}

	return nullptr;
}

////////////////////////////
//! \brief 현재 Owner, Spec, 마우스 위치를 기준으로 인디케이터 조준 결과를 만든다.
//! \param OutIndicatorResult 계산된 조준 결과
//! \return 계산에 성공하면 true
bool UMySkillIndicatorComponent::BuildIndicatorResult(FMySkillIndicatorResult& OutIndicatorResult) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const FVector OriginLocation = OwnerActor->GetActorLocation();
	FVector TargetLocation = OriginLocation;
	FHitResult TargetHit;
	bool bHasHitResult = false;
	bool bHasResolvedTarget = true;

	const bool bUsesCursor = ActiveIndicatorSpec.bFollowCursor
		|| ActiveIndicatorSpec.IndicatorType == EMySkillIndicatorType::GroundTarget;

	if (bUsesCursor)
	{
		bHasResolvedTarget = ResolveCursorTargetLocation(TargetLocation, TargetHit, bHasHitResult);
		if (!bHasResolvedTarget)
		{
			const FVector FallbackDirection = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
			TargetLocation = OriginLocation + FallbackDirection * ActiveIndicatorSpec.Range;
		}
	}
	else if (!ActiveIndicatorSpec.bFollowOwner && ActiveIndicatorSpec.Range > 0.0f)
	{
		const FVector ForwardDirection = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
		TargetLocation = OriginLocation + ForwardDirection * ActiveIndicatorSpec.Range;
	}

	const float DistanceBeforeClamp = FVector::Dist2D(OriginLocation, TargetLocation);
	if (ActiveIndicatorSpec.bClampToRange)
	{
		TargetLocation = ClampTargetLocationToRange(OriginLocation, TargetLocation);
	}

	FVector Direction = FVector(TargetLocation.X - OriginLocation.X, TargetLocation.Y - OriginLocation.Y, 0.0f);
	if (Direction.IsNearlyZero())
	{
		Direction = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	}
	else
	{
		Direction.Normalize();
	}

	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	const bool bWithinRange = ActiveIndicatorSpec.Range <= 0.0f
		|| DistanceBeforeClamp <= ActiveIndicatorSpec.Range + KINDA_SMALL_NUMBER
		|| ActiveIndicatorSpec.bClampToRange;

	OutIndicatorResult = FMySkillIndicatorResult();
	OutIndicatorResult.InputTag = ActiveIndicatorSpec.InputTag;
	OutIndicatorResult.OriginLocation = OriginLocation;
	OutIndicatorResult.TargetLocation = TargetLocation;
	OutIndicatorResult.Direction = Direction;
	OutIndicatorResult.Rotation = Direction.Rotation();
	OutIndicatorResult.TargetHit = TargetHit;
	OutIndicatorResult.bHasTargetHit = bHasHitResult;
	OutIndicatorResult.bIsValidTarget = !ActiveIndicatorSpec.bCheckValidTarget || (bHasResolvedTarget && bWithinRange);

	return true;
}

////////////////////////////
//! \brief 마우스 커서 기준 TargetLocation을 Trace 또는 평면 투영으로 계산한다.
//! \param OutTargetLocation 계산된 TargetLocation
//! \param OutHitResult Trace 성공 시 HitResult
//! \param bOutHasHitResult Trace HitResult 보유 여부
//! \return 위치 계산에 성공하면 true
bool UMySkillIndicatorComponent::ResolveCursorTargetLocation(FVector& OutTargetLocation, FHitResult& OutHitResult, bool& bOutHasHitResult) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutHitResult = FHitResult();
	bOutHasHitResult = false;

	const AMyPlayerController* MyPlayerController = Cast<AMyPlayerController>(GetOwningPlayerController());
	if (!MyPlayerController)
	{
		return false;
	}

	if (ActiveIndicatorSpec.bSnapToGround)
	{
		if (MyPlayerController->TryGetMouseWorldHitByChannel(
			ActiveIndicatorSpec.GroundTraceChannel,
			OutHitResult,
			CursorTraceDistance
		))
		{
			OutTargetLocation = OutHitResult.ImpactPoint;
			bOutHasHitResult = true;
			return true;
		}

		return false;
	}

	if (bFallbackToPlaneProjection)
	{
		return MyPlayerController->TryGetMouseWorldLocationOnPlane(AimPlaneZ, OutTargetLocation);
	}

	return false;
}

////////////////////////////
//! \brief TargetLocation을 Origin 기준 Range 안으로 제한한다.
//! \param OriginLocation 기준 위치
//! \param TargetLocation 제한 전 목표 위치
//! \return Range가 적용된 목표 위치
FVector UMySkillIndicatorComponent::ClampTargetLocationToRange(const FVector& OriginLocation, const FVector& TargetLocation) const
{
	if (ActiveIndicatorSpec.Range <= 0.0f)
	{
		return TargetLocation;
	}

	const FVector ToTarget2D(TargetLocation.X - OriginLocation.X, TargetLocation.Y - OriginLocation.Y, 0.0f);
	const float Distance2D = ToTarget2D.Size();
	if (Distance2D <= ActiveIndicatorSpec.Range || Distance2D <= KINDA_SMALL_NUMBER)
	{
		return TargetLocation;
	}

	const FVector Clamped2D = OriginLocation + ToTarget2D.GetSafeNormal() * ActiveIndicatorSpec.Range;
	return FVector(Clamped2D.X, Clamped2D.Y, TargetLocation.Z);
}
