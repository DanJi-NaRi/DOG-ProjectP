#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Boss/Core/CPP_BossTypes.h"
#include "CPP_BossAttackData.generated.h"

class ACPP_BossTelegraphActor;
class UAnimMontage;

USTRUCT(BlueprintType)
struct PROJECTP_API FBossHitShapeData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape")
	EBossAttackShape Shape = EBossAttackShape::Circle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (Units = "cm"))
	FVector LocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape != EBossAttackShape::Circle", EditConditionHides, Units = "deg"))
	float LocalYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape == EBossAttackShape::Circle || Shape == EBossAttackShape::Sector", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float InnerRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape == EBossAttackShape::Circle || Shape == EBossAttackShape::Sector", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float OuterRadius = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape == EBossAttackShape::Sector", EditConditionHides, ClampMin = "0.0", ClampMax = "360.0", Units = "deg"))
	float SectorAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape == EBossAttackShape::Rectangle", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float ForwardLength = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (EditCondition = "Shape == EBossAttackShape::Rectangle", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float HalfWidth = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float HalfHeight = 100.0f;
};

USTRUCT(BlueprintType)
struct PROJECTP_API FBossAttackWindowData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Window")
	FName WindowId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Window")
	TArray<FBossHitShapeData> HitShapes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Window", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attack|Window", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float CurseGaugeAmount = 0.0f;
};

UCLASS(BlueprintType)
class PROJECTP_API UCPP_BossAttackData : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Boss|Attack|Data")
	UAnimMontage* GetAttackMontage() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Attack|Data")
	TSubclassOf<ACPP_BossTelegraphActor> GetTelegraphActorClass() const;

	const TArray<FBossAttackWindowData>& GetAttackWindows() const;
	const FBossAttackWindowData* FindAttackWindow(FName WindowId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attack|Data", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ACPP_BossTelegraphActor> TelegraphActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attack|Data", meta = (AllowPrivateAccess = "true"))
	TArray<FBossAttackWindowData> AttackWindows;
};
