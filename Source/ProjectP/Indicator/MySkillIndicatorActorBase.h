////////////////////////////
//! \page MySkillIndicatorActorBase.h
//! \brief MyGAS 스킬 인디케이터 Actor 기반 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySkillIndicatorTypes.h"
#include "MySkillIndicatorActorBase.generated.h"

class USceneComponent;
class UDecalComponent;
class UMaterialInterface;

////////////////////////////
//! \class AMySkillIndicatorActorBase
//! \brief 스킬 조준 중 월드에 표시되는 인디케이터 Actor의 공통 기반 클래스이다.
UCLASS(Blueprintable)
class PROJECTP_API AMySkillIndicatorActorBase : public AActor
{
	GENERATED_BODY()

public:
	AMySkillIndicatorActorBase();

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	virtual void BeginIndicator(AActor* InOwnerActor, const FMySkillIndicatorSpec& InIndicatorSpec);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	virtual void UpdateIndicator(const FMySkillIndicatorResult& InIndicatorResult);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	virtual void FinishIndicator();

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	virtual void CancelIndicator();

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	bool IsIndicatorActive() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	FMySkillIndicatorSpec GetCurrentIndicatorSpec() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	FMySkillIndicatorResult GetCurrentIndicatorResult() const;

protected:
	bool IsDecalIndicatorType() const;
	bool IsLineDecalIndicatorType() const;
	float GetDecalRadius() const;
	float GetDecalLineLength() const;
	float GetDecalLineWidth() const;
	UMaterialInterface* GetDecalMaterial() const;
	void UpdateDecalVisual();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Indicator")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Indicator")
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Indicator")
	TObjectPtr<UDecalComponent> RangeDecalComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator|Decal", meta = (ClampMin = "1.0"))
	float DecalProjectionDepth = 256.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator|Decal", meta = (ClampMin = "0.0"))
	float DecalSurfaceOffset = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator|Decal", meta = (ClampMin = "1.0"))
	float MinDecalRadius = 32.0f;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Indicator")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Indicator")
	FMySkillIndicatorSpec CurrentIndicatorSpec;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Indicator")
	FMySkillIndicatorResult CurrentIndicatorResult;

	UPROPERTY(BlueprintReadOnly, Category = "MyGAS|Indicator")
	bool bIndicatorActive = false;
};
