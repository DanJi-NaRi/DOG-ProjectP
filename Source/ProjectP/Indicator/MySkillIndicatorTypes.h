////////////////////////////
//! \page MySkillIndicatorTypes.h
//! \brief MyGAS 스킬 인디케이터 공통 타입 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Materials/MaterialInterface.h"
#include "MySkillIndicatorTypes.generated.h"

////////////////////////////
//! \enum EMySkillIndicatorType
//! \brief 스킬 인디케이터가 표현할 기본 형태를 정의한다.
UENUM(BlueprintType)
enum class EMySkillIndicatorType : uint8
{
	None UMETA(DisplayName = "None"),
	Circle UMETA(DisplayName = "Circle"),
	Line UMETA(DisplayName = "Line"),
	Cone UMETA(DisplayName = "Cone"),
	GroundTarget UMETA(DisplayName = "Ground Target"),
	ProjectilePreview UMETA(DisplayName = "Projectile Preview")
};

////////////////////////////
//! \struct FMySkillIndicatorSpec
//! \brief 인디케이터 표시와 갱신에 필요한 공통 설정값이다.
USTRUCT(BlueprintType)
struct FMySkillIndicatorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	EMySkillIndicatorType IndicatorType = EMySkillIndicatorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0"))
	float Range = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0"))
	float Width = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float Angle = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bFollowOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bFollowCursor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bSnapToGround = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bCheckValidTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bClampToRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bShowRangeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	TObjectPtr<UMaterialInterface> RangeMaterial;
};

////////////////////////////
//! \struct FMySkillIndicatorResult
//! \brief 인디케이터 확정 시 Ability 발동에 전달할 조준 결과이다.
USTRUCT(BlueprintType)
struct FMySkillIndicatorResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FVector OriginLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	FHitResult TargetHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bHasTargetHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MyGAS|Indicator")
	bool bIsValidTarget = true;
};
