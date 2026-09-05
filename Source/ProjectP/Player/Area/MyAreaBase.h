////////////////////////////
//! \page MyAreaBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyAreaBase.generated.h"

class UDecalComponent;
class UNiagaraComponent;
class UPointLightComponent;
class USceneComponent;

////////////////////////////
//! \class AMyAreaBase
//! \brief SkillDefinition의 Area 값을 시각 요소에 반영하기 위한 기능 없는 장판 VFX 기반 Actor이다.
UCLASS(Blueprintable)
class PROJECTP_API AMyAreaBase : public AActor
{
	GENERATED_BODY()

public:
	AMyAreaBase();

	////////////////////////////
	//! \brief Definition에서 전달된 장판 반경과 지속시간을 시각 컴포넌트에 반영한다.
	//! \param InRadius 장판 반경(cm)
	//! \param InDuration 장판 지속시간(초)
	UFUNCTION(BlueprintCallable, Category = "MyGAS|Area")
	void ApplyAreaVisualSpec(float InRadius, float InDuration);

	////////////////////////////
	//! \brief 현재 적용된 장판 반경을 반환한다.
	//! \return 장판 반경(cm)
	UFUNCTION(BlueprintPure, Category = "MyGAS|Area")
	float GetCurrentRadius() const { return CurrentRadius; }

	////////////////////////////
	//! \brief 현재 적용된 장판 지속시간을 반환한다.
	//! \return 장판 지속시간(초)
	UFUNCTION(BlueprintPure, Category = "MyGAS|Area")
	float GetCurrentDuration() const { return CurrentDuration; }

protected:
	virtual void BeginPlay() override;

	////////////////////////////
	//! \brief 반경과 지속시간 반영 후 BP가 추가 시각 처리를 할 수 있도록 호출된다.
	//! \param Radius 장판 반경(cm)
	//! \param Duration 장판 지속시간(초)
	UFUNCTION(BlueprintImplementableEvent, Category = "MyGAS|Area")
	void OnAreaVisualSpecApplied(float Radius, float Duration);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPointLightComponent> PointLightComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyGAS|Area|Preview", meta = (ClampMin = "0.0"))
	float PreviewRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyGAS|Area|Preview", meta = (ClampMin = "0.0"))
	float PreviewDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MyGAS|Area|Preview")
	bool bApplyPreviewSpecOnBeginPlay = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area|Decal", meta = (ClampMin = "0.0"))
	float DecalDepth = 256.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area|Decal")
	bool bApplyDecalSizeFromRadius = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area|Niagara")
	bool bReinitializeNiagaraOnApply = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area|Light")
	bool bApplyPointLightRadius = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MyGAS|Area|Lifetime")
	bool bApplyLifeSpanFromDuration = true;

private:
	void ApplyDecalVisualSpec(float Radius);
	void ApplyNiagaraVisualSpec(float Radius, float Duration);
	void ApplyPointLightVisualSpec(float Radius);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	float CurrentRadius = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MyGAS|Area", meta = (AllowPrivateAccess = "true"))
	float CurrentDuration = 0.0f;
};
