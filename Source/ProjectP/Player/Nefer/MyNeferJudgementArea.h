////////////////////////////
//! \page MyNeferJudgementArea.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyNeferJudgementArea.generated.h"

class USphereComponent;

////////////////////////////
//! \class AMyNeferJudgementArea
//! \brief Nefer의 Judgement of Isis가 설치하는 장판 비주얼 Actor이다.
//! \note 폭발/틱/Decay 피해와 처치 회복 적용은 UMyGA_Nefer_JudgementOfIsis가 담당하며, 이 Actor는 시각 표현과 수명 관리만 한다.
UCLASS()
class PROJECTP_API AMyNeferJudgementArea : public AActor
{
	GENERATED_BODY()

public:
	AMyNeferJudgementArea();

	////////////////////////////
	//! \brief Judgement 장판의 시전자, 반경, 지속시간을 초기화한다.
	//! \param InSourceActor 장판을 설치한 Actor
	//! \param InAreaRadius 장판 반경(cm)
	//! \param InDuration 장판 지속시간(초)
	void InitializeJudgementArea(
		AActor* InSourceActor,
		float InAreaRadius,
		float InDuration
	);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nefer|Judgement Of Isis")
	TObjectPtr<USphereComponent> AreaComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Judgement Of Isis")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Judgement Of Isis")
	float AreaRadius = 600.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Nefer|Judgement Of Isis")
	float Duration = 8.0f;

private:
	UFUNCTION()
	void HandleSourceDestroyed(AActor* DestroyedActor);

	void HandleDurationFinished();

	FTimerHandle DurationTimerHandle;
};
