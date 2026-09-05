////////////////////////////
//! \page MyNeferSanctuaryArea.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyNeferSanctuaryArea.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USphereComponent;

////////////////////////////
//! \class AMyNeferSanctuaryArea
//! \brief Nefer의 Sanctuary of Isis가 설치하는 장판 비주얼 Actor이다.
//! \note 회복 적용은 UMyGA_Nefer_SanctuaryOfIsis의 장판 틱이 담당하며, 이 Actor는 시각 표현과 수명 관리만 한다.
UCLASS()
class PROJECTP_API AMyNeferSanctuaryArea : public AActor
{
	GENERATED_BODY()

public:
	AMyNeferSanctuaryArea();

	////////////////////////////
	//! \brief Sanctuary 장판의 시전자, 반경, 지속시간을 초기화한다.
	//! \param InSourceActor 장판을 설치한 Actor
	//! \param InAreaRadius 장판 반경(cm)
	//! \param InDuration 장판 지속시간(초)
	void InitializeSanctuary(
		AActor* InSourceActor,
		float InAreaRadius,
		float InDuration
	);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nefer|Sanctuary")
	TObjectPtr<USphereComponent> AreaComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Sanctuary")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Sanctuary")
	float AreaRadius = 400.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Sanctuary")
	float Duration = 6.0f;

private:
	UFUNCTION()
	void HandleSourceDestroyed(AActor* DestroyedActor);

	void HandleDurationFinished();

	FTimerHandle DurationTimerHandle;
};
