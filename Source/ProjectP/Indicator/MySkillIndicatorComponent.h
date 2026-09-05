////////////////////////////
//! \page MySkillIndicatorComponent.h
//! \brief MyGAS 스킬 인디케이터 관리 Component 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MySkillIndicatorTypes.h"
#include "MySkillIndicatorComponent.generated.h"

class AMySkillIndicatorActorBase;
class UMySkillIndicatorDataAsset;

////////////////////////////
//! \class UMySkillIndicatorComponent
//! \brief 로컬 플레이어의 스킬 인디케이터 표시, 갱신, 확정, 취소를 관리하는 Component이다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UMySkillIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMySkillIndicatorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	bool BeginIndicator(UMySkillIndicatorDataAsset* IndicatorDataAsset);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	bool BeginIndicatorWithSpec(const FMySkillIndicatorSpec& IndicatorSpec, TSubclassOf<AMySkillIndicatorActorBase> IndicatorActorClass);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	void UpdateIndicator(const FMySkillIndicatorResult& IndicatorResult);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	bool ConfirmIndicator(FMySkillIndicatorResult& OutIndicatorResult);

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	void CancelIndicator();

	UFUNCTION(BlueprintCallable, Category = "MyGAS|Indicator")
	void ClearIndicator();

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	bool IsIndicatorActive() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	FMySkillIndicatorSpec GetActiveIndicatorSpec() const;

	UFUNCTION(BlueprintPure, Category = "MyGAS|Indicator")
	FMySkillIndicatorResult GetActiveIndicatorResult() const;

private:
	bool IsLocalIndicatorOwner() const;
	const APlayerController* GetOwningPlayerController() const;
	bool BuildIndicatorResult(FMySkillIndicatorResult& OutIndicatorResult) const;
	bool ResolveCursorTargetLocation(FVector& OutTargetLocation, FHitResult& OutHitResult, bool& bOutHasHitResult) const;
	FVector ClampTargetLocationToRange(const FVector& OriginLocation, const FVector& TargetLocation) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator", meta = (AllowPrivateAccess = "true"))
	float AimPlaneZ = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float CursorTraceDistance = 100000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Indicator", meta = (AllowPrivateAccess = "true"))
	bool bFallbackToPlaneProjection = true;

	UPROPERTY(Transient)
	TObjectPtr<AMySkillIndicatorActorBase> ActiveIndicatorActor;

	UPROPERTY(Transient)
	FMySkillIndicatorSpec ActiveIndicatorSpec;

	UPROPERTY(Transient)
	FMySkillIndicatorResult ActiveIndicatorResult;
};
